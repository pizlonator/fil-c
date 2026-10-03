--!nonstrict

-- Local functions: `call`/`ret` to file-local subroutines with custom
-- caller/callee register conventions (the OpenSSL perlasm shape: a
-- non-`.globl`, non-signature label whose region contains a `ret`, called from
-- function bodies — or from other subroutines — with a private register
-- convention).
--
-- Model (per the approved design): a local call is an unconditional jump to a
-- per-caller CLONE of the subroutine; a local `ret` is a multi-way branch to
-- the continuations (the instructions after each call). The stack pointer is
-- never moved (no hardware push): at each callsite the transform materializes
-- the continuation address into one shared regalloc-owned "retaddr" temp per
-- (caller, clone), and each clone `ret` emits a compare-chain dispatch over
-- the clone's continuations. Return-address storage is therefore off-limits to
-- the program by construction (it lives in a web the input cannot address).
-- Clones are appended to the caller's body with renamed labels, so lift /
-- ptrflow / DCE / regalloc all color caller+clones as ONE function — which is
-- exactly what makes the custom register conventions (and caller-saved
-- registers the sub clobbers) work: reaching-definitions unions the sub's
-- register effects with the caller's webs across the call boundary.
--
-- This module is the shared front half:
--   * discover: identify subroutine regions at top level (a mid-body label
--     INSIDE a subroutine — OpenSSL's .Lenc_loop6 / _vpaes_schedule_low_round
--     — is a valid call target; the clone is then the region statements
--     REACHABLE from the mid-label, in source order — which may include
--     backward-branch loop heads BEFORE the label, like aesni's
--     .Lenc_loop8 reached from a `call .Lenc_loop8_enter`) and, x86_64 only,
--     labels INSIDE a signatured function's body (aesni set_encrypt_key's
--     `call .Lkey_expansion_128` self-calls): such a mid-body label's region
--     is a pristine copy of the owner's whole body entered at that label, so
--     the clone is exactly the code a hardware `call` would run — see the
--     mid-body soundness validation below for the frame-access rule that
--     makes cross-function landings safe,
--     claim the regions referenced by an unannotated or `#! local` call,
--     reject recursion (direct or mutual), fall-off-the-end bodies, and
--     branches that escape the clone range — EXCEPT a mid-body TAIL JOIN into
--     another subroutine's region (mont5's `jmp .Lsqr4x_sub_entry`): a B2
--     cross-function jump (tailcall.luau), recorded as a "tailjoin" edge
--     whose target tail augment clones into each caller with the jumping
--     subroutine's clone id (the +8 context and the ret dispatch are the
--     jumping subroutine's — the tail executes inside its activation). Tail
--     joins out of a mid-body clone of a signatured function are rejected:
--     the tail's +8 context would be the owner's, and the owner's frame does
--     not exist in a foreign caller's activation.
--     Discovery marks the claimed statements consumed so the driver's
--     top-level scan skips them. On arm64 discovery is a clean "not yet
--     supported" error.
--   * augment: append one renamed clone per (transitively) called subroutine
--     to each calling function's body, marking callsites (st.localCall) and
--     rets (st.localRet), inserting continuation labels, and applying the +8
--     displacement rule (below).
--
-- The +8 rule: hardware `call` pushes an 8-byte return address, so a
-- subroutine written against the real ABI sees the caller's `0(%rsp)` as its
-- own `8(%rsp)` (eight OpenSSL subs access the caller's frame with exactly
-- this return-address-compensated convention: rsaz __rsaz_512_mul/mulx/
-- reduce/reducex, mont5 mul4x_internal/mulx4x_internal, aesni-gcm
-- _aesni_ctr32_ghash_6x). Under the no-push clone model the frame pass keys a
-- clone statement's rsp-relative access at model depth d to frame offset
-- disp + D0 - d - 8 (hardware: the return-address word sits between the
-- clone's rsp and the caller's frame) — so the sub's `8(%rsp)` keys to the
-- caller's slot 0 at the callsite depth, and (with the caller's buffer a
-- fixed alloca region) `leaq 8(%rsp), %rdi` keys to region offset 0, which
-- the region redirect resolves to the region pointer. rbp-relative operands
-- are not biased (the call does not move rbp), and anchored-region accesses
-- keep their raw coordinates (the sub's own frame is its own).
--
-- The per-arch hooks live in: x86_64_isa.classify (marks classify as
-- branches, so every CFG consumer sees the true flow), x86_64_frame
-- (successors through the marks; the localRet depth-0 exemption), the
-- transform (retaddr temps, callsite lea+jmp emission, ret dispatch emission,
-- flags treated as call-clobbered across local calls). Nothing in regalloc /
-- emit / render changes.

local x86_64_parse = require("./x86_64_parse")
local x86_64_isa = require("./x86_64_isa")
local tailcall = require("./tailcall")
local err = require("./err")

local localcall = {}

-- Directives that switch the current section (mirrors sarcasm.luau): a
-- `.size`-less subroutine region also ends at one of these.
local SECTION_DIRECTIVES = {
	[".section"] = true, [".text"] = true, [".data"] = true, [".rodata"] = true,
	[".bss"] = true, [".previous"] = true,
	[".pushsection"] = true, [".popsection"] = true,
}

local function isCallMnem(m, arch)
	m = string.lower(m or "")
	if arch == "arm64" then return m == "bl" end
	return m == "call" or m == "callq"
end

local function isRetMnem(m, arch)
	m = string.lower(m or "")
	if arch == "arm64" then return m == "ret" end
	return x86_64_isa.baseMnemonic(m) == "ret"
end

-- Unconditionally trapping mnemonics (ud2 — including the `.byte 0x0f,0x0b`
-- spelling, which decodes to ud2 — int3/int $3/hlt): a control-flow path through
-- a trap has NO successors, exactly like ret. Mirrors x86_64_isa.classify's
-- fallsThrough=false and x86_64_frame's successors(); the clone-range walk below
-- is mnemonic-based (it runs on raw source statements, before classify), so it
-- needs its own copy of the trap set.
local TRAP_MNEM = { ud2 = true, int3 = true, int = true, hlt = true }

local function isTrapMnem(m)
	return TRAP_MNEM[string.lower(m or "")] == true
end

-- Is `st` a potentially-local call (a direct call with a bare-symbol target,
-- unannotated or carrying the explicit `#! local` marker)?
local function localCallTarget(st, arch)
	if st.kind ~= "insn" or not isCallMnem(st.mnemonic, arch) then return nil end
	if st.annotation ~= nil and st.annotation ~= "local" then return nil end
	local op = st.operands and st.operands[1]
	if op and op.kind == "sym" then return op.text end
	return nil
end

-- Prescan: the set of targets of every potentially-local call in the file.
-- splitFunctions uses this to leave a called no-signature `.type name,@function`
-- label unconsumed for discovery (instead of raising its no-signature error).
function localcall.prescanCalls(stmts, arch)
	local called = {}
	for _, st in stmts do
		local t = localCallTarget(st, arch)
		if t then called[t] = true end
	end
	return called
end

-- Discover the local subroutines. `stmts` is the parsed file, `consumed` the
-- splitFunctions consumption map (function name table per statement index),
-- `globals` the .globl set. `opts` may carry { functions = <splitFunctions
-- result>, aliases = <entry-alias name -> owner name> } for the sig'd-function
-- alias-entry claims below. Marks claimed region statements consumed and
-- returns nil (no local subroutines) or {
--   subs       = sub name -> { name, region, entryPos },
--   edges      = sub name -> { sub names it calls, in body order },
--   callsByFn  = function name -> { { stmt, target } ... },   -- direct calls
--   callsBySub = sub name  -> { { stmt, target } ... },       -- nested calls
-- }.
function localcall.discover(stmts, consumed, globals, arch, opts)
	opts = opts or {}

	-- Region identification. A subroutine region is a maximal span of
	-- unconsumed top-level statements headed by a non-.globl, non-signature
	-- label, bounded by consumed (function) content, a section switch, a
	-- `.size`, or a `.type OTHER,@function` (mirroring splitFunctions' body
	-- termination). Every later label in the span is a MID-LABEL of the
	-- region: OpenSSL calls labels inside a subroutine's body (`.Lenc_loop6`
	-- inside `_aesni_encrypt6`, `_vpaes_schedule_low_round` inside
	-- `_vpaes_schedule_round`), and the called subroutine for cloning purposes
	-- is [mid-label .. the region's ret(s)].
	local regions = {}
	local labelRegion = {}   -- label name -> its region
	local cur = nil
	for i, st in stmts do
		if consumed[i] then
			cur = nil
		elseif st.kind == "label" then
			local hasSig = st.annotation ~= nil and string.find(st.annotation, "%(") ~= nil
			if globals[st.name] or hasSig then
				-- a .globl or signature-annotated label is never subroutine content
				cur = nil
			else
				if not cur then
					cur = { stmts = {}, idx = {}, labels = {}, entry = st.name }
					table.insert(regions, cur)
				end
				if not cur.labels[st.name] then cur.labels[st.name] = #cur.stmts + 1 end
				table.insert(cur.stmts, st)
				table.insert(cur.idx, i)
			end
		elseif st.kind == "directive" and SECTION_DIRECTIVES[st.name] then
			cur = nil
		elseif st.kind == "directive" and st.name == ".size" then
			cur = nil
		elseif st.kind == "directive" and st.name == ".type" then
			local nm, kind = string.match(st.args or "", "^%s*([%w_.$]+)%s*,%s*(.+)$")
			if cur and nm and nm ~= cur.entry and kind and string.find(kind, "function", 1, true) then
				cur = nil
			end
		elseif cur then
			table.insert(cur.stmts, st)
			table.insert(cur.idx, i)
		end
	end

	-- x86_64: decode .byte-encoded instructions (every OpenSSL subroutine ends
	-- with `.byte 0xf3,0xc3` = rep ret) so ret detection sees spelled
	-- instructions. Undecodable sequences stay `.byte` directives and hit the
	-- usual in-body data rejection once cloned.
	if arch == "x86_64" then
		for _, r in regions do
			r.stmts = x86_64_parse.decodeByteInsns(r.stmts)
			r.labels = {}
			for pos, st in r.stmts do
				if st.kind == "label" and not r.labels[st.name] then
					r.labels[st.name] = pos
				end
			end
		end
	end
	for _, r in regions do
		for name in r.labels do labelRegion[name] = r end
	end

	-- Callsites in function bodies (direct calls) and inside regions (nested
	-- calls). consumed[i] is the owning function table for function content
	-- and nil for unconsumed (region) statements at this point.
	local fnCallsites = {}
	for i, st in stmts do
		local t = localCallTarget(st, arch)
		if t and type(consumed[i]) == "table" then
			table.insert(fnCallsites, { stmt = st, target = t, fn = consumed[i],
				annotated = st.annotation == "local" })
		end
	end

	-- Sig'd-function alias entries (x86_64): an unannotated (or `#! local`)
	-- call to an entry-adjacent ALIAS of a signatured function — mont5's
	-- `call __bn_sqr8x_internal` naming the second label at bn_sqr8x_internal's
	-- entry — claims the function's WHOLE body as the clone region (the alias
	-- is entry-adjacent, so the call enters the function exactly at its entry;
	-- the clone — frame setup included, rets dispatching to continuations — is
	-- then exactly the hardware call+ret under the no-push model). This is the
	-- only sound reading: such a body may address the caller's frame with the
	-- +8 return-address-compensated convention (bn_sqr8x_internal writes its
	-- tp[] into the caller's frame at 48+8(%rsp)), which no standalone sig'd
	-- callee may do — annotating the call and compiling it as an ordinary
	-- Fil-C-CC call would give the body its own frame and silently
	-- miscompile those accesses. An unannotated call to the function's OWN
	-- entry label keeps the annotate-the-callsite rejection (a direct sig'd
	-- call is the normal annotated form), and a call to a .GLOBL alias keeps
	-- it too (an exported symbol marshals through the CC). arm64 is unchanged:
	-- no claims are added there.
	local fnByName = {}
	local aliasFn = {}   -- claimable alias name -> owning function's entry name
	if arch == "x86_64" then
		for _, fn in (opts.functions or {}) do fnByName[fn.name] = fn end
		for a, o in (opts.aliases or {}) do
			if fnByName[o] then aliasFn[a] = o end
		end
	end
	-- The clone region for a sig'd function's body: a PRISTINE deep copy (the
	-- transform rewrites some memory operands in place, so the owner's
	-- already-compiled body is not a valid clone source), .byte-decoded (the
	-- driver's later whole-body decode of the owner is independent). idx is
	-- empty: the statements stay consumed by the owning function (the
	-- standalone compile proceeds normally alongside the clones).
	local fnRegions = {}
	local function fnRegion(owner)
		local r = fnRegions[owner]
		if r then return r end
		local fn = fnByName[owner]
		local fstmts = {}
		for pos, st in fn.body do fstmts[pos] = tailcall.deepCopyStmt(st) end
		fstmts = x86_64_parse.decodeByteInsns(fstmts)
		r = { stmts = fstmts, idx = {}, labels = {}, entry = owner, fnSub = true }
		for pos, st in fstmts do
			if st.kind == "label" and not r.labels[st.name] then r.labels[st.name] = pos end
		end
		fnRegions[owner] = r
		return r
	end

	-- Mid-body labels of signatured functions (x86_64): a call to a label
	-- INSIDE a sig'd function's body — not the entry itself, not an
	-- entry-adjacent alias (those resolve through aliasFn above), not .globl —
	-- resolves to a local subroutine whose region is the owner's WHOLE-body
	-- pristine copy entered at that label (the fnRegion machinery: the clone
	-- range is the reachable set from the label, so the clone is exactly the
	-- code a hardware `call` to that label would run). This is the aesni
	-- set_encrypt_key shape — `${PREFIX}_set_encrypt_key`/`__aesni_set_encrypt_key`
	-- calling its own .Lkey_expansion_* routines mid-body — which needs no
	-- perlasm restructuring: the key-expansion code stays inside the
	-- signatured function, and each caller (the function's own body, and any
	-- entry-adjacent-alias whole-body clone) gets its own renamed copy.
	local midOwner = {}
	if arch == "x86_64" then
		local aliasNames = opts.aliases or {}
		for _, fn in (opts.functions or {}) do
			for _, st in fn.body do
				if st.kind == "label" and st.name ~= fn.name
					and not globals[st.name] and not aliasNames[st.name]
					and not midOwner[st.name] then
					midOwner[st.name] = fn.name
				end
			end
		end
	end

	-- Claiming: a label is a local subroutine iff it is defined in a region
	-- (or names a sig'd function's alias entry or one of its mid-body labels,
	-- x86_64 only), is not .globl, and is referenced by a call from a function
	-- body or from an (already claimed) subroutine. Worklist over claim edges;
	-- claim order follows statement order (deterministic).
	local subs = {}
	local order = {}
	local function claimable(name)
		return (labelRegion[name] ~= nil or aliasFn[name] ~= nil
			or midOwner[name] ~= nil) and not globals[name]
	end
	local function claim(name)
		if subs[name] then return end
		if labelRegion[name] then
			local r = labelRegion[name]
			subs[name] = { name = name, region = r, entryPos = r.labels[name] }
		elseif aliasFn[name] then
			local r = fnRegion(aliasFn[name])
			subs[name] = { name = name, region = r, entryPos = r.labels[name] }
		else
			local r = fnRegion(midOwner[name])
			subs[name] = { name = name, region = r, entryPos = r.labels[name],
				midOwner = midOwner[name] }
		end
		table.insert(order, name)
	end
	local function localMismatch(cs)
		err.at(cs.stmt, "`local` annotation on call to '" .. cs.target
			.. "', which does not resolve to a local subroutine: "
			.. (cs.stmt.raw or cs.target))
	end
	for _, cs in fnCallsites do
		if claimable(cs.target) then
			claim(cs.target)
		elseif cs.annotated then
			localMismatch(cs)
		end
	end
	local qi = 1
	while qi <= #order do
		local sub = subs[order[qi]]
		qi += 1
		local r = sub.region
		for pos = sub.entryPos, #r.stmts do
			local st = r.stmts[pos]
			local t = localCallTarget(st, arch)
			if t then
				if claimable(t) then
					claim(t)
				elseif st.annotation == "local" then
					localMismatch({ stmt = st, target = t })
				end
			end
		end
	end

	if not order[1] then return nil end   -- no local subroutines

	-- arm64: the same model works there later (bl -> b, continuation
	-- materialized with adrp/add into a web, compare-chain dispatch); for now
	-- it is a clean compile-time rejection.
	if arch == "arm64" then
		-- Locate a callsite statement that claimed the first subroutine so the
		-- rejection names where the unsupported local call happens.
		local firstCsStmt
		for _, cs in fnCallsites do
			if cs.target == order[1] then firstCsStmt = cs.stmt break end
		end
		err.at(firstCsStmt, "local functions are not yet supported on arm64"
			.. " (local subroutine '" .. order[1] .. "' is called with a custom"
			.. " register convention)")
	end

	-- Reachable clone ranges (x86_64). The called subroutine is the set of
	-- region statements REACHABLE from the entry label, following fall-through
	-- and in-region branch targets — which may name labels BEFORE the entry
	-- (aesni's `call .Lenc_loop8_enter`: the called range contains a backward
	-- branch to the .Lenc_loop8 loop head, which precedes the entry label, so
	-- the naive [entry .. region end] span cannot contain the loop). The clone
	-- is the reachable set in source order — the whole loop structure, exactly
	-- like tailcall.luau's reachability-based region extraction for B2 shared
	-- tails. A region-head entry (the common case) reaches [entry .. end] minus
	-- any dead code, so existing subroutines clone exactly as before.
	for _, name in order do
		local sub = subs[name]
		local r = sub.region
		local labelPos = {}
		for pos, st in r.stmts do
			if st.kind == "label" then
				if not labelPos[st.name] then labelPos[st.name] = {} end
				table.insert(labelPos[st.name], pos)
			end
		end
		sub.labelPos = labelPos
		local reachable, work = {}, { sub.entryPos }
		while #work > 0 do
			local pos = table.remove(work)
			if not reachable[pos] then
				reachable[pos] = true
				if pos <= #r.stmts then
					local st = r.stmts[pos]
					local ft = true
					if st.kind == "insn" then
						local m = string.lower(st.mnemonic)
						local b = x86_64_isa.baseMnemonic(m)
						if isRetMnem(st.mnemonic, arch) then
							ft = false
						elseif b == "jmp" or b == "jcc" or string.match(m, "^loop") then
							local op = st.operands and st.operands[1]
							if op and op.kind == "sym" then
								for _, p in (labelPos[op.text] or {}) do table.insert(work, p) end
							end
							ft = b ~= "jmp"
						elseif isTrapMnem(st.mnemonic) then
							-- a trap terminates the path (no successors, no
							-- fall-through), exactly like ret
							ft = false
						end
					end
					if ft then table.insert(work, pos + 1) end
				end
			end
		end
		local clonePos = {}
		for pos = 1, #r.stmts do
			if reachable[pos] then table.insert(clonePos, pos) end
		end
		sub.clonePos = clonePos
		sub.reachesEnd = reachable[#r.stmts + 1] and true or false
	end

	-- Call edges (sub -> subs called from within its clone range) and the
	-- per-caller/per-sub callsite lists, all in body order (deterministic).
	local edges, callsBySub = {}, {}
	for _, name in order do
		local sub = subs[name]
		local es, cs = {}, {}
		local r = sub.region
		for _, pos in sub.clonePos do
			local t = localCallTarget(r.stmts[pos], arch)
			if t and subs[t] then
				table.insert(es, t)
				table.insert(cs, { stmt = r.stmts[pos], target = t })
			end
		end
		edges[name] = es
		callsBySub[name] = cs
	end
	local callsByFn = {}
	for _, cs in fnCallsites do
		if subs[cs.target] then
			local l = callsByFn[cs.fn.name]
			if not l then l = {}; callsByFn[cs.fn.name] = l end
			table.insert(l, { stmt = cs.stmt, target = cs.target })
		end
	end

	-- Tail joins (B2 cross-function jumps FROM INSIDE a subroutine — mont5's
	-- mul4x_internal/mulx4x_internal `jmp .Lsqr4x_sub_entry` into
	-- __bn_post4x_internal's shared tail): a branch out of the clone range to a
	-- label in ANOTHER subroutine's region clones that region's tail into each
	-- caller of the jumping subroutine, its rets dispatching to the jumping
	-- subroutine's continuations (see tailcall.luau and augment below). Record
	-- the edges here, compute+validate the tail regions (an escaping branch or
	-- fall-off inside a tail is rejected there), claim the tail regions'
	-- statements (they are subroutine text even when the region is never
	-- CALLED), and feed the tails' local calls into the cycle-detection graph
	-- (a subroutine reachable by calling through a tail is a recursion edge
	-- like any other). Any other branch out of a clone range keeps the
	-- cross-function-jump rejection below.
	local tailjoins = {}
	local tailRegions = {}
	if arch == "x86_64" then
		local rctx = { regions = {} }
		for _, name in order do
			local sub = subs[name]
			local r = sub.region
			local inRange = {}
			for _, pos in sub.clonePos do
				local st = r.stmts[pos]
				if st.kind == "label" then inRange[st.name] = true end
			end
			for _, pos in sub.clonePos do
				local st = r.stmts[pos]
				if st.kind == "insn" then
					local m = string.lower(st.mnemonic)
					local b = x86_64_isa.baseMnemonic(m)
					if b == "jmp" or b == "jcc" or string.match(m, "^loop") then
						local op = st.operands and st.operands[1]
						if op and op.kind == "sym" and not inRange[op.text] then
							local tr = labelRegion[op.text]
							if tr and tr ~= r then
								-- A mid-body clone of a signatured function may not tail
								-- join: the tail would execute with this clone's +8
								-- context (the OWNER's frame convention) inside a caller
								-- that may not be the owner, re-keying the tail's
								-- rsp-relative accesses against a frame the cloned text
								-- was not written against. Fail closed; the mont5 shape
								-- (a jump from a top-level subroutine) is unaffected.
								if sub.midOwner then
									err.at(st, "branch to '" .. op.text .. "' in local subroutine '"
										.. name .. "' (a mid-body clone of function '" .. sub.midOwner
										.. "') leaves the function; a tail join into another local"
										.. " subroutine is not supported from a mid-body clone: "
										.. (st.raw or st.mnemonic))
								end
								local region = tailcall.computeRegion(rctx, tr.entry, tr.stmts,
									tr.entry, op.text, tr.labels[op.text], name)
								local tj = { stmt = st, target = op.text, region = region,
									tailCalls = {} }
								for idx = 1, #region.stmts do
									local t = region.calls[idx]
									if t and subs[t] then table.insert(tj.tailCalls, t) end
								end
								if not tailjoins[name] then tailjoins[name] = {} end
								table.insert(tailjoins[name], tj)
								tailRegions[tr] = true
								for _, t in tj.tailCalls do table.insert(edges[name], t) end
							end
						end
					end
				end
			end
		end
	end

	-- Mid-body clone soundness (x86_64). A mid-body clone executes on the
	-- CALLER's stack at the caller's rsp: the frame pass re-keys the clone's
	-- rsp-relative displacements 8 lower (the +8 rule — the hardware
	-- return-address compensation) and leaves rbp-relative displacements
	-- absolute. Those displacements were written against the OWNER function's
	-- frame, so they only mean what the hardware would do when the clone runs
	-- in the owner's own activation:
	--   * a DIRECT call from the owner's body (the aesni
	--     aesni_set_encrypt_key -> .Lkey_expansion_* self-call shape) is
	--     exactly the hardware convention — a real `call` pushes the return
	--     address, so the sub's `8(%rsp)` is the caller's (here: the owner's
	--     own) `0(%rsp)` at the callsite depth, and rbp does not move;
	--   * any other landing would silently re-key those accesses to a foreign
	--     frame: a call from a DIFFERENT function's body (the clone would
	--     address the foreign caller's frame), or a nested call from inside
	--     another subroutine's clone range (the model compensates one
	--     return-address push per clone, not per nesting level, so even a
	--     chain that ends in the owner's activation keys one push off).
	-- A mid-body clone whose reachable range touches the stack frame at all
	-- (an rsp/rbp register or memory operand, or push/pop/enter/leave) is
	-- therefore accepted only when EVERY call edge into it is a direct call
	-- from the owner's body; a frame-free clone (registers and memory through
	-- other pointers only — the key-expansion shape) clones into any caller.
	local function frameTouching(sub)
		local r = sub.region
		local function touchesFrame(o)
			if type(o) ~= "table" then return false end
			if o.kind == "reg" and (o.rc == "sp" or o.name == "rbp"
				or o.name == "ebp" or o.name == "bp") then return true end
			if o.kind == "mem" then
				return touchesFrame(o.base) or touchesFrame(o.index)
			end
			return false
		end
		for _, pos in sub.clonePos do
			local st = r.stmts[pos]
			if st.kind == "insn" then
				local b = x86_64_isa.baseMnemonic(string.lower(st.mnemonic))
				if b == "push" or b == "pop" or b == "enter" or b == "leave" then
					return true
				end
				for _, op in (st.operands or {}) do
					if touchesFrame(op) then return true end
				end
			end
		end
		return false
	end
	for _, name in order do
		local sub = subs[name]
		if sub.midOwner and frameTouching(sub) then
			for _, cs in fnCallsites do
				if cs.target == name and cs.fn.name ~= sub.midOwner then
					err.at(cs.stmt, "localcall to '" .. name .. "', a mid-body label inside"
						.. " function '" .. sub.midOwner .. "', cannot be cloned into function '"
						.. cs.fn.name .. "': the cloned code addresses " .. sub.midOwner
						.. "'s stack frame (rsp/rbp-relative), which only exists in its own"
						.. " activation; call it only directly from '" .. sub.midOwner
						.. "' itself, or make the cloned range frame-free: "
						.. (cs.stmt.raw or cs.target))
				end
			end
			for _, css in pairs(callsBySub) do
				for _, cs in css do
					if cs.target == name then
						err.at(cs.stmt, "localcall to '" .. name .. "', a mid-body label inside"
							.. " function '" .. sub.midOwner .. "', from inside another local"
							.. " subroutine's clone would address " .. sub.midOwner
							.. "'s stack frame one return-address push off (the clone model"
							.. " compensates one push per clone, not per nesting level); call it"
							.. " only directly from '" .. sub.midOwner .. "': "
							.. (cs.stmt.raw or cs.target))
					end
				end
			end
		end
	end

	-- Recursion (direct or mutual) can never work with per-callsite cloning:
	-- each clone's continuations are fixed at compile time, so a re-entrant
	-- activation would have no continuation for its inner call. Detect cycles
	-- in the local-call graph (iterative DFS, sorted start order for
	-- determinism) and reject them cleanly.
	do
		local GRAY, BLACK = 1, 2
		local color = {}
		local names = {}
		for n in subs do table.insert(names, n) end
		table.sort(names)
		local function visit(n, stack)
			color[n] = GRAY
			table.insert(stack, n)
			for _, m in edges[n] do
				if color[m] == GRAY then
					local cyc = {}
					local rec = false
					for _, s in stack do
						if s == m then rec = true end
						if rec then table.insert(cyc, s) end
					end
					table.insert(cyc, m)
					-- Locate the callsite statement whose edge closes the cycle
					-- (falls back to any callsite in the closing subroutine,
					-- then to an unlocated message via err.at's nil tolerance —
					-- tail-join edges have no per-callsite record).
					local cycStmt
					for _, c in (callsBySub[n] or {}) do
						if c.target == m then cycStmt = c.stmt break end
					end
					if not cycStmt then
						local l = callsBySub[n] or {}
						cycStmt = l[1] and l[1].stmt or nil
					end
					err.at(cycStmt, "recursive local subroutine calls are not supported ("
						.. table.concat(cyc, " calls ") .. ")")
				elseif color[m] ~= BLACK then
					visit(m, stack)
				end
			end
			table.remove(stack)
			color[n] = BLACK
		end
		for _, n in names do
			if not color[n] then visit(n, {}) end
		end
	end

	-- Per-sub validation (x86_64): every branch in the clone range must stay
	-- inside it (a branch out is a cross-function jump — a separate later
	-- feature), and no control-flow path may fall off the range's end (the
	-- clone would fall into the next appended clone with a garbage retaddr).
	-- The clone range is the reachable set computed above: an in-region branch
	-- target — even one BEFORE the entry label (a loop head) — is inside the
	-- clone by construction.
	for _, name in order do
		local sub = subs[name]
		local r = sub.region
		local labelPos = sub.labelPos
		for _, pos in sub.clonePos do
			local st = r.stmts[pos]
			if st.kind == "insn" then
				local m = string.lower(st.mnemonic)
				local b = x86_64_isa.baseMnemonic(m)
				local isBr = b == "jmp" or b == "jcc" or string.match(m, "^loop") ~= nil
				if isBr then
					local op = st.operands and st.operands[1]
					if op and op.kind == "sym" and not labelPos[op.text] then
						-- A recorded tail-join edge (above) is supported: the
						-- target tail is cloned into each caller by augment.
						local isTailjoin = false
						for _, tj in (tailjoins[name] or {}) do
							if tj.stmt == st then isTailjoin = true break end
						end
						if not isTailjoin then
							err.at(st, "branch to '" .. op.text .. "' in local subroutine '"
								.. name .. "' leaves the subroutine (cross-function jumps to"
								.. " function entries are not supported from subroutines; mid-body"
								.. " tail joins into another local subroutine are): "
								.. (st.raw or st.mnemonic))
						end
					end
				end
			end
		end
		if sub.reachesEnd then
			-- An undecodable `.byte` run swallows its tail bytes (a rep-ret at the
			-- end of a run of legacy AES/SHA encodings hides the ret): name that
			-- likely cause in the message.
			local hint = ""
			local lastStmt
			for _, pos in sub.clonePos do
				local st = r.stmts[pos]
				lastStmt = st
				if st.kind == "directive" and st.name == ".byte" then
					hint = " (the region contains undecodable .byte encodings,"
						.. " which may hide the ret)"
					break
				end
			end
			err.at(lastStmt, "local subroutine '" .. name .. "' can fall off its end;"
				.. " every control-flow path must end with a ret instruction" .. hint)
		end
	end

	-- Mark every region that contains a claimed label consumed (the whole
	-- region is subroutine text; an uncalled prefix ahead of a claimed
	-- mid-label is dead content and is never cloned). A region that is never
	-- called but is the target of a tail join is claimed the same way — its
	-- tail is cloned into each jumping subroutine's callers.
	for _, r in regions do
		local claimed = false
		for _, name in order do
			if subs[name].region == r then claimed = true break end
		end
		if tailRegions[r] then claimed = true end
		if claimed then
			for _, i in r.idx do consumed[i] = "local" end
		end
	end

	return {
		subs = subs, order = order, edges = edges,
		callsByFn = callsByFn, callsBySub = callsBySub,
		tailjoins = tailjoins,
	}
end

-- Append this caller's clones to its body. `fnName` is the caller's name,
-- `body` a fresh array of its statements, `lc` the discovery result, and
-- `userLabels` the file-global user-label set (synthesized names are bumped
-- past it). Marks each callsite st.localCall = { clone, entry, cont } and each
-- clone ret st.localRet = { clone, conts }, inserts continuation labels, and
-- returns the augmented body plus the clones descriptor (id -> { sub, entry,
-- conts }) the transform consumes. Clone order is sorted sub-name order
-- (DETERMINISM); continuation order per clone is augmented-body order.
function localcall.augment(fnName, body, lc, userLabels, arch)
	local direct = lc.callsByFn[fnName]

	-- Transitive closure of called subs (direct + nested + the local calls of
	-- any tail-join tails cloned below — mont5's shape).
	local needed, work = {}, {}
	for _, cs in direct do
		if not needed[cs.target] then needed[cs.target] = true; table.insert(work, cs.target) end
	end
	local wi = 1
	while wi <= #work do
		local w = work[wi]
		for _, t in lc.edges[w] do
			if not needed[t] then needed[t] = true; table.insert(work, t) end
		end
		for _, tj in ((lc.tailjoins or {})[w] or {}) do
			for _, t in tj.tailCalls do
				if not needed[t] then needed[t] = true; table.insert(work, t) end
			end
		end
		wi += 1
	end
	local order = {}
	for n in needed do table.insert(order, n) end
	table.sort(order)

	-- Synthesized label names: `.Llocalsub_<fn>_<sub>_<orig>` for clone
	-- labels, `.Llocalsub_<fn>_<sub>_cont_<k>` for continuations, all bumped
	-- past user label names (file-global assembly symbols).
	local used = {}
	for n in (userLabels or {}) do used[n] = true end
	local function fresh(base)
		local nm, k = base, 0
		while used[nm] do k += 1; nm = base .. "_" .. k end
		used[nm] = true
		return nm
	end

	local cloneId, cloneLabels, clones = {}, {}, {}
	for id, name in ipairs(order) do
		cloneId[name] = id
		local sub = lc.subs[name]
		local ren = {}
		local r = sub.region
		for _, pos in (sub.clonePos or {}) do
			local st = r.stmts[pos]
			if st.kind == "label" then
				ren[st.name] = fresh(".Llocalsub_" .. fnName .. "_" .. name .. "_" .. st.name)
			end
		end
		cloneLabels[name] = ren
		clones[id] = { sub = name, entry = ren[name], conts = {} }
	end

	-- Tail-join edges (mont5's .Lsqr4x_sub_entry shape): precompute each tail
	-- clone's label renames and entry label. The tail clone rides the JUMPING
	-- subroutine's clone context — its statements carry this clone's id for the
	-- +8 displacement rule (the tail executes inside the subroutine's
	-- activation, whose caller pushed a return address) and its rets dispatch
	-- to the jumping subroutine's continuations.
	local tjLabels = {}
	for _, name in ipairs(order) do
		local tjs = (lc.tailjoins or {})[name]
		if tjs then
			local list = {}
			for _, tj in tjs do
				local ren = {}
				for _, st in tj.region.stmts do
					if st.kind == "label" then
						ren[st.name] = fresh(".Ltailjoin_" .. fnName .. "_" .. name .. "_" .. st.name)
					end
				end
				table.insert(list, { tj = tj, ren = ren, entry = ren[tj.target] })
			end
			tjLabels[name] = list
		end
	end

	-- Operand copy: rename in-clone label references. Nested base/index tables
	-- are copied too — regions are shared across callers and downstream passes
	-- rewrite operands in place, so every cloned operand table must be fresh.
	local function copyOp(op, ren)
		if type(op) ~= "table" then return op end
		local cp = {}
		for k, v in pairs(op) do
			if k == "base" or k == "index" then
				cp[k] = copyOp(v, ren)
			elseif (k == "text" and op.kind == "sym") or k == "dispSym" then
				cp[k] = ren[v] or v
			else
				cp[k] = v
			end
		end
		return cp
	end
	local function cloneStmt(st, ren, id, annSuffix)
		if st.kind == "label" then
			return { kind = "label", name = ren[st.name], line = st.line }
		end
		if st.kind == "insn" then
			local cp = {}
			for k, v in pairs(st) do cp[k] = v end
			if st.operands then
				local ops = {}
				for i, op in st.operands do ops[i] = copyOp(op, ren) end
				cp.operands = ops
			end
			-- Mark clone membership: the frame pass applies the +8 rule (the
			-- return-address compensation on rsp-relative accesses) from it.
			cp.localClone = id
			return cp
		end
		return st   -- blanks/directives are never mutated downstream
	end

	local newBody = {}
	local function emitCont(clone)
		local nm = fresh(".Llocalsub_" .. fnName .. "_" .. clones[clone].sub .. "_cont")
		table.insert(newBody, { kind = "label", name = nm })
		table.insert(clones[clone].conts, nm)
		return nm
	end

	-- The caller's own callsites.
	local directByStmt = {}
	for _, cs in direct do directByStmt[cs.stmt] = cs.target end
	for _, st in body do
		table.insert(newBody, st)
		local tgt = directByStmt[st]
		if tgt then
			local id = cloneId[tgt]
			st.localCall = { clone = id, entry = clones[id].entry }
			st.localCall.cont = emitCont(id)
		end
	end

	-- The clones. Rets are collected and marked only after EVERY callsite is
	-- known (a clone's dispatch lists all of its continuations).
	local retMarks = {}
	for id, name in ipairs(order) do
		local sub = lc.subs[name]
		local ren = cloneLabels[name]
		local nestedByStmt = {}
		for _, cs in lc.callsBySub[name] do nestedByStmt[cs.stmt] = cs.target end
		local tjByStmt = {}
		for _, e in (tjLabels[name] or {}) do tjByStmt[e.tj.stmt] = e end
		local r = sub.region
		for pi, pos in ipairs(sub.clonePos) do
			local st = r.stmts[pos]
			local cp = cloneStmt(st, ren, id, fnName .. "/" .. name)
			if pi == 1 then
				-- validateBody: a clone's FIRST statement reached by FALL-THROUGH
				-- (instead of a jump edge — the localCall edge, or a backward
				-- branch inside the clone) is a fall-off-the-end of the preceding
				-- body — the same rejection as a function without a ret. The
				-- first statement is the called label for a region-head entry,
				-- but a mid-label entry whose reachable region includes a
				-- backward loop head (aesni's .Lenc_loop8) starts BEFORE the
				-- entry label — and the entry label itself is then legitimately
				-- fall-through-reachable inside the clone, so only the first
				-- statement carries the mark.
				cp.localCloneEntry = true
			end
			table.insert(newBody, cp)
			local tje = tjByStmt[st]
			if tje and cp.kind == "insn" then
				-- Tail-join branch: retarget to the tail clone's entry (appended
				-- right after this clone's statements below).
				cp.operands[1].text = tje.entry
			end
			if cp.kind == "insn" then
				local tgt = nestedByStmt[st]
				if tgt then
					local tid = cloneId[tgt]
					cp.localCall = { clone = tid, entry = clones[tid].entry }
					cp.localCall.cont = emitCont(tid)
				elseif isRetMnem(cp.mnemonic, arch) then
					table.insert(retMarks, { cp, id })
				end
			end
		end
		-- The tail-join clones of this subroutine (mont5): the target region's
		-- tail, cloned with this clone's id (the +8 context and the ret
		-- dispatch are the jumping subroutine's — see above).
		for _, e in (tjLabels[name] or {}) do
			local tj, tren = e.tj, e.ren
			local tnestedByStmt = {}
			for idx = 1, #tj.region.stmts do
				local t = tj.region.calls[idx]
				if t then tnestedByStmt[tj.region.stmts[idx]] = t end
			end
			for idx, st in tj.region.stmts do
				local cp = cloneStmt(st, tren, id, fnName .. "/" .. name .. "/" .. tj.target)
				if idx == 1 then
					-- validateBody: the clone's FIRST statement is reached only
					-- through branches (the tail-join edge or a backward branch
					-- inside the region — mont5's .Lsqr4x_sub loop head precedes
					-- the join label, and the region's own text falls THROUGH the
					-- join label, which must NOT carry this mark): falling INTO
					-- it means the preceding body fell off its end.
					cp.localCloneEntry = true
				end
				table.insert(newBody, cp)
				if cp.kind == "insn" then
					local tgt = tnestedByStmt[st]
					if tgt and cloneId[tgt] then
						local tid = cloneId[tgt]
						cp.localCall = { clone = tid, entry = clones[tid].entry }
						cp.localCall.cont = emitCont(tid)
					elseif isRetMnem(cp.mnemonic, arch) then
						table.insert(retMarks, { cp, id })
					end
				end
			end
		end
	end
	for _, rm in retMarks do
		rm[1].localRet = { clone = rm[2], entry = clones[rm[2]].entry,
			conts = clones[rm[2]].conts }
	end

	return newBody, clones
end

return localcall
