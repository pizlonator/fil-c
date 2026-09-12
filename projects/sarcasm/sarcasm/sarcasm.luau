--!nonstrict
-- sarcasm: SAfe Runtime Capability-enforced Assembler. Rewrites annotated Yolo-C ARM64
-- assembly into memory-safe, Fil-C-linkable assembly, then (by default) invokes `as`.

local fs = require("@lute/fs")
local process = require("@lute/process")

-- Shared modules.
local sig = require("./sig")
local transform = require("./transform")
local regalloc = require("./regalloc")
local emit = require("./emit")
local build = require("./build")
local detect = require("./detect")
local localcall = require("./localcall")
local tailcall = require("./tailcall")
local prof = require("./prof")

-- Per-architecture module pairs (arm64_* / x86_64_*).
local arm64_parse = require("./arm64_parse")
local x86_64_parse = require("./x86_64_parse")
local arm64_glue = require("./arm64_glue")
local x86_64_glue = require("./x86_64_glue")
local arm64_codegen = require("./arm64_codegen")
local x86_64_codegen = require("./x86_64_codegen")
local arm64_frame = require("./arm64_frame")
local x86_64_frame = require("./x86_64_frame")
local arm64_render = require("./arm64_render")
local x86_64_render = require("./x86_64_render")

local sarcasm = {}

-- Per-architecture backend bundle. `spillReload`/`spillStore` build the load/store IR for
-- a spilled temp at frame offset `off`; `emitFip` renders (x86 always emits AT&T output).
local function makeTarget(arch, syntax)
	if arch == "x86_64" then
		local backend = { cg = x86_64_codegen, frame = x86_64_frame, arch = "x86_64" }
		return {
			arch = "x86_64", syntax = syntax,
			-- OpenSSL perlasm emits instructions as raw .byte sequences in
			-- function bodies (legacy-gas workarounds); decode the known
			-- encodings back into ordinary instructions at parse time, so every
			-- downstream pass sees spelled ones (x86_64_parse.decodeByteInsns).
			decodeBody = x86_64_parse.decodeByteInsns,
			transformFunction = function(nm, sd, body, extra) return transform.transformFunction(nm, sd, body, backend, extra) end,
			emitFip = function(out, tr, cr) return emit.emitFip(out, tr, cr, x86_64_render, "att") end,
			glue = x86_64_glue,
			allocatable = x86_64_codegen.ALLOCATABLE,
			spillReload = function(f, off)
				local n = build.insn("mov", { build.rt(f), build.memx(build.rp(4), { dispVal = off ~= 0 and off or nil }) }, { defs = {f} })
				n.spillReload = true; n.spillOff = off; n.spillTemp = f; return n
			end,
			spillStore = function(f, off)
				local n = build.insn("mov", { build.memx(build.rp(4), { dispVal = off ~= 0 and off or nil }), build.rt(f) }, { uses = {f} })
				n.spillStore = true; n.spillOff = off; n.spillTemp = f; return n
			end,
		}
	end
	local backend = { cg = arm64_codegen, frame = arm64_frame, arch = "arm64" }
	return {
		arch = "arm64", syntax = nil,
		transformFunction = function(nm, sd, body, extra) return transform.transformFunction(nm, sd, body, backend, extra) end,
		emitFip = function(out, tr, cr) return emit.emitFip(out, tr, cr, arm64_render, nil) end,
		glue = arm64_glue,
		allocatable = arm64_codegen.ALLOCATABLE,
		spillReload = function(f, off)
			local n = build.insn("ldr", { build.rt(f), build.mem(build.sp(), off) }, { defs = {f} })
			n.spillReload = true; n.spillOff = off; n.spillTemp = f; return n
		end,
		spillStore = function(f, off)
			local n = build.insn("str", { build.rt(f), build.mem(build.sp(), off) }, { uses = {f} })
			n.spillStore = true; n.spillOff = off; n.spillTemp = f; return n
		end,
	}
end

-- Directives that switch the current section. A `.size`-less function body also ends
-- at one of these: the content after a section switch cannot belong to the function's
-- text, so treating it as top-level content (on arm64: dead data is dropped; live data
-- is rejected with the top-level "globals cannot be made memory-safe" message rather
-- than the misleading "data in a function body") matches the `.size`-terminated case.
local SECTION_DIRECTIVES = {
	[".section"] = true, [".text"] = true, [".data"] = true, [".rodata"] = true,
	[".bss"] = true, [".previous"] = true,
	[".pushsection"] = true, [".popsection"] = true,
}

-- Split parsed statements into functions. A function starts at a label carrying a
-- `;! signature` annotation and ends at its `.size NAME, .-NAME` directive (or, when
-- that is omitted, at the next function's `.type OTHER, %function` declaration or at
-- a section-switch directive). Every symbol declared as a function
-- (`.type NAME, %function` / `@function`) that is defined here (has a label) MUST
-- carry a signature annotation; otherwise we cannot make it memory-safe, so it is a
-- compile-time error rather than a silently dropped function — EXCEPT a label the
-- local-subroutine prescan saw as the target of a potentially-local call
-- (opts.localCalled): such a label is left unconsumed for localcall.discover, which
-- either claims it as a local subroutine or leaves it to the top-level scan. A
-- .globl no-signature function always errors (opts.globals).
-- Returns { functions = { {name, sigAnn, body={stmts}} }, consumed }, plus the
-- accepted ALIAS map (alias name -> owner entry name) and the per-function
-- ordered .globl-alias lists (owner name -> { alias names }) for the driver's
-- getter/alias emission.
local function splitFunctions(stmts, opts)
	opts = opts or {}
	local localCalled = opts.localCalled or {}
	local globalsSet = opts.globals or {}
	-- pass 1: collect declared function symbols
	local funcSyms = {}
	for _, st in stmts do
		if st.kind == "directive" and st.name == ".type" then
			local nm, kind = string.match(st.args, "^%s*([%w_.$]+)%s*,%s*(.+)$")
			if nm and kind and string.find(kind, "function", 1, true) then
				funcSyms[nm] = true
			end
		end
	end

	-- Alias entry labels (tailcall.luau): a label immediately adjacent to a
	-- sig-annotated function label names the same entry. A PRECEDING alias
	-- (asm_AES_encrypt:/AES_encrypt:, .Lenc_rounds:/Camellia_EncryptBlock_Rounds:)
	-- is consumed here (never body content); a FOLLOWING local alias
	-- (_shaext_shortcut:, .L_init_clmul:) stays an ordinary body label (branches
	-- to it from within the body keep working) and is additionally recorded for
	-- cross-function resolution; a following .GLOBL alias is consumed like a
	-- preceding one (its raw symbol must never be emitted into a FIP body).
	local aliasScan = tailcall.scanAliases(stmts, opts.arch)
	local aliases = {}
	local aliasGlobals = {}
	-- Does any statement of the partially-built body reference the label? (A
	-- label the current function branches to is its own loop/jump target, never
	-- a next function's alias.)
	local function bodyRefs(body, name)
		for _, bst in body do
			for _, op in (bst.operands or {}) do
				if op.kind == "sym" and op.text == name then return true end
				if op.kind == "mem" and op.dispSym == name then return true end
			end
		end
		return false
	end

	-- pass 2: split into functions, enforcing that declared functions are annotated
	local functions = {}
	local consumed = {}
	local cur = nil
	for idx, st in stmts do
		if st.kind == "label" then
			local hasSig = st.annotation ~= nil and string.find(st.annotation, "%(") ~= nil
			if hasSig then
				cur = { name = st.name, sigAnn = st.annotation, body = { st }, startIdx = idx }
				table.insert(functions, cur)
				consumed[idx] = cur
			else
				local al = aliasScan[st.name]
				if al and (al.where == "pre" or globalsSet[st.name])
					and not (cur and bodyRefs(cur.body, st.name)) then
					consumed[idx] = "alias"
					aliases[st.name] = al.owner
					if globalsSet[st.name] then
						local l = aliasGlobals[al.owner]
						if not l then l = {}; aliasGlobals[al.owner] = l end
						table.insert(l, st.name)
					end
				elseif funcSyms[st.name] and (globalsSet[st.name] or not localCalled[st.name]) then
					error("sarcasm: function '" .. st.name .. "' has no signature annotation "
						.. "(add one to its label, e.g. `" .. st.name .. ": ;! int(ptr)`)")
				elseif cur then
					-- a following local alias: record it AND keep the label in the body
					if al then aliases[st.name] = al.owner end
					table.insert(cur.body, st)
					consumed[idx] = cur
				end
			end
		elseif cur then
			-- Handwritten asm often omits `.size`: the next function's declaration
			-- (`.type OTHER, %function`) also ends this body, so the next function's
			-- prelude is not absorbed into this one.
			if st.kind == "directive" and st.name == ".type" then
				local nm, kind = string.match(st.args, "^%s*([%w_.$]+)%s*,%s*(.+)$")
				if nm and nm ~= cur.name and kind and string.find(kind, "function", 1, true) then
					cur = nil
				end
			end
			-- A section switch also ends a `.size`-less body: what follows is top-level
			-- content, not function text (see SECTION_DIRECTIVES). The directive itself
			-- is left unconsumed so the top-level handler sees it.
			if cur and st.kind == "directive" and SECTION_DIRECTIVES[st.name] then
				cur = nil
			end
			if cur then
				table.insert(cur.body, st)
				consumed[idx] = cur
				if st.kind == "directive" and st.name == ".size" then
					cur = nil
				end
			end
		end
	end
	return functions, consumed, aliases, aliasGlobals
end
sarcasm.splitFunctions = splitFunctions

-- Build the regalloc instruction array from transform IR nodes.
-- Functions whose fipNodes stream exceeds this many nodes take the scalable
-- regalloc path: fail-stub noreturn calls (marked noreturnCall by the codegen
-- fail-stub builders) are modeled as NOT falling through. With the historical
-- (small-function) model every fail stub falls into the next, so liveness
-- chains backward through the whole stub run: every check's eff/lower temps
-- become live at every earlier stub, an O(stubs^2) interference clique and
-- O(insns x stubs) live-set memory (75+ GB on OpenSSL's aes-gcm-avx512, whose
-- two 60K-line functions emit 230K+ IR nodes). The panic calls ARE noreturn
-- (the trap path never resumes), so not falling through is the faithful model
-- — but it changes liveness and with it the coloring, so it is enabled only
-- where the exact historical coloring is unaffordable; every function small
-- enough keeps the exact historical path (and its byte-identical output).
local BIG_RA_INSNS = 32768

local function buildRaInsns(nodes, bigMode)
	local ra = {}
	for _, node in nodes do
		if node.kind == "label" then
			table.insert(ra, { defs = {}, uses = {}, isLabelDef = node.name, fallsThrough = true })
		elseif node.kind == "insn" then
			local control = node.control or (node.c and node.c.control) or "none"
			local targets = node.targets or (node.c and node.c.targets)
			local fallsThrough = node.fallsThrough
			if fallsThrough == nil then fallsThrough = node.c and node.c.fallsThrough end
			if fallsThrough == nil then fallsThrough = true end
			if bigMode and node.noreturnCall then fallsThrough = false end
			local mv = node.move
			if mv then
				-- A move-marked insn is always a pure single-def copy (both cg.move
				-- implementations: defs = {dst}, uses = {src}, move = {dst, src}).
				-- The coloring backstop's move-source exemption compares move.src
				-- against ANY def of the insn, so it (and regalloc's move bookkeeping,
				-- which keys on move.dst) is only sound for exactly that shape:
				-- assert it here rather than trusting it.
				if #(node.defTemps or {}) ~= 1 or node.defTemps[1] ~= mv.dst then
					error("sarcasm: internal error: move-marked insn must have exactly one def, equal to its move destination")
				end
				-- The move-exclusion (src and dst may share a color because a
				-- same-color move is a droppable no-op) is only sound for a FULL-WIDTH
				-- (64-bit) reg-reg move: a narrower mov zero-extends, so a source web
				-- live past the move would lose its upper bits in the shared register.
				local ops = node.stmt and node.stmt.operands or node.operands
				local wide = true
				if ops and #ops == 2 and ops[1].kind == "reg" and ops[2].kind == "reg" then
					wide = ((ops[1].width or 64) >= 64) and ((ops[2].width or 64) >= 64)
				end
				mv.wide = wide
			end
			table.insert(ra, {
				defs = node.defTemps or {},
				uses = node.useTemps or {},
				move = mv,
				targets = targets,
				fallsThrough = fallsThrough,
			})
		elseif node.kind == "epilogue" then
			table.insert(ra, { defs = {}, uses = node.useTemps or {}, fallsThrough = true })
		elseif node.kind == "rootstore" then
			table.insert(ra, { defs = {}, uses = node.useTemps or {}, fallsThrough = true })
		elseif node.kind == "prologue" then
			table.insert(ra, { defs = {}, uses = {}, fallsThrough = true })
		else
			table.insert(ra, { defs = {}, uses = {}, fallsThrough = true })
		end
	end
	return ra
end

-- Redirect every reference to a spilled temp in `node` to its fresh replacement.
local function remapNodeTemps(node, remap)
	local function replList(list)
		if list then for i, t in list do if remap[t] then list[i] = remap[t] end end end
	end
	replList(node.defTemps)
	replList(node.useTemps)
	if node.move then
		if remap[node.move.dst] then node.move.dst = remap[node.move.dst] end
		if remap[node.move.src] then node.move.src = remap[node.move.src] end
	end
	local function remapOp(op)
		if not op then return end
		if op.kind == "reg" then
			if op.tempRef and remap[op.tempRef] then op.tempRef = remap[op.tempRef] end
			if node.opTemp and node.opTemp[op] and remap[node.opTemp[op]] then
				node.opTemp[op] = remap[node.opTemp[op]]
			end
		elseif op.kind == "mem" then
			remapOp(op.base); remapOp(op.index)
		end
	end
	local ops = node.stmt and node.stmt.operands or node.operands
	if ops then for _, op in ops do remapOp(op) end end
end

-- Rewrite spilled temps: around each def/use of a spilled temp, insert a store/reload
-- through a fresh short-lived temp (tagged so the emitter can eliminate redundant
-- reloads and size the frame). Returns the new node list.
--
-- Outgoing stack-arg windows (x86_64: cg.beginStackArgs/endStackArgs around a
-- call's stack-argument marshalling) drop rsp by N for their span, so a spill
-- reload/store created INSIDE one would address the wrong frame slot: stamp it
-- with the window's shift (node.spillShift); the renderer adds it back
-- (x86_64_render.renderInsn). The spillOff slot key stays unshifted so slot
-- packing and reload elimination keep working on the logical slot.
local function rewriteSpills(nodes, spilledSet, slotOf, allocTemp, spillBase, target)
	local out = {}
	local curShift = 0
	for _, node in nodes do
		if node.kind ~= "insn" then
			-- Non-insn nodes carry live temps too (the epilogue reads the myth temp; a
			-- rootstore reads a lower temp), and buildRaInsns feeds those uses to the
			-- allocator. A spilled use here needs a reload in front of the node exactly
			-- like an insn use: leaving it in place never eliminates the temp, so the
			-- next coloring round spills it again and the spill loop oscillates forever
			-- (observed on ecp_nistz256's sqr_mont: the function-wide myth web was
			-- re-spilled every round off the epilogue's use).
			local remap = {}
			for _, t in (node.useTemps or {}) do
				if spilledSet[t] then remap[t] = remap[t] or allocTemp() end
			end
			if next(remap) then
				for t, f in remap do
					local n = target.spillReload(f, spillBase + 8 * slotOf[t])
					if curShift ~= 0 then n.spillShift = curShift end
					table.insert(out, n)
				end
				remapNodeTemps(node, remap)
				-- The rootstore renderer keys off node.temp (not useTemps).
				if node.temp and remap[node.temp] then node.temp = remap[node.temp] end
			end
			table.insert(out, node)
			continue
		end
		local remap, isUse, isDef = {}, {}, {}
		for _, t in (node.useTemps or {}) do
			if spilledSet[t] then remap[t] = remap[t] or allocTemp(); isUse[t] = true end
		end
		for _, t in (node.defTemps or {}) do
			if spilledSet[t] then remap[t] = remap[t] or allocTemp(); isDef[t] = true end
		end
		if not next(remap) then
			if node.shiftBegin then curShift = node.shiftBegin
			elseif node.shiftEnd then curShift = 0 end
			table.insert(out, node)
			continue
		end
		for t, f in remap do
			if isUse[t] then
				local n = target.spillReload(f, spillBase + 8 * slotOf[t])
				if curShift ~= 0 then n.spillShift = curShift end
				table.insert(out, n)
			end
		end
		remapNodeTemps(node, remap)
		table.insert(out, node)
		for t, f in remap do
			if isDef[t] then
				local n = target.spillStore(f, spillBase + 8 * slotOf[t])
				if curShift ~= 0 then n.spillShift = curShift end
				table.insert(out, n)
			end
		end
		if node.shiftBegin then curShift = node.shiftBegin
		elseif node.shiftEnd then curShift = 0 end
	end
	return out
end

-- Pack spill slots by coloring their interference graph: two spilled values whose live
-- ranges never overlap can share one stack offset. Without this, every spilled temp gets a
-- unique slot and frames balloon (e.g. large-arm64 needed ~7.7 KB). We build a
-- slot-variable view of the final code (a spill store defines its slot, a reload uses it),
-- run the same backward liveness the register allocator uses, then greedily color slots
-- (a slot's store interferes with everything live across it). Rewrites each spill node's
-- offset and returns the packed slot count.
local function packSpillSlots(tr, spillBase, numSlots, target, bigMode)
	if numSlots <= 1 then return numSlots end
	local function slotIdOf(off) return math.floor((off - spillBase) / 8) end

	-- slot-variable insns parallel to fipNodes, preserving the CFG (labels + control flow).
	local slotInsns = {}
	for _, node in tr.fipNodes do
		if node.kind == "label" then
			table.insert(slotInsns, { defs = {}, uses = {}, isLabelDef = node.name, fallsThrough = true })
		elseif node.kind == "insn" then
			local control = node.control or (node.c and node.c.control) or "none"
			local targets = node.targets or (node.c and node.c.targets)
			local ft = node.fallsThrough
			if ft == nil then ft = node.c and node.c.fallsThrough end
			if ft == nil then ft = true end
			if bigMode and node.noreturnCall then ft = false end
			local defs, uses = {}, {}
			if node.spillStore then defs = { slotIdOf(node.spillOff) } end
			if node.spillReload then uses = { slotIdOf(node.spillOff) } end
			table.insert(slotInsns, { defs = defs, uses = uses, targets = targets, fallsThrough = ft })
		else
			table.insert(slotInsns, { defs = {}, uses = {}, fallsThrough = true })
		end
	end

	local liveOut = regalloc.liveOut(slotInsns)
	-- interference: a slot's store (its only def point) interferes with every slot live
	-- across that store. This def-to-live-out construction is the standard, complete
	-- interference relation (the register allocator uses the same one): any two slots that
	-- are ever simultaneously live get an edge at whichever's store sees the other live. It
	-- only does work at spill stores, so it's linear in the spill traffic (an all-pairs
	-- build would be quadratic in the live set at every instruction).
	local adj = {}
	for s = 0, numSlots - 1 do adj[s] = {} end
	for i, ins in slotInsns do
		if ins.defs[1] ~= nil then
			local lo = liveOut[i]
			if lo then
				for _, d in ins.defs do
					for t in lo do if t ~= d then adj[d][t] = true; adj[t][d] = true end end
				end
			end
		end
	end
	-- greedy graph coloring (colors = compact slot offsets)
	local color = {}
	local ncolors = 0
	for s = 0, numSlots - 1 do
		local used = {}
		for t in adj[s] do if color[t] ~= nil then used[color[t]] = true end end
		local c = 0
		while used[c] do c += 1 end
		color[s] = c
		if c + 1 > ncolors then ncolors = c + 1 end
	end
	-- Rewrite spill nodes to their packed offset. The offset lives inside the memory
	-- operand (target-specific shape), so rebuild the node via the target constructor rather
	-- than poking a field; that keeps the rendered address and the reload-elim key in sync.
	-- A spillShift (outgoing stack-arg window re-basing, see rewriteSpills) rides
	-- along: it is a property of the node's POSITION, not of its slot offset.
	for i, node in tr.fipNodes do
		if node.kind == "insn" and node.spillReload then
			tr.fipNodes[i] = target.spillReload(node.spillTemp, spillBase + 8 * color[slotIdOf(node.spillOff)])
			tr.fipNodes[i].spillShift = node.spillShift
		elseif node.kind == "insn" and node.spillStore then
			tr.fipNodes[i] = target.spillStore(node.spillTemp, spillBase + 8 * color[slotIdOf(node.spillOff)])
			tr.fipNodes[i].spillShift = node.spillShift
		end
	end
	return ncolors
end

-- Color the function, spilling to the stack and re-running IRC until it converges.
-- Sets tr.numSpillSlots and returns the final coloring.
local function allocateWithSpilling(tr, target)
	local nextTemp = tr.nextTemp or 0
	local function allocTemp()
		nextTemp += 1
		tr.temps[nextTemp] = { spillCost = 1e9 }  -- reload temps are short-lived; never re-spill
		return nextTemp
	end
	local slotOf, numSlots = {}, 0
	-- Spill slots sit above the frame header, the GC root slots, and the (x86_64-only,
	-- 16-byte-aligned) FP/SIMD region: reserved vector-save area + materialized FP
	-- frame-slot clusters + the materialized incoming yolo stack-argument area + its
	-- alignment pad. Must match the target render's computeLayout
	-- (x86_64_render.luau / arm64_render.luau).
	local numRoots = tr.numRoots or 0
	local fpRegion = (tr.fpSaveBytes or 0) + (tr.fpBytes or 0) + (tr.yoloBytes or 0)
	local fpPad = (fpRegion > 0 and numRoots % 2 == 1) and 8 or 0
	-- Mirror x86_64_render.computeLayout's widened pad: a dynamically-aligned
	-- frame (effective alignment A > 16, from `and $-N, %rsp` notes covering
	-- vmovdqa-class materialized clusters) shifts the FP/SIMD region up so the
	-- root-area shift is a multiple of A. Spill offsets are absolute (no later
	-- shift rides them), so the allocation base must include the same widening;
	-- otherwise the first spills land inside the cluster tail (openssl AES-GCM
	-- encrypt: 64-byte cluster store at phys 2944 covers [2944,3008) while
	-- spills started at 2992, clobbering the spilled aes_keys capability and
	-- faulting its 16-byte round-key check). No-op on arm64 and on 16-byte
	-- frames (A = 16 admits no widening), matching computeLayout exactly.
	local A = tr.frameAlignReq or 16
	if A < 16 then A = 16 end
	if A > 16 then
		while ((8 * numRoots + fpPad) % A) ~= 0 do fpPad += 8 end
	end
	local spillBase = 16 + 8 * numRoots + fpPad + fpRegion
	local bigMode = #tr.fipNodes > BIG_RA_INSNS
	prof.mark("fn " .. (tr.name or "?") .. " allocate (" .. #tr.fipNodes .. " fipNodes"
		.. (bigMode and ", big" or "") .. ")")
	for _ = 1, 32 do
		-- spill-cost heuristic: cost ~ number of def/use occurrences, so IRC prefers
		-- spilling rarely-used temps (a value used once is cheap to reload; a value used
		-- many times, like a base pointer, is kept in a register). noSpill temps keep 1e9.
		local cnt = {}
		for _, node in tr.fipNodes do
			for _, t in (node.defTemps or {}) do cnt[t] = (cnt[t] or 0) + 1 end
			for _, t in (node.useTemps or {}) do cnt[t] = (cnt[t] or 0) + 1 end
		end
		for t, d in tr.temps do
			if (d.spillCost or 0) < 1e9 then d.spillCost = math.max(cnt[t] or 1, 1) end
		end
		local cr = regalloc.color(buildRaInsns(tr.fipNodes, bigMode), tr.temps,
			{ allocatable = target.allocatable, reserved = tr.reservedColors })
		if #cr.spilled == 0 then
			-- allocation converged; pack spill slots (reuse offsets for non-overlapping slots)
			tr.numSpillSlots = packSpillSlots(tr, spillBase, numSlots, target, bigMode)
			return cr
		end
		local spilledSet = {}
		for _, t in cr.spilled do
			spilledSet[t] = true
			if slotOf[t] == nil then slotOf[t] = numSlots; numSlots += 1 end
		end
		tr.fipNodes = rewriteSpills(tr.fipNodes, spilledSet, slotOf, allocTemp, spillBase, target)
	end
	error("sarcasm: register allocation did not converge (excessive register pressure)")
end

-- x86_64 data-object collection (global variables) -------------------------------------
--
-- Contiguous top-level data under `.section .rodata*`/`.data`/`.bss` (plus
-- `.comm`/`.lcomm`) becomes Fil-C data objects (DOs), mirroring what FilPizlonator
-- emits for a C global (verified against clang -O2 -S output): a 16-byte header
-- (.quad payloadEnd, .quad flags<<48 with ObjectFlagGlobal at bit 48 and
-- ObjectFlagReadonly at bit 49 for const data), the payload at DO+16 (so the
-- payload base IS the capability lower), a leading pad when an alignment wider
-- than 16 is required, a `.LpizlonatedGP_` flight-pointer cache cell and a
-- `.LpizlonatedGS_` slow-init function for writable data, and for EXPORTED
-- (`.globl`/`.comm`) labels additionally a `.globl pizlonated_<sym>` getter plus
-- the unsafe-export alias `.globl <sym>` + `.set <sym>, pizlonatedDO_<sym>+16`.
--
-- One DO per maximal contiguous BLOCK of local data (labels map to payload
-- offsets, so "lea a base label and read past a sub-label" keeps working); one DO
-- per exported label with extent to the next label, so C-side bounds are exact.
-- Only scalar initializers: .byte/.short/.word/.value/.long/.quad (numeric),
-- .float/.double, .ascii/.asciz (decoded and re-emitted as .byte lists so the
-- computed payload size is exact by construction), .zero/.space/.fill,
-- .p2align/.align padding. `.quad label`-style pointer initializers (constant
-- relocations) are a clean compile-time rejection. Truly unreferenced,
-- non-global blocks are dropped (the existing dead-content rule).
--
-- Returns dataGlobals (label -> desc) and dos (emission-ordered DO records), and
-- marks every collected statement consumed so the top-level scan skips it.

-- Decode a gas string literal body (the text between the quotes) into a byte
-- array. Handles the C-style escapes gas documents; anything else (\x hex,
-- unknown escapes) is a clean rejection — a misdecoded byte count would silently
-- corrupt the payload bounds.
local function decodeGasString(body, raw)
	local bytes = {}
	local SIMPLE = { n = 10, t = 9, r = 13, b = 8, f = 12, a = 7, v = 11,
		["\\"] = 92, ['"'] = 34, ["'"] = 39, ["?"] = 63 }
	local i, n = 1, #body
	while i <= n do
		local c = string.sub(body, i, i)
		if c == "\\" then
			local d = string.sub(body, i + 1, i + 1)
			if SIMPLE[d] then
				table.insert(bytes, SIMPLE[d]); i += 2
			elseif string.match(d, "[0-7]") then
				local oct = string.match(body, "^[0-7][0-7]?[0-7]?", i + 1)
				table.insert(bytes, tonumber(oct, 8) % 256)
				i += 1 + #oct
			else
				error("sarcasm: unsupported string escape '\\" .. d
					.. "' in data string literal: " .. raw)
			end
		else
			table.insert(bytes, string.byte(c)); i += 1
		end
	end
	return bytes
end

-- Split a data directive's argument string into top-level comma fields, string-aware
-- (a comma inside "..." does not split; \" escapes are honored).
local function splitDataArgs(s)
	local out, cur, inStr = {}, {}, false
	local i = 1
	while i <= #s do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr; table.insert(cur, c)
		elseif inStr and c == "\\" then
			table.insert(cur, c)
			i += 1
			if i <= #s then table.insert(cur, string.sub(s, i, i)) end
		elseif c == "," and not inStr then
			table.insert(out, (string.gsub(table.concat(cur), "^%s*(.-)%s*$", "%1")))
			cur = {}
		else
			table.insert(cur, c)
		end
		i += 1
	end
	local last = (string.gsub(table.concat(cur), "^%s*(.-)%s*$", "%1"))
	if last ~= "" then table.insert(out, last) end
	return out
end

-- x86_64 widths (bytes) of the supported scalar numeric data directives. (.word is
-- 2 bytes on x86 — this collector is x86_64-only.)
local DATA_WIDTH = { [".byte"] = 1, [".short"] = 2, [".hword"] = 2, [".2byte"] = 2,
	[".value"] = 2, [".word"] = 2, [".long"] = 4, [".int"] = 4, [".4byte"] = 4,
	[".quad"] = 8, [".8byte"] = 8, [".float"] = 4, [".single"] = 4, [".double"] = 8 }

local function collectX86Data(stmts, consumed, globals, hidden, referenced)
	local dataGlobals = {}
	local dos = {}

	-- Section classification. Returns "ro" (read-only collectable), "w" (writable
	-- collectable) or nil (not collectable).
	local function sectionKind(name)
		if name == ".rodata" or string.match(name, "^%.rodata%.") then return "ro" end
		if string.match(name, "^%.data%.rel%.ro") then return "ro" end
		if name == ".data" or string.match(name, "^%.data%.") then return "w" end
		if name == ".bss" or string.match(name, "^%.bss%.") then return "w" end
		return nil
	end

	-- Fold a numeric constant argument (no symbol allowed) for a data directive.
	local function numArg(text, raw)
		local e = x86_64_parse.parseDispExpr(text, "displacement")
		if e.sym then
			error("sarcasm: symbolic data initializer '" .. text .. "' is not supported"
				.. " (pointer initializers and constant relocations cannot be made"
				.. " memory-safe automatically): " .. raw)
		end
		return e.const
	end

	-- Byte size of a data-directive statement; raises a clean error for anything
	-- outside the scalar-initializer set. Evaluated TOLERANTLY during the walk (pcall)
	-- and re-raised at finalization only when the block proves live, so dead
	-- detector-probe content keeps dropping silently.
	local function itemSize(st)
		local w = DATA_WIDTH[st.name]
		if w then
			local fields = splitDataArgs(st.args or "")
			if #fields == 0 then
				error("sarcasm: data directive with no arguments: " .. (st.raw or st.name))
			end
			for _, f in fields do numArg(f, st.raw or st.name) end
			return w * #fields
		end
		if st.name == ".ascii" or st.name == ".asciz" or st.name == ".string" then
			local zTerm = (st.name ~= ".ascii") and 1 or 0
			local fields = splitDataArgs(st.args or "")
			if #fields == 0 then
				error("sarcasm: data directive with no arguments: " .. (st.raw or st.name))
			end
			local total, bytes = 0, {}
			for _, f in fields do
				local body = string.match(f, '^"(.*)"$')
				if not body then
					error("sarcasm: " .. st.name .. " argument is not a string literal: "
						.. (st.raw or st.name))
				end
				local b = decodeGasString(body, st.raw or st.name)
				for _, v in b do table.insert(bytes, v) end
				total += #b + zTerm
				for _ = 1, zTerm do table.insert(bytes, 0) end
			end
			st.decodedBytes = bytes   -- re-emitted as .byte lists (exact size)
			return total
		end
		if st.name == ".zero" or st.name == ".space" or st.name == ".skip" then
			local fields = splitDataArgs(st.args or "")
			if #fields < 1 or #fields > 2 then
				error("sarcasm: malformed " .. st.name .. " directive: " .. (st.raw or st.name))
			end
			local n = numArg(fields[1], st.raw or st.name)
			if n ~= math.floor(n) or n < 0 then
				error("sarcasm: " .. st.name .. " size is not a non-negative integer: "
					.. (st.raw or st.name))
			end
			return n
		end
		if st.name == ".fill" then
			local fields = splitDataArgs(st.args or "")
			if #fields < 1 or #fields > 3 then
				error("sarcasm: malformed .fill directive: " .. (st.raw or st.name))
			end
			local count = numArg(fields[1], st.raw or st.name)
			local sz = fields[2] and numArg(fields[2], st.raw or st.name) or 1
			if count < 0 or sz < 0 then
				error("sarcasm: .fill count/size is negative: " .. (st.raw or st.name))
			end
			return count * sz
		end
		error("sarcasm: data directive '" .. st.name .. "' is not supported in a data"
			.. " object (only scalar initializers: .byte/.short/.long/.quad, .ascii/.asciz,"
			.. " .zero/.space/.fill, alignment padding): " .. (st.raw or st.name))
	end

	-- Tolerant size: returns the size, or (0, errmsg) for content outside the
	-- supported set — the error is re-raised at finalization iff the block is live.
	local function trySize(st)
		local ok, res = pcall(itemSize, st)
		if ok then return res, nil end
		return 0, res
	end

	-- Alignment (bytes) of an alignment-directive statement, or nil if the form is
	-- unsupported (a 3-arg .p2align with a max-skip, a nonzero fill, a
	-- non-power-of-two .align — those change the layout semantics we mirror).
	local function itemAlign(st)
		local fields = splitDataArgs(st.args or "")
		local a
		if st.name == ".p2align" then
			if #fields < 1 or #fields > 2 then return nil end
			local p = tonumber(fields[1])
			if not p or p ~= math.floor(p) or p < 0 then return nil end
			a = 2 ^ p
		else   -- .align: on x86 ELF the argument is the byte alignment
			if #fields ~= 1 then return nil end
			a = tonumber(fields[1])
			if not a or a ~= math.floor(a) or a < 1 or a < 2
				or a ~= 2 ^ math.floor(math.log(a) / math.log(2) + 0.5) then
				return nil
			end
		end
		if fields[2] and tonumber(fields[2]) ~= 0 then return nil end   -- nonzero fill
		return a
	end

	local block = nil   -- { kind, items = {..}, labels = { {name=, off=}... }, maxAlign, off }

	local function openBlock(kind)
		if not block then block = { kind = kind, items = {}, labels = {}, maxAlign = 1, off = 0 } end
	end

	local function defineDesc(cname, exported, hiddenFlag, readonly, doAlign)
		local pad = (doAlign > 16) and (doAlign - 16) or 0
		local doSym = "pizlonatedDO_" .. cname
		return {
			doSym = doSym, baseSym = ".Lglobalbase_" .. cname,
			readonly = readonly, exported = exported, hidden = hiddenFlag or nil,
			gpSym = ".LpizlonatedGP_" .. cname, gsSym = ".LpizlonatedGS_" .. cname,
			payloadOff = pad + 16,
		}, pad
	end

	-- Total content extent of a block (the end offset of its last item).
	local function blockOffEnd(items)
		local last = items[#items]
		return last and (last.off + (last.size or 0)) or 0
	end

	-- Block finalization: liveness, validation, segmentation at exported labels, and
	-- DO/desc construction. Called at every block boundary (and at the end of the walk).
	local function finalizeBlock()
		if not block then return end
		local items, labels, kind, maxAlign = block.items, block.labels, block.kind, block.maxAlign
		block = nil
		-- Liveness: any label referenced from compiled code or exported makes the
		-- block live; an unreferenced, non-global block is dead content (dropped).
		local anyLive = false
		for _, l in labels do
			if referenced[l.name] or globals[l.name] then anyLive = true break end
		end
		if not anyLive then return end
		-- The block is live: deferred content errors fire now.
		for _, it in items do
			if it.err then error(it.err) end
		end
		if #labels == 0 then
			-- Live by construction impossible (liveness needs a label), but keep a
			-- defensive drop: unlabeled data is unnameable padding.
			return
		end
		-- Segmentation: an EXPORTED label starts its own segment (extent to the next
		-- label, so C-side bounds are exact), and a label following an exported
		-- label's segment starts a fresh local segment. Segments only start at
		-- labels; unlabeled items before a segment's first label belong to the
		-- preceding segment — and items ahead of the FIRST label ride along in the
		-- first (non-exported) segment's payload (keccak1600's `.align 256` + eight
		-- zero quads before `iotas`: the leading bytes position the label at the
		-- exact input offset — the `testq $255,%r15` round-count trick depends on
		-- it). Every EXPORTED segment's first label still sits at payload offset 0
		-- (so its unsafe-export alias `doSym + pad + 16` names it exactly, and its
		-- leading items drop like before); a local first label sits at its natural
		-- input offset.
		local segs = {}
		local cur = nil
		for _, l in labels do
			local startsExport = globals[l.name] and true or false
			if startsExport or cur == nil or cur.exported then
				local startOff = (cur == nil and not startsExport) and 0 or l.off
				cur = { firstLabel = l, exported = startsExport, labels = {}, startOff = startOff }
				table.insert(segs, cur)
			end
			table.insert(cur.labels, l)
		end
		for si, seg in segs do
			local segStart = seg.startOff
			local segEnd = (segs[si + 1] and segs[si + 1].startOff) or blockOffEnd(items)
			-- segment liveness: exported, or any label referenced
			local segLive = seg.exported
			if not segLive then
				for _, l in seg.labels do
					if referenced[l.name] then segLive = true break end
				end
			end
			if segLive then
				local firstSym = seg.firstLabel.name
				if dataGlobals[firstSym] then
					error("sarcasm: duplicate definition of global '" .. firstSym .. "'")
				end
				local doAlign = math.max(8, maxAlign)
				local desc, pad = defineDesc(firstSym, seg.exported,
					hidden[firstSym], kind == "ro", doAlign)
				local payloadSize = math.max(8, math.ceil((segEnd - segStart) / 8) * 8)
				local segItems = {}
				for _, it in items do
					if it.off >= segStart and it.off < segEnd then
						table.insert(segItems, { kind = it.kind, name = it.st.name,
							args = it.kind == "data" and it.st.args or nil,
							decoded = it.kind == "data" and it.st.decodedBytes or nil,
							off = it.off - segStart, size = it.size })
					end
				end
				for _, l in seg.labels do
					if l ~= seg.firstLabel and dataGlobals[l.name] then
						error("sarcasm: duplicate definition of global '" .. l.name .. "'")
					end
					dataGlobals[l.name] = desc
				end
				table.insert(dos, {
					firstSym = firstSym, doSym = desc.doSym, baseSym = desc.baseSym,
					gpSym = desc.gpSym, gsSym = desc.gsSym,
					readonly = desc.readonly,
					exportedSym = seg.exported and firstSym or nil,
					hidden = hidden[firstSym] or nil,
					align = doAlign, pad = pad, payloadSize = payloadSize,
					totalSize = pad + 16 + payloadSize,
					items = segItems,
				})
			end
		end
	end

	-- The walk. `curSec` is the current section kind (nil = not collectable);
	-- statements consumed by splitFunctions (function content) are skipped here and
	-- always break a block.
	local curSec = nil
	for i, st in stmts do
		if consumed[i] then
			finalizeBlock()
		elseif st.kind == "directive" then
			local nm = st.name
			if nm == ".section" then
				finalizeBlock()
				local secName = string.match(st.args or "", '^%s*"?([^",]+)"?')
				curSec = secName and sectionKind(secName) or nil
			elseif nm == ".text" then finalizeBlock(); curSec = nil
			elseif nm == ".data" then finalizeBlock(); curSec = "w"
			elseif nm == ".rodata" then finalizeBlock(); curSec = "ro"
			elseif nm == ".bss" then finalizeBlock(); curSec = "w"
			elseif nm == ".previous" or nm == ".pushsection" or nm == ".popsection" then
				finalizeBlock(); curSec = nil
			elseif nm == ".comm" or nm == ".common" or nm == ".lcomm" then
				finalizeBlock()
				-- `.comm name,size[,align]` (.common alias): its own writable DO of
				-- `size` zero bytes, exported (a documented semantic change: commons
				-- become strong definitions and lose cross-module merging).
				-- `.lcomm` is the same but local. Consumed in ANY section.
				local fields = splitDataArgs(st.args or "")
				local cname = fields[1]
				local csize = fields[2] and tonumber(fields[2])
				local calign = fields[3] and tonumber(fields[3])
				if not cname or not string.match(cname, "^[%w_.$]+$") or not csize
					or csize ~= math.floor(csize) or csize < 0
					or (calign ~= nil and (calign ~= math.floor(calign) or calign < 1
						or calign ~= 2 ^ math.floor(math.log(calign) / math.log(2) + 0.5))) then
					error("sarcasm: malformed " .. nm .. " directive (expected `"
						.. nm .. " name,size[,align]` with a numeric size and a"
						.. " power-of-two alignment): " .. (st.raw or nm))
				end
				if st.annotation then
					error("sarcasm: annotation '" .. st.annotation .. "' on a directive ('"
						.. nm .. "') is not supported")
				end
				local exported = (nm ~= ".lcomm")
				if dataGlobals[cname] then
					error("sarcasm: duplicate definition of global '" .. cname .. "'")
				end
				local doAlign = math.max(8, calign or 8)
				local desc, pad = defineDesc(cname, exported, hidden[cname], false, doAlign)
				local payloadSize = math.max(8, math.ceil(csize / 8) * 8)
				dataGlobals[cname] = desc
				table.insert(dos, {
					firstSym = cname, doSym = desc.doSym, baseSym = desc.baseSym,
					gpSym = desc.gpSym, gsSym = desc.gsSym,
					readonly = false, exportedSym = exported and cname or nil,
					hidden = hidden[cname] or nil,
					align = doAlign, pad = pad, payloadSize = payloadSize,
					totalSize = pad + 16 + payloadSize,
					items = { { kind = "label", name = cname, off = 0, size = 0 },
						{ kind = "data", name = ".zero", args = tostring(csize), off = 0, size = csize } },
				})
				consumed[i] = "data"
			elseif curSec and (nm == ".p2align" or nm == ".align") then
				-- alignment padding inside (or leading) a data block
				if st.annotation then
					error("sarcasm: annotation '" .. st.annotation .. "' on a directive ('"
						.. nm .. "') is not supported")
				end
				local a = itemAlign(st)
				openBlock(curSec)
				if not a then
					-- Tolerated like bad content: the error is re-raised at
					-- finalization iff the block proves live (dead-content drop).
					table.insert(block.items, { kind = "data", st = st, off = block.off,
						size = 0, err = "sarcasm: unsupported alignment directive in a data"
							.. " object (only `.p2align N` / `.p2align N, 0` and a power-of-two"
							.. " `.align A` are supported): " .. (st.raw or nm) })
				else
					local pad = (block.off % a == 0) and 0 or (a - block.off % a)
					block.off += pad
					if a > block.maxAlign then block.maxAlign = a end
					-- (no item recorded: the gap is re-emitted from the offsets)
				end
				consumed[i] = "data"
			elseif curSec and transform.DATA_DIRECTIVES[nm]
				and nm ~= ".equ" and nm ~= ".set" and nm ~= ".macro" then
				-- A data-emitting directive in a collectable section: collect it
				-- (tolerantly — content errors are re-raised at finalization iff the
				-- block proves live, keeping the dead-content drop rule).
				if st.annotation then
					error("sarcasm: annotation '" .. st.annotation .. "' on a directive ('"
						.. nm .. "') is not supported")
				end
				openBlock(curSec)
				local sz, err = trySize(st)
				table.insert(block.items, { kind = "data", st = st, off = block.off,
					size = sz, err = err })
				block.off += sz
				consumed[i] = "data"
			elseif nm == ".type" or nm == ".size" or nm == ".globl" or nm == ".global"
				or nm == ".hidden" or nm == ".weak" or nm == ".local" or nm == ".protected"
				or nm == ".internal" then
				-- Symbol metadata: transparent inside a data block (the .globl/.hidden
				-- sets were already recorded by the driver scan).
			elseif transform.DATA_DIRECTIVES[nm] then
				-- data directive in a non-collectable section: NOT collected; the
				-- top-level scan keeps its dead-drop/live-reject behavior.
				finalizeBlock()
			else
				-- any other directive (structural): breaks a block but keeps the section
				finalizeBlock()
			end
		elseif st.kind == "label" then
			if curSec then
				if st.annotation then
					error("sarcasm: annotation '" .. st.annotation .. "' on a label ('" .. st.name
						.. "') in a data section is not supported")
				end
				openBlock(curSec)
				table.insert(block.labels, { name = st.name, off = block.off })
				table.insert(block.items, { kind = "label", st = st, off = block.off, size = 0 })
				consumed[i] = "data"
			else
				-- a label in a non-collectable section falls to the top-level scan
				-- (dead-drop / live-reject), which also breaks any open block
				finalizeBlock()
			end
		elseif st.kind == "insn" then
			finalizeBlock()   -- the top-level scan raises "instruction outside any function"
		end
		-- blanks are transparent
	end
	finalizeBlock()

	return dataGlobals, dos
end
-- Compile one parsed input into output assembly text. `debug` (from -g) makes origins
-- carry source file/line/column (pointing at the input assembly) for runtime diagnostics.
function sarcasm.compile(stmts, fileDirective, debug, target)
	target = target or makeTarget("arm64", nil)
	local glue = target.glue
	-- The .globl/.hidden sets: needed by splitFunctions (a .globl no-signature
	-- function always errors) and by local-subroutine discovery (a .globl label
	-- is never a subroutine), ahead of the top-level scan that also uses them.
	-- .globl/.global take a COMMA-SEPARATED name list (gas accepts `.globl a, b`).
	-- Register EVERY name: the single-name regex silently dropped the later names,
	-- so a global label they named escaped the top-level rejection below and
	-- vanished from the object (the same file with one name per directive was
	-- cleanly rejected — an opaque link error instead of a clean diagnostic).
	-- (.weak/.protected/.internal never flow through this scan at all — they stay
	-- accepted+ignored like other structural directives; making them start
	-- registering names would change their semantics, not fix a drop. .hidden IS
	-- recorded now: the x86_64 data-object collector propagates it to the emitted
	-- getter/alias, without changing the dead-drop/live-reject scan.)
	local globals = {}
	local hidden = {}
	for _, st in stmts do
		if st.kind == "directive" and (st.name == ".globl" or st.name == ".global") then
			for nm in string.gmatch(st.args or "", "[%w_.$]+") do
				globals[nm] = true
			end
		end
		if st.kind == "directive" and st.name == ".hidden" then
			for nm in string.gmatch(st.args or "", "[%w_.$]+") do
				hidden[nm] = true
			end
		end
	end
	-- Local subroutines (x86_64): the prescan collects the targets of every
	-- potentially-local call (unannotated or `#! local`) so splitFunctions can
	-- leave called no-signature `.type name,@function` labels unconsumed for
	-- discovery; discovery then claims the subroutine regions (marking their
	-- statements consumed so the top-level scan skips them) and validates the
	-- local-call graph (recursion, fall-off, escaping branches). On arm64 a
	-- discovered local subroutine is a clean "not yet supported" error.
	local localCalled = localcall.prescanCalls(stmts, target.arch)
	local functions, consumed, aliases, aliasGlobals = splitFunctions(stmts,
		{ globals = globals, localCalled = localCalled, arch = target.arch })
	prof.mark("splitFunctions")
	local locals = localcall.discover(stmts, consumed, globals, target.arch,
		{ functions = functions, aliases = aliases })
	prof.mark("localcall.discover")
	-- Same-file FUNCTION name resolution (x86_64, for `#! funcref` and
	-- `.section .init` calls): every signature-annotated function name AND
	-- alias-entry name maps to the OWNING function (an alias entry shares the
	-- owner's function object — pizlonatedFO_<owner>). A lookup miss means the
	-- symbol is not defined as a function here (extern, data, or code label).
	-- subNames names the local subroutines: they have no signature and no
	-- function object, so they get a dedicated rejection rather than the
	-- extern treatment (whose getter symbol would only fail at link time).
	local fnNames = {}
	for _, fn in functions do fnNames[fn.name] = fn.name end
	for a, o in aliases do fnNames[a] = o end
	local subNames = nil
	if locals then
		subNames = {}
		for nm in pairs(locals.subs) do subNames[nm] = true end
	end
	-- x86_64: decode .byte-encoded instructions in EVERY function body up front
	-- (the per-function loop decodes again below — a no-op): the cross-function
	-- jump analysis (tailcall.luau) and its B2 shared-tail region extraction
	-- need spelled instructions (a ret hides in every OpenSSL tail's rep-ret
	-- .byte run).
	if target.decodeBody then
		for _, fn in functions do fn.body = target.decodeBody(fn.body) end
	end
	prof.mark("decodeBody")
	-- Cross-function jumps (x86_64): the B1/B2 resolution context (tail branches
	-- to signatured function entries become call+epilogue; mid-body shared-tail
	-- joins clone the region into the jumper — tailcall.luau). arm64 does not
	-- run this: an annotated jump reaches validateBody's annotation walk, which
	-- rejects it with a clean "not yet supported" error.
	local xj = nil
	if target.arch == "x86_64" then
		xj = tailcall.buildContext(functions, aliases, locals)
	end
	local archDirectives = {}   -- arm64 only: forwarded .arch/.arch_extension lines
	local dataGlobals, dos = nil, nil   -- x86_64 only: collected data objects (DOs)
	-- x86_64 only: validated `.section .init`/`.fini` annotated calls, in input
	-- order (see the top-level scan below; emitted by the glue after the data
	-- objects).
	local initFrags = {}
	do
		-- Top-level (inter-function) content scan. On x86_64 the data-object
		-- collector (collectX86Data, above) runs FIRST: contiguous data under
		-- .rodata/.data/.bss and .comm/.lcomm becomes Fil-C data objects, so the
		-- scan below never sees it. Everything else — and on arm64 ALL top-level
		-- data, with identical historical semantics — follows the rule below.
		-- (Historical: on x86_64 the original behavior silently dropped top-level
		-- content, so a user `.data`/`.quad`/`.globl myglob` block vanished while
		-- the compile and — when nothing referenced the symbol — the link both
		-- succeeded: silent wrong behavior, and combined with a
		-- `leaq myglob(%rip)` reach it hands a raw address to a non-pointer web.)
		-- Content a user can OBSERVE that sarcasm cannot make memory-safe — data
		-- with a global or code-referenced label outside the collectable
		-- sections, symbol/macro definitions (`.equ`/`.set`/`.macro`), code
		-- outside functions — must be REJECTED, not silently dropped. Provably
		-- dead content — an unreferenced, non-global local label and the data
		-- bytes under it — is dropped: nothing in this file or any other can name
		-- it, so dropping it cannot miscompile (this keeps detector-probe inputs
		-- full of dead .rodata strings working).
		-- Structural directives (.text/.section headers, .p2align, .cfi_*, .type,
		-- .size, .ident, .addrsig*, .ltorg, .arch, ...) remain accepted+ignored —
		-- including `.section .note.GNU-stack,"",@progbits` and `.text`, which every
		-- real input carries — except .arch/.arch_extension on arm64, which are
		-- FORWARDED into the output: the default `as` target is base armv8-a, so an
		-- input that enables ISA extensions (crypto, FP16, ...) for the instructions
		-- sarcasm passes through would otherwise fail to assemble the rewritten file.
		for _, st in stmts do
			if target.arch == "arm64" and st.kind == "directive"
				and (st.name == ".arch" or st.name == ".arch_extension") then
				table.insert(archDirectives, "\t" .. st.name .. "\t" .. (st.args or ""))
			end
		end
		local referenced = {}
		local function noteRef(op)
			if not op then return end
			if op.kind == "sym" then referenced[op.text] = true end
			if op.kind == "mem" then
				noteRef(op.base); noteRef(op.index)
				if op.dispSym then referenced[op.dispSym] = true end
			end
		end
		for i, st in stmts do
			if consumed[i] then
				for _, op in (st.operands or {}) do noteRef(op) end
			end
		end
		-- x86_64 only: collect top-level data (and .comm/.lcomm) into Fil-C data
		-- objects (DOs) BEFORE the rejection scan — collected statements are marked
		-- consumed, so the scan skips them. arm64 keeps the scan's identical
		-- dead-drop/live-reject behavior for all top-level data.
		if target.arch == "x86_64" then
			dataGlobals, dos = collectX86Data(stmts, consumed, globals, hidden, referenced)
		end
		local recentLabels = {}
		-- `.section .init` / `.section .fini` content (x86_64): a straight-line
		-- run of annotated DIRECT calls at top level (the OpenSSL x86_64cpuid
		-- shape — the linker glues .init chunks into _init, which Fil-C runs as
		-- constructors). topSection tracks the current top-level section (the
		-- section-switch directives are never consumed, so this loop sees them
		-- all); initFrags collects the validated calls in input order for the
		-- glue emission below. Anything else in those sections — a label, a
		-- non-call instruction, an unannotated call, data — is a clean
		-- rejection (on arm64 the whole thing is "not yet supported").
		local topSection = nil
		local function sectionHead(args)
			local nm = args and string.match(args, "^%s*([^,%s]+)")
			return nm
		end
		for i, st in stmts do
			if not consumed[i] then
				local sectSwitch = st.kind == "directive" and SECTION_DIRECTIVES[st.name]
				if st.kind == "directive" then
					if st.name == ".section" or st.name == ".pushsection" then
						topSection = sectionHead(st.args)
					elseif st.name == ".text" or st.name == ".data"
						or st.name == ".rodata" or st.name == ".bss" then
						topSection = st.name
					elseif st.name == ".previous" or st.name == ".popsection" then
						topSection = nil
					end
				end
				local inInit = (topSection == ".init" or topSection == ".fini")
					and not sectSwitch
				if inInit and st.kind ~= "blank" then
					local sectDesc = "`.section " .. topSection .. "`"
					local raw = st.raw or st.mnemonic
						or (st.kind == "label" and (st.name .. ":") or nil)
						or (st.kind == "directive" and (st.name .. " " .. (st.args or "")) or "?")
					if st.kind == "label" then
						error("sarcasm: a label in " .. sectDesc .. " content is not supported"
							.. " (only annotated direct calls may appear there): " .. raw)
					elseif st.kind == "directive" then
						-- Structural directives (visibility, .type, alignment, ...)
						-- emit no section content and stay accepted; data-emitting
						-- directives and annotated ones are rejected.
						if transform.DATA_DIRECTIVES[st.name] then
							error("sarcasm: data in " .. sectDesc .. " content is not supported"
								.. " (only annotated direct calls may appear there): directive '"
								.. st.name .. "'")
						end
						if st.annotation then
							error("sarcasm: annotation '" .. st.annotation .. "' on a directive ('"
								.. st.name .. "') is not supported")
						end
					else
						-- an instruction in .init/.fini content
						if target.arch ~= "x86_64" then
							error("sarcasm: " .. sectDesc .. " content is not yet supported"
								.. " on this target: " .. raw)
						end
						local m = string.lower(st.mnemonic or "")
						local op = st.operands and st.operands[1]
						if (m ~= "call" and m ~= "callq") or not op or op.kind ~= "sym" then
							error("sarcasm: only annotated direct calls are supported in "
								.. sectDesc .. " content: " .. raw)
						end
						if not st.annotation then
							error("sarcasm: call to '" .. op.text .. "' in " .. sectDesc
								.. " content has no signature annotation (annotate it, e.g."
								.. " `call " .. op.text .. " ;! void()`): " .. raw)
						end
						-- The .init context runs from _init as plain SysV code — there
						-- are no incoming Fil-C argument registers, so only void() calls
						-- can be marshalled soundly (they take no args and return
						-- nothing).
						local cs = sig.parseSignature(st.annotation)
						if not cs.isVoid or cs.argWords ~= 0 then
							error("sarcasm: only void() calls are supported in " .. sectDesc
								.. " content (the .init context has no Fil-C argument"
								.. " state): " .. raw)
						end
						local owner = fnNames[op.text]
						if not owner and dataGlobals and dataGlobals[op.text] then
							error("sarcasm: call target '" .. op.text .. "' in " .. sectDesc
								.. " content is a data object, not a function: " .. raw)
						end
						if not owner and subNames and subNames[op.text] then
							error("sarcasm: call target '" .. op.text .. "' in " .. sectDesc
								.. " content is a local subroutine (it has no function"
								.. " object): " .. raw)
						end
						table.insert(initFrags, { sect = topSection, target = op.text,
							owner = owner, line = st.line })
						consumed[i] = "init"
					end
				elseif st.kind == "blank" then
					if st.annotation then
						error("sarcasm: annotation '" .. st.annotation
							.. "' on an otherwise-empty line (annotations go on the instruction's own line)")
					end
				elseif st.kind == "insn" then
					error("sarcasm: instruction outside any function (only annotated functions"
						.. " are compiled): " .. (st.raw or st.mnemonic))
				elseif st.kind == "label" then
					if globals[st.name] or referenced[st.name] then
						error("sarcasm: label '" .. st.name .. "' outside any function is not supported"
							.. " (globals cannot be made memory-safe automatically)")
					end
					table.insert(recentLabels, st.name)
				elseif st.kind == "directive" then
					if transform.DATA_DIRECTIVES[st.name] then
						-- .equ/.set/.macro/.comm define symbols-as-values whose uses we
						-- cannot track: always reject. Data emitters reject only when a
						-- live (global or referenced) label names them. (x86_64: .comm
						-- and friends were already consumed by the data-object
						-- collector, so the .comm arm only fires on arm64.)
						local always = st.name == ".equ" or st.name == ".set" or st.name == ".macro"
							or st.name == ".comm" or st.name == ".common" or st.name == ".lcomm"
						local live = nil
						for _, ln in recentLabels do
							if globals[ln] or referenced[ln] then live = ln; break end
						end
						if always or live then
							error("sarcasm: data sections and symbol/macro definitions are not supported"
								.. " (globals cannot be made memory-safe automatically): directive '"
								.. st.name .. "'" .. (live and (" (under label '" .. live .. "')") or ""))
						end
						recentLabels = {}
					end
					if st.annotation then
						error("sarcasm: annotation '" .. st.annotation .. "' on a directive ('"
							.. st.name .. "') is not supported")
					end
				end
			end
		end
	end
	local out = {}
	if fileDirective then table.insert(out, "\t.file\t\"" .. fileDirective .. "\"") end
	table.insert(out, "\t.text")
	for _, d in ipairs(archDirectives) do table.insert(out, d) end

	local calledExterns = {}   -- sig-name -> desc, for callsite thunks
	local definedFI = {}       -- FI symbols defined here (skip resolver thunks for these)
	local emittedET = {}       -- generic thunks are per-signature: emit each once per module

	-- File-global user-label name set. Synthesized label names are function-local
	-- BY CONSTRUCTION (they embed the function name) EXCEPT the x86_64 rel8
	-- trampoline names .Lsarctramp_<fn>_<n>{t,s}, which are file-global assembly
	-- symbols: a user label in ONE function named exactly like another function's
	-- generated trampoline name collides in the object (gas dies on the temp file
	-- with "symbol ... is already defined" plus a knock-on rel8 error). Collect
	-- every user label of every function here so the render-stage trampoline
	-- allocator can bump past all of them (x86_64_render.header seeds its used-name
	-- set from this; arm64 ignores it).
	local fileUserLabels = {}
	for _, fn in functions do
		for _, st in fn.body do
			if st.kind == "label" then fileUserLabels[st.name] = true end
		end
	end
	-- The localcall clone renames (`.Llocalsub_<fn>_<sub>_<orig>`) are file-global
	-- symbols too: bump them past EVERY label in the file, including top-level
	-- data/subroutine labels that are not in any function body.
	local fileAllLabels = {}
	for _, st in stmts do
		if st.kind == "label" then fileAllLabels[st.name] = true end
	end
	for n in fileUserLabels do fileAllLabels[n] = true end

	for _, fn in functions do
		local sigDesc = sig.parseSignature(fn.sigAnn)
		-- Signature class check, arch-aware: float and double are fully supported
		-- on BOTH arches (FP args pass in xmm0..xmm7 / v0..v7 in declaration order
		-- among the FP args, FP returns ride in xmm0 / v0, and the generic/buffer
		-- thunks marshal them); long double stays rejected (filcc gives it
		-- signature word 0 and NO fast CC) and so do the vector classes (a vec4
		-- signature would need q-register / xmm-quad CC machinery).
		sig.checkSupportedClasses(sigDesc, nil, { arch = target.arch })
		-- Entry-signature arity limits. The fast CC packs GPR argument words DENSELY
		-- (a pointer arg = 2 words); x86_64 packs the first FOUR into rdx,rcx,r8,r9
		-- and passes the rest on the stack as densely packed 8-byte slots, which
		-- sarcasm now marshals in both directions (entry unpack + yolo stack-arg
		-- redirection; the SysV 7th+ integer arguments are read from 8+8i(%rsp) at
		-- entry), so x86_64 signatures have NO GPR word limit. arm64 packs into the
		-- SIX registers x2..x7 and keeps its 3-argument/6-word limit (stack words
		-- are not marshalled there). FP args (float/double, both arches) pass in
		-- the FP registers instead and are bounded separately (at most 8:
		-- xmm0..xmm7 on x86_64, v0..v7 on arm64).
		if target.arch ~= "x86_64" then
			local gprArgs, words = 0, 0
			for k = 1, sigDesc.argWords do
				if not sig.isFpClass(sigDesc.argTypes[k]) then
					gprArgs += 1
					words += (sigDesc.argTypes[k] == 7) and 2 or 1
				end
			end
			if gprArgs > 3 then
				error("sarcasm: >3 register args not yet supported")
			end
			if words > 6 then
				error("sarcasm: function '" .. fn.name
					.. "': >6 register argument words not yet supported (this signature needs "
					.. tostring(words) .. ")")
			end
		end
		local fpArgs = 0
		for k = 1, sigDesc.argWords do
			if sig.isFpClass(sigDesc.argTypes[k]) then fpArgs += 1 end
		end
		if fpArgs > 8 then
			error("sarcasm: function '" .. fn.name
				.. "': >8 floating-point arguments not yet supported (this signature needs "
				.. tostring(fpArgs) .. ")")
		end
		-- body = statements after the function label up to (not incl) .size
		local body = {}
		for _, st in fn.body do table.insert(body, st) end
		-- Cross-function jumps (x86_64): rewrite B1 tail branches (annotated
		-- call + epilogue jump, via blocks for conditional ones) and B2
		-- mid-body shared-tail joins (per-jumper region clones) BEFORE localcall
		-- augmentation — local calls inside the B2 clones feed its subroutine
		-- closure — and before any validation (the rewrite turns cross-function
		-- branches into local ones; validateBody then sees the final body).
		if xj then
			body = tailcall.rewriteFunction(fn.name, body, xj, locals, fileAllLabels)
		end
		-- Local subroutines: append this caller's per-caller clones (renamed
		-- labels, marked callsites/rets, continuation labels, the +8
		-- displacement rule on rsp-relative clone operands) BEFORE any other
		-- pass sees the body, so validation, the frame pass, the lift and
		-- regalloc all see caller+clones as one function.
		local clonesInfo = nil
		if locals and locals.callsByFn[fn.name] then
			body, clonesInfo = localcall.augment(fn.name, body, locals, fileAllLabels, target.arch)
		end
		-- x86_64 only: decode .byte-encoded instructions (OpenSSL perlasm
		-- legacy-gas workarounds) before any other pass sees the body, so
		-- validation, the frame pass and codegen all see ordinary spelled
		-- instructions. Undecodable sequences stay `.byte` directives and keep
		-- hitting the usual in-body data rejection.
		if target.decodeBody then body = target.decodeBody(body) end
		prof.mark("fn " .. fn.name .. " pre-transform")
		local tr = target.transformFunction(fn.name, sigDesc, body,
			{ globals = dataGlobals, clones = clonesInfo, fnNames = fnNames, subNames = subNames })
		prof.mark("fn " .. fn.name .. " transform")
		tr.fileUserLabels = fileUserLabels

		-- allocate registers (spilling to the stack + re-running IRC as needed)
		local colorResult = allocateWithSpilling(tr, target)
		prof.mark("fn " .. fn.name .. " regalloc")

		-- source-location info for -g origins (points at the input assembly)
		local fnDbg = debug and { filename = fileDirective or "?", funcLine = tr.funcLine or 0 } or nil

		-- emit getter, FIP body, generic thunk, FO/origins, access origins, alias
		glue.getter(out, fn.name)
		target.emitFip(out, tr, colorResult)
		if not emittedET[sigDesc.signature] then
			glue.genericThunk(out, sigDesc.signature, sigDesc.argWords, sigDesc.retWords, sigDesc.retIsPtr,
				sigDesc.argTypes, sigDesc.retTypes)
			emittedET[sigDesc.signature] = true
		end
		glue.foAndOrigins(out, fn.name, sigDesc.signature, tr.numRoots, fnDbg)
		for _, ao in tr.accessOrigins do
			if ao.maskedOrigin then
				-- AVX512 {k}-masked / AVX2 vector-masked fail-site origin: a
				-- plain 16-byte filc_origin (the masked fail functions take a
				-- filc_origin*, not an optimized-access-check origin).
				glue.maskedOrigin(out, ao.sym, fn.name,
					debug and { line = ao.line or 0 } or nil)
			else
				glue.accessOrigin(out, ao.sym, ao.size, ao.align, fn.name, ao.write,
					debug and { line = ao.line or 0 } or nil)
			end
		end
		glue.alias(out, fn.name, sigDesc.signature)
		definedFI["pizlonatedFI" .. sigDesc.signature .. "_" .. fn.name] = true
		-- .globl ALIAS entry labels (asm_AES_encrypt for AES_encrypt): the alias
		-- shares this function's body and signature, so its getter and
		-- direct-call (FI) symbols are plain .set aliases — without them, C
		-- callers of the alias (and same-file annotated calls to it) would not
		-- link. Emission order is the input's statement order (deterministic).
		for _, an in ipairs(aliasGlobals[fn.name] or {}) do
			glue.getterAlias(out, an, fn.name, hidden[an])
			glue.callAlias(out, an, fn.name, sigDesc.signature, hidden[an])
			definedFI["pizlonatedFI" .. sigDesc.signature .. "_" .. an] = true
		end

		for _, ext in tr.calledExterns do
			calledExterns[ext.sigName] = ext
		end
		prof.mark("fn " .. fn.name .. " emit")
	end

	-- Emit weak/hidden callsite resolver thunks for called externals (deduped, and only
	-- when we don't define the same FI symbol locally). DETERMINISM: calledExterns is
	-- keyed by sig-name STRING, and a VM's string-keyed hash-table iteration order is an
	-- implementation detail not derived from the input — it must not choose emission
	-- order, so sort the keys: identical input then emits identical output. Sorting also
	-- keeps that guarantee if a future Luau seeds string hashes.
	local thunkSigs = {}
	for sigName in calledExterns do table.insert(thunkSigs, sigName) end
	table.sort(thunkSigs)
	for _, sigName in thunkSigs do
		local ext = calledExterns[sigName]
		if not definedFI[sigName] then
			glue.callsiteThunk(out, ext.name, ext.sigDesc)
		end
	end

	-- x86_64 data objects (DOs, GP cells, slow-init functions, getters, unsafe-export
	-- aliases) for the file's collected globals. Emission order is input order
	-- (deterministic).
	if dos and glue.dataGlobals then glue.dataGlobals(out, dos) end

	-- `.section .init`/`.fini` fragments (x86_64): the collected annotated calls,
	-- marshalled as Fil-C constructor calls, emitted back into their sections in
	-- input order (deterministic).
	if #initFrags > 0 and glue.initSections then glue.initSections(out, initFrags) end

	table.insert(out, "\t.section\t\".note.GNU-stack\",\"\",@progbits")
	return table.concat(out, "\n") .. "\n"
end

local PROGNAME = "sarcasm"
local VERSION = "0.1.0"

local USAGE = "usage: " .. PROGNAME .. " [-o <file>] [-g] [-S|--no-assemble] [--x86_64|--arm64] [--intel|--at&t] [--as <command>] <input.s>"

local HELP = table.concat({
	PROGNAME .. " " .. VERSION .. " — SAfe Runtime Capability-enforced Assembler",
	"",
	"Rewrites annotated Yolo-C ARM64/X86_64 assembly into memory-safe, Fil-C-linkable",
	"assembly, then assembles it with `as` (a drop-in-ish assembler replacement).",
	"",
	USAGE,
	"",
	"Options:",
	"  -o <file>          Output object file (default: <input>.o).",
	"                     With -S, the assembly output path (default: <input>.yolo.s).",
	"  -g                 Emit source file/line/column debug info in Fil-C origins",
	"                     (points at the input assembly) for better runtime diagnostics.",
	"  -S, --no-assemble  Emit transformed assembly only; do not run the assembler.",
	"  --as <command>     Assembler to invoke in the default mode (default: as).",
	"  --x86_64           Select the X86_64 backend; the input syntax (AT&T or Intel)",
	"                     is still auto-detected.",
	"  --arm64            Select the ARM64 backend.",
	"  --intel            Select the X86_64 backend with Intel input syntax.",
	"  --at&t             Select the X86_64 backend with AT&T input syntax.",
	"      --version      Print version information and exit.",
	"  -h, --help         Print this help and exit.",
	"",
	"Input is ARM64 or X86_64 (AT&T or Intel) assembly annotated with ;! directives, e.g.:",
	"  hash:  ;! unsigned(ptr)        (function label: Fil-C signature)",
	"  ldr x1, [x0]  ;! load ptr      (pointer load from memory)",
	"  str x1, [x0]  ;! store ptr     (pointer store to memory)",
	"  bl foo        ;! int(ptr, size_t)   (call: callee signature)",
	"",
	"Annotations may be introduced by ;! on either architecture, by #! on X86_64 (where #",
	"is the comment introducer), or by //! on ARM64 (where // is); the examples above use",
	";! throughout.",
	"",
	"Input is ARM64 or X86_64 (auto-detected, or forced with --x86_64/--arm64; the",
	"syntax is auto-detected, or forced with --intel/--at&t). The target options",
	"conflict with each other (--x86_64 with --arm64, --intel with --at&t, and any",
	"architecture selector with a syntax-pinning selector) and are rejected as usage",
	"errors. Assembly that cannot be proven safe is rejected.",
}, "\n")

-- Report a command-line usage error (clean message, no stack trace); exit code 2.
local function usageError(msg: string): number
	print(PROGNAME .. ": error: " .. msg)
	print("Try '" .. PROGNAME .. " --help' for more information.")
	return 2
end

-- Report a processing diagnostic (clean message, no stack trace); exit code 1.
local function diag(msg: string): number
	print(PROGNAME .. ": " .. msg)
	return 1
end

-- CLI: as-like. Args: [-o OUT] [-S|--no-assemble] [--as CMD] INPUT.s
function sarcasm.main(argv)
	local input, output, noAssemble, asCmd, debug = nil, nil, false, "as", false
	-- Explicit target selection: --x86_64/--arm64 force the architecture, --intel/--at&t
	-- force the architecture AND the input syntax. nil means auto-detect (detect.luau).
	-- Repeating a flag is harmless; conflicting flags are rejected as soon as both have
	-- been seen, with the most recently seen flag named first in the error.
	local forcedArch, forcedArchFlag = nil, nil
	local forcedSyntax, forcedSyntaxFlag = nil, nil
	local i = 2
	while i <= #argv do
		local a = argv[i]
		if a == "-h" or a == "--help" then
			print(HELP); return 0
		elseif a == "--version" then
			print(PROGNAME .. " " .. VERSION); return 0
		elseif a == "-o" then
			i += 1; output = argv[i]
			if not output then return usageError("option '-o' requires an argument") end
		elseif a == "-g" then
			debug = true
		elseif a == "-S" or a == "--no-assemble" then
			noAssemble = true
		elseif a == "--as" then
			i += 1; asCmd = argv[i]
			if not asCmd then return usageError("option '--as' requires an argument") end
		elseif a == "--x86_64" then
			if forcedArch == "arm64" then
				return usageError("options '--x86_64' and '" .. forcedArchFlag .. "' select conflicting architectures")
			end
			if forcedSyntax then
				return usageError("option '--x86_64' conflicts with '" .. forcedSyntaxFlag .. "', which pins the assembly syntax")
			end
			forcedArch, forcedArchFlag = "x86_64", "--x86_64"
		elseif a == "--arm64" then
			if forcedArch == "x86_64" then
				return usageError("options '--arm64' and '" .. forcedArchFlag .. "' select conflicting architectures")
			end
			forcedArch, forcedArchFlag = "arm64", "--arm64"
		elseif a == "--intel" or a == "--at&t" then
			local syn = (a == "--intel") and "intel" or "att"
			if forcedSyntax and forcedSyntax ~= syn then
				return usageError("options '" .. a .. "' and '" .. forcedSyntaxFlag .. "' select conflicting assembly syntaxes")
			end
			if forcedArch == "arm64" then
				return usageError("options '" .. a .. "' and '" .. forcedArchFlag .. "' select conflicting architectures")
			end
			if forcedArchFlag == "--x86_64" then
				return usageError("option '" .. a .. "' conflicts with '--x86_64', which leaves the assembly syntax to auto-detection")
			end
			forcedArch, forcedArchFlag = "x86_64", a
			forcedSyntax, forcedSyntaxFlag = syn, a
		elseif string.sub(a, 1, 1) == "-" then
			return usageError("unrecognized option '" .. a .. "'")
		else
			if input then
				return usageError("more than one input file given ('" .. input .. "' and '" .. a .. "')")
			end
			input = a
		end
		i += 1
	end
	if not input then
		print(USAGE)
		print("Try '" .. PROGNAME .. " --help' for more information.")
		return 2
	end
	if not fs.exists(input) then
		return diag(input .. ": no such file")
	end

	local text = fs.read(fs.open(input, "r"))
	local arch, syntax = detect.detect(text)
	-- Apply the explicit target flags. --intel/--at&t pin both the architecture and the
	-- input syntax; a bare --x86_64/--arm64 forces only the architecture and keeps the
	-- auto-detected syntax (so a .intel_syntax/.att_syntax directive in the input still
	-- applies to --x86_64; --arm64 always ends up with no syntax).
	if forcedSyntax then
		arch, syntax = "x86_64", forcedSyntax
	elseif forcedArch == "x86_64" then
		arch = "x86_64"
		-- A bare --x86_64 can still end up with NO syntax: detect returns a nil syntax
		-- only for arm64, so this happens exactly when --x86_64 is forced onto input
		-- with no syntax directive and no x86 marker. An unadorned x86_64 parse would
		-- silently be INTEL parsing (x86_64_parse.parseString only special-cases "att"),
		-- but GAS's default x86 syntax is AT&T, so default the syntaxless forced case
		-- to AT&T. An explicitly passed --intel/--at&t is applied above and still wins.
		syntax = syntax or "att"
	elseif forcedArch == "arm64" then
		arch, syntax = "arm64", nil
	end
	local target = makeTarget(arch, syntax)
	local fileName = string.match(input, "([^/]+)$")
	-- Parse + compile under one pcall: parse-time rejections (e.g. an unresolvable
	-- numeric label reference) get the same clean "sarcasm: <file>: <msg>" treatment
	-- as compile-time rejections.
	local ok, asm = pcall(function()
		local stmts = (arch == "x86_64") and x86_64_parse.parseString(text, syntax) or arm64_parse.parseString(text)
		prof.mark("parse")
		local r = sarcasm.compile(stmts, fileName, debug, target)
		prof.mark("compile")
		return r
	end)
	prof.report()
	if not ok then
		local msg = tostring(asm)
		-- Errors we raise deliberately are prefixed "sarcasm:" and are user-facing:
		-- present them cleanly. Anything else is an unexpected internal error, which we
		-- re-raise so the underlying stack trace is preserved for debugging.
		local idx = string.find(msg, "sarcasm:", 1, true)
		if idx then
			return diag(input .. ": " .. string.sub(msg, idx + 8):gsub("^%s+", ""))
		end
		error(msg, 0)
	end

	local function stripExt(p) return (string.gsub(p, "%.[%w]+$", "")) end

	if noAssemble then
		local outFile = output or (stripExt(input) .. ".yolo.s")
		fs.write(fs.open(outFile, "w+"), asm)
		return 0
	end

	-- default: write temp .yolo.s next to the .o then run `as` to produce the object.
	local objOut = output or (stripExt(input) .. ".o")
	local tmpAsm = stripExt(objOut) .. ".yolo.s"
	fs.write(fs.open(tmpAsm, "w+"), asm)
	local res = process.run({ asCmd, tmpAsm, "-o", objOut })
	if not res.ok then
		print(res.stderr)
		return res.exitcode ~= 0 and res.exitcode or 1
	end
	return 0
end

return sarcasm
