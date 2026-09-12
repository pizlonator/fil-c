--!nonstrict
-- The GIMSO transform (architecture-independent): rewrite a lifted yolo function into a
-- memory-safe Fil-C fast-entrypoint body (IR nodes), plus metadata for glue/prologue/
-- epilogue emission. The walk and every invisicap sequence (access checks, pointer loads/
-- stores with the FUGC store barrier, atomic pointer loads/stores/compare-exchange and
-- pointer RMWs via the Fil-C runtime, calls, pollchecks, allocations) live here ONCE; the
-- actual instructions are produced through a per-arch codegen `cg` (arm64_codegen /
-- x86_64_codegen), so the safety-critical logic is single-source across ISAs. (The
-- atomic pointer operations are shared: both backends implement their annotations —
-- x86_64 with cmpxchg/movq forms and SysV marshalling, arm64 with cas/LSE forms and
-- AAPCS64 marshalling — except `;! load store ptr`, which arm64 rejects for lack of
-- a non-atomic memory-destination RMW instruction.)
--
-- transformFunction(name, sigDesc, body, backend) where backend = { cg = <codegen module>,
-- frame = <frame module> }.

local lift = require("./lift")
local ptrflow = require("./ptrflow")
local sig = require("./sig")
local build = require("./build")
local framePass = require("./frame")

local prof = require("./prof")

local transform = {}

-- NOTE: the historical `;! alloca ...` annotations (`alloca size (x)`,
-- `alloca result (x)`, `alloca result size=N [align=A]`) were removed. Any
-- annotation mentioning `alloca` is now rejected as unrecognized (see
-- validateBody's unrecognized-annotation error). Stack allocation is spelled
-- with the `.alloca` directive instead (see README.md).

-- `save capability (name)` / `restore capability (name)` annotation clauses
-- (architecture-independent). A clause marks a value-producing (register-dest)
-- instruction: `save` records the destination register's capability under
-- `name`, and a later `restore` re-attaches that capability to its own
-- destination register (the restore must be dominated by the save). Clauses
-- must stand ALONE as the whole annotation: combining one with another
-- annotation (a call signature, `load ptr`, ...) is rejected —
-- copy the value through a mov first if the instruction already carries an
-- annotation. Returns nil when `ann` carries no capability clause, else
-- { save = <name|nil>, restore = <name|nil> }.
local function parseCapAnnot(ann)
	if not ann then return nil end
	if not string.find(ann, "capability", 1, true) then return nil end
	local clauses = {}
	for _, kind in { "save", "restore" } do
		local init = 1
		while true do
			local s, e, nm = string.find(ann, kind .. "%s+capability%s*%(%s*([%w_]+)%s*%)", init)
			if not s then break end
			table.insert(clauses, { kind = kind, s = s, e = e, name = nm })
			init = e + 1
		end
	end
	if #clauses == 0 then
		error("sarcasm: malformed capability annotation '" .. ann
			.. "' (expected `save capability (name)` or `restore capability (name)`"
			.. " as the whole annotation)")
	end
	local save, restore = nil, nil
	for _, cl in clauses do
		if not string.match(cl.name, "^[A-Za-z_][A-Za-z0-9_]*$") then
			error("sarcasm: invalid variable name '" .. cl.name .. "' in `" .. cl.kind
				.. " capability` annotation (a name starts with a letter or underscore"
				.. " and contains only letters, digits and underscores): " .. ann)
		end
		if cl.kind == "save" then
			if save then
				error("sarcasm: multiple `save capability` clauses in one annotation: " .. ann)
			end
			save = cl.name
		else
			if restore then
				error("sarcasm: multiple `restore capability` clauses in one annotation: " .. ann)
			end
			restore = cl.name
		end
	end
	-- Anything besides the clauses means a combination, which is not supported.
	table.sort(clauses, function(a, b) return a.s > b.s end)
	local base = ann
	for _, cl in clauses do
		base = string.sub(base, 1, cl.s - 1) .. string.sub(base, cl.e + 1)
	end
	if string.match(base, "%S") then
		error("sarcasm: `save/restore capability` cannot be combined with another annotation"
			.. " ('" .. ann .. "'); put it on its own instruction (copy the value through"
			.. " a mov first if the instruction already carries an annotation)")
	end
	return { save = save, restore = restore }
end

-- `use capability %reg` annotation (architecture-independent; `;!` on both
-- architectures, `#!`/`//!` also accepted on x86_64/arm64 respectively).
--
--   The single unified capability-source selector. It goes on either:
--   (a) an address-arithmetic instruction (add/sub/lea/and/or) whose inputs
--   carry MORE THAN ONE capability: the result draws its capability from
--   the NAMED register instead of the pointer-flow scan's (arbitrary)
--   pick. With zero or one capability sources the annotation is unnecessary
--   but harmless (it must still name a capability-carrying use). Example:
--   `add %rax,%r11` where both hold pointers — `#! use capability %r11`
--   keeps %r11's capability; or
--   (b) any instruction with a memory operand (loads, stores, vector moves
--   like `vmovdqu`, ...): the memory access is guarded by the NAMED
--   register's capability, which must be the memory operand's base or
--   index. It covers the case where the assembler moved the logical base
--   out of base position (x86_64-xlate.pl flips `disp(%r13,%rdi)` to
--   `disp(%rdi,%r13)` for performance when the base is %rbp/%r13 — the
--   address is unchanged, but the base-first capability selection would
--   then pick the wrong object; `vmovdqu %ymm,(%a,%b) #! use capability %a`
--   keeps the annotation on the written base), and stale-base shapes
--   (`xor %rdi,%rdi` leaves a zero-valued scalar on a stale pointer web;
--   naming the live index overrides it). The effective address is still
--   computed from base+index+disp as written — only the capability source
--   changes. On a lea (which is both arithmetic and memory-shaped) the
--   annotation selects the computed value's pointer source.
--
--   An arithmetic instruction with a memory operand (e.g. `add (%rdi),%rax`)
--   gets both effects: the result draws from the named source and the
--   access is guarded by it.
--
-- The clause must stand ALONE as the whole annotation (like save/restore:
-- combine with nothing). The register is spelled as an ordinary operand
-- (`%r11` in AT&T, `r11` in Intel — the `%` is optional); `%fil_` pseudos
-- may be named too. `new capability` (and the historical typo spellings
-- `new capabiltiy` / `use capabiltiy`) are accepted as deprecated aliases
-- for compatibility and behave identically.
--
-- Returns nil when `ann` carries no capability-selector clause, else
-- { kind = "use", reg = <register text> }. Malformed capability-ish
-- text is NOT an error here (parseCapAnnot reports it); a clause
-- that fails shape validation in the caller is a clean error there.
local function parseUseNewCapAnnot(ann)
	if not ann then return nil end
	if not string.find(ann, "capab", 1, true) then return nil end
	-- The unified selector is spelled `use capability`; `new capability`
	-- (and the historical typo spellings `new capabiltiy` / `use capabiltiy`)
	-- are deprecated aliases. All spellings normalize to kind "use".
	-- (Lua patterns have no alternation, so each spelling is matched
	-- explicitly.)
	local kind, rest = nil, nil
	rest = string.match(ann, "^%s*use%s+capability%s+(.-)%s*$")
	if rest then kind = "use" end
	if not kind then
		rest = string.match(ann, "^%s*new%s+capability%s+(.-)%s*$")
		if rest then kind = "use" end
	end
	if not kind then
		rest = string.match(ann, "^%s*new%s+capabiltiy%s+(.-)%s*$")
		if rest then kind = "use" end
	end
	if not kind then
		rest = string.match(ann, "^%s*use%s+capabiltiy%s+(.-)%s*$")
		if rest then kind = "use" end
	end
	if not kind then return nil end
	-- optional surrounding parens (`use capability(%r11)`).
	local paren = string.match(rest, "^%(%s*(.-)%s*%)$")
	if paren then rest = paren end
	if rest == "" or string.find(rest, "%s") then return nil end
	return { kind = kind, reg = rest }
end

-- Resolve the register named by a `use capability` clause to a GPR use
-- occurrence of the classified instruction `c` (for temp resolution the
-- caller re-resolves against its own classify result and reads opTemp).
-- Physical names compare case-insensitively (the parsers lowercase them)
-- while `%fil_` pseudo idents keep their case (exact match second).
local function resolveCapRegUse(c, regText)
	local t = string.gsub(regText, "^%%", "")
	for _, o in (c.uses or {}) do
		if o.kind == "reg" and o.rc == "gpr" and o.num ~= nil and o.name then
			if string.lower(t) == o.name or t == o.name then
				return o
			end
		end
	end
	return nil
end

-- The register operand of a store that is not the memory operand (the stored value).
local function storeValueOperand(stmt, mem)
	for _, op in stmt.operands do
		if op.kind == "reg" and op ~= mem then return op end
	end
	return nil
end

-- Directives that EMIT data (or define symbols/macros as values). sarcasm compiles
-- functions only: such content — whether inside a function body (literal pools, jump
-- tables) or between functions (data sections) — cannot be given a capability
-- automatically, so it is rejected rather than silently dropped. Shared with the
-- driver-level top-level scan in sarcasm.luau. (The IN-BODY use is enforced on BOTH
-- architectures via validateBody below; the TOP-LEVEL scan also runs on BOTH
-- architectures with identical semantics — see sarcasm.luau's compile.)
local DATA_DIRECTIVES = {}
for _, d in {
	".word", ".xword", ".quad", ".long", ".int", ".short", ".hword",
	".2byte", ".4byte", ".8byte", ".byte", ".ascii", ".asciz", ".string",
	".zero", ".space", ".fill", ".float", ".single", ".double", ".inst",
	".value",
	".dc", ".dc.b", ".dc.l", ".dc.s", ".dc.w", ".dc.x", ".dcb", ".dcb.b",
	".dcb.l", ".dcb.s", ".dcb.w", ".dcb.x", ".ds", ".skip",
	".equ", ".set", ".macro", ".comm", ".common", ".lcomm",
} do DATA_DIRECTIVES[d] = true end
transform.DATA_DIRECTIVES = DATA_DIRECTIVES

-- Validate a function's RAW body before the frame pass runs (the frame pass rewrites
-- stack accesses, stripping annotations from the statements it virtualizes, so
-- annotation validation must see the original statements). SHARED by both
-- architectures (each backend sets cgm.strictBodyValidation):
--  * unannotated `bl`/`call` are rejected (raw calls are not marshalled — and on
--    x86_64 the historical passthrough silently miscompiled calls to same-file
--    annotated callees, landing in the callee's FIP body without marshalling, and
--    could never link against C callees, which filcc names pizlonated_*); indirect
--    calls and indirect branches are handled by the shared annotation walk below
--    (a REGISTER-indirect call with a signature annotation is supported there —
--    on both architectures — while a memory-operand or unannotated one is
--    rejected; every register-target branch classifies as an indirect branch and
--    is rejected too — on x86_64 that is `jmp *%reg` / `jmp *mem`, which the
--    renderer would otherwise pass through verbatim as an uncontrolled branch);
--  * a `b`/`jmp`/cond-branch to a non-local target is a tail call — not yet
--    supported; a branch whose target operand is not a label at all (x86
--    `jcc *%reg`, which is not even encodable, or a raw absolute `jmp $imm`) is
--    likewise rejected;
--  * every `;!` annotation must be a recognized form (a ptr
--    annotation with a valid instruction shape, or a callsite signature on a call);
--  * a `;!` annotation on a mid-body label, an empty line, or a directive is
--    rejected (only the function-entry label carries the signature — any other
--    non-instruction annotation would be silently dropped);
--  * data-emitting directives inside the body are rejected;
--  * a branch to the function's own entry name is rejected (the entry symbol is
--    renamed away, so the branch would only fail at link time; re-entering the
--    prologue would also re-run the SOV check with a perturbed rsp and grow the
--    frame unboundedly);
--  * every control-flow path must reach a `ret`: a body that can fall off its end
--    (a shared reachability worklist over statement indices) is rejected, since the
--    emitted FIP body would fall through into sarcasm's own next emission (the
--    generic-entry thunk / fail stubs) and execute through caller-garbage registers.
--    A body ending in an infinite loop has NO reachable fall-off and stays accepted.
--  * arch-specific forged-address instructions are rejected (arm64:
--    adrp/adr/ldr-literal via cgm.checkForgedAddress; x86_64: the absolute-address
--    forms are rejected in classify instead).
local function validateBody(body, cgm, funcName)
	local localLabels = {}
	-- label name -> { statement index... } for the reachability walk below. Numeric
	-- labels (1f/1b) are already rewritten to plain unique symbols by numlabel.resolve
	-- before this pass runs, so this map covers every branch-target spelling.
	local labelIndices = {}
	for i, st in body do
		if st.kind == "label" then
			localLabels[st.name] = true
			if not labelIndices[st.name] then labelIndices[st.name] = {} end
			table.insert(labelIndices[st.name], i)
		end
	end
	for _, st in body do
		if st.kind == "blank" then
			-- an annotation all alone on its line is a typo (the annotation belongs on
			-- the instruction's own line) — silently dropping it could skip a barrier.
			if st.annotation then
				error("sarcasm: annotation '" .. st.annotation
					.. "' on an otherwise-empty line (annotations go on the instruction's own line)")
			end
		elseif st.kind == "label" then
			-- The function-entry label (body[1]) carries the ;! signature. An annotation
			-- on any other label would be silently ignored (annotations are consumed from
			-- instructions only), so reject it like the blank-line case.
			if st.annotation and st ~= body[1] then
				error("sarcasm: annotation '" .. st.annotation .. "' on a label ('" .. st.name
					.. "') inside a function body is not supported (only the function-entry label"
					.. " carries the signature annotation; instruction annotations go on the instruction's"
					.. " own line)")
			end
		elseif st.kind == "directive" then
			if DATA_DIRECTIVES[st.name] then
				error("sarcasm: data in a function body is not supported (literal pools,"
					.. " jump tables and constants cannot be made memory-safe automatically): directive '"
					.. st.name .. "'")
			end
			if st.annotation then
				error("sarcasm: annotation '" .. st.annotation .. "' on a directive ('"
					.. st.name .. "') is not supported")
			end
		elseif st.kind == "insn" then
			local c = cgm.classify(st)
			local ann = st.annotation
			local raw = st.raw or st.mnemonic
			-- Pseudoregisters are 64-bit GPRs only: a pseudo appearing as a DIRECT
			-- register operand of an instruction that also names an FP/vector register
			-- (x86 xmm/k/st, arm64 neon/sve) is rejected. (A pseudo nested inside a
			-- memory operand is an ADDRESS use — an ordinary GPR value — and stays
			-- legal, as does a vector access whose address rides a pseudo base.)
			local hasPseudo, hasVec, pseudoName = false, false, nil
			for _, o in (st.operands or {}) do
				if o.kind == "reg" and o.pseudo then hasPseudo = true; pseudoName = o.name end
				if o.kind == "reg" and (o.rc == "xmm" or o.rc == "neon" or o.rc == "sve") then
					hasVec = true
				end
			end
			if hasPseudo and hasVec then
				error("sarcasm: pseudoregister `" .. (pseudoName or "?")
					.. "` may only be used as a 64-bit GPR (not with FP/vector registers): " .. raw)
			end
			if c.control == "call" then
				if not ann then
					local callee = st.operands and st.operands[1] and st.operands[1].text or "?"
					error("sarcasm: call to '" .. callee .. "' has no signature annotation"
						.. " (annotate the callsite, e.g. `" .. string.lower(st.mnemonic) .. " "
						.. callee .. " ;! int(ptr)`): " .. raw)
				end
			elseif c.control == "indcall" then
				-- An indirect call is validated by the SHARED annotation walk below
				-- (memory-operand, missing-annotation and target-must-be-a-pointer
				-- rejections), so nothing is rejected here: on both architectures a
				-- register-indirect call with a signature annotation is supported,
				-- and every other indirect call shape is rejected by the walk.
			elseif c.control == "indirect" then
				error("sarcasm: indirect branch through a raw register cannot be made memory-safe"
					.. " (computed goto / switch jump tables are not supported): " .. raw)
			elseif (c.control == "branch" or c.control == "condbranch") and c.targets
				and not (ann and string.find(ann, "%(")) then
				-- A label-target branch must stay inside the function (a branch to a
				-- non-local label is a tail call). A branch with NO label operand has
				-- no provable target at all: x86 `jcc *%reg` is not even encodable
				-- (rendering would silently emit the star-less direct form), `jmp
				-- $imm` is a raw absolute branch, and on arm64 every non-label branch
				-- target already classifies as "indirect" above — so this arm fires
				-- only for a degenerate operand-less branch there. A branch carrying a
				-- SIGNATURE annotation (a tail call) is deferred to the annotation
				-- walk below: it validates the shape and reports unresolved targets.
				if #c.targets == 0 then
					error("sarcasm: branch with a non-label target cannot be made memory-safe"
						.. " (computed goto / raw absolute branches are not supported): " .. raw)
				end
				for _, t in c.targets do
					if t == funcName then
						-- The entry label is in localLabels (it is body[1]) but the entry
						-- symbol itself is renamed away, so this branch would only die at
						-- link time. Worse, if it did link, re-entering the prologue would
						-- re-run the SOV check against a perturbed rsp and grow the frame
						-- unboundedly. Reject at compile time instead.
						error("sarcasm: branch to '" .. t .. "' re-enters this function's entry;"
							.. " re-executing the prologue cannot be proven safe: " .. raw)
					end
					if not localLabels[t] then
						error("sarcasm: branch to '" .. t .. "' leaves the function"
							.. " (a tail call); tail calls are not yet supported: " .. raw)
					end
				end
			end
			if ann then
				-- The `;! alloca ...` annotations were removed: any annotation starting
				-- with the word `alloca` is rejected here with a pointer to the
				-- `.alloca` directive (rather than falling through to a confusing
				-- "signature annotation" or "unrecognized annotation" diagnostic).
				if string.match(ann, "^%s*alloca%s") then
					error("sarcasm: the `alloca` annotations (`alloca size (x)`, `alloca result (x)`, "
						.. "`alloca result size=N`) were removed; allocate with the `.alloca size, "
						.. "alignment, result` directive instead (see README.md): " .. raw)
				end
				local useNew = parseUseNewCapAnnot(ann)
				local cap = nil
				if not useNew then cap = parseCapAnnot(ann) end
				if useNew then
					-- `use capability` (standalone only — enforced by the
					-- full-match in parseUseNewCapAnnot; `new capability`
					-- and the typo spellings are deprecated aliases):
					-- shape-check now (the lifted-web check in the
					-- annotation walk below re-resolves against the actual
					-- use temps). On address arithmetic (add/sub/lea/and/or)
					-- the named register must be a general-register use
					-- (it selects the result's capability); otherwise the
					-- instruction must have a memory operand and the named
					-- register must be its base or index (it guards the
					-- access). An arithmetic instruction with a memory
					-- operand gets both effects.
					do
						local bm = cgm.baseMnemonic(string.lower(st.mnemonic))
						local isArith = bm == "add" or bm == "sub" or bm == "lea"
							or bm == "and" or bm == "or" or bm == "orr"
						if isArith then
							local occ = resolveCapRegUse(c, useNew.reg)
							if not occ then
								error("sarcasm: `use capability " .. useNew.reg .. "` names"

									.. " a register that is not a general-register use of this"

									.. " instruction: " .. raw)
							end
							-- An arithmetic instruction with a memory operand
							-- gets BOTH effects (the ALU result draws from the
							-- named source AND the access is guarded by it),
							-- so the named register must ALSO be the memory
							-- operand's base or index. Otherwise the access
							-- would be guarded with the wrong capability
							-- (e.g. `add (%rdi),%rax #! use capability %rax`
							-- would guard the (%rdi) access with %rax's
							-- capability).
							local mem = cgm.memOperand(st)
							if mem then
								local isBase = mem.base and mem.base.rc == occ.rc
									and mem.base.num == occ.num
								local isIndex = mem.index and mem.index.rc == occ.rc
									and mem.index.num == occ.num
								if not (isBase or isIndex) then
									error("sarcasm: `use capability " .. useNew.reg .. "` must"

										.. " name the memory operand's base or index register"

										.. " (this address-arithmetic instruction has a memory"

										.. " operand, so the named register also guards the"

										.. " access): " .. raw)
								end
							end
						else
							local mem = cgm.memOperand(st)
							if not mem then
								error("sarcasm: `use capability` requires an address-arithmetic"

									.. " instruction (add/sub/lea/and/or) or an instruction"

									.. " with a memory operand (got '" .. st.mnemonic .. "'): " .. raw)
							end
							local occ = resolveCapRegUse(c, useNew.reg)
							local isBase = mem.base and occ and mem.base.rc == occ.rc
								and mem.base.num == occ.num
							local isIndex = mem.index and occ and mem.index.rc == occ.rc
								and mem.index.num == occ.num
							if not (occ and (isBase or isIndex)) then
								error("sarcasm: `use capability " .. useNew.reg .. "` must"

									.. " name the memory operand's base or index register: " .. raw)
							end
						end
					end
				elseif cap then
					-- save/restore capability (standalone only — enforced by
					-- parseCapAnnot): the instruction must define a general
					-- register. (The lifted-web check in the annotation walk
					-- below re-validates against the actual def temps.)
					local hasDef = false
					for _, d in (c.defs or {}) do
						if d.kind == "reg" and d.rc == "gpr" then hasDef = true break end
					end
					if not hasDef then
						error("sarcasm: `save/restore capability` requires an instruction"
							.. " that produces a value into a register (this instruction"
							.. " defines no general register): " .. raw)
					end
				elseif ann == "load ptr" then
					-- arm64 provides an early shape validator with detailed reasons;
					-- x86_64 defines no such hook and relies on the shared annotation
					-- walk below (its ptrAccessKind must return the plain access kind,
					-- and a memory-destination RMW is rejected there).
					if cgm.validatePtrAccess then cgm.validatePtrAccess(st, "load") end
				elseif ann == "store ptr" then
					if cgm.validatePtrAccess then cgm.validatePtrAccess(st, "store") end
				elseif sig.PTR_ANNOTATIONS[ann] then
					-- atomic/pointer-RMW family: the per-arch shape validator runs;
					-- the transform's annotation walk re-validates later.
					if cgm.ptrAtomicShape then cgm.ptrAtomicShape(ann, st) end
				elseif ann == "local" then
					-- `#! local`: the explicit marker on a local-subroutine call.
					-- Discovery (localcall.luau) already validated that the target
					-- resolves to a local subroutine (a mismatch errored there);
					-- the mark must be present here.
					if not st.localCall then
						error("sarcasm: `local` annotation on an instruction that is not"
							.. " a call to a local subroutine: " .. raw)
					end
				elseif ann == "global ptr" and cgm.globalSupport then
					-- `#! global ptr` (x86_64): on a rip-relative lea or any rip-relative
					-- memory access to an EXTERN global, materialize the flight pointer
					-- by calling the global's getter. Shape: a lea or memory operand of
					-- the form sym(%rip) (no @reloc decoration, no index — x86 has
					-- no rip+index addressing mode).
					local mem = cgm.memOperand(st)
					if not (mem and mem.base and mem.base.rc == "rip" and mem.dispSym
						and not mem.dispReloc and not mem.index) then
						error("sarcasm: `global ptr` requires a rip-relative global reference"
							.. " (a lea or memory operand of the form sym(%rip)): " .. raw)
					end
				elseif ann == "funcref" then
					-- `#! funcref` (x86_64): on a rip-relative lea to a FUNCTION
					-- symbol, materialize the function's flight pointer (its
					-- function-object capability) into the destination register,
					-- so a later `#! store ptr` stores a code pointer C can call
					-- indirectly. Shape: a lea of the form sym(%rip) to a bare
					-- symbol (no @reloc decoration, no index, no displacement).
					-- Target RESOLUTION (same-file function vs extern vs data)
					-- happens in the transform's annotation classification, which
					-- has the driver's function/data maps.
					if not cgm.globalSupport then
						error("sarcasm: `funcref` is not yet supported on this target: " .. raw)
					end
					local bm = cgm.baseMnemonic(string.lower(st.mnemonic))
					local mem = cgm.memOperand(st)
					if bm ~= "lea" or not (mem and mem.base and mem.base.rc == "rip"
						and mem.dispSym and not mem.dispReloc and not mem.index
						and (mem.dispVal or 0) == 0) then
						error("sarcasm: `funcref` requires a rip-relative lea to a bare"
							.. " function symbol (leaq sym(%rip), %reg): " .. raw)
					end
				elseif (c.control == "branch" or c.control == "condbranch")
					and string.find(ann, "%(") then
					-- Tail-call signature annotation on a direct jump (a B1
					-- cross-function tail branch — tailcall.luau). Shape: a direct
					-- jmp/jcc to a bare symbol. Sites resolved by the driver's
					-- cross-function-jump rewrite were already turned into
					-- annotated calls before this pass ran; anything still here is
					-- malformed, unsupported on this target, or unresolved.
					local op = st.operands and st.operands[1]
					if not (op and op.kind == "sym") then
						error("sarcasm: signature annotation '" .. ann .. "' on a jump requires"
							.. " a direct jump to a bare symbol (a tail call): " .. raw)
					end
					sig.parseSignature(ann)
					if not cgm.crossFunctionJumps then
						error("sarcasm: cross-function jumps are not yet supported on this"
							.. " target (annotated jump to '" .. op.text .. "'): " .. raw)
					end
					error("sarcasm: jump to '" .. op.text .. "' with signature annotation '"
						.. ann .. "' does not resolve to a function with a matching signature"
						.. " (the target must be a signature-annotated function or alias entry"
						.. " in this file, or an extern symbol): " .. raw)
				elseif c.control == "call" or c.control == "indcall" then
					-- callsite signature (direct or register-indirect call): parse now
					-- to reject malformed sigs early (the transform re-parses via
					-- callSigOf when marshalling).
					sig.parseSignature(ann)
				elseif string.find(ann, "%(") then
					error("sarcasm: signature annotation '" .. ann
						.. "' on a non-call instruction is not supported: " .. raw)
				elseif ann == "" then
					error("sarcasm: empty annotation: " .. raw)
				else
					error("sarcasm: unrecognized annotation '" .. ann
						.. "' (expected 'load ptr', 'store ptr', or a callsite"
						.. " signature like 'int(ptr)'): " .. raw)
				end
			end
			if cgm.checkForgedAddress then cgm.checkForgedAddress(st) end
		end
	end

	-- Control-flow reachability (runs AFTER the per-statement checks above, so an
	-- instruction with an unprovable or misannotated form reports its own precise
	-- error before this pass ever runs; by the time we get here every branch target
	-- is a local label and `classify` has already been proven safe on every insn).
	-- Worklist over statement indices starting at the first statement:
	--   * unconditional label branch (control == "branch") -> only its label target;
	--   * conditional branch (control == "condbranch", incl. cbz/tbz/b.cond) ->
	--     its label target + fall-through;
	--   * `ret` (control == "return") -> nothing;
	--   * every other statement — instructions (including calls and indirect forms,
	--     which return to the fall-through), labels, blanks and directives ->
	--     fall-through.
	-- If the index one past the last statement is reachable, some control-flow path
	-- reaches the end of the body without executing `ret`. The emitted FIP body would
	-- then fall through into sarcasm's own next emission (the generic-entry thunk or a
	-- fail stub) and execute through caller-garbage registers, so the body is rejected
	-- at compile time. A body ending in an infinite loop has NO reachable fall-off and
	-- stays accepted.
	local class = {}
	for _, st in body do
		if st.kind == "insn" then class[st] = cgm.classify(st) end
	end
	local reachable = {}
	local work = { 1 }
	while #work > 0 do
		local i = table.remove(work)
		if not reachable[i] then
			reachable[i] = true
			if i <= #body then
				local st = body[i]
				local fallsThrough = true
				if st.kind == "insn" then
					local c = class[st]
					if c.control == "branch" or c.control == "condbranch" then
						fallsThrough = (c.control == "condbranch")
						for _, t in (c.targets or {}) do
							for _, ti in (labelIndices[t] or {}) do
								table.insert(work, ti)
							end
						end
					elseif c.control == "return" then
						fallsThrough = false
					end
				end
				if fallsThrough then
					-- A clone entry (localcall.luau) or a cross-function-jump block
					-- entry (tailcall.luau: B1 via blocks, B2 shared-tail clones,
					-- the synthetic epilogue block) is only ever reached through its
					-- jump edge: falling INTO one means the preceding body (the
					-- caller's, or another clone's) fell off its end — the same
					-- rejection as falling off the whole body.
					local nxt = body[i + 1]
					if nxt and (nxt.localCloneEntry or nxt.crossJumpEntry) then
						error("sarcasm: function '" .. funcName .. "' can fall off the end"
							.. " of its body; every control-flow path must end with a ret"
							.. " instruction: " .. (st.raw or st.mnemonic or "?"))
					end
					table.insert(work, i + 1)
				end
			end
		end
	end
	if reachable[#body + 1] then
		local last = body[#body]
		local lastRaw = (last and (last.raw
			or (last.kind == "label" and (last.name .. ":") or nil)
			or last.text or last.mnemonic)) or "?"
		error("sarcasm: function '" .. funcName .. "' can fall off the end of its body;"
			.. " every control-flow path must end with a ret instruction: " .. lastRaw)
	end
end

function transform.transformFunction(name, sigDesc, body, backend, extra)
	local cgm = backend.cg          -- codegen module (static config + make)
	local framePolicy = backend.frame  -- per-arch frame policy (analyzeFrame + rewrite)
	local cc = cgm.cc

	-- Same-file data objects collected by the driver (x86_64 global variables):
	-- label -> { doSym, baseSym, readonly, exported, hidden, gpSym, gsSym,
	-- payloadOff }. nil when the target has no globals support (arm64).
	local dataGlobals = extra and extra.globals or nil

	-- Same-file FUNCTION name resolution (x86_64, `#! funcref`): function and
	-- alias-entry name -> owning function name (an alias shares the owner's
	-- function object). subNames: local-subroutine names (no function object —
	-- named for clean rejections). nil when the driver has no such resolution
	-- (arm64, where `#! funcref` is rejected anyway).
	local fnNames = extra and extra.fnNames or nil
	local subNames = extra and extra.subNames or nil

	-- Local subroutines (localcall.luau): this caller's per-caller clones
	-- descriptor (clone id -> { sub, entry, conts }), or nil when the function
	-- calls no local subroutine. The body already carries the marked callsites
	-- (st.localCall) and clone rets (st.localRet); classify models both as
	-- branches, so the CFG consumers below needed no changes.
	local clonesInfo = extra and extra.clones or nil

	-- Reject unannotated/indirect calls, non-local/non-label branches (including a
	-- branch to this function's own entry name), bodies that can fall off their end,
	-- unknown or misplaced annotations, in-body data, and forged addresses BEFORE ANY
	-- other pass runs (see validateBody). SHARED: BOTH backends set
	-- cgm.strictBodyValidation (the x86_64 backend used to keep its historical
	-- acceptance of raw calls, which silently miscompiled calls to same-file annotated
	-- callees). This must precede the frame analysis below: the frame pass's control
	-- flow walk only understands validated bodies (a branch targeting a label at the
	-- very end of an unvalidated body would index past the last statement), and its
	-- rewrites strip annotations, so validation must see the original statements.
	if cgm.strictBodyValidation then
		validateBody(body, cgm, name)
	end
	prof.mark("  [" .. name .. "] validateBody")

	-- Floating-point signature classes (float/double), supported on BOTH backends.
	-- FP args pass in the arch's vector registers (xmm0..xmm7 on x86_64, v0..v7 on
	-- arm64) in declaration order among the FP args, unmodeled by the GPR webs —
	-- they flow through the body verbatim; an FP return rides in xmm0 / v0. The
	-- hasFp guards below are defense in depth (a backend must set cgm.fpSignatures
	-- and provide the cg.storeFpArg / cg.loadFpRet atoms to accept FP signatures;
	-- sig.checkSupportedClasses rejects FP classes otherwise).
	local function isFp(t) return sig.isFpClass(t) end
	local retIsFp = (not sigDesc.isVoid) and isFp(sigDesc.retTypes[1])
	local function csHasFp(cs)
		if cs.retTypes[1] and isFp(cs.retTypes[1]) then return true end
		for _, t in cs.argTypes do if isFp(t) then return true end end
		return false
	end
	local function fpOrdinalOf(argTypes, k)
		local m = 0
		for i = 1, k - 1 do
			if isFp(argTypes[i]) then m += 1 end
		end
		return m
	end

	-- An FP-returning function leaves the value in xmm0/v0; the body never produces a
	-- yolo GPR result (no synthetic ret-register use at `ret`, like the void case —
	-- an FP-ret body may leave the integer return registers undefined).
	local returnRegs = (sigDesc.isVoid or retIsFp) and {} or {0}
	local funcLine = (body[1] and body[1].line) or 0
	local curLine = funcLine

	-- Frame geometry (size, whether rbp/x29 is the frame pointer) is needed before the
	-- region scan: fixed alloca bases are normalized to sp-relative frame offsets, which
	-- requires frameSize. The analysis is pure; frame.preprocess re-runs it internally.
	local fctx = nil
	if framePolicy.analyzeFrame then
		-- 4th return (x86_64 only): FP/SIMD stack-slot materialization info. The
		-- signature is passed so the frame analysis can force the vector-save
		-- reservation for FP-signature functions (see arm64_frame.analyzeFrame and
		-- x86_64_frame.analyzeFrame). 5th return (x86_64 only): the incoming yolo
		-- stack-argument info (slot count, which slots FP instructions read, and the
		-- materialized area's base offset when any do).
		local fs, fpo, ufp, fpinfo, yolo = framePolicy.analyzeFrame(body, sigDesc)
		prof.mark("  [" .. name .. "] analyzeFrame")
		fctx = { frameSize = fs, fpOffset = fpo, usesFp = ufp, fp = fpinfo, yolo = yolo,
			-- the frame base depth (x86_64: the +8 rule's clone displacement bias
			-- origin for local subroutines — localcall.luau)
			callerDepth = framePolicy.frameBaseDepth and framePolicy.frameBaseDepth() or nil,
			-- the rbp-relative normalization base (x86_64: rbp = rsp + rbpNorm;
			-- 0 for the rbp-at-the-bottom layout — the frame rewrite's exact
			-- rbp coordinate origin)
			rbpNorm = framePolicy.rbpNormalization and framePolicy.rbpNormalization() or nil }
	end

	-- FP values ride in the arch's vector registers (xmm on x86_64, v on arm64)
	-- even when the body itself never names one: an FP argument/return of the ENTRY
	-- signature (handled by the frame analysis above — arm64_frame /
	-- x86_64_frame force the reservation from the signature), an FP value passed
	-- BETWEEN two calls (an FP-returning call feeding an FP-argument call), or FP
	-- arguments placed for a call by the source program. Around every INJECTED
	-- runtime call the fpSave/fpRestore machinery must be active, or such a value
	-- would be silently clobbered — so force the vector-save reservation whenever
	-- any CALLSITE signature in play has a float/double class and the frame
	-- analysis reserved nothing itself. (Gated on cgm.fpSignatures — a backend
	-- without FP support never gets here.)
	if fctx and fctx.fp and cgm.fpSignatures and fctx.fp.saveBytes == 0 then
		local bodyUsesFp = false
		local function sigUsesFp(cs)
			if cs.retTypes[1] and isFp(cs.retTypes[1]) then return true end
			for _, t in cs.argTypes do
				if isFp(t) then return true end
			end
			return false
		end
		for _, st in body do
			if st.kind == "insn" and st.annotation and string.find(st.annotation, "%(") then
				local ok, cs = pcall(sig.parseSignature, st.annotation)
				if ok and cs and sigUsesFp(cs) then
					bodyUsesFp = true
					break
				end
			end
		end
		if bodyUsesFp then fctx.fp.saveBytes = backend.frame.VEC_SAVE_BYTES end
	end

	-- NOTE: the historical deferred dynamic-`alloca` idiom (a `;! alloca size (x)`
	-- definition plus a `;! alloca result (x)` use, with the input stack math marked
	-- and dropped) was removed along with the `;! alloca` annotations. Stack
	-- allocation is spelled with the `.alloca` directive instead (see README.md),
	-- which allocates without touching %rsp at all.

	-- Does statement `from` dominate statement `to` in `body`? True iff `to` is
	-- NOT reachable from function entry when `from` is removed (labels are
	-- transparent; a call falls through; an indirect jump leaves the
	-- function). Used by the capability save/restore dominance check after pointer flow.
	-- save/restore dominance check after pointer flow.
	local function stmtDominates(from, to)
		local bodyOrd, labelAt = {}, {}
		for i, st in body do
			bodyOrd[st] = i
			if st.kind == "label" and labelAt[st.name] == nil then labelAt[st.name] = i end
		end
		local blocked, seen, work = { [bodyOrd[from]] = true }, {}, {}
		for i = 1, #body do
			if body[i].kind == "insn" then table.insert(work, i) break end
		end
		while #work > 0 do
			local i = table.remove(work)
			if seen[i] or blocked[i] then continue end
			seen[i] = true
			if i == bodyOrd[to] then return false end
			local st = body[i]
			if st.kind ~= "insn" then
				if i < #body then table.insert(work, i + 1) end
				continue
			end
			local m = string.lower(st.mnemonic)
			local b = cgm.baseMnemonic(m)
			if b == "jmp" then
				local indirect = true
				for _, o in st.operands do
					if o.kind == "sym" then
						indirect = false
						if labelAt[o.text] then table.insert(work, labelAt[o.text]) end
					end
				end
				if indirect then continue end
			elseif b == "jcc" or string.match(m, "^loop") then
				for _, o in st.operands do
					if o.kind == "sym" and labelAt[o.text] then
						table.insert(work, labelAt[o.text])
					end
				end
				if i < #body then table.insert(work, i + 1) end
			elseif b == "ret" or b == "leave" then
				-- no successors
			else
				if i < #body then table.insert(work, i + 1) end
			end
		end
		return true
	end


	-- GC-allocation "regions": sp-relative extents whose address math redirects into
	-- a GC allocation. The only region source left is the fixed-frame escape
	-- promotion below (D9): the historical `;! alloca result size=N` annotated
	-- regions were removed with the `;! alloca` annotations (stack allocation is
	-- spelled with the `.alloca` directive, which never touches %rsp).
	local allocaRegions = {}
	local function regionOf(spOff, dyn)
		for _, r in allocaRegions do
			if r.anchored == (dyn and true or false)
				and spOff >= r.base and spOff < r.base + r.size then return r end
		end
		return nil
	end

	-- Fixed-frame escape promotion (D9, x86_64): the body takes the address of
	-- its fixed frame (bsaes's `leaq 32(%rbp), %rsi` handed to a helper — a
	-- "lea cluster" once the pointer is copied/offset and the traffic flows
	-- through the derived registers). A plain frame slot's address cannot
	-- escape, so promote the WHOLE fixed frame to a GC region [0, frameSize):
	-- its slots stop virtualizing and every fixed-frame access redirects into
	-- the region (checked accesses; the region's capability flows to helpers
	-- with the lea). The detection must run with the annotated regions present
	-- (a lea into one of THOSE is no escape), so the analysis runs again here
	-- — it is pure and simply re-marks the body. No escape -> no region ->
	-- byte-identical output.
	local frameRegion = nil
	if backend.arch == "x86_64" and framePolicy.analyzeFrame then
		local fs2, _, _, fpinfo2, yolo2, frameEscapes = framePolicy.analyzeFrame(body, sigDesc, allocaRegions)
		-- The region-carrying run sees caller-saved stack-pointer saves the
		-- region-less early run cannot record (its region corner needs the
		-- regions), so its FP/SIMD cluster and yolo-argument layout info is a
		-- superset of the early run's: it alone covers stack accesses through
		-- stack+offset alias registers. Take it for the layout (the rewrite's
		-- own run inside preprocess can only record LESS — the promoted frame
		-- region turns some carriers back into region traffic — so the layout
		-- always covers what the rewrite materializes).
		if fpinfo2 ~= nil then fctx.fp = fpinfo2 end
		if yolo2 ~= nil then fctx.yolo = yolo2 end
		-- Take the region-carrying run's frameSize too: the early run's
		-- region-sensitive outputs (depths, bands, and hence the B2-extended
		-- frameSize) are best-effort geometry with no rewrite consumer — every
		-- x86_* mark is cleared and recomputed by this run and again by the
		-- preprocess run — so the D9 promotion size must not depend on them.
		-- (The early run's remaining scalars — fpOffset/usesFp — are pure
		-- prologue geometry, region-invariant by construction.) Recording more
		-- saves can only resolve more depths (fewer "dyn", more banded
		-- accesses counted), so fs2 >= the early size; adopting it never
		-- under-covers the promotion.
		if fs2 ~= nil and fctx then fctx.frameSize = fs2 end
		if frameEscapes and fctx and fctx.frameSize > 0 then
			-- LAST in the list: an explicit (annotated) region overlapping the
			-- frame wins the membership test.
			frameRegion = { base = 0, size = fctx.frameSize, align = 16,
				anchored = false, frameRegion = true }
			table.insert(allocaRegions, frameRegion)
		end
	end


	-- (validateBody already ran at the top of this function — before the frame
	-- analysis and every rewrite — see the comment there.)

	prof.mark("  [" .. name .. "] pre-preprocess")
	body = framePass.preprocess(body, allocaRegions, framePolicy, sigDesc)
	prof.mark("  [" .. name .. "] framePass.preprocess")

	-- per-call signature info
	local callSigCache = {}
	local function callSigOf(stmt)
		local ann = stmt.annotation
		if not ann then return nil end
		if callSigCache[stmt] == nil then
			local cs = sig.parseSignature(ann)
			-- Per-arch callsite arity validation (the argument marshal has a finite
			-- number of CC registers). Raises a clean sarcasm: error when over limit.
			if cgm.checkCallsiteSig then
				-- a direct call names a symbol; an indirect call's target register
				-- has a register name instead of a symbol text.
				local op1 = stmt.operands and stmt.operands[1]
				local callee = (op1 and (op1.text or op1.name)) or "?"
				cgm.checkCallsiteSig(cs, callee)
			end
			callSigCache[stmt] = cs
		end
		return callSigCache[stmt]
	end
	-- The yolo SOURCE of callsite argument k: its SysV argument register (the first
	-- cc.yoloRegCount integer-class arguments), or — x86_64 only, for the 7th and
	-- later integer-class argument — the frame-slot pseudo-register of the body's
	-- OUTGOING stack-argument area (the compiler-style placement: the caller stores
	-- outgoing stack argument o (0-based) at 8*o(%rsp) relative to its frame base,
	-- so the callee finds it above the return address). Reaching-definitions then
	-- connects the body's store of that slot to the call's use of its value.
	-- EXCEPTION: a TAIL CALL (st.tailCall — the cross-function-jump rewrite,
	-- tailcall.luau) sources its stack arguments from the jumper's own INCOMING
	-- yolo stack-argument slots instead: a hardware tail `jmp` passes the incoming
	-- arguments through in place, so the call conversion marshals exactly those
	-- values (the outgoing area has no definition of them — the body jumped).
	local function callArgSource(cs, k, stmt)
		local yreg = cc.yoloArg(k, cs.argTypes)
		if yreg then return yreg end
		local n = 0
		for i = 1, k - 1 do if not isFp(cs.argTypes[i]) then n += 1 end end
		if stmt and stmt.tailCall then return framePass.yoloSlotNum(n - cc.yoloRegCount) end
		return framePass.SLOT_BASE + 8 * (n - cc.yoloRegCount)
	end

	local function callInfo(stmt)
		local cs = callSigOf(stmt)
		if not cs then return nil end
		-- Only the GPR-class arguments get yolo GPR uses (FP args pass through the
		-- arch's vector registers unmodeled); an FP result is returned in xmm0/v0,
		-- so there is no yolo result register to model.
		local argRegs = {}
		for k = 1, cs.argWords do
			if not isFp(cs.argTypes[k]) then
				table.insert(argRegs, callArgSource(cs, k, stmt))
			end
		end
		local resultReg = (cs.isVoid or (cs.retTypes[1] and isFp(cs.retTypes[1]))) and nil or 0
		return { argRegs = argRegs, resultReg = resultReg }
	end

	-- ORDERING: cgm.classify rejects unsafe instructions (x86_64: fp.unsafeReject
	-- inside x86.classify — loope/loopz/loopne/loopnz, pushf/popf, salc, BCD, ...).
	-- A rejection raises here, aborting compilation strictly BEFORE the DCE rounds
	-- and before any flagsLiveFrom save-scan below ever runs, so no rejected
	-- instruction can reach either flag consumer (the coarse flagUse/flagDef and
	-- flagUseMask/flagDefMask entries for rejected mnemonics are unreachable
	-- belt-and-braces — see x86_64_isa.luau's ORDERING PROOF).
	local lf = lift.liftFunction(body, { classify = cgm.classify, returnRegs = returnRegs, callInfo = callInfo })
	prof.mark("  [" .. name .. "] lift")

	local temps = lf.temps
	local nextTemp = lf.nextTemp
	local function allocTemp(desc)
		nextTemp += 1
		temps[nextTemp] = desc or {}
		return nextTemp
	end

	-- Implicit-counter instructions (x86 loop/loopq/loopl and jrcxz/jecxz) name the
	-- register they read/decrement ONLY implicitly: the emitted instruction always
	-- operates on physical rcx, so the counter's web must LIVE in physical rcx or the
	-- modeled web and the hardware register silently diverge (a stale counter, a
	-- loop that never terminates, a jrcxz that branches on garbage). Those operands
	-- carry fixedReg="rcx" from classify; precolor their (unified RMW) web here so
	-- the allocator pins it to the physical register.
	--
	-- The counter's fixed color is also EXCLUSIVE (all counter webs share one
	-- physical register — the 32-bit loopl/jecxz spellings fix the low half of the
	-- same rcx): nothing else may ever occupy rcx while a counter web exists, or a
	-- coalesced/colored web (e.g. a pointer argument's capability lower, which the
	-- fast CC delivers in rcx) is silently destroyed by the counter's own
	-- `movq $n, %rcx`. So the counter webs are collected here: (a) every fixed color
	-- is RESERVED for the whole function (removed from the allocator's color pool
	-- and protected against coalescing — see regalloc.luau's `reserved` option),
	-- which also de-precolors the entry/dense-arg webs the CC seeds from rcx: their
	-- entry unpack move (rcx -> allocated web) becomes a real copy and regalloc
	-- spills/reloads them like any web; and (b) the first counter web is kept as
	-- the representative the transform uses to save/restore physical rcx around
	-- every call (below). Functions without counters reserve nothing and are
	-- unchanged. Nested loops with two counters are hardware-faithful: each
	-- counter def is a modeled rcx web def, and both webs alias the same register
	-- exactly as the hardware register does.
	local counterWebs, counterWebSet = {}, {}
	local function noteFixedRegWeb(r, node)
		local t = r.fixedReg and node.opTemp and node.opTemp[r] or nil
		if t then
			temps[t].precolor = r.fixedReg
			if not counterWebSet[t] then
				counterWebSet[t] = true
				table.insert(counterWebs, t)
			end
		end
	end
	for _, node in lf.nodes do
		if node.kind == "insn" and node.opTemp then
			for _, r in (node.c.uses or {}) do noteFixedRegWeb(r, node) end
			for _, r in (node.c.defs or {}) do noteFixedRegWeb(r, node) end
		end
	end
	local counterRegs = {}
	for _, t in counterWebs do counterRegs[temps[t].precolor] = true end
	local reservedColors = {}
	for r in counterRegs do table.insert(reservedColors, r) end
	table.sort(reservedColors)   -- determinism: string-keyed iteration order is not
	local counterRepWeb = counterWebs[1]   -- nil when the function has no counter

	-- Precolored physical-register temps: ONE node per machine register (as IRC requires).
	local pregTemps = {}
	local function pColored(num)
		if not pregTemps[num] then
			pregTemps[num] = allocTemp({ precolor = cgm.precolorName(num), phys = num })
		end
		return pregTemps[num]
	end

	-- Seeds for pointer flow: pointer args occupy their yolo argument registers (or,
	-- for the SysV 7th+ integer-class arguments on x86_64, their yolo incoming
	-- stack-argument slots — virtual webs the frame rewrite redirects the body's
	-- entry-time 8+8i(%rsp) reads to). FP args (float/double) pass in the arch's
	-- vector registers unmodeled: no yolo GPR, no entry temp — the entry unpack
	-- below skips them entirely.
	local argPtrTemps = {}
	local argInfo = {}
	local yoloArgAt = {}   -- yolo incoming stack-arg slot index -> signature arg index
	local yoloOrdinal = 0  -- 0-based ordinal among the non-FP args (SysV placement)
	for k, ty in sigDesc.argTypes do
		if isFp(ty) then
			argInfo[k] = { isPtr = false, fp = true, fpOrdinal = fpOrdinalOf(sigDesc.argTypes, k),
				yoloReg = nil, entryTemp = nil }
		else
			local j = yoloOrdinal
			yoloOrdinal += 1
			local yreg = cc.yoloArg(k, sigDesc.argTypes)
			local yslot = nil
			if not yreg and cgm.yoloStackArgs then
				yslot = j - cc.yoloRegCount  -- 0-based incoming stack-arg slot
				yoloArgAt[yslot] = k
			end
			local et = nil
			if yslot then et = lf.entryTemp[framePass.yoloSlotNum(yslot)]
			elseif yreg then et = lf.entryTemp[yreg] end
			argInfo[k] = { isPtr = (ty == 7), yoloReg = yreg, yoloSlot = yslot, entryTemp = et }
			if ty == 7 and et then table.insert(argPtrTemps, et) end
		end
	end

	-- classify nodes: load-ptr / store-ptr / call / alloca. EVERY annotation on an
	-- instruction must be recognized and honored — an unrecognized or mismatched
	-- one was historically SILENTLY IGNORED, and a `;! load ptr` / `;! store ptr`
	-- on the wrong instruction shape was a silent miscompile (the annotated
	-- instruction was replaced by an invisicap load/store, dropping the original
	-- operation). Both are now clean compile-time errors.
	-- Body-local label set (x86_64): a rip-relative lea to a body-local label is
	-- address arithmetic on a CODE label and keeps its historical passthrough; a
	-- rip-relative reference to anything else must resolve as a global (same-file
	-- data object or an extern global with `#! global ptr`).
	local bodyLocalLabels = {}
	if dataGlobals then
		for _, st in body do
			if st.kind == "label" then bodyLocalLabels[st.name] = true end
		end
	end
	-- Global-variable nodes (x86_64): a mem operand of the form sym(%rip) whose
	-- symbol resolves to a same-file data object (globalLeaNodes for a lea,
	-- globalMemNodes for a real access), or an EXTERN symbol annotated
	-- `#! global ptr` (globalGetterNodes — getter materialization).
	local globalLeaNodes, globalMemNodes, globalGetterNodes = {}, {}, {}
	-- `#! funcref` nodes (x86_64): a rip-relative lea to a FUNCTION symbol.
	-- Value: { fo = <owner name> } for a same-file function (materialize the
	-- function object directly) or { getter = <sym> } for an extern function
	-- (materialize through the cross-module getter, like `#! global ptr`).
	local funcrefNodes = {}

	local loadPtrDefs, storePtrNodes, callNodes, allocaResultDefs = {}, {}, {}, {}
	-- `.alloca` directive nodes (mnemonic "alloca" with the parser
	-- dotAlloca marker): replaced by a GC allocation at emission.
	local dotAllocaNodes = {}
	local callPtrDefs = {}
	-- Capability save/restore (`save capability (v)` / `restore capability (v)`):
	-- capSaveVars maps var -> save node, capSaveNodes maps save node -> var,
	-- capRestoreNodes maps restore node -> var.
	local capSaveVars, capSaveNodes, capRestoreNodes = {}, {}, {}
	-- `use capability` directives (node -> register text as written; the
	-- resolved use temp rides on the node itself as node.useCapTemp /
	-- node.newCapTemp for pointer flow and the access lowering).
	-- Both temps carry the same web: useCapTemp feeds pointer flow's ALU
	-- result selection, newCapTemp feeds computeAddr's access guard, so one
	-- annotation drives whichever effect(s) apply. (`new capability` and the
	-- typo spellings are deprecated aliases normalized to `use` by
	-- parseUseNewCapAnnot, so a single map covers every spelling.)
	local useCapNodes = {}
	-- Annotated register-indirect calls (`call *%reg ;! sig(...)` on x86_64,
	-- `blr xN ;! sig(...)` on arm64).
	local indcallNodes = {}
	-- Atomic pointer operations and pointer RMWs (x86_64 cmpxchg/movq forms;
	-- arm64 cas/LSE forms — `;! load store ptr` excepted, arm64-only rejection).
	local atomicLoadPtrDefs, atomicStorePtrNodes = {}, {}
	local atomicCasPtrDefs, loadStorePtrNodes, atomicRmwPtrNodes = {}, {}, {}
	for _, node in lf.nodes do
		if node.kind == "insn" then
			local ann = node.stmt.annotation
			local raw = node.stmt.raw or node.stmt.mnemonic or "?"
			-- Global-variable classification (x86_64): an instruction with a
			-- rip-relative symbolic operand (sym(%rip)). Same-file symbols resolve
			-- to their data object (lea seeds / checked direct accesses); an EXTERN
			-- symbol requires `#! global ptr` (getter materialization); an
			-- unannotated rip-relative lea to anything but a body-local code label
			-- is a clean rejection (it would silently materialize a raw address).
			if dataGlobals then
				local mem = cgm.memOperand(node.stmt)
				if mem and mem.base and mem.base.rc == "rip" and mem.dispSym
					and not mem.dispReloc and not mem.index then
					local sym = mem.dispSym
					local desc = dataGlobals[sym]
					local bm = cgm.baseMnemonic(string.lower(node.stmt.mnemonic))
					if ann == "funcref" then
						-- `#! funcref` resolution (shape already validated by
						-- validateBody): the target must be a FUNCTION. A same-file
						-- function (or alias entry — it shares the owner's function
						-- object) materializes pizlonatedFO_<owner> directly; an
						-- extern function goes through its cross-module getter
						-- (pizlonated_<sym>, the `#! global ptr` contract). A DATA
						-- object, a body-local code label or a local subroutine is
						-- a clean error.
						if fnNames and fnNames[sym] then
							funcrefNodes[node] = { fo = fnNames[sym] }
						elseif desc then
							error("sarcasm: `funcref` target '" .. sym .. "' is a data"
								.. " object, not a function: " .. raw)
						elseif bodyLocalLabels[sym] then
							error("sarcasm: `funcref` target '" .. sym .. "' is a local"
								.. " code label, not a function: " .. raw)
						elseif subNames and subNames[sym] then
							error("sarcasm: `funcref` target '" .. sym .. "' is a local"
								.. " subroutine (it has no function object to point at): "
								.. raw)
						else
							funcrefNodes[node] = { getter = sym }
						end
					elseif desc then
						if bm == "lea" then
							globalLeaNodes[node] = desc
						else
							globalMemNodes[node] = desc
						end
					elseif ann == "global ptr" then
						if bodyLocalLabels[sym] then
							error("sarcasm: `global ptr` target '" .. sym
								.. "' is a local code label, not a global: " .. raw)
						end
						globalGetterNodes[node] = sym
					elseif bm == "lea" and not bodyLocalLabels[sym] then
						error("sarcasm: rip-relative address of '" .. sym .. "' cannot be"
							.. " materialized safely (if '" .. sym .. "' is a Fil-C global"
							.. " defined in another module, annotate the instruction with"
							.. " `#! global ptr`): " .. raw)
					end
				end
			end
			-- `.alloca` directive: seed its result as a pointer (ptrflow allocates the
			-- lower) and record it for allocation emission below. A standalone
			-- `save`/`restore capability` annotation on the directive is honored by the
			-- capClause arm below (it records off the same lifted def).
			if node.stmt.dotAlloca then
				dotAllocaNodes[node] = true
				allocaResultDefs[node] = true
			end
			local useNewClause = parseUseNewCapAnnot(ann)
			local capClause = nil
			if not useNewClause then capClause = parseCapAnnot(ann) end
			if useNewClause then
				-- `use capability` (standalone only — enforced by the
				-- full-match; shape-validated by validateBody; `new capability`
				-- and the typo spellings are deprecated aliases): resolve the
				-- named register's use temp and stash it on the node for
				-- pointer flow (ALU result selection on address arithmetic)
				-- and the access lowering (computeAddr's guard on memory
				-- operands). Both temps ride the same web so whichever
				-- effect(s) apply draw from the named source. The transform
				-- validates after pointer flow that the named web actually
				-- carries a capability.
				local occ = resolveCapRegUse(node.c, useNewClause.reg)
				if not occ then
					error("sarcasm: `use capability "
						.. useNewClause.reg .. "` names a register that is not a"

						.. " general-register use of this instruction: " .. raw)
				end
				local t = node.opTemp and node.opTemp[occ] or nil
				if not t then
					error("sarcasm: internal error: `use capability "
						.. useNewClause.reg .. "` has no lifted web temp: " .. raw)
				end
				node.useCapTemp = t
				node.newCapTemp = t
				useCapNodes[node] = useNewClause.reg
			elseif capClause then
				-- Capability save/restore (standalone only — enforced by
				-- parseCapAnnot): record for pointer flow. The instruction
				-- defines a register (checked in validateBody, re-checked
				-- here against the lifted webs).
				local d = node.defTemps and node.defTemps[1]
				if not d then
					error("sarcasm: `save/restore capability` requires an instruction"
						.. " that produces a value into a register (this instruction"
						.. " defines no register): " .. raw)
				end
				if capClause.save then
					if capSaveVars[capClause.save] then
						error("sarcasm: duplicate `save capability (" .. capClause.save .. ")`"
							.. " (a name may only be saved once per function): " .. raw)
					end
					capSaveVars[capClause.save] = node
					capSaveNodes[node] = capClause.save
				end
				if capClause.restore then capRestoreNodes[node] = capClause.restore end
			elseif ann and sig.PTR_ANNOTATIONS[ann] then
				-- cmpxchg8b/cmpxchg16b cannot be made to work on pointers: an
				-- invisicap is a (intval, lower) pair whose updates cannot be
				-- synthesized from a double-width compare-and-swap. Reject every
				-- ptr-family annotation on them (including the future atomic
				-- forms, so they can never sneak through).
				local bm = cgm.baseMnemonic(string.lower(node.stmt.mnemonic))
				if bm == "cmpxchg8b" or bm == "cmpxchg16b" then
					error("sarcasm: " .. bm .. " cannot operate on pointers: " .. raw)
				end
				if ann == "load ptr" or ann == "store ptr" then
					local kind = cgm.ptrAccessKind and cgm.ptrAccessKind(node.stmt) or "other"
					if kind == "rmw" then
						error("sarcasm: `" .. ann .. "` on a read-modify-write instruction;"
							.. " pointer RMWs need `load store ptr` (or `atomic load store ptr`): " .. raw)
					end
					local want = (ann == "load ptr") and "load" or "store"
					if kind ~= want then
						error("sarcasm: `" .. ann .. "` requires a plain 8-byte GPR " .. want
							.. " instruction: " .. raw)
					end
					if ann == "load ptr" then loadPtrDefs[node] = true else storePtrNodes[node] = true end
				else
					-- atomic/pointer-RMW annotations: the per-arch shape validator
					-- returns the sequence kind (raising the shape errors).
					if not cgm.ptrAtomicShape then
						error("sarcasm: `" .. ann .. "` is not supported on this target: " .. raw)
					end
					local shape = cgm.ptrAtomicShape(ann, node.stmt)
					if shape == "atomicload" then atomicLoadPtrDefs[node] = true
					elseif shape == "atomicstore" then atomicStorePtrNodes[node] = true
					elseif shape == "cas" then atomicCasPtrDefs[node] = true
					elseif shape == "rmw" then loadStorePtrNodes[node] = true
					elseif shape == "atomicrmw" then atomicRmwPtrNodes[node] = true
					else error("sarcasm: internal error: bad ptr-atomic shape " .. tostring(shape)) end
				end
			elseif ann == "global ptr" and cgm.globalSupport then
				-- shape-validated by validateBody; recorded above (globalGetterNodes)
				-- or same-file (globalLeaNodes/globalMemNodes) — nothing further.
			elseif ann == "funcref" and cgm.globalSupport then
				-- shape-validated by validateBody; resolved above (funcrefNodes).
			elseif node.c.control == "call" and ann then
				callNodes[node] = ann
				local cs = callSigOf(node.stmt)
				if cs and cs.retIsPtr then callPtrDefs[node] = true end
			elseif node.c.control == "indcall" then
				-- Indirect calls (a register-indirect call with a callsite signature
				-- annotation is supported on BOTH architectures; the emission is
				-- arch-neutral through the cg hooks). A MEMORY-indirect call
				-- (`call *mem`) can never be made memory-safe: the loaded value
				-- carries no capability. A register-indirect call is supported
				-- exactly like an annotated direct call, but only when it carries a
				-- callsite signature — an unannotated one would get no
				-- callee-is-a-function checks (and historically didn't even model
				-- the caller-saved clobbers or the result redefinition).
				if cgm.memOperand(node.stmt) then
					error("sarcasm: indirect call through a memory operand cannot be made"
						.. " memory-safe (load the function pointer into a register first,"
						.. " e.g. with `load ptr`, then call the register): " .. raw)
				end
				if not ann then
					error("sarcasm: indirect call has no signature annotation (annotate"
						.. " the callsite with the callee's signature, e.g. `int(ptr)`): "
						.. raw)
				end
				indcallNodes[node] = ann
				local cs = callSigOf(node.stmt)
				if cs and cs.retIsPtr then callPtrDefs[node] = true end
			elseif ann then
				if string.find(ann, "%(") then
					error("sarcasm: signature annotation `" .. ann .. "` on a non-call"
						.. " instruction: " .. raw)
				end
				error("sarcasm: unrecognized annotation: " .. ann .. " (on: " .. raw .. ")")
			end
		end
	end

	-- Dead-code elimination over the lifted webs. An instruction whose every modeled
	-- definition (its GPR webs, plus the condition flags when it writes them) is dead at
	-- that point, and which has no other effect — no memory access, no control transfer,
	-- no stack movement, no annotation — is removed. This cleans up leftover address
	-- arithmetic and, when it writes the condition flags, actively harmful dead flag
	-- writes sitting between a cmp and its branch (which would corrupt the pending
	-- condition). Liveness is computed on the CFG over
	-- webs (each web is one temp shared by all its defs and uses), with the flags as a
	-- pseudo-temp — loop-carried uses keep their defs alive, and removing one def never
	-- desyncs a web that is read anywhere reachable.
	local function dceEligible(node)
		if node.kind ~= "insn" then return false end
		local st, c = node.stmt, node.c
		-- annotated instructions are load-bearing (every annotation routes the node
		-- through a special lowering — atomic/ptr/call) and never dead
		if st.annotation then return false end
		-- a `.alloca` directive has a side effect (the allocation) and is never dead,
		-- annotated or not.
		if st.dotAlloca then return false end
		-- unknown semantics (classify could not fully model the effects) and pinned
		-- implicit-register instructions (div/mul/cmpxchg/... — the pin moves around
		-- them are part of their lowering) are never dead; control flow (jmp/jcc/
		-- ret/call/leave/loop-family — anything with c.control or branch targets)
		-- transfers or observes state no def-set can express; rep string ops
		-- (movs/stos) copy/fill memory — effectful whatever their modeled defs say
		if not c or c.unknown or c.pin or c.rep or c.control ~= "none" then return false end
		if c.targets and #c.targets > 0 then return false end
		local m = string.lower(st.mnemonic)
		local b = cgm.baseMnemonic(m)
		-- faulting, serializing, stack-moving, or otherwise effectful instructions are
		-- never dead code, whatever their modeled defs say
		if b == "div" or b == "idiv" or b == "cpuid" or b == "rdtsc" or b == "rdtscp"
			or b == "push" or b == "pop" or b == "leave" or b == "enter"
			or string.match(m, "^loop") then
			return false
		end
		local mem = nil
		for _, o in st.operands do
			if o.kind == "mem" then mem = o end
			if o.kind == "reg" then
				if o.rc == "sp" then return false end
				if fctx and fctx.usesFp and o.name == "rbp" then return false end
			end
		end
		-- only `lea` may name memory (an address computation, not an access). Every
		-- real access is excluded: a checked load/store's TRAP is observable behavior
		-- (removing it would remove a safety check), and the frame rewrite has already
		-- turned the input's own stack slots into virtual slot-web register operands —
		-- which is why a frame-slot load/store reaches here WITHOUT a memory operand
		-- and is DCE-eligible (a slot web is a register web; its liveness below is
		-- exact, and a dead slot store can no longer trap or be observed).
		if mem and b ~= "lea" then return false end
		local hasDef = false
		for _, r in c.defs do
			if r.rc ~= "gpr" then return false end
			hasDef = true
		end
		if not hasDef and not (cgm.flagDef and cgm.flagDef(st)) then return false end
		return true
	end
	do
		local FLAGSENT = "FLAGS"
		local function sameSet(a, b)
			for t in pairs(a) do if not b[t] then return false end end
			for t in pairs(b) do if not a[t] then return false end end
			return true
		end
		local function dceRound(nodes)
			-- node-level CFG (labels are nodes; a branch targets the label node, whose
			-- successors carry the flow on)
			local labelAt = {}
			for i, nd in nodes do
				if nd.kind == "label" and labelAt[nd.name] == nil then
					labelAt[nd.name] = i
				end
			end
			local succ = {}
			for i = 1, #nodes do
				local s = {}
				local nd = nodes[i]
				if nd.kind == "insn" and nd.c and nd.c.targets then
					for _, t in nd.c.targets do
						if labelAt[t] then table.insert(s, labelAt[t]) end
					end
				end
				if (nd.kind ~= "insn" or not nd.c or nd.c.fallsThrough ~= false)
					and i < #nodes then
					table.insert(s, i + 1)
				end
				succ[i] = s
			end
			-- per-node use/def sets over webs plus the flags pseudo-temp. The flags are
			-- ONE shared resource classified by the SAME cgm.flagUse/cgm.flagDef
			-- predicates the flag-save machinery uses (flagsLiveFrom), so DCE and the
			-- injected-sequence bracketing can never disagree about which instructions
			-- read or write them.
			local nodeUse, nodeDef = {}, {}
			for i = 1, #nodes do
				local u, d = {}, {}
				local nd = nodes[i]
				if nd.kind == "insn" then
					local st = nd.stmt
					for _, t in (nd.useTemps or {}) do u[t] = true end
					for _, t in (nd.defTemps or {}) do d[t] = true end
					if cgm.flagUse and cgm.flagUse(st) then u[FLAGSENT] = true end
					if cgm.flagDef and cgm.flagDef(st) then d[FLAGSENT] = true end
				end
				nodeUse[i], nodeDef[i] = u, d
			end
			-- Backward liveness fixpoint; liveOut[i] is the set live on EXIT from node
			-- i. The equations are liveOut[i] = ∪ liveIn[succ] and liveIn[j] =
			-- use[j] ∪ (liveOut[j] − def[j]): a node's own defs are killed only at its
			-- own live-IN boundary — they must stay visible in its live-OUT set, where
			-- a def this node just produced is exactly what a forward-reachable reader
			-- consumes. (Killing a node's defs in its own liveOut — as an earlier
			-- revision did — hid every def from its own exit set, so any def whose
			-- readers were not on a loop back-edge looked dead and its instruction was
			-- deleted: flag writes feeding a forward branch, and the register/slot
			-- store+load webs of the frame's virtualized slots.) The least fixpoint is
			-- order-independent; iterating nodes in fixed reverse index order keeps the
			-- iteration itself deterministic, and the sets are keyed by temp ids, so
			-- the result is byte-identical across processes.
			local liveOut = {}
			for i = 1, #nodes do liveOut[i] = {} end
			local changed = true
			while changed do
				changed = false
				for i = #nodes, 1, -1 do
					local out = {}
					for _, j in succ[i] do
						for t in pairs(nodeUse[j]) do out[t] = true end
						for t in pairs(liveOut[j]) do
							if not nodeDef[j][t] then out[t] = true end
						end
					end
					if not sameSet(out, liveOut[i]) then
						liveOut[i] = out
						changed = true
					end
				end
			end
			local kept, removed = {}, false
			for i, nd in nodes do
				local dead = false
				if nd.kind == "insn" and dceEligible(nd) then
					dead = true
					local st = nd.stmt
					-- a def is dead only when nothing forward-reachable reads it: the
					-- check is against the LIVE-OUT set, where this node's own defs are
					-- still visible (only a successor's live-in subtracts defs)
					for _, t in (nd.defTemps or {}) do
						if liveOut[i][t] then dead = false break end
					end

					-- the flags: a full flag write is a modeled def (FLAGSENT entered
					-- the fixpoint's def set), so liveness protects it the same way; an
					-- instruction whose flag writes the model deliberately does NOT
					-- track as a def (the read-and-write adc/sbb family, and partial
					-- writes like inc/dec/rol/bt — see flagWriteNoDef) must also stay
					-- while a flag consumer is forward-reachable, because deleting it
					-- would change the flags that consumer reads. Flag consumers that
					-- write nothing (jcc/setcc/cmov, arm csel/cset) are unaffected:
					-- deleting them cannot change any flags.
					if dead and liveOut[i][FLAGSENT] then
						if (cgm.flagDef and cgm.flagDef(st))
							or (cgm.flagWriteNoDef and cgm.flagWriteNoDef(st)) then
							dead = false
						end
					end
				end
				if dead then removed = true else table.insert(kept, nd) end
			end
			return kept, removed
		end
		while true do
			local kept, removed = dceRound(lf.nodes)
			lf.nodes = kept
			if not removed then break end
		end
	end
	prof.mark("  [" .. name .. "] DCE")


	-- The atomic pointer load (`;! atomic load ptr`) and the pointer compare-exchange
	-- (`;! atomic ptr` on cmpxchg — its def temp is the implicit accumulator def web)
	-- define pointer results exactly like `;! load ptr`: seed them the same way so
	-- ptrflow allocates their lower temps.
	local ptrSeedDefs = {}
	for n in pairs(loadPtrDefs) do ptrSeedDefs[n] = true end
	for n in pairs(atomicLoadPtrDefs) do ptrSeedDefs[n] = true end
	for n in pairs(atomicCasPtrDefs) do ptrSeedDefs[n] = true end
	-- Global leas (same-file data-object leas, and extern `#! global ptr` lea
	-- materializations) define pointer values: seed them so ptrflow allocates
	-- their lower temps. (Same-file DIRECT accesses and extern direct accesses
	-- are scalar — they materialize a fresh (iv, lo) pair at the access site,
	-- never a web pointer.)
	for n in pairs(globalLeaNodes) do ptrSeedDefs[n] = true end
	for n in pairs(globalGetterNodes) do
		if cgm.baseMnemonic(string.lower(n.stmt.mnemonic)) == "lea" then
			ptrSeedDefs[n] = true
		end
	end
	-- `#! funcref` leas (x86_64) define pointer values (the function's flight
	-- pointer — a full capability): seed them so ptrflow allocates their lower
	-- temps. The lower is a link-time constant into static ELF memory (the FO
	-- object), so like the global getters it needs no GC root store.
	for n in pairs(funcrefNodes) do ptrSeedDefs[n] = true end
	if cgm.atomicRmwPlan then
		-- arm64: the LSE pointer-RMW's old-value destination register (ldadd's
		-- Wt / swp's Wt) receives the slot's old value — a genuine pointer
		-- (old.iv, old.lo), exactly like an `;! atomic load ptr` result. Seed it
		-- the same way so ptrflow allocates its lower temp. (x86_64's annotated
		-- RMWs have no destination register; nothing is seeded there.)
		for n in pairs(atomicRmwPtrNodes) do ptrSeedDefs[n] = true end
	end

	-- Region-redirect leas and rsp copies (x86_64): a `leaq disp(%rsp), %rd` /
	-- `movq %rsp, %rd` landing in a fixed alloca region becomes a region-pointer
	-- web at emission (cg.regionRedirect). Seed them so ptrflow allocates their
	-- lower temps and propagates them to COPIES of the region pointer (the wp
	-- `leaq 128(%rsp), %r10; movq %r10, %rbx` — checked traffic flows through the
	-- copy exactly as through the lea). The emission defines the seeded lower from
	-- the region's own lower, so every copy shares one capability.
	if #allocaRegions > 0 and cgm.memOperand then
		for _, node in lf.nodes do
			if node.kind == "insn" then
				local st = node.stmt
				local bm = cgm.baseMnemonic(string.lower(st.mnemonic))
				local ops = st.operands or {}
				local mem = cgm.memOperand(st)
				local isRcpMov = bm == "mov" and ops[1] and ops[1].kind == "reg"
					and ops[1].rc ~= "sp" and ops[2] and ops[2].rc == "sp"
				-- Indexed leas seed exactly like non-indexed ones: membership
				-- keys on the static displacement (the two-step idiom's first
				-- lea, `lea D(%rsp),%t`, seeds the same way; the dynamic index
				-- is covered by the runtime bounds checks at every use).
				-- cg.regionRedirect emits the matching value redirect (region
				-- base + static disp, index preserved), so the seed's lower is
				-- always defined there — the two stay in sync by construction.
				if (bm == "lea" and mem and mem.base and not mem.dispSym)
					or isRcpMov then
					local off = 0
					local spBase = isRcpMov
					if isRcpMov and st.localClone then
						-- the +8 rule for a clone's movq %rsp (see the lea case below)
						local d0 = st.x86_depth
						if d0 ~= nil and type(d0) ~= "table" and d0 ~= "dyn"
							and fctx and fctx.callerDepth then
							off = fctx.callerDepth - d0 - 8
						end
					end
					if bm == "lea" then
						off = mem.dispVal or 0
						spBase = mem.base.rc == "sp"
						if not spBase then
							if fctx and fctx.usesFp and mem.base.name == "rbp" then
								-- rbp-relative: the frame rewrite's own normalization
								-- base (rbp = rsp + rbpNorm; 0 at rbp-at-the-bottom).
								off = (fctx.rbpNorm or fctx.frameSize) + off
							else
								off = nil
							end
						elseif st.localClone then
							-- the +8 rule for clone statements at a static depth
							-- (the return-address compensation — localcall.luau;
							-- keyed exactly like the frame rewrite)
							local d0 = st.x86_depth
							if d0 ~= nil and type(d0) ~= "table" and d0 ~= "dyn"
								and fctx and fctx.callerDepth then
								off = off + fctx.callerDepth - d0 - 8
							end
						end
					end
					if off then
						local d = st.x86_depth
						local hit = nil
						if spBase and not st.x86_prologue and type(d) == "table" then
							-- A lea/rsp-copy in a region scope (anchored or
							-- dynreg) computes a region pointer: seed it.
							if d.anchored and off >= d.anchored.base and off < d.anchored.base + d.anchored.size then
								hit = true
							elseif d.dynreg then
								hit = true
							end
						elseif regionOf(off, false) then
							hit = true
						end
						if hit then ptrSeedDefs[node] = true end
					end
				end
			end
		end
	end

	local pf = ptrflow.analyze(lf,
		{ argPtrTemps = argPtrTemps, loadPtrDefs = ptrSeedDefs, allocaResultDefs = allocaResultDefs,
			callPtrDefs = callPtrDefs, capSave = capSaveNodes, capRestore = capRestoreNodes },
		allocTemp, { baseMnemonic = cgm.baseMnemonic, widenSwaps = backend.arch == "x86_64" })
	prof.mark("  [" .. name .. "] ptrflow")

	-- `use capability` validation. Pointer flow already honored each
	-- directive (selecting the named source); what remains is checking the
	-- contract: the named register must actually carry a capability at
	-- convergence, otherwise the annotation is a lie (it would silently
	-- produce a null-capability check or a capability-free web). Scanned in
	-- statement order for deterministic diagnostics.
	if next(useCapNodes) ~= nil then
		for _, node in lf.nodes do
			if node.kind == "insn" then
				local uv = useCapNodes[node]
				if uv then
					local t = node.useCapTemp
					if not (t and pf.isPtr[t] and pf.lowerTemp[t]) then
						error("sarcasm: `use capability " .. uv .. "` names a register"

							.. " that does not carry a capability (only a"

							.. " pointer-carrying register can source one): "

							.. (node.stmt.raw or node.stmt.mnemonic or "?"))
					end
				end
			end
		end
	end

	-- Capability save/restore validation. Pointer flow already threaded each
	-- restore (sharing the saved lower with the restore's web, and pinning it
	-- there against the natural merge); what remains is validating the
	-- contract. Scanned in statement order for deterministic diagnostics.
	if next(capSaveNodes) ~= nil or next(capRestoreNodes) ~= nil then
		for _, node in lf.nodes do
			if node.kind == "insn" then
				local sv = capSaveNodes[node]
				if sv then
					-- The frozen snapshot: the source capability at phase-1
					-- convergence (immune to later clobbering math on the
					-- same web).
					local shot = pf.capShot and pf.capShot[sv] or nil
					local slo = shot and shot.lo or nil
					if not (shot and shot.web and slo and pf.isPtr[shot.web]) then
						error("sarcasm: `save capability (" .. sv .. ")` is on an"
							.. " instruction that does not produce a pointer value (only"
							.. " a pointer-carrying register can be saved): "
							.. (node.stmt.raw or node.stmt.mnemonic or "?"))
					end
					if pf.dynamic and pf.dynamic[shot.web] then
						error("sarcasm: `save capability (" .. sv .. ")` cannot save a"
							.. " dynamically-tracked capability (the saved register's"
							.. " capability varies per execution — it merges pointers"
							.. " from different origins): "
							.. (node.stmt.raw or node.stmt.mnemonic or "?"))
					end
				end
			end
		end
		for _, node in lf.nodes do
			if node.kind == "insn" then
				local rv = capRestoreNodes[node]
				if rv then
					local snode = capSaveVars[rv]
					if not snode then
						error("sarcasm: `restore capability (" .. rv .. ")` has no matching"
							.. " `save capability (" .. rv .. ")` in this function: "
							.. (node.stmt.raw or node.stmt.mnemonic or "?"))
					end
					if not stmtDominates(snode.stmt, node.stmt) then
						error("sarcasm: `restore capability (" .. rv .. ")` is not dominated"
							.. " by its `save capability (" .. rv .. ")` (every path from"
							.. " function entry to the restore must execute the save); restore:"
							.. " " .. (node.stmt.raw or "?") .. "; save: "
							.. (snode.stmt.raw or "?"))
					end
					local web = node.defTemps and node.defTemps[1]
					if web and pf.dynamic and pf.dynamic[web] then
						error("sarcasm: `restore capability (" .. rv .. ")` targets a"
							.. " dynamically-tracked capability (the restored register"
							.. " merges pointers from different origins, so no single"
							.. " saved capability fits it): "
							.. (node.stmt.raw or node.stmt.mnemonic or "?"))
					end
				end
			end
		end
	end


	-- The promoted fixed-frame region (D9): stable pointer + lower temps (there
	-- is no result web to borrow a lower from — the region is the whole frame,
	-- allocated up top), ptrflow-marked so region leas seed their lowers from
	-- it and copies of the lea results share one capability.
	if frameRegion and not frameRegion.ptrTemp then
		frameRegion.ptrTemp = allocTemp({ role = "regionptr" })
		frameRegion.loTemp = allocTemp({ isLower = true, role = "regionlo" })
		pf.isPtr[frameRegion.ptrTemp] = true
		pf.lowerTemp[frameRegion.ptrTemp] = frameRegion.loTemp
	end

	-- loop headers (targets of back edges) get a pollcheck. A header whose back edge
	-- comes from an implicit-counter instruction (loop/loopq/loopl/jrcxz/jecxz) must
	-- additionally PRESERVE physical rcx across the pollcheck's slow path: the
	-- counter's web is precolored to rcx (see the fixedReg precolor above), and the
	-- runtime call clobbers it. loopHeaderRcx records the counter web temp per label.
	local loopHeaderLabels = {}
	local loopHeaderRcx = {}
	for _, node in lf.nodes do
		if node.kind == "insn" and node.c.targets and #node.c.targets > 0 then
			for _, t in node.c.targets do
				local ti = lf.labelToFlow[t]
				if ti and ti <= node.fi then
					loopHeaderLabels[t] = true
					for _, r in (node.c.uses or {}) do
						if r.fixedReg and node.opTemp and node.opTemp[r] then
							loopHeaderRcx[t] = node.opTemp[r]
						end
					end
				end
			end
		end
	end

	local out = {}
	local function emit(n) table.insert(out, n) end
	local vMyth = allocTemp({ role = "myth" })
	temps[vMyth].spillCost = 1e9
	local originSym = ".Lfilc_origin_" .. name

	-- Local subroutines: one shared "retaddr" temp per (caller, clone), defined
	-- at each callsite by the continuation-address lea and read by the clone's
	-- ret dispatch. An ordinary regalloc web (allocated in clone-id order —
	-- determinism), never a pointer: it lives in a register or a sarcasm-owned
	-- spill slot, so the return-address storage is off-limits to the program by
	-- construction. Live from the callsite through the whole clone to its ret —
	-- including across any annotated calls inside the clone, which IRC's
	-- precolored caller-saved clobbers push to a callee-saved color or a spill.
	local retaddrTemps = {}
	if clonesInfo then
		for id = 1, #clonesInfo do
			retaddrTemps[id] = allocTemp({ role = "localret" })
		end
	end

	emit({ kind = "prologue", name = name })

	local cg = cgm.make({ emit = emit, allocTemp = allocTemp, pColored = pColored, pf = pf, temps = temps,
		frameCtx = fctx, globals = dataGlobals })

	-- Condition-flag (NZCV) liveness at instrumentation points. The instrumented
	-- sequences (access checks, invisicap loads/stores, pollchecks, allocations) write
	-- the condition flags (cmp/tst); gcc -O2 routinely leaves flags live across a
	-- memory instruction (`cmp; ldr; adcs`), so an inserted flag write would corrupt
	-- the program's pending condition. Scan forward from the insertion point: if the
	-- next flag-relevant instruction is a USE (csel/b.cond/adcs/...) the flags are
	-- live and the sequence must be bracketed by a save/restore; if it is a DEF
	-- (cmp/subs) the incoming flags are dead; calls clobber the flags per the ABI
	-- (no valid code reads them across a call); at a basic-block end (label, branch,
	-- return, or the end of the function) conservatively assume live-out — a flag
	-- consumer in a successor block (e.g. the b.cond terminating THIS block) counts.
	--
	-- The capability is keyed off cg.saveFlags/cg.restoreFlags + cgm.flagUse/flagDef,
	-- which BOTH backends provide: arm64 via mrs/msr nzcv into a virtual temp,
	-- x86_64 via a pushfq/popq <temp> ... pushq <temp>/popfq round-trip through a
	-- virtual temp (x86_64_isa's FLAG_USE/FLAG_DEF classify its flag reads/writes).
	-- The scan is live on both architectures, so an instrumented sequence is only
	-- bracketed when a flag consumer actually reaches it.
	local nodeIndex = {}
	for i, nd in lf.nodes do nodeIndex[nd] = i end
	local labelIndex = {}
	for i, nd in lf.nodes do
		if nd.kind == "label" and labelIndex[nd.name] == nil then labelIndex[nd.name] = i end
	end
	local flagLiveCache = {}
	local flagCapable = (cgm.flagUse and cgm.flagDef and cg.saveFlags and cg.restoreFlags) and true or false
	-- Precise backward flag-liveness fixpoint over the node CFG (labels are
	-- nodes; a branch targets the label node, whose successors carry the flow
	-- on — the same graph the DCE pass above uses). liveIn[i] is true iff a
	-- flag consumer is reachable from node i before any full flag write, so
	-- an instrumented sequence inserted at node i needs its save/restore
	-- bracket exactly then. Consequences, all automatic from the equations:
	-- flags are dead at (real) calls — the ABI clobbers them, modeled as a
	-- synthetic def — and before a `ret` (nothing reads past it); straight-
	-- line defs kill, uses revive, labels/branches propagate to successors.
	--   * a local CALL is not a real call: hardware `call` does not write the
	--     flags, so the program's flags flow INTO the clone and the clone may
	--     read them before defining them (rsaz_512_mul's mulx carry chain
	--     reads CF across `call __rsaz_512_mulx`; mont5's sqr8x likewise).
	--     Its successors are the fall-through continuation AND the clone
	--     entry, and it defines nothing itself.
	--   * a local RET clobbers the flags (the ret dispatch's compare chain),
	--     modeled as a def, so a flag consumer reached only past the clone's
	--     return does NOT make the incoming flags live. Its successors are
	--     the clone's continuation labels (no fall-through: the dispatch
	--     branches to the continuations).
	-- Partial flag writes (inc/dec, shifts, bt, stc, ...) kill precisely the
	-- flags they write (x86_64_isa's per-flag masks): an `inc` keeps CF alive
	-- for a later `adc` but not ZF for a later `jz`, and jcc/setcc/cmov read
	-- only their condition's flags. A save/restore bracket passes the flags
	-- through unchanged, so bracketing elides exactly when no live flag
	-- crosses the sequence.
	local flagLiveIn = nil
	local function buildFlagLiveness()
		if flagLiveIn then return end
		flagLiveIn = {}
		local n = #lf.nodes
		local use, def, succ = {}, {}, {}
		for i = 1, n do
			use[i], def[i], succ[i] = 0, 0, {}
		end
		-- Per-flag use/def masks (x86_64 classifies each EFLAGS bit; arm64's
		-- all-or-nothing NZCV rides the same interface coarsely). Falls back
		-- to the coarse predicates when a backend offers no masks.
		-- An UNMODELED instruction (classify's unknown flag — a register-only
		-- form that passed validation) defaults SAFE: its flag read is
		-- assumed LIVE (all bits) so a required save/restore is never
		-- omitted; its def stays 0 (propagate) so nothing is killed.
		local function useMaskOf(st, c)
			if c and c.unknown then return 4294967295 end
			if cgm.flagUseMask then return cgm.flagUseMask(st) or 0 end
			if cgm.flagUse and cgm.flagUse(st) then return 4294967295 end
			return 0
		end
		local function defMaskOf(st)
			if cgm.flagDefMask then return cgm.flagDefMask(st) or 0 end
			if cgm.flagDef and cgm.flagDef(st) then return 4294967295 end
			return 0
		end
		for i, nd in lf.nodes do
			if nd.kind == "insn" then
				local st = nd.stmt
				if st.localRet then
					-- the clone returns here; the ret dispatch clobbers the
					-- flags, so the incoming flags are dead past this point.
					def[i] = 4294967295
					if st.localRet.conts then
						for _, t in st.localRet.conts do
							local ti = t and labelIndex[t]
							if ti then table.insert(succ[i], ti) end
						end
					end
				elseif st.localCall then
					-- jump to the clone AND fall through to the continuation.
					local ent = st.localCall.entry
					local ei = ent and labelIndex[ent]
					if ei then table.insert(succ[i], ei) end
					if i < n then table.insert(succ[i], i + 1) end
					use[i] = useMaskOf(st, nd.c)
					def[i] = defMaskOf(st)
				else
					local ctrl = nd.c and nd.c.control or "none"
					use[i] = useMaskOf(st, nd.c)
					def[i] = defMaskOf(st)
					if ctrl == "call" or ctrl == "indcall" then
						-- a real call clobbers the flags per the ABI (no valid
						-- code reads them across a call).
						def[i] = 4294967295
					end
					if nd.c and nd.c.targets then
						for _, t in nd.c.targets do
							local ti = labelIndex[t]
							if ti then table.insert(succ[i], ti) end
						end
					end
					if (not nd.c or nd.c.fallsThrough ~= false) and i < n then
						table.insert(succ[i], i + 1)
					end
				end
			else
				if i < n then table.insert(succ[i], i + 1) end
			end
		end
		local liveIn = {}
		for i = 1, n do liveIn[i] = 0 end
		local changed = true
		while changed do
			changed = false
			for i = n, 1, -1 do
				-- liveIn[i] = use[i] | (liveOut[i] & ~def[i]): a flag read
				-- here is live; a flag live in any successor survives unless
				-- this node fully rewrites it. Partial writes (in neither
				-- mask) propagate incoming liveness soundly.
				local v = use[i]
				if v == 0 then
					for _, j in succ[i] do
						v = bit32.bor(v, liveIn[j])
					end
					v = bit32.band(v, bit32.bnot(def[i]))
				else
					local w = 0
					for _, j in succ[i] do
						w = bit32.bor(w, liveIn[j])
					end
					v = bit32.bor(v, bit32.band(w, bit32.bnot(def[i])))
				end
				if v ~= liveIn[i] then liveIn[i] = v changed = true end
			end
		end
		flagLiveIn = liveIn
	end
	local function flagsLiveFrom(i)
		if not flagCapable then return false end
		if flagLiveCache[i] ~= nil then return flagLiveCache[i] end
		buildFlagLiveness()
		-- Past the last node nothing can read the flags: dead.
		local res = (i <= #lf.nodes) and flagLiveIn[i] ~= 0 or false
		flagLiveCache[i] = res
		return res
	end
	-- Run f() emitting an instrumented sequence, bracketed by a flag save/restore iff
	-- the program's flags are live at the sequence's insertion point.
	local function withFlagSave(live, f)
		if not live then f(); return end
		local t = cg.saveFlags()
		f()
		cg.restoreFlags(t)
	end

	-- GC root slots
	local rootTemps, loToSlot = {}, {}
	local function rootSlotFor(loT)
		if loToSlot[loT] == nil then
			table.insert(rootTemps, loT)
			loToSlot[loT] = #rootTemps - 1
			if temps[loT] then temps[loT].spillCost = 1e9 end
		end
		return loToSlot[loT]
	end
	local function emitRootStore(loT) emit({ kind = "rootstore", temp = loT, index = rootSlotFor(loT), useTemps = { loT } }) end

	-- entry CC unpacking: myth <- myth reg; params from CC iv/lo regs into their yolo webs.
	-- FP args (float/double) are SKIPPED entirely: their value is already in its
	-- vector register (xmm0..xmm7 / v0..v7, in declaration order among the FP args)
	-- and the body reads it there — no move, no entry temp, no rooting.
	-- A dense argument word beyond the arch's CC register count (x86_64: beyond the
	-- fourth, in rdx,rcx,r8,r9) arrives on the stack in the caller's outgoing-args
	-- area ([entry_rsp + 8 + 8*w] above the return address, densely packed in
	-- declaration word order — pizlonated clang's exact placement, verified for
	-- 5..14-word signatures incl. a pointer straddling the r9/stack boundary); the
	-- cg.loadInArg loads read it there (read-only), whatever yolo register or slot
	-- the word feeds — e.g. void(ptr,ptr,size_t) delivers size_t at [rsp+8], which
	-- the yolo SysV placement wants in %rdx.
	cg.move(vMyth, pColored(cc.myth), true)
	for k, ty in sigDesc.argTypes do
		local info = argInfo[k]
		if info.entryTemp then
			local s = cc.denseSlot(sigDesc.argTypes, k)   -- 0-based dense intval word
			if s < cc.argRegCount then
				cg.move(info.entryTemp, pColored(cc.argIv(sigDesc.argTypes, k)), true)
			else
				cg.loadInArg(info.entryTemp, s - cc.argRegCount)
			end
			if info.isPtr then
				local loT = pf.lowerTemp[info.entryTemp]
				if s + 1 < cc.argRegCount then
					cg.move(loT, pColored(cc.argLo(sigDesc.argTypes, k)), true)
				else
					cg.loadInArg(loT, s + 1 - cc.argRegCount)
				end
				emitRootStore(loT)
			end
		end
	end
	-- Materialized incoming yolo stack-argument area (x86_64): FP/vector instructions
	-- cannot ride the GPR slot webs, so the frame rewrite materializes their
	-- entry-time reads of the incoming SysV stack arguments (e.g.
	-- `movd 8(%rsp), %xmm5`) into a reserved frame area (yolo.base). Fill each
	-- FP-read slot from the same incoming fast-CC word the slot web takes. The word
	-- is always stack-resident: a yolo stack argument has at least yoloRegCount
	-- integer-class predecessors, hence a dense word index >= yoloRegCount >=
	-- argRegCount (both 6/4 on x86_64).
	local yolo = fctx and fctx.yolo
	if yolo and yolo.base then
		for i = 0, yolo.count - 1 do
			if yolo.fpRead[i] then
				local k = yoloArgAt[i]
				local s = cc.denseSlot(sigDesc.argTypes, k)
				local scratch = allocTemp({ role = "yoloarg" })
				cg.loadInArg(scratch, s - cc.argRegCount)
				local slot = build.memx(build.rp(4), { dispVal = yolo.base + 8 * i })
				slot.frameSlot = true   -- shifted past the GC root area with everything else
				slot.size = 8           -- lets the renderer synthesize the movq suffix
				emit(build.insn("mov", { slot, build.rt(scratch) }, { uses = { scratch } }))
			end
		end
	end

	-- Physical-rcx preservation across EVERY call a counter function emits
	-- (annotated direct calls, annotated indirect calls, and the injected
	-- runtime calls: pollcheck slow path, filc_allocate, the aux/barrier slow
	-- paths, the atomic pointer operations). The counter's web is pinned to
	-- physical rcx, and rcx is both a fast-CC argument register (a call's 4th
	-- GPR argument word, or a pointer argument's lower at dense slot 2 — the
	-- marshalling moves WRITE rcx before the call) and caller-saved (the callee
	-- destroys it), so an unprotected counter is silently reset/clobbered at the
	-- first call in its live range (a `loop` countdown whose body calls never
	-- terminates). The precolored web itself cannot be spilled by the allocator
	-- (precolored temps are never spill candidates), so the transform brackets
	-- each call sequence with an explicit save/restore through a fresh scratch
	-- web: the scratch is live ACROSS the call, so the allocator keeps it out of
	-- every caller-saved register (callee-saved or spilled) — the same mechanism
	-- the pollcheck's back-edge save has always used, now uniform. All counter
	-- webs alias the same physical rcx, so one representative web per sequence
	-- preserves the register for all of them. Functions without counters emit
	-- nothing (counterRepWeb is nil). Defined here, BEFORE any call-emitting
	-- sequence below references it.
	local function counterGuardArm()
		if not counterRepWeb then return nil end
		local scratch = allocTemp({ role = "countersave" })
		cg.move(scratch, counterRepWeb)
		return scratch
	end
	local function counterGuardRestore(scratch)
		if scratch then cg.move(counterRepWeb, scratch) end
	end

	-- access-fail stubs (bookkeeping here; the stub body is arch-specific via cg.failStub).
	local accessOrigins, failStubs, failCounter = {}, {}, 0
	local function failLabelFor(ivTemp, loTemp, size, align, isWrite)
		failCounter += 1
		local aco = ".Lfilc_aco_" .. name .. "_" .. failCounter
		local lbl = ".Lfail_" .. name .. "_" .. failCounter
		-- The runtime's optimized-access-check origin cannot express alignments
		-- wider than a word (filc_optimized_access_check_fail asserts
		-- check_origin->alignment <= FILC_WORD_SIZE), so the recorded alignment
		-- is clamped to 8 — exactly like the compiler (FilPizlonator's
		-- buildCheck does std::min(Alignment, WordSize)). The CHECK above still
		-- tests the full alignment (e.g. cmpxchg16b's 16); since the clamped
		-- origin could not attribute a word-aligned-but-not-fully-aligned
		-- pointer (every recorded test would pass — the runtime's "Should not
		-- be reached" assert), that failure goes to a dedicated stub reporting
		-- the true alignment (see emitAccessCheck). For the merely-word-
		-- misaligned case the clamp only affects the trap-message text.
		table.insert(accessOrigins, { sym = aco, size = size, align = math.min(align, 8),
			write = isWrite and true or false, line = curLine })
		cg.failStub(failStubs, lbl, ivTemp, loTemp, aco)
		return lbl
	end

	-- access check for a precomputed effective address (capLo==nil => null cap, always traps).
	-- Check order: null capability, alignment, not-readonly/CanWrite (writes), lower
	-- bound, upper bound. This matches the compiler's emitted order exactly:
	-- FilPizlonator's canonicalizeAccessChecks stable-sorts each access's checks by
	-- CheckKind, whose declaration order is ValidObject < Alignment < CanWrite <
	-- LowerBound < UpperBound (verified against filcc -O0/-O2 IR: the CanWrite aux
	-- test precedes the lower-bound compare). Note the runtime's hand-written
	-- inline helper (filc_check_aligned_access_with_name) and sarcasm's own
	-- pointer-store sequence (emitPtrStore below) both do the bounds tests BEFORE
	-- the readonly reject instead — every order traps equivalently, since all
	-- failures branch to the same fail stub / panic, so the difference is
	-- unobservable.
	local function emitAccessCheck(effTemp, capLo, size, align, isWrite)
		if capLo == nil then
			cg.jump(failLabelFor(effTemp, nil, size, align, isWrite))
			return
		end
		local lbl = failLabelFor(effTemp, capLo, size, align, isWrite)
		cg.branchZero(capLo, lbl)                 -- null capability
		if align > 1 then
			-- An alignment wider than a word (cmpxchg16b's 16 — x86_64 only;
			-- arm64 access alignments cap at 8) cannot be attributed by the
			-- optimized fail stub: the origin records the word-clamped
			-- alignment, so an in-bounds, writable, word-aligned-but-not-fully-
			-- aligned pointer would pass every recorded test and hit the
			-- runtime's "Should not be reached" assert (a pas crash, not a
			-- clean trap). Give the alignment failure its own stub, reporting
			-- the TRUE alignment through filc_check_aligned_access_fail.
			local aLbl = lbl
			if align > 8 then
				failCounter += 1
				aLbl = ".Lalignfail_" .. name .. "_" .. failCounter
				cg.alignFailStub(failStubs, aLbl, effTemp, capLo, size, align, isWrite)
			end
			cg.testMaskBranch(effTemp, align - 1, "ne", aLbl)
		end
		if isWrite then
			-- CanWrite: every heap write rejects read-only/special objects,
			-- exactly like compiled Fil-C (and the ptr-store path below): the
			-- aux word at [lower-8] carries ObjectFlagReadonly|ObjectFlagFree
			-- at bits 49-50. The fail origin's write bit makes the runtime
			-- report "cannot write to read-only object." (resp. free object).
			local auxword = allocTemp({ role = "scratch" })
			cg.load(auxword, capLo, -8, 8)
			cg.testMaskBranch(auxword, "0x6000000000000", "ne", lbl)
		end
		cg.cmpBranch(effTemp, capLo, "ult", lbl)  -- eff < lower
		local upper = allocTemp({ role = "scratch" })
		cg.load(upper, capLo, -16, 8)             -- upper = [lower-16]
		if size > 1 then
			-- Overflow-free upper-bound form, exactly like the compiler
			-- (FilPizlonator's buildCheck comment: "Ptr <= Upper - Size" is an
			-- "overflow-free way of saying Ptr + Size <= Upper"). The naive
			-- `hi = eff + size; hi ugt upper` WRAPS for eff in
			-- [2^64 - size, 2^64) (reachable: pointer base + a huge index via
			-- lea), letting the check pass so the CPU faults on the wild
			-- address — a SIGSEGV instead of a clean filc trap. Real uppers
			-- are < 2^48, so upper - size never wraps, and any eff that large
			-- fails the compare.
			local upperMinusSize = allocTemp({ role = "scratch" })
			cg.subImm(upperMinusSize, upper, size)      -- upperMinusSize = upper - size
			cg.cmpBranch(effTemp, upperMinusSize, "ugt", lbl)  -- eff > upper - size
		else
			cg.cmpBranch(effTemp, upper, "uge", lbl)  -- eff >= upper
		end
	end

	-- AVX512 {k}-masked / AVX2 vmaskmov-vpmaskmov heap access check (x86_64 only —
	-- the codegen surfaces cgm.maskedAccessInfo for the supported forms). This is
	-- EXACTLY the check algorithm the Fil-C compiler emits for the masked
	-- intrinsics (FilPizlonator's lowerIntrinsicAccess, verified against clang
	-- output): for a fixed-position access of N lanes x E bytes (V = N*E the full
	-- footprint), flight ptr (eff, lower), upper at [lower-16]:
	--   1. ValidObject: lower == 0 fails EVEN when the mask is zero (the runtime
	--      reports "cannot access pointer with null object." via a size=0 origin).
	--   2. Stores: CanWrite (aux flags at [lower-8], the uniform
	--      READONLY|FREE mask, like every sarcasm store).
	--   3. Fast path: eff < lower -> below-slow; eff > upper - V -> above-slow
	--      (upper - V is overflow-free, like the plain check's upper - size);
	--      else execute with no mask work.
	--   4. Below-slow: mask == 0 -> execute (masked-off lanes touch nothing).
	--      Fixed-position: i = cttz(mask); fail iff eff < lower - i*E (the first
	--      enabled lane must be in bounds; on success rejoin the upper check —
	--      the tail lanes may still be out, e.g. an object smaller than V).
	--      Contiguous (expand/compress): always fail (the access starts at eff).
	--   5. Above-slow: mask == 0 -> execute. Fixed-position: h = index of the
	--      highest set mask bit; fail iff eff > upper - (h+1)*E. Contiguous:
	--      c = popcount(mask); fail iff eff > upper - c*E.
	--   6. Fail: the runtime's dedicated masked fail functions
	--      (filc_masked_read/write_check_fail, resp. filc_expand_read /
	--      filc_compress_write_check_fail) — "masked read not in bounds (even
	--      accounting for the mask)." & co.
	local maskedCounter = 0
	local function emitMaskedAccessCheck(effTemp, capLo, info, align, isWrite)
		maskedCounter += 1
		local tag = name .. "_" .. maskedCounter
		local E, N, V = info.elem, info.lanes, info.vecsize
		-- ValidObject + CanWrite share ONE size=0 origin: the runtime then
		-- reports "cannot access pointer with null object." (resp. "cannot
		-- access pointer to free object." / "cannot write to read-only
		-- object."), exactly like filcc's masked path.
		if capLo == nil then
			cg.jump(failLabelFor(effTemp, nil, 0, 1, isWrite))
			return
		end
		local lbl = failLabelFor(effTemp, capLo, 0, 1, isWrite)
		cg.branchZero(capLo, lbl)                                 -- ValidObject
		if align > 1 then
			-- Aligned forms (vmovdqa32/64, vmovaps/pd): keep the plain path's
			-- alignment check (clamped to 8 by accessSizeAlign, exactly like
			-- the unmasked forms). It needs its own size=V origin: a size=0
			-- origin could not attribute the failure (the runtime would hit
			-- its "Should not be reached" assert). Conservative when the mask
			-- is zero (hardware suppresses even alignment faults there).
			local aLbl = failLabelFor(effTemp, capLo, V, align, isWrite)
			cg.testMaskBranch(effTemp, align - 1, "ne", aLbl)
		end
		if isWrite then
			-- CanWrite, same aux-flags test as every store (see emitAccessCheck).
			local auxword = allocTemp({ role = "scratch" })
			cg.load(auxword, capLo, -8, 8)
			cg.testMaskBranch(auxword, "0x6000000000000", "ne", lbl)
		end
		local upperL = ".Lmaskupper_" .. tag
		local belowL = ".Lmaskbelow_" .. tag
		local aboveL = ".Lmaskabove_" .. tag
		local execL  = ".Lmaskexec_" .. tag
		-- The mask-refined bounds failures go to the runtime's dedicated masked
		-- fail function (a new fail-stub kind), with a generic 16-byte filc_origin
		-- (maskedOrigin record; the size/vector details travel as call arguments).
		failCounter += 1
		local failL = ".Lmaskfail_" .. name .. "_" .. failCounter
		local mko = ".Lfilc_mko_" .. name .. "_" .. failCounter
		table.insert(accessOrigins, { sym = mko, maskedOrigin = true, line = curLine })
		local maskT = allocTemp({ role = "mask" })
		-- fast path
		cg.cmpBranch(effTemp, capLo, "ult", belowL)
		cg.label(upperL)
		local upper = allocTemp({ role = "scratch" })
		cg.load(upper, capLo, -16, 8)
		local upperMinusV = allocTemp({ role = "scratch" })
		cg.subImm(upperMinusV, upper, V)               -- overflow-free (real upper < 2^48)
		cg.cmpBranch(effTemp, upperMinusV, "ugt", aboveL)
		cg.jump(execL)
		-- E is always a power of two (1/2/4/8), so lane-extent multiplies are shifts.
		local shE = ({ [1] = 0, [2] = 1, [4] = 2, [8] = 3 })[E]
		-- below-slow
		cg.label(belowL)
		cg.maskToGpr(maskT, info)
		cg.branchZero(maskT, execL)
		if info.contiguous then
			cg.jump(failL)
		else
			local i = allocTemp({ role = "scratch" })
			emit(build.insn("tzcntq", { build.rt(i), build.rt(maskT) },
				{ defs = { i }, uses = { maskT } }))
			if shE > 0 then
				emit(build.insn("shl", { build.rt(i), build.imm(shE) }, { defs = { i }, uses = { i } }))
			end
			local bound = allocTemp({ role = "scratch" })
			cg.subReg(bound, capLo, i)                 -- lower - i*E (overflow-free)
			cg.cmpBranch(effTemp, bound, "ult", failL)
			cg.jump(upperL)
		end
		-- above-slow
		cg.label(aboveL)
		cg.maskToGpr(maskT, info)
		cg.branchZero(maskT, execL)
		if info.contiguous then
			local c = allocTemp({ role = "scratch" })
			emit(build.insn("popcntq", { build.rt(c), build.rt(maskT) },
				{ defs = { c }, uses = { maskT } }))
			if shE > 0 then
				emit(build.insn("shl", { build.rt(c), build.imm(shE) }, { defs = { c }, uses = { c } }))
			end
			local bound = allocTemp({ role = "scratch" })
			cg.subReg(bound, upper, c)                 -- upper - c*E (overflow-free)
			cg.cmpBranch(effTemp, bound, "ugt", failL)
			cg.jump(execL)
		else
			local h = allocTemp({ role = "scratch" })
			emit(build.insn("bsrq", { build.rt(h), build.rt(maskT) },
				{ defs = { h }, uses = { maskT } }))
			cg.addImm(h, h, 1)                         -- h+1 lanes through the top
			if shE > 0 then
				emit(build.insn("shl", { build.rt(h), build.imm(shE) }, { defs = { h }, uses = { h } }))
			end
			local bound = allocTemp({ role = "scratch" })
			cg.subReg(bound, upper, h)                 -- upper - (h+1)*E (overflow-free)
			cg.cmpBranch(effTemp, bound, "ugt", failL)
			cg.jump(execL)
		end
		cg.label(execL)
		cg.maskedFailStub(failStubs, failL, effTemp, capLo, maskT, info, isWrite, mko)
	end

	-- invisicap pointer load: load (dstIv, dstLo) from the address of `mem`.
	-- preEff/preLo optionally supply a precomputed (effective address, capability
	-- lower) pair (global-variable materialization); otherwise computeAddr.
	local ptrldCounter = 0
	local function emitPtrLoad(node, mem, dstIv, dstLo, preEff, preLo)
		ptrldCounter += 1
		local tag = name .. "_" .. ptrldCounter
		local effTemp, baseLo = preEff, preLo
		if not effTemp then effTemp, baseLo = cg.computeAddr(node, mem) end
		emitAccessCheck(effTemp, baseLo, 8, 8)
		if baseLo == nil then cg.moveZero(dstLo); cg.moveZero(dstIv); return end
		local offInObj = allocTemp({ role = "offobj" })
		cg.subReg(offInObj, effTemp, baseLo)                       -- offInObj = eff - lower
		local aux = allocTemp({ role = "aux" })
		cg.load(aux, baseLo, -8, 8); cg.maskLow48(aux)             -- aux = [lower-8] & 0xffffffffffff
		local nullL, haveL, direct = ".Lnulllo_" .. tag, ".Lhavelo_" .. tag, ".Ldirlo_" .. tag
		cg.branchZero(aux, nullL)
		local auxE = allocTemp({ role = "auxentry" })
		cg.loadIndexed(auxE, aux, offInObj, 1, 8)                  -- auxE = [aux + offInObj]
		cg.testMaskBranch(auxE, 1, "eq", direct)                   -- bit0 clear -> direct
		local boxp = allocTemp({ role = "box" })
		cg.move(boxp, auxE); cg.andImmInPlace(boxp, -2); cg.load(dstLo, boxp, 8, 8)  -- box: lower=[(auxE&~1)+8]
		cg.jump(haveL)
		cg.label(direct); cg.move(dstLo, auxE, true); cg.jump(haveL)
		cg.label(nullL); cg.moveZero(dstLo)
		cg.label(haveL); cg.load(dstIv, effTemp, 0, 8)             -- dstIv = [eff]
	end

	-- invisicap pointer store (aux allocation + readonly reject + FUGC store barrier).
	local ptrstCounter = 0
	local function emitPtrStore(node, mem, valIv, valLo, preEff, preLo)
		ptrstCounter += 1
		local tag = name .. "_st_" .. ptrstCounter
		local effTemp, baseLo = preEff, preLo
		if not effTemp then effTemp, baseLo = cg.computeAddr(node, mem) end
		if baseLo == nil then emitAccessCheck(effTemp, nil, 8, 8, true); return end
		if valLo == nil then valLo = allocTemp({ role = "nulllo" }); cg.moveZero(valLo) end
		local lbl = failLabelFor(effTemp, baseLo, 8, 8, true)
		cg.branchZero(baseLo, lbl)
		cg.testMaskBranch(effTemp, 7, "ne", lbl)                   -- alignment
		cg.cmpBranch(effTemp, baseLo, "ult", lbl)                  -- lower bound
		local auxword = allocTemp({ role = "auxword" })
		cg.load(auxword, baseLo, -8, 8)
		cg.testMaskBranch(auxword, "0x6000000000000", "ne", lbl)   -- read-only/special reject
		local upper = allocTemp({ role = "scratch" })
		cg.load(upper, baseLo, -16, 8)
		-- Same overflow-free upper-bound form as emitAccessCheck (upper - 8,
		-- never eff + 8: eff + 8 wraps for eff in [2^64-8, 2^64)).
		local upperMinus8 = allocTemp({ role = "scratch" })
		cg.subImm(upperMinus8, upper, 8); cg.cmpBranch(effTemp, upperMinus8, "ugt", lbl)  -- upper bound
		local offInObj = allocTemp({ role = "offobj" })
		cg.subReg(offInObj, effTemp, baseLo)
		local aux = allocTemp({ role = "aux" })
		cg.move(aux, auxword); cg.maskLow48(aux)
		local needAux, barrier, doBarrier, storeLbl, doneLbl =
			".Lneedaux_" .. tag, ".Lbarrier_" .. tag, ".Ldobar_" .. tag, ".Lstore_" .. tag, ".Ldone_" .. tag
		cg.branchZero(aux, needAux)
		cg.label(barrier)
		cg.markingStateNonzeroBranch(doBarrier)
		cg.label(storeLbl)
		cg.storeIndexed(valLo, aux, offInObj, 1)
		cg.store(valIv, effTemp, 0)
		cg.jump(doneLbl)
		-- need-aux: filc_object_ensure_aux_ptr_outline(myth, lower-16) -> aux
		-- (injected returning runtime call, out of the hot line: preserve the function's
		-- live FP/SIMD state around it — no-op for GPR-only functions)
		cg.label(needAux)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		local header = allocTemp({ role = "scratch" })
		cg.subImm(header, baseLo, 16)
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), header)
		cg.call("filc_object_ensure_aux_ptr_outline", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.fpRestore()
		counterGuardRestore(rcxSave)
		cg.move(aux, pColored(cc.rtRet))
		cg.jump(barrier)
		-- do-barrier: filc_store_barrier_for_lower_slow(myth, lower) — but only for a
		-- non-null lower: the runtime's filc_store_barrier_for_lower guards
		-- `marking && lower`, and its _slow entrypoint asserts the lower is non-null
		-- (storing a NULL/integer-derived pointer has no object to barrier).
		cg.label(doBarrier)
		local rcxSave = counterGuardArm()
		cg.branchZero(valLo, storeLbl)
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), valLo)
		cg.call("filc_store_barrier_for_lower_slow", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.fpRestore()
		counterGuardRestore(rcxSave)
		cg.jump(storeLbl)
		cg.label(doneLbl)
	end

	-- Pre/post-index writeback for annotated pointer accesses. The invisicap sequence
	-- replaces the load/store wholesale, so the base register's `base += index` side
	-- effect must be re-applied explicitly (e.g. `ldr x0, [x0], #8 ;! load ptr` walking
	-- an array). Only the intval temp advances; the lower (capability) is unchanged —
	-- it is the same object. The effective address used for the access is already
	-- correct (old base for post-index, base+index for pre-index via computeAddr), so
	-- a single add/sub after the sequence covers both forms. (x86_64 has no writeback
	-- addressing modes, so this is a no-op there.)
	local function emitWriteback(node, mem)
		if not mem or not mem.writeback then return end
		local baseT = node.opTemp[mem.base]
		if not baseT then
			error("sarcasm: writeback base of an annotated pointer access has no usable value: "
				.. (node.stmt and node.stmt.raw or "?"))
		end
		local amt = (mem.index and mem.index.kind == "imm")
			and (mem.index.value or tonumber(mem.index.text)) or nil
		if not amt then
			error("sarcasm: annotated pointer access with a non-immediate writeback index"
				.. " is not supported: " .. (node.stmt and node.stmt.raw or "?"))
		end
		if amt > 0 then cg.addImm(baseT, baseT, amt)
		elseif amt < 0 then cg.subImm(baseT, baseT, -amt) end
	end

	-- pollcheck: myth flags at [myth+8], bits 0xe.
	local pollCounter = 0
	local function emitPollcheck(rcxWeb)
		pollCounter += 1
		-- EVERY pollcheck in a function with an implicit counter must preserve
		-- physical rcx: the slow path is a runtime call, and the counter's web
		-- lives in rcx. The counter's OWN back edge passes its web explicitly
		-- (loopHeaderRcx); any other header (a GC-churn loop whose back edge is a
		-- plain jmp, a CAS retry loop) falls back to the representative counter
		-- web — all counter webs alias the same physical rcx, so saving through
		-- any of them preserves the register.
		rcxWeb = rcxWeb or counterRepWeb
		local okLbl = ".Lpollok_" .. name .. "_" .. pollCounter
		local flags = allocTemp({ role = "scratch" })
		cg.load(flags, vMyth, 8, 1)
		cg.testMaskBranch(flags, 0xe, "eq", okLbl, 32)
		-- Slow path, out of the hot line (the not-taken branch skips it): an injected
		-- RETURNING runtime call the source program never saw, so its live FP/SIMD
		-- state must be preserved around it (fpSave/fpRestore are no-ops when the
		-- function uses no vector registers).
		-- An implicit-counter back edge (loop/loopq/loopl/jrcxz/jecxz) also requires
		-- preserving physical rcx: the counter's web is precolored to rcx and the
		-- call clobbers it. The value rides the slow path through a fresh scratch
		-- web (which, being live across the call, the allocator keeps out of the
		-- clobbered registers); the fast path never touches rcx.
		local rcxScratch
		if rcxWeb then
			rcxScratch = allocTemp({ role = "scratch" })
			cg.move(rcxScratch, rcxWeb)
		end
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.leaGlobal(cc.rtArg(2), originSym)
		cg.call("filc_pollcheck_slow", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.fpRestore()
		if rcxWeb then cg.move(rcxWeb, rcxScratch) end
		cg.label(okLbl)
	end

	-- Atomic pointer operations and pointer RMWs, via the Fil-C runtime — exactly
	-- the calls filcc emits for C11 atomic pointer operations (verified against
	-- build/bin/clang output; see ABI-NOTES-x86.md). The callees are nounwind
	-- runtime entrypoints: a filc_ptr argument is two consecutive argument words
	-- (intval first, then lower), a filc_ptr result comes back as two words
	-- (x86_64 SysV: rax=intval, rdx=lower; arm64 AAPCS64: x0=intval, x1=lower) —
	-- so there is NO exception-flag check after these calls (unlike
	-- pizlonated-function calls). Returned lowers are rooted
	-- immediately (a safepoint may run before the next use); pointer ARGUMENTS need
	-- no caller rooting (the runtime protects them itself). Each sequence emits the
	-- usual inline access check first (null-cap -> align -> CanWrite -> lower ->
	-- upper) so a trap gets sarcasm's origin; the runtime re-checks internally.
	--
	-- EFLAGS (x86_64): a flag consumer (jcc/setcc) immediately after the annotated
	-- instruction must see the flags the NATIVE instruction would produce, but the
	-- injected sequences clobber EFLAGS. The RMW/CAS paths therefore keep the
	-- pre-op value (and the source operand) live in webs and re-execute the
	-- operation on a scratch copy as the LAST flag-affecting step (the result
	-- moves and root stores in between don't touch flags) — their contract is to
	-- produce the NATIVE flags of the operation, so they are deliberately NOT
	-- wrapped in the flag save/restore (`not` and adc/sbb carry-ins remain the
	-- documented-indeterminate exceptions).
	-- The CLOBBERING of injected bounds checks everywhere else is fixed by the
	-- flag-liveness scan on BOTH architectures now: every instrumented sequence
	-- (access checks, invisicap loads/stores, pollchecks, allocations, masked
	-- checks) is bracketed with a save/restore of the program's flags exactly when
	-- a consumer can reach them (arm64: mrs/msr nzcv; x86_64: pushfq/popq +
	-- pushq/popfq through a virtual temp), and flags are dead most of the time.
	-- NZCV (arm64): only `;! atomic ptr` (cas) has a flag contract — NZCV is
	-- recomputed as (expected − old) as the last flag-affecting step, mirroring
	-- the x86 cmpxchg form so a following b.eq/cset behaves natively. The native
	-- arm64 forms (ldar/stlr/ldapr, the LSE RMWs) write NO flags, so their
	-- sequences instead PRESERVE the program's live NZCV around themselves
	-- (withFlagSave at the dispatch).
	local function marshalResultPtr(dstIv, dstLo)
		cg.move(dstIv, pColored(cc.rtRet))
		cg.move(dstLo, pColored(cc.rtRetLo))
		emitRootStore(dstLo)
	end

	-- filc_load_ptr_atomic_with_manual_tracking_outline(slot) -> old (rtRet=iv,
	-- rtRetLo=lo). No myth arg, does not allocate, can only panic.
	local function emitAtomicLoadCall(effTemp, capLoTemp)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), effTemp)
		cg.move(pColored(cc.rtArg(2)), capLoTemp)
		cg.call("filc_load_ptr_atomic_with_manual_tracking_outline",
			{ pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.fpRestore()
		counterGuardRestore(rcxSave)
	end

	-- filc_store_ptr_atomic_outline(myth, slot, value). Can allocate (creates the
	-- 16-byte atomic box / ensures aux) => GC safepoint; barriers value internally.
	local function emitAtomicStoreCall(effTemp, capLoTemp, valIv, valLo)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), effTemp)
		cg.move(pColored(cc.rtArg(3)), capLoTemp)
		cg.move(pColored(cc.rtArg(4)), valIv)
		cg.move(pColored(cc.rtArg(5)), valLo)
		cg.call("filc_store_ptr_atomic_outline",
			{ pColored(cc.rtArg(1)), pColored(cc.rtArg(2)), pColored(cc.rtArg(3)),
				pColored(cc.rtArg(4)), pColored(cc.rtArg(5)) }, cg.callerSavedClobbers())
		cg.fpRestore()
		counterGuardRestore(rcxSave)
	end

	-- filc_strong_cas_ptr_with_manual_tracking(myth, slot, 0, expected, new) -> old.
	-- Can allocate => GC safepoint. Success <=> returned old.iv == expected.iv (a
	-- raw-intval comparison; the runtime compares intvals only). The expected's
	-- lower may be null ("integer guess") — the comparison ignores it.
	-- new_value marshalling is arch-specific: on x86_64 (SysV, six argument
	-- registers) it travels on the STACK ([rsp]=iv, [rsp+8]=lower at the call;
	-- see codegen's pushStackPtr); on arm64 (AAPCS64) all eight argument words
	-- fit in x0..x7, so it marshals in registers (cg.casNewInRegs).
	local function emitAtomicCasCall(effTemp, capLoTemp, expIv, expLo, newIv, newLo)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), effTemp)
		cg.move(pColored(cc.rtArg(3)), capLoTemp)
		cg.moveZero(pColored(cc.rtArg(4)))          -- offset = 0
		cg.move(pColored(cc.rtArg(5)), expIv)
		cg.move(pColored(cc.rtArg(6)), expLo)
		local casUses = { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)), pColored(cc.rtArg(3)),
			pColored(cc.rtArg(4)), pColored(cc.rtArg(5)), pColored(cc.rtArg(6)) }
		if cg.casNewInRegs then
			cg.move(pColored(cc.rtArg(7)), newIv)
			cg.move(pColored(cc.rtArg(8)), newLo)
			table.insert(casUses, pColored(cc.rtArg(7)))
			table.insert(casUses, pColored(cc.rtArg(8)))
		else
			cg.pushStackPtr(newIv, newLo)
		end
		cg.call("filc_strong_cas_ptr_with_manual_tracking", casUses, cg.callerSavedClobbers())
		if not cg.casNewInRegs then cg.popStackPtr() end
		cg.fpRestore()
		counterGuardRestore(rcxSave)
	end

	-- filc_xchg_ptr_with_manual_tracking(myth, slot, 0, new) -> old: the exact
	-- primitive filcc emits for C11 atomic_exchange (FilPizlonator's AtomicRMW
	-- Xchg case — a direct call, no pollcheck; the runtime retries a weak CAS
	-- internally and barriers the new value itself). Can allocate (the retry
	-- path creates the atomic box) => GC safepoint. Six argument words (myth,
	-- slot.iv, slot.lower, offset, new.iv, new.lower) — they fit in argument
	-- registers on BOTH ABIs (x86_64 SysV has six; the CAS call above needs
	-- the stack only for words seven and eight). Currently only reached from
	-- the arm64 LSE swp lowering (x86_64 has no swp).
	local function emitAtomicXchgCall(effTemp, capLoTemp, newIv, newLo)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), effTemp)
		cg.move(pColored(cc.rtArg(3)), capLoTemp)
		cg.moveZero(pColored(cc.rtArg(4)))          -- offset = 0
		cg.move(pColored(cc.rtArg(5)), newIv)
		cg.move(pColored(cc.rtArg(6)), newLo)
		cg.call("filc_xchg_ptr_with_manual_tracking",
			{ pColored(cc.rtArg(1)), pColored(cc.rtArg(2)), pColored(cc.rtArg(3)),
				pColored(cc.rtArg(4)), pColored(cc.rtArg(5)), pColored(cc.rtArg(6)) },
			cg.callerSavedClobbers())
		cg.fpRestore()
		counterGuardRestore(rcxSave)
	end

	-- `;! atomic load ptr`: movq (mem), %reg — the runtime atomic pointer load.
	local function emitAtomicPtrLoad(node, mem, preEff, preLo)
		local dstIv = node.defTemps[1]
		if not dstIv then
			error("sarcasm: `atomic load ptr` requires a modeled (GPR) destination"
				.. " register: " .. (node.stmt.raw or "?"))
		end
		local dstLo = pf.lowerTemp[dstIv]
		local effTemp, capLo = preEff, preLo
		if not effTemp then effTemp, capLo = cg.computeAddr(node, mem) end
		emitAccessCheck(effTemp, capLo, 8, 8, false)
		if capLo == nil then cg.moveZero(dstLo); cg.moveZero(dstIv); emitRootStore(dstLo); return end
		emitAtomicLoadCall(effTemp, capLo)
		marshalResultPtr(dstIv, dstLo)
	end

	-- `;! atomic store ptr`: movq %reg, (mem) — the runtime atomic pointer store.
	local function emitAtomicPtrStore(node, mem, preEff, preLo)
		local valOp = storeValueOperand(node.stmt, mem)
		local valIv = valOp and node.opTemp[valOp] or nil
		if not valIv then
			error("sarcasm: `atomic store ptr` requires a modeled (GPR) value"
				.. " register: " .. (node.stmt.raw or "?"))
		end
		local valLo = pf.lowerTemp[valIv]
		local effTemp, capLo = preEff, preLo
		if not effTemp then effTemp, capLo = cg.computeAddr(node, mem) end
		emitAccessCheck(effTemp, capLo, 8, 8, true)
		if capLo == nil then return end
		if valLo == nil then valLo = allocTemp({ role = "nulllo" }); cg.moveZero(valLo) end
		emitAtomicStoreCall(effTemp, capLo, valIv, valLo)
	end

	-- `;! atomic ptr` on cmpxchgq (lock or not — both go through the runtime call):
	-- pointer compare-exchange. BYPASSES emitPinned: the expected value (the
	-- implicit accumulator's use web) travels in argument registers instead of
	-- physical rax; the old value arrives in rax/rdx and defines the accumulator's
	-- def web (seeded as a pointer by ptrflow).
	local function emitAtomicPtrCas(node, mem, preEff, preLo)
		local c = node.c
		local pin = c.pin
		local accUseT = (pin and pin.uses and pin.uses[1]) and node.opTemp[pin.uses[1]] or nil
		local accDefT = (pin and pin.defs and pin.defs[1]) and node.opTemp[pin.defs[1]] or nil
		if not (accUseT and accDefT) then
			error("sarcasm: internal error: cmpxchg implicit accumulator has no temp: "
				.. (node.stmt.raw or "?"))
		end
		local accDefLo = pf.lowerTemp[accDefT]
		local accUseLo = pf.lowerTemp[accUseT]
		local srcOp = node.stmt.operands[2]
		local newIv = srcOp and node.opTemp[srcOp] or nil
		if not newIv then
			error("sarcasm: `atomic ptr` requires a modeled (GPR) new-value register: "
				.. (node.stmt.raw or "?"))
		end
		local newLo = pf.lowerTemp[newIv]
		local effTemp, capLo = preEff, preLo
		if not effTemp then effTemp, capLo = cg.computeAddr(node, mem) end
		emitAccessCheck(effTemp, capLo, 8, 8, true)
		-- The expected intval must survive the call for the flag recompute below:
		-- the expected and result webs can COINCIDE (a cmpxchg retry loop unions
		-- the accumulator's use and def webs), so compare against a private copy.
		local expCopy = allocTemp({ role = "casexp" })
		cg.move(expCopy, accUseT)
		if capLo == nil then
			cg.moveZero(accDefT); cg.moveZero(accDefLo); emitRootStore(accDefLo); return
		end
		if not accUseLo then accUseLo = allocTemp({ role = "nullexp" }); cg.moveZero(accUseLo) end
		if not newLo then newLo = allocTemp({ role = "nullnew" }); cg.moveZero(newLo) end
		emitAtomicCasCall(effTemp, capLo, accUseT, accUseLo, newIv, newLo)
		marshalResultPtr(accDefT, accDefLo)
		-- Native cmpxchg sets flags from (accumulator - dest); dest's old value is
		-- the returned old value, so (expected - old) reproduces the native flags
		-- exactly (ZF=1 iff the exchange happened). MUST be the last flag-affecting
		-- step: a following je/sete behaves natively.
		emit(build.insn("cmp", { build.rt(expCopy), build.rt(accDefT) },
			{ uses = { expCopy, accDefT } }))
	end

	-- Pointer-RMW support (`;! load store ptr` / `;! atomic load store ptr`).
	local RMW_UNARY = { inc = true, dec = true, neg = true, ["not"] = true }

	-- The source operand of a supported pointer-RMW node: a register web temp or an
	-- immediate operand (neither for the unary inc/dec/neg/not forms).
	local function rmwSource(node)
		local bm = cgm.baseMnemonic(string.lower(node.stmt.mnemonic))
		if RMW_UNARY[bm] then return nil, nil, true end
		local so = node.stmt.operands[2]
		if so and so.kind == "reg" then
			local t = node.opTemp[so]
			if not t then
				error("sarcasm: `" .. node.stmt.annotation .. "` source register has no"
					.. " temp: " .. (node.stmt.raw or "?"))
			end
			return t, nil, false
		end
		return nil, so, false   -- immediate operand (the 8-byte form was validated)
	end

	-- Re-emit the annotated ALU operation as a register operation: dstT = dstT OP src.
	local function emitRmwAluOp(node, dstT, srcT, srcImmOp, unary)
		local operands = { build.rt(dstT) }
		local uses = { dstT }
		if not unary then
			if srcT then table.insert(operands, build.rt(srcT)); table.insert(uses, srcT)
			else table.insert(operands, srcImmOp) end
		end
		emit(build.insn(node.stmt.mnemonic, operands, { defs = { dstT }, uses = uses }))
	end

	-- Recompute the native flags of the RMW (old OP src) as the last flag-affecting
	-- step (see the EFLAGS comment above). `not` sets no flags (native behavior is
	-- "flags unchanged", unrecoverable across the injected checks — the documented
	-- EFLAGS caveat); adc/sbb read a carry-in the checks already clobbered (also
	-- documented-indeterminate, like the pinned adcx forms) — and note the clobbered
	-- carry-in already affected the STORED result too: the op itself ran on it right
	-- after the load/check sequence (deterministically CF=0 for the non-atomic
	-- `;! load store ptr` path, whose last flag-affecting instruction is a `test`;
	-- whatever the runtime call left for the atomic forms).
	local function emitRmwFlags(node, oldIv, srcT, srcImmOp, unary)
		local scratch = allocTemp({ role = "rmwflags" })
		cg.move(scratch, oldIv)
		emitRmwAluOp(node, scratch, srcT, srcImmOp, unary)
	end

	-- `;! load store ptr`: NON-atomic RMW on a pointer slot = the box-aware
	-- non-atomic pointer load, the ALU op on the intval, then the non-atomic
	-- pointer store (checks incl. CanWrite, ensure-aux, barrier, lower+iv stores).
	-- The capability rides through unchanged — Fil-C pointer arithmetic through
	-- memory.
	local function emitLoadStorePtr(node, mem, preEff, preLo)
		local srcT, srcImmOp, unary = rmwSource(node)
		local oldIv = allocTemp({ role = "lspold" })
		local oldLo = allocTemp({ role = "lsplo" })
		emitPtrLoad(node, mem, oldIv, oldLo, preEff, preLo)
		emitRootStore(oldLo)
		local newIv = allocTemp({ role = "lspnew" })
		cg.move(newIv, oldIv)
		emitRmwAluOp(node, newIv, srcT, srcImmOp, unary)
		emitPtrStore(node, mem, newIv, oldLo, preEff, preLo)
		emitRmwFlags(node, oldIv, srcT, srcImmOp, unary)
	end

	-- `;! atomic load store ptr` on ARM64: an LSE atomic RMW (ldadd/ldset/ldeor/
	-- ldclr/swp, or the st<op> no-result aliases) on a pointer slot.
	--
	-- ldadd/ldset/ldeor/ldclr (and the st<op> aliases): the native instruction
	-- is already a single atomic RMW, so the faithful lowering is the runtime
	-- compare-exchange loop — exactly the x86 `lock` form's shape (inline
	-- checks once before the loop; per iteration a pollcheck, an atomic load,
	-- the op re-executed on the intval as a register operation, and
	-- filc_strong_cas_ptr_with_manual_tracking with expected = current old and
	-- new = old.iv OP src carrying the SAME lower — the capability rides
	-- through: Fil-C pointer arithmetic through memory).
	--
	-- swp is the exception: its "op" is a plain move of the SOURCE, so the new
	-- value is not derived from the old one and must carry the SOURCE's
	-- capability — a source pointer into a DIFFERENT object stored with the
	-- old value's lower would trap on first dereference. swp therefore lowers
	-- to a single filc_xchg_ptr_with_manual_tracking call (the exact primitive
	-- filcc emits for C11 atomic_exchange) with new = (src.iv, src's OWN
	-- lower); a non-pointer source has no lower temp and gets a null
	-- capability (mirroring emitAtomicPtrStore — storing an integer into a
	-- pointer slot is legal; it just can't be dereferenced afterwards).
	--
	-- Either way the old-value destination register (ldadd's Wt / swp's Wt)
	-- receives the old value AS A POINTER (old.iv, old.lo, rooted — exactly
	-- like an `;! atomic load ptr` result); the st<op> forms have no
	-- destination. NZCV is preserved across the whole sequence (the native
	-- instruction writes no flags — the dispatch below brackets the emission
	-- withFlagSave when the program's flags are live).
	local lseRmwCounter = 0
	local function emitAtomicLoadStorePtrLse(node, mem)
		local m = string.lower(node.stmt.mnemonic)
		local plan = cgm.atomicRmwPlan(m)
		local ops = node.stmt.operands
		-- operand roles (swp and the ld<op> forms agree: ops[1] = source,
		-- ops[2] = old-value destination); the st<op> aliases have no
		-- old-value destination.
		local srcOp = ops[1]
		local dstOp = (plan.kind == "st") and nil or ops[2]
		local srcT = srcOp and node.opTemp[srcOp] or nil
		if not srcT then
			error("sarcasm: `atomic load store ptr` source register has no temp: "
				.. (node.stmt.raw or "?"))
		end
		local dstT = dstOp and node.opTemp[dstOp] or nil
		local effTemp, capLo = cg.computeAddr(node, mem)
		emitAccessCheck(effTemp, capLo, 8, 8, true)
		-- a null capability trapped above; keep the (unreachable) fall-through
		-- well-formed with a null-lower marshal temp
		local capLoArg = capLo
		if not capLoArg then capLoArg = allocTemp({ role = "nullcap" }); cg.moveZero(capLoArg) end
		local oldIv = allocTemp({ role = "alspold" })
		local oldLo = allocTemp({ role = "alsplo" })
		if plan.alu == "mov" then
			-- swp: the new value IS the source operand, so it carries the
			-- SOURCE's capability (a null capability for a non-pointer
			-- source) — never the old value's lower. One runtime exchange
			-- call, no CAS loop (filcc's C11 atomic_exchange shape).
			local newLo = pf.lowerTemp[srcT]
			if not newLo then newLo = allocTemp({ role = "nullnew" }); cg.moveZero(newLo) end
			emitAtomicXchgCall(effTemp, capLoArg, srcT, newLo)
			marshalResultPtr(oldIv, oldLo)
		else
			local newIv = allocTemp({ role = "alspnew" })
			lseRmwCounter += 1
			local loopTop = ".Llcasloop_" .. name .. "_" .. lseRmwCounter
			cg.label(loopTop)
			-- a safepoint on every iteration (a contended CAS loop must not starve
			-- the GC's stop-the-world; filcc's atomic-RMW loops pollcheck too)
			emitPollcheck()
			emitAtomicLoadCall(effTemp, capLoArg)
			marshalResultPtr(oldIv, oldLo)
			-- new = old OP src (the annotated op re-executed as a register
			-- operation on the intval), carrying the SAME lower as old.
			emit(build.insn(plan.alu, { build.rt(newIv), build.rt(oldIv), build.rt(srcT) },
				{ defs = { newIv }, uses = { oldIv, srcT } }))
			emitAtomicCasCall(effTemp, capLoArg, oldIv, oldLo, newIv, oldLo)
			local casIv = allocTemp({ role = "alspcas" })
			cg.move(casIv, pColored(cc.rtRet))
			cg.cmpBranch(oldIv, casIv, "ne", loopTop)   -- mismatch: retry
		end
		-- the old-value destination receives the old value AS A POINTER (old.iv,
		-- old.lo — seeded in ptrflow, like an `;! atomic load ptr` result); the
		-- lower rides the final iteration's marshal temps.
		if dstT then
			cg.move(dstT, oldIv)
			local dstLo = pf.lowerTemp[dstT]
			if dstLo then
				cg.move(dstLo, oldLo)
				emitRootStore(dstLo)
			end
		end
	end

	-- `;! atomic load store ptr`: WITHOUT `lock`, an atomic load, the op, and an
	-- atomic store (each access atomic; the RMW as a whole is not). WITH `lock`, a
	-- compare-exchange loop through the runtime — a real atomic RMW. (x86_64; the
	-- arm64 LSE form lowers to emitAtomicLoadStorePtrLse above.)
	local alrmwCounter = 0
	local function emitAtomicLoadStorePtr(node, mem, preEff, preLo)
		if cgm.atomicRmwPlan then
			-- arm64: the LSE RMW is already atomic, so only the compare-exchange
			-- loop lowering exists; NZCV is preserved across it (the native
			-- instruction writes no flags).
			withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
				emitAtomicLoadStorePtrLse(node, mem)
			end)
			return
		end
		local srcT, srcImmOp, unary = rmwSource(node)
		local effTemp, capLo = preEff, preLo
		if not effTemp then effTemp, capLo = cg.computeAddr(node, mem) end
		emitAccessCheck(effTemp, capLo, 8, 8, true)
		-- a null capability trapped above; keep the (unreachable) fall-through
		-- well-formed with a null-lower marshal temp
		local capLoArg = capLo
		if not capLoArg then capLoArg = allocTemp({ role = "nullcap" }); cg.moveZero(capLoArg) end
		local oldIv = allocTemp({ role = "alspold" })
		local oldLo = allocTemp({ role = "alsplo" })
		local newIv = allocTemp({ role = "alspnew" })
		if not node.stmt.lockPrefix then
			emitAtomicLoadCall(effTemp, capLoArg)
			marshalResultPtr(oldIv, oldLo)
			cg.move(newIv, oldIv)
			emitRmwAluOp(node, newIv, srcT, srcImmOp, unary)
			emitAtomicStoreCall(effTemp, capLoArg, newIv, oldLo)
		else
			alrmwCounter += 1
			local loopTop = ".Lcasloop_" .. name .. "_" .. alrmwCounter
			cg.label(loopTop)
			-- a safepoint on every iteration (a contended CAS loop must not starve
			-- the GC's stop-the-world; filcc's atomic-RMW loops pollcheck too)
			emitPollcheck()
			emitAtomicLoadCall(effTemp, capLoArg)
			marshalResultPtr(oldIv, oldLo)
			cg.move(newIv, oldIv)
			emitRmwAluOp(node, newIv, srcT, srcImmOp, unary)
			emitAtomicCasCall(effTemp, capLoArg, oldIv, oldLo, newIv, oldLo)
			local casIv = allocTemp({ role = "alspcas" })
			cg.move(casIv, pColored(cc.rtRet))
			cg.cmpBranch(oldIv, casIv, "ne", loopTop)   -- mismatch: retry
		end
		emitRmwFlags(node, oldIv, srcT, srcImmOp, unary)
	end

	local excLabel = nil
	local function ensureExcLabel() if not excLabel then excLabel = ".Lexc_" .. name end return excLabel end
	local calledExterns = {}

	-- move the web's capability (its lower, or null) into a physical register.
	local function emitCapMove(destReg, web)
		local pDest = pColored(destReg)
		local loT = web and pf.lowerTemp[web]
		if loT then cg.move(pDest, loT) else cg.moveZero(pDest) end
		return pDest
	end

	-- Fast-CC argument marshalling for a call with signature cs whose yolo argument
	-- webs are argTemps (keyed by yolo argument SOURCE — SysV register number, or
	-- the outgoing-area slot register for a 7th+ integer argument; see callArgSource).
	-- Appends the precolored dense argument registers to `uses` and returns the FP
	-- vector uses ({[xmm/v reg] = widthBytes} or nil) and the outgoing stack-arg
	-- window size (0 when the signature's dense GPR words fit in the CC registers).
	--
	-- Dense words beyond cc.argRegCount travel on the stack, densely packed in
	-- declaration word order at 8*j(%rsp) AT the call — pizlonated clang's exact
	-- placement (verified for 5..14-word signatures incl. a pointer straddling the
	-- r9/stack boundary). The stores execute inside an outgoing stack-arg window:
	-- rsp is dropped over the area BEFORE the stores (cg.beginStackArgs) and
	-- restored right after the call (cg.endStackArgs, which the caller emits BEFORE
	-- the exception-flag test so both the normal and the exception path leave rsp
	-- balanced). Any spill reloads regalloc inserts for the stores' sources are
	-- stamped with the window's shift and re-based at render time (node.spillShift;
	-- see sarcasm.luau's rewriteSpills and x86_64_render.renderInsn).
	local function marshalFastArgs(cs, argTemps, uses, stmt)
		local fpVecUses = nil
		local stackWords = nil
		for k = 1, cs.argWords do
			if isFp(cs.argTypes[k]) then
				-- FP arg: passes through its v register (placed there by the source
				-- program, per the fast CC) — no GPR marshalling, but the value must
				-- stay live up to the call for the FP save/restore liveness.
				local m = fpOrdinalOf(cs.argTypes, k)
				fpVecUses = fpVecUses or {}
				fpVecUses[m] = sig.fpWidthBytes(cs.argTypes[k])
			else
				local aTemp = argTemps[callArgSource(cs, k, stmt)]
				local s = cc.denseSlot(cs.argTypes, k)
				if s < cc.argRegCount then
					cg.move(pColored(cc.argIv(cs.argTypes, k)), aTemp)
					table.insert(uses, pColored(cc.argIv(cs.argTypes, k)))
				else
					stackWords = stackWords or {}
					table.insert(stackWords, { off = 8 * (s - cc.argRegCount), temp = aTemp })
				end
				if cs.argTypes[k] == 7 then
					if s + 1 < cc.argRegCount then
						table.insert(uses, emitCapMove(cc.argLo(cs.argTypes, k), aTemp))
					else
						-- the pointer's lower word is stack-resident: marshal the
						-- web's capability, or a null lower when it carries none
						-- (exactly emitCapMove's moveZero case). Re-read the lower
						-- from its GC ROOT SLOT (a fixed frame offset) rather than
						-- keeping the lower web live up to the call: lower webs are
						-- no-spill (they must stay available for root stores), so a
						-- call with many pointer arguments would otherwise run out
						-- of registers at the marshalling point. The slot is
						-- guaranteed current — every lower is rooted where its web
						-- is defined (entry, ptr loads, ptr-returning calls,
						-- allocations) — and the scratch temp rides the window like
						-- any other store source (spill reloads re-base there).
						local loT = pf.lowerTemp[aTemp]
						local slotIdx = loT and loToSlot[loT]
						if slotIdx then
							local scratch = allocTemp({ role = "capreload" })
							cg.loadFrameSlot(scratch, 16 + 8 * slotIdx)
							stackWords = stackWords or {}
							table.insert(stackWords, { off = 8 * (s + 1 - cc.argRegCount), temp = scratch })
						else
							stackWords = stackWords or {}
							table.insert(stackWords, { off = 8 * (s + 1 - cc.argRegCount),
								temp = loT, cap = true })
						end
					end
				end
			end
		end
		local winN = 0
		if stackWords then
			local maxOff = 0
			for _, sw in stackWords do if sw.off > maxOff then maxOff = sw.off end end
			winN = math.ceil((maxOff + 8) / 16) * 16
			cg.beginStackArgs(winN)
			for _, sw in stackWords do
				if sw.cap and not sw.temp then
					cg.storeStackArgZero(sw.off)
				else
					cg.storeStackArg(sw.temp, sw.off)
				end
			end
		end
		return fpVecUses, winN
	end

	local function emitCall(node)
		local cs = callSigOf(node.stmt)
		local callee = node.stmt.operands[1].text
		calledExterns[callee] = { name = callee, sig = cs.signature, sigDesc = cs,
			sigName = "pizlonatedFI" .. cs.signature .. "_" .. callee }
		-- A backend that accepts FP signatures must provide the marshalling atoms
		-- (FP signatures are rejected at checkCallsiteSig otherwise); error
		-- defensively if one got here without them.
		local hasFp = csHasFp(cs)
		if hasFp and not cg.storeFpArg then
			error("sarcasm: internal error: floating-point signature marshalling is"
				.. " not implemented for this target (call to '" .. callee .. "')")
		end
		local argTemps = {}
		for _, r in node.c.uses do if r.argReg then argTemps[r.num] = node.opTemp[r] end end
		-- The guard arms BEFORE the marshalling: a dense argument word (or a
		-- pointer argument's lower) rides rcx into the call, which would silently
		-- reset the counter every iteration. EXCEPTION — a POINTER-returning call
		-- (retIsPtr): under the FIP CC the callee delivers the result's lower in
		-- retLo = rcx, so hardware rcx AFTER the call IS the returned lower and
		-- the saved pre-call counter value no longer exists on hardware (the
		-- callee's return destroyed it). Restoring it here would stomp the
		-- returned lower (a `loop` after a ptr-returning call then counted down
		-- stale garbage — an endless loop, and a jrcxz took the wrong branch).
		-- So a ptr-returning call skips the preserve and instead DEFINES the
		-- counter web from the result's lower web right after the result
		-- unpacking (below) — hardware-exact under the FIP CC.
		local wireCounterFromLower = cs.retIsPtr and counterRepWeb ~= nil
		local rcxSave = nil
		if not wireCounterFromLower then rcxSave = counterGuardArm() end
		cg.move(pColored(cc.myth), vMyth)
		local callUses = { pColored(cc.myth) }
		local fpVecUses, winN = marshalFastArgs(cs, argTemps, callUses, node.stmt)
		local callNode = cg.call("pizlonatedFI" .. cs.signature .. "_" .. callee, callUses, cg.callerSavedClobbers())
		if fpVecUses then callNode.fpVecUses = fpVecUses end
		-- restore rsp over the outgoing stack-arg area BEFORE the exception-flag
		-- test: both the normal and the exception path then see a balanced stack
		-- (the flag rides the %al result register, which the add does not touch).
		if winN > 0 then cg.endStackArgs(winN) end
		cg.exceptionBranch(ensureExcLabel())
		if not cs.isVoid and not isFp(cs.retTypes[1]) then
			cg.move(node.defTemps[1], pColored(cc.retIv))
			if cs.retIsPtr then
				-- a pointer call result is (iv, lo); define its lower from retLo and root it
				-- before any later safepoint can run.
				local loT = pf.lowerTemp[node.defTemps[1]]
				if loT then
					cg.move(loT, pColored(cc.retLo))
					emitRootStore(loT)
					if wireCounterFromLower then
						-- the counter web is pinned to physical rcx, whose hardware
						-- value after this call is exactly the returned lower:
						-- define the modeled web from the lower web so the model
						-- matches the register (a plain copy — rcx already holds
						-- this value, so it changes no hardware behavior).
						cg.move(counterRepWeb, loT)
					end
				end
			end
		end
		-- The restore sits after the result unpacking: a pointer result's lower is
		-- delivered in retLo = rcx, so rcx is still a CALL RESULT here — restoring
		-- the counter before reading it would overwrite the callee's lower with the
		-- saved counter (the exception edge skips the restore; it never returns).
		-- A ptr-returning call restored nothing (its counter web was defined from
		-- the returned lower above, which is what hardware rcx holds); rcxSave is
		-- nil there and this is a no-op.
		counterGuardRestore(rcxSave)
	end

	-- Annotated register-indirect call (`call *%reg ;! sig(...)` on x86_64,
	-- `blr xN ;! sig(...)` on arm64 — the sequence below is arch-neutral; the
	-- x86-isms live in the cg hooks). Emits filcc's exact indirect-call
	-- sequence for a flight pointer (P = target intval, L = target lower):
	--   1. L == 0 -> fail (null capability);
	--   2. aux = [L-8]; (aux & 0x780000000000000) != 0x80000000000000 -> fail
	--      (object's special type must be FUNCTION);
	--   3. P != (aux & 0xFFFFFFFFFFFF) -> fail (ptr must equal the object's aux
	--      pointer);
	--   4. [L+16] == S (the callsite signature number) -> fast path: call *[L+0]
	--      with the fast-CC marshalling below; otherwise the GENERIC path marshals
	--      the arguments through my_thread's CC buffer (data words at 128(myth),
	--      aux/lower words at 384(myth)), sets the argument byte size (the CC's
	--      third runtime argument register: edx / x2), and calls the generic
	--      entrypoint *[L+8], then tests the exception flag FIRST (on the
	--      exception edge the ret-size register is undefined, so the ret-size
	--      check must not run first), checks the returned ret_size, and unmarshals
	--      the result from the buffer into the fast-CC return registers — exactly
	--      the L_generic mismatch path of the direct-call callsite thunk
	--      (x86_64_glue / arm64_glue callsiteThunk), INLINED. filcc instead
	--      dispatches to pizlonated1ET<S> there, but that weak thunk only exists
	--      when some C compilation unit in the link contains an indirect callsite
	--      with signature S — referencing it would be a link error otherwise, so
	--      sarcasm must not (and does not) depend on it. The buffer-CC callee has
	--      the SAME callsite-visible ABI as the fast entrypoint (flag bit0 =
	--      exception flag; result in the ret registers after unmarshalling), so
	--      both paths rejoin at the shared exception check + result unpacking.
	--   5. fail: (P, L) = (arg0, arg1), call filc_check_function_call_fail
	--      (noreturn; the runtime re-derives the precise cause).
	-- No bounds check: a function object's bounds are degenerate (upper == lower).
	-- GC safety is exactly emitCall's: the target's lower and every pointer
	-- argument's lower were rooted into the frame's root slots when their webs
	-- were defined (entry, `;! load ptr`, a ptr-returning call), so every lower
	-- live across the call is in its slot; a pointer result's lower is rooted
	-- right after the call.
	local indcallCounter = 0
	local function emitIndirectCall(node)
		local cs = callSigOf(node.stmt)
		local raw = node.stmt.raw or node.stmt.mnemonic or "?"
		-- A backend that accepts FP signatures must provide the marshalling atoms
		-- (FP signatures are rejected at checkCallsiteSig otherwise); error
		-- defensively if one got here without them.
		local hasFp = csHasFp(cs)
		if hasFp and not cg.storeFpArg then
			error("sarcasm: internal error: floating-point signature marshalling is"
				.. " not implemented for this target (indirect call): " .. raw)
		end
		local targetOp = node.stmt.operands and node.stmt.operands[1]
		local fpIv = targetOp and node.opTemp[targetOp] or nil
		local fpLo = fpIv and pf.lowerTemp[fpIv] or nil
		if not (fpIv and pf.isPtr[fpIv] and fpLo) then
			-- The target register's web is not a known pointer value, so its
			-- lower (capability) is unknowable and the callee-is-a-function
			-- checks cannot be emitted.
			error("sarcasm: indirect call target must be a function pointer value"
				.. " (a pointer argument, or the result of `load ptr` /"
				.. " a ptr-returning call): " .. raw)
		end
		indcallCounter += 1
		local tag = name .. "_" .. indcallCounter
		local genL = ".Lindcall_generic_" .. tag
		local rejoinL = ".Lindcall_rejoin_" .. tag
		local failL = ".Lindcall_fail_" .. tag
		local retfailL = ".Lindcall_retfail_" .. tag

		-- yolo argument webs (SysV argument registers), exactly like emitCall.
		local argTemps = {}
		for _, r in node.c.uses do if r.argReg then argTemps[r.num] = node.opTemp[r] end end

		-- One guard covers BOTH call paths (fast + generic): the save sits before
		-- either marshalling, and each path restores right after its own call
		-- (the noreturn fail stubs need no restore). EXCEPTION — a
		-- POINTER-returning call (retIsPtr): retLo = rcx is the returned lower on
		-- BOTH paths (the fast CC returns it in rcx; the generic path unmarshals
		-- it from the buffer into rcx), so hardware rcx after the call IS the
		-- returned lower and the saved pre-call counter value no longer exists.
		-- The call skips the preserve and DEFINES the counter web from the
		-- result's lower web at the shared unpack below — hardware-exact under
		-- the FIP CC (the direct-call site does the same; see it for the full
		-- rationale).
		local wireCounterFromLower = cs.retIsPtr and counterRepWeb ~= nil
		local rcxSave = nil
		if not wireCounterFromLower then rcxSave = counterGuardArm() end

		-- 1. null capability
		cg.branchZero(fpLo, failL)
		-- 2. special type == FUNCTION (aux word at [L-8])
		local auxT = allocTemp({ role = "scratch" })
		cg.load(auxT, fpLo, -8, 8)
		local maskedT = allocTemp({ role = "scratch" })
		cg.andConst(maskedT, auxT, "0x780000000000000")   -- maskedT = aux & FUNCTION-type mask
		local valT = allocTemp({ role = "scratch" })
		cg.movBig(valT, "0x80000000000000")
		cg.cmpBranch(maskedT, valT, "ne", failL)
		-- 3. ptr == canonical entrypoint (aux & 0xFFFFFFFFFFFF)
		local canonT = allocTemp({ role = "scratch" })
		cg.move(canonT, auxT, true)
		cg.maskLow48(canonT)
		cg.cmpBranch(fpIv, canonT, "ne", failL)
		-- 4. signature dispatch: [L+16] == S -> fast entrypoint at [L+0]
		local sigT = allocTemp({ role = "scratch" })
		cg.load(sigT, fpLo, 16, 8)
		-- the signature number can exceed any arch's immediate-compare range, so
		-- the compare goes through the widened hook (x86_64: plain cmp imm32;
		-- arm64: plain 12-bit cmp when it fits, else the movz/sub-cmp schemes).
		cg.cmpImmBranchWidened(sigT, cs.signature, "ne", genL)
		local fnT = allocTemp({ role = "scratch" })
		cg.load(fnT, fpLo, 0, 8)
		-- fast path: myth = CC myth, fo = callee lower, dense fast-CC args.
		-- FP args are SKIPPED (they pass through their vector registers, placed there
		-- by the source program per the fast CC) — but they must stay live up to the
		-- call, so the call node is tagged with their vector uses for the FP
		-- save/restore liveness (fpVecUses, see the backends' expandFpSaves).
		cg.move(pColored(cc.myth), vMyth)
		cg.move(pColored(cc.fo), fpLo)
		local fastUses = { pColored(cc.myth), pColored(cc.fo), fnT }
		local fpVecUses, winN = marshalFastArgs(cs, argTemps, fastUses)
		local fastNode = cg.emitIndirectCallInsn(fnT, fastUses, cg.callerSavedClobbers())
		if fpVecUses then fastNode.fpVecUses = fpVecUses end
		-- restore rsp over the outgoing stack-arg area before rejoining (the generic
		-- path never entered the window — it branched away before the marshalling).
		if winN > 0 then cg.endStackArgs(winN) end
		cg.jump(rejoinL)
		-- generic (buffer-CC) path for a signature mismatch: marshal the args into
		-- my_thread's CC buffer, set the argument byte size, call *[L+8]. The buffer
		-- word index is the argument's DECLARATION index for EVERY class: GPR intval
		-- words at 128+8i (lower words at 384+8i), FP args as one 8-byte word at
		-- 128+8i (a float writes the low 4 bytes; the upper bytes are never read)
		-- with a zero aux word at 384+8i — exactly filcc's buffer marshalling. The
		-- FP values are read straight out of their vector registers (still live
		-- there).
		cg.label(genL)
		local argBytes = cs.argWords * 8
		local zeroT = nil
		for k = 1, cs.argWords do
			local ty = cs.argTypes[k]
			local bufOff = 128 + 8 * (k - 1)
			local loOff = 384 + 8 * (k - 1)
			if isFp(ty) then
				cg.storeFpArg(ty, fpOrdinalOf(cs.argTypes, k), vMyth, bufOff)
				if not zeroT then zeroT = allocTemp({ role = "scratch" }); cg.moveZero(zeroT) end
				cg.store(zeroT, vMyth, loOff)
			else
				local aTemp = argTemps[callArgSource(cs, k)]
				cg.store(aTemp, vMyth, bufOff)
				local loT = (ty == 7) and pf.lowerTemp[aTemp] or nil
				if loT then
					cg.store(loT, vMyth, loOff)
				else
					-- scalar arg (or a pointer arg whose web has no capability):
					-- the aux word is 0, exactly like the callsite thunk.
					if not zeroT then zeroT = allocTemp({ role = "scratch" }); cg.moveZero(zeroT) end
					cg.store(zeroT, vMyth, loOff)
				end
			end
		end
		cg.moveImm(pColored(cc.rtArg(3)), argBytes)   -- argument byte size (CC arg 3)
		cg.move(pColored(cc.myth), vMyth)             -- myth = CC myth register
		cg.move(pColored(cc.fo), fpLo)                -- fo = callee lower
		cg.emitGenericIndirectCall(fpLo,
			{ pColored(cc.myth), pColored(cc.fo), pColored(cc.rtArg(3)), fpLo },
			cg.callerSavedClobbers())
		-- Test the exception flag IMMEDIATELY after the generic call, BEFORE the
		-- ret-size check and the result unmarshal — exactly L_generic's order in
		-- the callsite thunk (and filcc's pizlonated1ET thunk). On the exception
		-- edge the buffer CC leaves the ret-size register (rdx / x1) UNDEFINED, so
		-- a ret-size check running first could spuriously fire
		-- filc_cc_rets_check_failure instead of propagating the exception. Branches
		-- to the SAME shared exception label the fast path's rejoin test uses.
		cg.exceptionBranch(ensureExcLabel())
		if cs.retWords >= 1 then
			-- the buffer CC returns the actual result byte size in the retIv
			-- register (rdx / x1); it must cover the signature's result words (else
			-- filc_cc_rets_check_failure), then the result unmarshals into the
			-- fast-CC return registers. An FP result (float/double) unmarshals from
			-- the buffer into xmm0 (v0 on arm64) instead of the retIv register.
			local retBytes = cs.retWords * 8
			cg.cmpImmBranchWidened(pColored(cc.retIv), retBytes - 1, "ule", retfailL)
			if isFp(cs.retTypes[1]) then
				cg.loadFpRet(cs.retTypes[1], vMyth, 128)
			else
				cg.load(pColored(cc.retIv), vMyth, 128, 8)
				if cs.retIsPtr then cg.load(pColored(cc.retLo), vMyth, 384, 8) end
			end
		end
		-- rejoin: exception-flag test + result unpacking, identical to emitCall.
		-- (On the generic fall-through edge the flag is already known clear, so
		-- the shared re-test just falls through.)
		cg.label(rejoinL)
		cg.exceptionBranch(ensureExcLabel())
		if not cs.isVoid and not isFp(cs.retTypes[1]) then
			cg.move(node.defTemps[1], pColored(cc.retIv))
			if cs.retIsPtr then
				local loT = pf.lowerTemp[node.defTemps[1]]
				if loT then
					cg.move(loT, pColored(cc.retLo))
					emitRootStore(loT)
					if wireCounterFromLower then
						-- the counter web is pinned to physical rcx, whose hardware
						-- value after this call is exactly the returned lower on
						-- both paths: define the modeled web from the lower web
						-- (a plain copy — rcx already holds this value).
						cg.move(counterRepWeb, loT)
					end
				end
			end
		end
		-- Shared restore for BOTH call paths, after the shared result unpacking:
		-- retLo = rcx is a call RESULT on both paths (the fast CC returns the
		-- lower in rcx; the generic path unmarshals it from the buffer into rcx),
		-- so the counter's saved value may only be written back once no consumer
		-- of the callee's return words remains. A ptr-returning call restored
		-- nothing (its counter web was defined from the returned lower above);
		-- rcxSave is nil there and this is a no-op.
		counterGuardRestore(rcxSave)
		-- fail stubs (noreturn; emitted at function end): the function-call check
		-- failure gets the flight pointer (arg0 = P, arg1 = L); the buffer-CC
		-- ret-size failure gets (arg0 = actual size, arg1 = expected size, arg2 = 0).
		cg.emitIndcallFailStub(failStubs, failL, fpIv, fpLo)
		if cs.retWords >= 1 then
			cg.emitRetsFailStub(failStubs, retfailL, pColored(cc.retIv), cs.retWords * 8)
		end
	end

	-- Global-variable materializations (x86_64; reached only when the driver
	-- collected data objects or a `#! global ptr` annotation was validated).
	--
	-- emitGlobalGetter: EXTERN global — call pizlonated_<sym>@PLT (rdi=myth,
	-- rsi=0; result rax=intval, rdx=lower), the exact cross-module contract
	-- FilPizlonator emits for an address-of-global. An injected nounwind runtime
	-- call: caller-saved clobbers, fpSave/fpRestore, counterGuard; NO exception
	-- branch (getters are nounwind) and NO root store (a global's lower is a
	-- link-time constant into static ELF memory — FUGC never moves it).
	local function emitGlobalGetter(sym, dstIv, dstLo)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.moveZero(pColored(cc.rtArg(2)))
		local n = cg.call("pizlonated_" .. sym,
			{ pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		n.globalGetter = true   -- fpSave/fpRestore pair preserves vector state (see expandFpSaves)
		cg.fpRestore()
		counterGuardRestore(rcxSave)
		cg.move(dstIv, pColored(cc.rtRet))
		cg.move(dstLo, pColored(cc.rtRetLo))
	end

	-- emitGlobalInlineGP: same-file WRITABLE global — the inline-GP fast path
	-- (mirrors filcc's same-module materialization, verified against clang):
	-- load the `.LpizlonatedGP_` cell (intval at +0, lower at +8); if either
	-- word is zero, call `.LpizlonatedGS_` (the slow-init function, which
	-- REGISTERS the object into the GC's global root set — required before any
	-- pointer store into the object) and materialize `leaq <doSym>+payloadOff`
	-- into both words; rejoin.
	local gpMatCounter = 0
	local function emitGlobalInlineGP(desc, dstIv, dstLo)
		gpMatCounter += 1
		local tag = name .. "_" .. gpMatCounter
		local slowL = ".Lgslow_" .. tag
		local doneL = ".Lgdone_" .. tag
		cg.loadRipSym(dstLo, desc.gpSym, 8)
		cg.loadRipSym(dstIv, desc.gpSym)
		cg.branchZero(dstLo, slowL)
		cg.branchZero(dstIv, slowL)
		cg.jump(doneL)
		cg.label(slowL)
		local rcxSave = counterGuardArm()
		cg.fpSave()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.moveZero(pColored(cc.rtArg(2)))
		local n = build.insn("call", { build.sym(desc.gsSym) },
			{ control = "call", uses = { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) },
				defs = cg.callerSavedClobbers() })
		n.globalGetter = true
		emit(n)
		cg.fpRestore()
		counterGuardRestore(rcxSave)
		cg.leaRipSym(dstIv, desc.doSym, desc.payloadOff)
		cg.leaRipSym(dstLo, desc.doSym, desc.payloadOff)
		cg.label(doneL)
	end

	-- Materialize the (effective-address, capability-lower) pair for a pointer
	-- access to a global memory operand. Extern symbols always go through the
	-- getter (the cross-module contract). Same-file WRITABLE globals go through
	-- inline-GP for STORE-class accesses only (forcing registration — a pointer
	-- store into an unregistered global would be invisible to the GC); same-file
	-- loads and READONLY globals return nil (the caller's computeAddr produces
	-- the plain lea materialization: loads see aux==0 correctly, stores trap in
	-- the sequence's CanWrite check, exactly like compiled code). `disp` folds
	-- into the effective address.
	local function materializeGlobalPtrAccess(node, mem, forStore)
		if not (mem and mem.base and mem.base.rc == "rip" and mem.dispSym
			and not mem.dispReloc and not mem.index and dataGlobals) then
			return nil, nil
		end
		local sym = mem.dispSym
		local desc = dataGlobals[sym]
		if desc and (desc.readonly or not forStore) then return nil, nil end
		if not desc and bodyLocalLabels[sym] then
			error("sarcasm: `" .. (node.stmt.annotation or "") .. "` pointer access to the"
				.. " local code label '" .. sym .. "' is not a global access: "
				.. (node.stmt.raw or "?"))
		end
		local gpIv = allocTemp({ role = "gmativ" })
		local gpLo = allocTemp({ role = "gmatlo" })
		if desc then
			emitGlobalInlineGP(desc, gpIv, gpLo)
		else
			emitGlobalGetter(sym, gpIv, gpLo)
		end
		local disp = mem.dispVal or 0
		local effTemp = gpIv
		if disp ~= 0 then
			effTemp = allocTemp({ role = "eff" })
			cg.addImm(effTemp, gpIv, disp)
		end
		return effTemp, gpLo
	end


	-- `rep movs*` / `rep stos*` (and their bare single-step forms): a checked
	-- block copy / fill over the implicit registers. The direction flag must be
	-- clear (a set DF would copy/fill backwards, which this lowering does not
	-- model — it traps); the source range ([%rsi, %rsi + %rcx*elem), movs
	-- only) gets a read check and the destination range ([%rdi, ...)) a write
	-- check, each against its pointer's capability (bounds plus CanWrite for
	-- writes, exactly like the ordinary access check but over the dynamic
	-- %rcx-derived length). With %rcx == 0 the instruction is a no-op and all
	-- checks, including the DF check, are skipped — exactly like hardware.
	-- The webs ride the physical registers only across the instruction itself
	-- (explicit pin moves in and out — the fail-stub marshalling also needs
	-- rdi/rsi, so the webs stay virtual and colorable); the pin-out also
	-- defines the post-instruction webs (rsi/rdi advanced by the count, rcx
	-- zeroed), so later reads see the hardware values.
	local function emitRepString(node)
		local rep = node.c.rep
		local raw = node.stmt.raw or node.stmt.mnemonic
		local function repTemp(occ, what)
			local t = occ and node.opTemp and node.opTemp[occ] or nil
			if not t then
				error("sarcasm: internal error: rep string " .. what .. " has no web temp: " .. raw)
			end
			return t
		end
		local rcxT = repTemp(rep.rcxUse, "%rcx")
		local rcxDefT = repTemp(rep.rcxDef, "%rcx result")
		local rdiT = repTemp(rep.rdi, "%rdi")
		local rsiT = rep.rsi and repTemp(rep.rsi, "%rsi") or nil
		local raxT = rep.rax and repTemp(rep.rax, "%rax") or nil
		-- Byte count N: %rcx * elem under `rep` (by doubling adds — elem is
		-- 1/2/4/8), or exactly one element for the bare single-step form
		-- (which ignores %rcx entirely, like the hardware).
		local nT = nil
		if rep.repPrefix then
			if rep.elem > 1 then
				nT = allocTemp({ role = "repstring" })
				cg.move(nT, rcxT)
				local doublings = ({ [2] = 1, [4] = 2, [8] = 3 })[rep.elem]
				for _ = 1, doublings do cg.addReg(nT, nT, nT) end
			else
				nT = rcxT
			end
		else
			nT = allocTemp({ role = "repstring" })
			cg.moveImm(nT, rep.elem)
		end
		-- Fail stub for a range failure: filc_check_aligned_access_fail takes
		-- the (dynamic) byte count as an argument, so the panic attributes
		-- the true range (a fixed-size optimized origin could not — the
		-- runtime would find the recorded single element in bounds and hit
		-- its "Should not be reached" assert). One stub per checked range.
		-- sizeOverride (nil = report nT, otherwise an immediate byte count)
		-- exists for the count-overflow guard below: the wrapped nT would
		-- under-report the range (and could even look in-bounds to the
		-- runtime), so the overflow stub reports SIZE_MAX (-1) instead,
		-- which exceeds every object's room and always panics cleanly.
		local function repFailStub(ptrT, capLo, isWrite, sizeOverride)
			failCounter += 1
			local lbl = ".Lrepfail_" .. name .. "_" .. failCounter
			local a1, a2, a3 = cc.rtArg(1), cc.rtArg(2), cc.rtArg(3)
			local a4, a5, a6 = cc.rtArg(4), cc.rtArg(5), cc.rtArg(6)
			table.insert(failStubs, build.label(lbl))
			table.insert(failStubs, build.insn("mov", { build.rp(a1), build.rt(ptrT) },
				{ defs = { pColored(a1) }, uses = { ptrT } }))
			if capLo then
				table.insert(failStubs, build.insn("mov", { build.rp(a2), build.rt(capLo) },
					{ defs = { pColored(a2) }, uses = { capLo } }))
			else
				table.insert(failStubs, build.insn("mov", { build.rp(a2), build.imm(0) },
					{ defs = { pColored(a2) } }))
			end
			if sizeOverride ~= nil then
				table.insert(failStubs, build.insn("mov", { build.rp(a3), build.imm(sizeOverride) },
					{ defs = { pColored(a3) } }))
			else
				table.insert(failStubs, build.insn("mov", { build.rp(a3), build.rt(nT) },
					{ defs = { pColored(a3) }, uses = { nT } }))
			end
			table.insert(failStubs, build.insn("mov", { build.rp(a4), build.imm(1) },
				{ defs = { pColored(a4) } }))
			table.insert(failStubs, build.insn("mov", { build.rp(a5), build.imm(isWrite and 1 or 0) },
				{ defs = { pColored(a5) } }))
			table.insert(failStubs, build.insn("mov", { build.rp(a6), build.imm(0) },
				{ defs = { pColored(a6) } }))
			-- 7th word (ptr_name lower; the iv is null so the runtime ignores
			-- the name) rides the stack at [rsp] AT the call, exactly like
			-- cg.pushStackPtr.
			local nameLoSlot = build.memx(build.rp(4), { dispVal = -16 })
			nameLoSlot.size = 8
			table.insert(failStubs, build.insn("mov", { nameLoSlot, build.imm(0) }, {}))
			table.insert(failStubs, build.insn("sub", { build.rp(4), build.imm(16) }, {}))
			table.insert(failStubs, build.insn("call",
				{ build.sym("filc_check_aligned_access_fail", "plt") },
				{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2),
					pColored(a3), pColored(a4), pColored(a5), pColored(a6) } }))
			return lbl
		end
		local dstFail = repFailStub(rdiT, pf.lowerTemp[rdiT], true)
		local srcFail = nil
		if rsiT then srcFail = repFailStub(rsiT, pf.lowerTemp[rsiT], false) end
		-- Count-overflow guard: under `rep` with elem = 2^k > 1, N is
		-- computed as %rcx << k by doubling adds, which wraps mod 2^64 for
		-- %rcx >= 2^(64-k) (e.g. 2^61 qwords). The wrapped N would pass the
		-- room checks below while the hardware copies %rcx elements OOB.
		-- Reject %rcx > (2^64-1)/elem up front, i.e. %rcx with any of the
		-- top k bits set, before the range checks. The stub reports SIZE_MAX
		-- (-1 renders as `mov $-1`, all 64 bits set), which exceeds every
		-- object's room so the runtime always panics with a clean range
		-- error instead of seeing the wrapped small N. Attributed to the
		-- first-checked range (source for movs, destination for stos); an
		-- overflowing count exceeds every room, so either attribution is
		-- sound. Bare forms (N = elem constant) and elem == 1 (N == %rcx,
		-- exact) cannot overflow and need no guard.
		local overflowFail = nil
		if rep.repPrefix and rep.elem > 1 then
			if rsiT then overflowFail = repFailStub(rsiT, pf.lowerTemp[rsiT], false, -1)
			else overflowFail = repFailStub(rdiT, pf.lowerTemp[rdiT], true, -1) end
		end
		-- Direction-flag violations trap with an illegal instruction (there is
		-- no access failure to attribute — the pointers may be perfectly
		-- valid — so no runtime fail function applies; this is the same pure
		-- trap the `.alloca` alignment failure uses).
		failCounter += 1
		local dfFail = ".Ldfail_" .. name .. "_" .. failCounter
		table.insert(failStubs, build.label(dfFail))
		table.insert(failStubs, build.insn("ud2", {},
			{ control = "none", fallsThrough = false, noreturnCall = true }))
		failCounter += 1
		local doneLbl = ".Lrep_" .. name .. "_" .. failCounter
		-- Under `rep`, %rcx == 0 means the instruction touches nothing — skip
		-- the checks (but still run the pins and the no-op rep below, so the
		-- post-webs match the hardware values on every path). The bare form
		-- always moves one element, so it always checks.
		if rep.repPrefix then cg.branchZero(rcxT, doneLbl) end
		-- Direction flag: trap when set (bit 10 of the saved rflags).
		do
			local dfT = cg.saveFlags()
			cg.testMaskBranch(dfT, 0x400, "ne", dfFail)
		end
		-- Count overflow: %rcx >= 2^(64-k) wraps N = %rcx*elem (elem = 2^k).
		-- Exactly the top-k-bits-set condition, tested as a mask so no
		-- 64-bit immediate compare (and no precision-hostile big constants
		-- in Luau doubles) is needed. Skipped for %rcx == 0 above, which is
		-- a no-op and must not trap.
		if overflowFail then
			local ovMask = ({ [2] = "0x8000000000000000",
				[4] = "0xC000000000000000",
				[8] = "0xE000000000000000" })[rep.elem]
			cg.testMaskBranch(rcxT, ovMask, "ne", overflowFail)
		end
		local function checkRange(ptrT, capLo, isWrite, fail)
			if capLo == nil then
				-- a capability-free address: any nonzero-length range traps.
				cg.jump(fail)
				return
			end
			cg.branchZero(capLo, fail)
			if isWrite then
				-- CanWrite: reject read-only/special objects, like every heap write.
				local auxword = allocTemp({ role = "repstring" })
				cg.load(auxword, capLo, -8, 8)
				cg.testMaskBranch(auxword, "0x6000000000000", "ne", fail)
			end
			cg.cmpBranch(ptrT, capLo, "ult", fail)
			local upper = allocTemp({ role = "repstring" })
			cg.load(upper, capLo, -16, 8)
			cg.cmpBranch(ptrT, upper, "ugt", fail)
			-- Overflow-free length check, mirroring the plain access check's
			-- upper - size form: room = upper - ptr (no wrap — ptr <= upper
			-- was just proven), then N > room fails.
			local room = allocTemp({ role = "repstring" })
			cg.subReg(room, upper, ptrT)
			cg.cmpBranch(nT, room, "ugt", fail)
		end
		if rsiT then checkRange(rsiT, pf.lowerTemp[rsiT], false, srcFail) end
		checkRange(rdiT, pf.lowerTemp[rdiT], true, dstFail)
		cg.label(doneLbl)
		-- Pin the webs into the implicit registers, run the instruction, and
		-- pin the results back out into the post-instruction webs. Two webs
		-- may already live in physical rcx: the count (a loop-counter or
		-- jrcxz web — the only fixedReg webs) and/or the post count (a later
		-- jrcxz precolors the rep's own rcx-def web). A precolored web needs
		-- no pin move — it IS the physical register:
		--   * count pinned: skip the rcx pin-in; the post count is modeled
		--     directly (under `rep` the hardware zeroes %rcx; the bare form
		--     leaves it alone, an identity copy).
		--   * post count pinned: pin the count straight into it (a same-color
		--     move the emitter drops when count and post coincide); the
		--     hardware then leaves the post value in place — no pin-out.
		-- (No counter save/restore is needed either way: a counter live
		-- across the rep IS the count web — its post value is exactly what
		-- the pins model — and otherwise no counter is live here.)
		local countPinned = temps[rcxT] and temps[rcxT].precolor == "rcx"
		local defPinned = temps[rcxDefT] and temps[rcxDefT].precolor == "rcx"
		if rsiT then cg.move(pColored(6), rsiT) end
		cg.move(pColored(7), rdiT)
		if defPinned then cg.move(rcxDefT, rcxT)
		elseif not countPinned then cg.move(pColored(1), rcxT) end
		if raxT then cg.move(pColored(0), raxT) end
		emit(node)
		if rsiT then cg.move(rsiT, pColored(6)) end
		cg.move(rdiT, pColored(7))
		if not defPinned then
			if countPinned then
				if rep.repPrefix then cg.moveZero(rcxDefT)
				else cg.move(rcxDefT, rcxT) end
			else
				cg.move(rcxDefT, pColored(1))
			end
		end
	end

	-- `.alloca size, alignment, result`: allocate `size` bytes with `alignment`
	-- and define the result web with the payload pointer. Unlike the historical
	-- `;! alloca` annotations, nothing touches %rsp: the allocation is a GC object
	-- (`filc_allocate`), the payload (allocation + 16) is 16-aligned, and wider
	-- alignments over-allocate and align up — the capability lower stays the
	-- payload base (where the object header lives), so bounds checks cover the
	-- whole allocation. The lower is rooted like every other allocation.
	-- size/alignment operands: immediates move in; registers (including pseudos)
	-- and spill slots arrive as webs through node.opTemp (the frame rewrite turned
	-- slot references into slot webs). A non-immediate alignment is validated at
	-- runtime (power of two, nonzero) with a clean trap otherwise.
	local function emitDotAlloca(node)
		local ops = node.stmt.operands or {}
		local raw = node.stmt.raw or ".alloca"
		local sizeOp, alignOp = ops[1], ops[2]
		if not (sizeOp and alignOp and ops[3]) then
			error("sarcasm: `.alloca` takes exactly 3 operands (size, alignment, result): " .. raw)
		end
		local function operandTemp(op, what)
			if op.kind == "imm" then return nil end
			local t = node.opTemp and node.opTemp[op]
			if not t then
				error("sarcasm: `.alloca` " .. what .. " has no modeled value (must be an immediate, a register, or a spill slot): " .. raw)
			end
			return t
		end
		local sizeTemp = allocTemp({ role = "dotalloca" })
		do
			local st = operandTemp(sizeOp, "size")
			if st then cg.move(sizeTemp, st)
			else
				local v = sizeOp.value or tonumber(sizeOp.text) or 0
				if v >= -2147483648 and v <= 2147483647 then cg.moveImm(sizeTemp, v)
				else cg.movBig(sizeTemp, v) end
			end
		end
		-- Alignment: immediate (compile-time power-of-two check) or register web
		-- (runtime power-of-two check with a clean trap).
		local alignImm = nil
		local alignTemp = operandTemp(alignOp, "alignment")
		if not alignTemp then
			alignImm = alignOp.value or tonumber(alignOp.text)
			if not alignImm or alignImm < 1 or alignImm ~= math.floor(alignImm) then
				error("sarcasm: `.alloca` alignment must be a positive power of two: " .. raw)
			end
			local p = 1
			while p < alignImm do p = p * 2 end
			if p ~= alignImm then
				error("sarcasm: `.alloca` alignment must be a positive power of two: " .. raw)
			end
		end
		-- Allocation size: the request, plus slack for aligning up (at most align-16
		-- for align > 16; harmless otherwise). Capability bounds come from the real
		-- object, so even a wrapped-around size stays sound (accesses trap).
		local allocSize = allocTemp({ role = "dotalloca" })
		if alignImm then
			if alignImm <= 16 then cg.move(allocSize, sizeTemp)
			else cg.addImm(allocSize, sizeTemp, alignImm - 16) end
		else
			cg.addReg(allocSize, sizeTemp, alignTemp)
		end
		local rcxSave = counterGuardArm()
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), allocSize)
		-- Injected returning runtime call: preserve live FP/SIMD state around it
		-- (no-op for GPR-only functions).
		cg.fpSave()
		cg.call("filc_allocate", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.fpRestore()
		counterGuardRestore(rcxSave)
		local resultIv = node.defTemps and node.defTemps[1]
		if not resultIv then
			error("sarcasm: `.alloca` result has no modeled (GPR) destination: " .. raw)
		end
		local resultLo = pf.lowerTemp[resultIv]
		if not resultLo then
			error("sarcasm: internal error: `.alloca` result web has no capability lower: " .. raw)
		end
		local ret = pColored(cc.rtRet)
		local payload = allocTemp({ role = "dotalloca" })
		cg.effAddr(payload, ret, 16)
		cg.effAddr(resultLo, ret, 16)
		emitRootStore(resultLo)
		if alignImm and alignImm <= 16 then
			-- 16-aligned payload already satisfies the request.
			cg.move(resultIv, payload, true)
		elseif alignImm then
			-- Over-allocated: align the payload up (mask math, exact for powers of two).
			cg.addImm(resultIv, payload, alignImm - 1)
			cg.andImmInPlace(resultIv, -alignImm)
		else
			-- Dynamic alignment: validate nonzero power-of-two (pure trap on failure)
			-- then align the payload up.
			-- through the payload capability.
			failCounter += 1
			local lbl = ".Lallocaalignfail_" .. name .. "_" .. failCounter
			local okLbl = ".Lallocaalignok_" .. name .. "_" .. failCounter
			cg.branchZero(alignTemp, lbl)
			local m1 = allocTemp({ role = "dotalloca" })
			cg.subImm(m1, alignTemp, 1)
			local axb = allocTemp({ role = "dotalloca" })
			cg.andReg(axb, alignTemp, m1)
			cg.branchNonZero(axb, lbl)
			cg.jump(okLbl)
			cg.label(lbl)
			cg.trap()
			cg.label(okLbl)
			local biased = allocTemp({ role = "dotalloca" })
			cg.addReg(biased, payload, m1)
			local z = allocTemp({ role = "dotalloca" })
			cg.moveZero(z)
			local neg = allocTemp({ role = "dotalloca" })
			cg.subReg(neg, z, alignTemp)
			cg.andReg(resultIv, biased, neg)
		end
	end

	-- Redirect an sp-relative address computation that lands inside the
	-- fixed-frame escape-promotion region to the allocation pointer.
	-- Arch-specific; returns true if it replaced the node.
	local function emitRegionRedirect(node)
		if #allocaRegions == 0 or not cg.regionRedirect then return false end
		return cg.regionRedirect(node, regionOf)
	end

	-- The promoted fixed-frame region's allocation (D9): emitted ONCE, up top,
	-- before any body statement — every fixed-frame access and lea redirects
	-- into the region, so it must exist from the first instruction. Mirrors
	-- (payload = allocation + 16, lower rooted).
	if frameRegion then
		withFlagSave(flagsLiveFrom(1), function()
			local rcxSave = counterGuardArm()
			cg.move(pColored(cc.rtArg(1)), vMyth)
			cg.moveImm(pColored(cc.rtArg(2)), frameRegion.size)
			cg.fpSave()
			cg.call("filc_allocate", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) },
				cg.callerSavedClobbers())
			cg.fpRestore()
			counterGuardRestore(rcxSave)
			local ret = pColored(cc.rtRet)  -- allocation base; payload = base + 16
			cg.effAddr(frameRegion.loTemp, ret, 16)
			cg.effAddr(frameRegion.ptrTemp, ret, 16)
			emitRootStore(frameRegion.loTemp)
		end)
	end

	-- Walk the lifted body.
	for _, node in lf.nodes do
		if node.kind == "label" then
			if node.name ~= name and not string.match(node.name, "^%.LF[BE]") then
				emit(node)
				if loopHeaderLabels[node.name] then
					withFlagSave(flagsLiveFrom(nodeIndex[node] + 1), function()
						emitPollcheck(loopHeaderRcx[node.name])
					end)
				end
			end
		elseif node.kind == "other" then
			-- drop directives
		elseif node.kind == "insn" then
			local c = node.c
			curLine = (node.stmt and node.stmt.line) or curLine
			if dotAllocaNodes[node] then
				-- `.alloca` directive: replaced wholesale by the GC allocation.
				withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
					emitDotAlloca(node)
				end)
			elseif node.stmt.localCall then
				-- A local call is an unconditional jump to the per-caller clone's
				-- entry, with the continuation address materialized into the
				-- clone's shared retaddr temp (the no-push clone model: rsp never
				-- moves, and the temp lives in a web the program cannot address).
				-- A clone with exactly ONE continuation dispatches with a plain
				-- jmp and never reads the temp: skip the lea.
				local lc = node.stmt.localCall
				if #clonesInfo[lc.clone].conts > 1 then
					cg.leaRipSym(retaddrTemps[lc.clone], lc.cont)
				end
				cg.jump(lc.entry)
			elseif node.stmt.localRet then
				-- A local ret is a multi-way branch to the clone's continuations:
				-- compare the retaddr temp against each continuation's address
				-- (the final one falls to an unconditional jmp — totality is
				-- guaranteed, the clone is only reachable through our callsites).
				-- The compare clobbers the flags; flags are documented
				-- call-clobbered across local calls (see flagsLiveFrom).
				local lr = node.stmt.localRet
				local t = retaddrTemps[lr.clone]
				local conts = clonesInfo[lr.clone].conts
				for k = 1, #conts - 1 do
					local scratch = allocTemp({ role = "localret" })
					cg.leaRipSym(scratch, conts[k])
					cg.cmpBranch(scratch, t, "eq", conts[k])
				end
				cg.jump(conts[#conts])
			elseif c.control == "return" then
				-- An FP-returning function (float/double) leaves the value in its
				-- vector return register (xmm0 / v0) — skip the retIv move (and the
				-- ptr-lower cap move; an FP return is never a pointer), exactly like
				-- the void case skips both.
				if not sigDesc.isVoid and not retIsFp then
					local retTemp
					for _, r in c.uses do if r.retReg then retTemp = node.opTemp[r] end end
					if retTemp then cg.move(pColored(cc.retIv), retTemp) end
					if sigDesc.retIsPtr then emitCapMove(cc.retLo, retTemp) end
				end
				cg.clearRetFlag()
				emit({ kind = "epilogue", name = name, useTemps = { vMyth } })
				emit(build.insn("ret", {}, { control = "return", fallsThrough = false }))
			elseif c.control == "call" and callNodes[node] then
				emitCall(node)
			elseif c.control == "indcall" then
				-- The annotation classification above already rejected every
				-- unannotated or memory-operand indirect call, so only an
				-- annotated register-indirect call reaches here (on both
				-- architectures).
				emitIndirectCall(node)
			elseif emitRegionRedirect(node) then
				-- sp-relative address computation redirected into an alloca region
			elseif c.rep then
				-- rep movs*/stos* (or a bare single-step movs*/stos*):
				-- bounds-checked block copy/fill over the implicit registers.
				withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
					emitRepString(node)
				end)
			else
				local mem = cgm.memOperand(node.stmt)
				local fref = funcrefNodes[node]
				local glea = globalLeaNodes[node]
				local gget = globalGetterNodes[node]
				if fref then
					-- `#! funcref` lea (x86_64): define (iv, lower) as the
					-- function's flight pointer. This is a pointer seed (ptrflow
					-- allocated the destination's lower temp); the lower is a
					-- link-time constant into static ELF memory — no GC root
					-- store (exactly like the global getters).
					local dstIv = node.defTemps and node.defTemps[1]
					if not dstIv then
						error("sarcasm: `funcref` lea has no modeled (GPR) destination"
							.. " register: " .. (node.stmt.raw or "?"))
					end
					local dstLo = pf.lowerTemp[dstIv]
					if fref.fo then
						-- Same-file function: the function object is emitted by
						-- this module, so materialize it directly — retarget the
						-- lea to pizlonatedFO_<owner>+16(%rip), exactly the
						-- getter's output (intval == lower == the FO payload, the
						-- canonical entrypoint of the in-object header). The lea
						-- itself renders the (rewritten) verbatim form; the
						-- synthesized payload lea defines the capability lower.
						local foSym = "pizlonatedFO_" .. fref.fo
						mem.dispSym = foSym
						mem.dispVal = 16
						emit(node)
						cg.leaRipSym(dstLo, foSym, 16)
					else
						-- Extern function: the cross-module getter
						-- (pizlonated_<sym>; rdi=myth, rsi=0 -> rax=iv,
						-- rdx=lower), the `#! global ptr` contract — an injected
						-- nounwind runtime call.
						withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
							emitGlobalGetter(fref.getter, dstIv, dstLo)
						end)
					end
				elseif glea then
					-- Same-file global lea (rip-relative): a pointer seed. iv = the
					-- lea's value (payload + disp); lo = the data object's payload
					-- base. READONLY objects materialize directly (a static
					-- object's header is linker-initialized — no registration);
					-- WRITABLE ones go through inline-GP (the lea's uses may
					-- include pointer stores, which need registration).
					local dstIv = node.defTemps and node.defTemps[1]
					if not dstIv then
						error("sarcasm: global lea has no modeled (GPR) destination"
							.. " register: " .. (node.stmt.raw or "?"))
					end
					local dstLo = pf.lowerTemp[dstIv]
					local disp = (mem and mem.dispVal) or 0
					if glea.readonly then
						-- the lea itself renders verbatim (its dispSym is the user
						-- label at its payload offset); the synthesized payload-base
						-- lea defines the capability lower
						emit(node)
						cg.leaRipSym(dstLo, glea.baseSym)
					else
						withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
							emitGlobalInlineGP(glea, dstIv, dstLo)
						end)
						if disp ~= 0 then cg.addImm(dstIv, dstIv, disp) end
					end
				elseif gget and cgm.baseMnemonic(string.lower(node.stmt.mnemonic)) == "lea" then
					-- Extern `#! global ptr` lea: the getter call defines (iv, lo).
					local dstIv = node.defTemps and node.defTemps[1]
					if not dstIv then
						error("sarcasm: `global ptr` lea has no modeled (GPR) destination"
							.. " register: " .. (node.stmt.raw or "?"))
					end
					local dstLo = pf.lowerTemp[dstIv]
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						emitGlobalGetter(gget, dstIv, dstLo)
					end)
					local disp = (mem and mem.dispVal) or 0
					if disp ~= 0 then cg.addImm(dstIv, dstIv, disp) end
				elseif gget then
					-- Extern `#! global ptr` direct access: getter materialization,
					-- then the checked access through (iv+disp, lo). The memory
					-- operand is REWRITTEN to the materialized effective address
					-- (register-indirect): the verbatim rip-relative form would
					-- need the defining module's unsafe-export alias at link time,
					-- while the getter always returns the true payload address.
					local size, align = cgm.accessSizeAlign(node.stmt)
					local isWrite = cgm.isStoreInsn(node.stmt)
					local minfo = cgm.maskedAccessInfo and cgm.maskedAccessInfo(node.stmt)
					local effTemp = nil
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						local gpIv = allocTemp({ role = "ggetiv" })
						local gpLo = allocTemp({ role = "ggetlo" })
						emitGlobalGetter(gget, gpIv, gpLo)
						local disp = (mem and mem.dispVal) or 0
						effTemp = gpIv
						if disp ~= 0 then
							effTemp = allocTemp({ role = "eff" })
							cg.addImm(effTemp, gpIv, disp)
						end
						if minfo then
							emitMaskedAccessCheck(effTemp, gpLo, minfo, align, isWrite)
						else
							emitAccessCheck(effTemp, gpLo, size, align, isWrite)
						end
					end)
					local newBase = build.rt(effTemp)
					mem.base = newBase
					mem.dispSym = nil
					mem.dispVal = nil
					node.opTemp[newBase] = effTemp
					table.insert(node.useTemps, effTemp)
					if node.c and node.c.uses then table.insert(node.c.uses, newBase) end
					if not (cg.emitPinned and cg.emitPinned(node)) then emit(node) end
				elseif cg.lowerSpecial(node, mem) then
					-- arch-specific passthrough lowering (e.g. x86 cltq -> movslq, lea passthrough)
				elseif loadPtrDefs[node] then
					local dstIv = node.defTemps[1]
					-- an FP/vector (or otherwise unmodeled) destination has no temp:
					-- reject cleanly instead of indexing the lower-temp table with nil.
					if not dstIv then
						error("sarcasm: `load ptr` requires a modeled (GPR) destination"
							.. " register: " .. (node.stmt.raw or "?"))
					end
					local dstLo = pf.lowerTemp[dstIv]
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						-- an EXTERN rip-relative operand materializes through the
						-- getter first (same-file globals ride computeAddr)
						local gEff, gLo = materializeGlobalPtrAccess(node, mem, false)
						emitPtrLoad(node, mem, dstIv, dstLo, gEff, gLo)
						emitWriteback(node, mem)
					end)
					emitRootStore(dstLo)
				elseif storePtrNodes[node] then
					local valOp = storeValueOperand(node.stmt, mem)
					local valIv = valOp and node.opTemp[valOp] or nil
					if not valIv then
						error("sarcasm: `store ptr` requires a modeled (GPR) value"
							.. " register: " .. (node.stmt.raw or "?"))
					end
					local valLo = pf.lowerTemp[valIv]
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						-- extern via the getter, same-file WRITABLE via inline-GP
						-- (forces registration); same-file READONLY traps in the
						-- sequence's CanWrite check
						local gEff, gLo = materializeGlobalPtrAccess(node, mem, true)
						emitPtrStore(node, mem, valIv, valLo, gEff, gLo)
						emitWriteback(node, mem)
					end)
				elseif atomicLoadPtrDefs[node] then
					-- arm64: ldar/ldapr write no flags — preserve the program's
					-- live NZCV around the injected check+call. x86_64: the
					-- `;! atomic load ptr` sequence (check + runtime call)
					-- clobbers EFLAGS the same way, so the bracket applies there
					-- too now that the flag scan is live on both architectures.
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						local gEff, gLo = materializeGlobalPtrAccess(node, mem, false)
						emitAtomicPtrLoad(node, mem, gEff, gLo)
					end)
				elseif atomicStorePtrNodes[node] then
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						local gEff, gLo = materializeGlobalPtrAccess(node, mem, true)
						emitAtomicPtrStore(node, mem, gEff, gLo)
					end)
				elseif atomicCasPtrDefs[node] then
					-- bypasses emitPinned entirely: the runtime call needs no
					-- physical rax pinning (see emitAtomicPtrCas)
					local gEff, gLo = materializeGlobalPtrAccess(node, mem, true)
					emitAtomicPtrCas(node, mem, gEff, gLo)
				elseif loadStorePtrNodes[node] then
					local gEff, gLo = materializeGlobalPtrAccess(node, mem, true)
					emitLoadStorePtr(node, mem, gEff, gLo)
				elseif atomicRmwPtrNodes[node] then
					local gEff, gLo = materializeGlobalPtrAccess(node, mem, true)
					emitAtomicLoadStorePtr(node, mem, gEff, gLo)
				elseif mem then
					cg.checkMemInsn(node, mem)
					local effTemp, capLo = cg.computeAddr(node, mem)
					local size, align = cgm.accessSizeAlign(node.stmt)
					local isWrite = cgm.isStoreInsn(node.stmt)
					-- AVX512 {k}-masked / AVX2 vmaskmov memory operands get the
					-- mask-aware check (x86_64 only; nil everywhere else).
					local minfo = cgm.maskedAccessInfo and cgm.maskedAccessInfo(node.stmt)
					if minfo then
						-- the masked sequence ends at its .Lmaskexec_ join label,
						-- which every non-failing path reaches, so the restore
						-- lands correctly; the save temp stays live across the
						-- internal labels and regalloc handles it.
						withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
							emitMaskedAccessCheck(effTemp, capLo, minfo, align, isWrite)
						end)
					else
						withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
							emitAccessCheck(effTemp, capLo, size, align, isWrite)
						end)
					end
					-- an implicit-GPR-pinned instruction (x86 div/mul/cmpxchg/...)
					-- emits its pin moves around itself; the access check above
					-- rightly precedes the pin-in (checks never touch GPRs)
					if not (cg.emitPinned and cg.emitPinned(node)) then emit(node) end
				else
					if not (cg.emitPinned and cg.emitPinned(node)) then emit(node) end
				end
			end
			-- Lockstep lower maintenance for ptrflow's dynamic-lower webs: each
			-- pointer def of a dynamic web is mirrored by a lower def from the same
			-- source, so a web whose runtime value alternates between pointer
			-- origins (a loop-carried pointer swap, a conflicting join, a cmov
			-- select) keeps its (iv, lower) pairing exact. dynCopy only carries
			-- mov/lea/cmov/xchg defs, all of which were emitted immediately above.
			local dyn = pf.dynCopy and pf.dynCopy[node]
			if dyn then
				if dyn.xchg then
					-- The crossed pair: exchange the two sides' lowers. A source
					-- may alias the other copy's destination (the loop-carried
					-- swap), so read a clobbered source into a scratch first.
					local copies = dyn.copies
					if #copies == 2 then
						local c1, c2 = copies[1], copies[2]
						local s1, s2 = c1.src, c2.src
						if s1 == c2.dst then
							local t = allocTemp({ role = "loswap" }); cg.move(t, s1, true); s1 = t
						end
						if s2 == c1.dst then
							local t = allocTemp({ role = "loswap" }); cg.move(t, s2, true); s2 = t
						end
						cg.move(c1.dst, s1, true)
						cg.move(c2.dst, s2, true)
					elseif #copies == 1 then
						cg.move(copies[1].dst, copies[1].src, true)
					end
				elseif dyn.cmov then
					-- A cmov's lockstep copy must be CONDITIONAL like the iv merge
					-- it mirrors: emit a cmov on the lower temps with the node's
					-- own condition, immediately after the iv cmov — nothing
					-- between the two writes the flags, so the lower cmov reads
					-- exactly the condition the iv cmov consumed. (The rmw shape
					-- — the destination lower is both used and defined — matches
					-- the hardware cmov's.)
					for _, cp in dyn.copies do
						local mnem = node.stmt and node.stmt.mnemonic or "cmov"
						emit(build.insn(mnem, { build.rt(cp.dst), build.rt(cp.src) },
							{ defs = { cp.dst }, uses = { cp.dst, cp.src } }))
					end
				else
					for _, cp in dyn.copies do cg.move(cp.dst, cp.src, true) end
				end
			end
			-- The deferred-idiom size definition lowered above: feed the fresh
			-- alloca-size temp from the instruction's def (after the instruction
			-- itself, so the def is available).
			if sizeMoveAfter then
				cg.move(sizeMoveAfter, node.defTemps[1])
			end
		else
			emit(node)
		end
	end

	if excLabel then
		emit(build.label(excLabel))
		emit({ kind = "epilogue", name = name, useTemps = { vMyth } })
		emit(build.insn("ret", {}, { control = "return", fallsThrough = false }))
	end
	for _, s in failStubs do emit(s) end

	-- FP/SIMD save/restore expansion: cg.fpSave/fpRestore emitted PLACEHOLDER
	-- nodes around the injected returning runtime calls; expand them now into
	-- the exact vector save/restore movs the post-transform width-liveness
	-- analysis proves live at each site. Runs BEFORE the frameSlot numRoots
	-- shift below so the expanded movs' frameSlot operands are shifted with
	-- everything else, and pre-regalloc (the movs carry no temps — regalloc
	-- never sees vector-class registers, and no placeholder node kind ever
	-- leaks past this point). The third argument is the final GC root count
	-- (arm64's hook computes its slot base from it directly; x86_64's hook
	-- ignores it — it tags slots frameSlot and rides the shift below instead).
	if cg.expandFpSaves then cg.expandFpSaves(out, fctx, #rootTemps) end

	-- DETERMINISM: externsList is consumed per-function to build the module-level
	-- callsite-thunk map, whose emission order is fixed in sarcasm.compile; build it
	-- in sorted-callee order so the array (and anything downstream of it) does not
	-- inherit hash iteration order from the string-keyed calledExterns map.
	local externsList = {}
	do
		local calleeNames = {}
		for callee in calledExterns do table.insert(calleeNames, callee) end
		table.sort(calleeNames)
		for _, callee in calleeNames do table.insert(externsList, calledExterns[callee]) end
	end

	-- GC root-slot ABI fixup (x86_64 FP/SIMD): the Fil-C GC reads a frame's roots as
	-- frame->lowers[index] (filc_frame: parent, origin, then lowers[]), so the root
	-- slots MUST occupy frame offsets [16, 16+8*numRoots). The FP/SIMD regions — the
	-- vector-save area, the materialized frame-slot clusters, and the materialized
	-- incoming yolo stack-argument area, all marked frameSlot — were assigned
	-- offsets relative to base 16 by analyzeFrame / preprocess / codegen before
	-- numRoots was known; shift them past the root area (and its 16-byte alignment
	-- pad, matching computeLayout) now. The shift is a multiple of 16, preserving
	-- the clusters' and the yolo area's mod-16 alignment residues (movaps-class
	-- accesses). No-op on arm64 (no frameSlot operands) and whenever there are no
	-- roots.
	do
		local shift = 8 * #rootTemps
		local fpRegion = (fctx and fctx.fp and (fctx.fp.bytes + fctx.fp.saveBytes) or 0)
			+ (fctx and fctx.yolo and fctx.yolo.bytes or 0)
		if fpRegion > 0 and #rootTemps % 2 == 1 then
			shift += 8
		end
		-- Mirror computeLayout's widened pad: the shift must be a multiple of
		-- the EFFECTIVE frame alignment (the cluster-based width the layout
		-- uses: max cluster areq capped at 64, 16 when none needs more) so the
		-- clusters' alignment residues survive it. The raw `and $-N` note max
		-- (fctx.fp.alignReq, uncapped e.g. 1024) must NOT be used here: the
		-- layout pads only to the effective width, so a raw-width shift would
		-- land shifted slots/saveOff outside the laid-out FP region.
		local alignMod = 16
		for _, c in ((fctx and fctx.fp and fctx.fp.clusters) or {}) do
			if c.areq and c.areq > alignMod then alignMod = math.min(c.areq, 64) end
		end
		if alignMod > 16 then
			while (shift % alignMod) ~= 0 do shift += 8 end
		end
		if shift > 0 then
			local seen = {}
			local function shiftOps(ops)
				for _, o in (ops or {}) do
					if o.kind == "mem" and o.frameSlot and not seen[o] then
						seen[o] = true
						o.dispVal = o.dispVal + shift
					end
				end
			end
			for _, node in out do
				shiftOps(node.operands)
				if node.stmt then shiftOps(node.stmt.operands) end
			end
		end
	end

	return {
		name = name, sigDesc = sigDesc, fipNodes = out, temps = temps, vMyth = vMyth,
		originSym = originSym, numRoots = #rootTemps, funcLine = funcLine, nextTemp = nextTemp,
		rootTemps = rootTemps, accessOrigins = accessOrigins, calledExterns = externsList,
		lf = lf, pf = pf,
		-- Fixed colors of the function's implicit-counter webs (empty unless the
		-- body contains loop/loopl/jrcxz/jecxz): the allocator removes these from
		-- its color pool and never coalesces into them, so nothing else can ever
		-- occupy the counter's physical register (see regalloc.luau's `reserved`).
		reservedColors = reservedColors,
		-- FP/SIMD frame-slot region sizes for the layout (0 on arm64 / when no FP
		-- stack slot was materialized).
		fpBytes = (fctx and fctx.fp and fctx.fp.bytes) or 0,
		fpSaveBytes = (fctx and fctx.fp and fctx.fp.saveBytes) or 0,
		-- materialized incoming yolo stack-argument area size for the layout
		-- (x86_64 only; 0 when no FP instruction reads an incoming stack argument).
		yoloBytes = (fctx and fctx.yolo and fctx.yolo.bytes) or 0,
		-- Effective frame alignment (x86_64 only): the widest alignment any
		-- materialized cluster needs (16 when none needs more). The taint scan
		-- proved each such cluster's input offsets aligned under an
		-- `and $-N, %rsp` note; the layout dynamically aligns the synthesized
		-- frame to this width so they stay aligned in the output.
		frameAlignReq = (function()
			local effA = 16
			for _, c in ((fctx and fctx.fp and fctx.fp.clusters) or {}) do
				if c.areq and c.areq > effA then effA = math.min(c.areq, 64) end
			end
			return effA
		end)(),
	}
end

return transform
