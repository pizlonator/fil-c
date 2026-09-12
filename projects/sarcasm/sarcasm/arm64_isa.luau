--!nonstrict
-- ARM64 instruction semantics: classify a parsed instruction into register
-- defs/uses and control-flow behavior. Register operands are reported as the
-- reg operand tables produced by parse.luau (with .num/.width/.name).

local arm64 = {}

-- Data-processing mnemonics whose first operand is the destination (def) and the
-- rest are sources (uses).
local DST_FIRST = {}
for _, m in {
	"mov","movz","mvn","neg","negs","add","adds","sub","subs","adc","adcs","sbc","sbcs",
	"and","ands","orr","orn","eor","eon","bic","bics",
	"lsl","lsr","asr","ror","lslv","lsrv","asrv","rorv",
	"mul","madd","msub","mneg","smull","umull","smulh","umulh","sdiv","udiv",
	"sxtw","sxtb","sxth","uxtw","uxtb","uxth","sxtl","uxtl","clz","cls","rbit","rev","rev16","rev32",
	"adr","adrp","csel","csinc","csinv","csneg","cinc","cinv","cneg","cset","csetm",
	"ubfx","sbfx","ubfm","sbfm","bfi","bfxil","extr","lsl.","abs",
	-- FP/SIMD -> GPR conversions and moves: the GPR (if any) is the destination.
	"fcvtzs","fcvtzu","fcvtas","fcvtau","fcvtms","fcvtmu","fcvtns","fcvtnu",
	"fcvtps","fcvtpu","fjcvtzs","umov","smov","mrs","sysl",
} do DST_FIRST[m] = true end

-- Instructions whose GPR operands are ALL uses (never defs): GPR -> FP/SIMD conversions
-- (scvtf/ucvtf have an FP destination), vector splat (dup), and system-register writes
-- (msr) / cache+system ops whose GPR is an address or value argument (dc/ic/tlbi/at/sys).
-- Without this list the unknown-mnemonic fallback would treat the first GPR as a def and
-- sever its register web (miscompile: the producer's value never reaches the instruction).
local GPR_ALL_USES = {}
for _, m in { "scvtf", "ucvtf", "dup", "msr", "dc", "ic", "tlbi", "at", "sys" } do GPR_ALL_USES[m] = true end

-- movk / bfi / bfxil read-modify-write the destination.
local DST_RMW = { movk=true, bfi=true, bfxil=true, ins=true }

-- Flags-only (no GPR destination); all reg operands are uses.
local FLAGS_ONLY = { cmp=true, cmn=true, tst=true, ccmp=true, ccmn=true }

-- Loads: first operand is a destination.
local LOADS = {
	ldr=true, ldrb=true, ldrh=true, ldrsb=true, ldrsh=true, ldrsw=true,
	ldur=true, ldurb=true, ldurh=true, ldursb=true, ldursh=true, ldursw=true,
	ldar=true, ldarb=true, ldarh=true, ldapr=true, ldxr=true, ldaxr=true,
	-- RCpc2 (unscaled) acquire loads and sub-word exclusive loads.
	ldaprb=true, ldaprh=true, ldapur=true, ldapurb=true, ldapurh=true,
	ldapursb=true, ldapursh=true, ldapursw=true,
	ldxrb=true, ldaxrb=true, ldxrh=true, ldaxrh=true,
	-- NEON single-/multi-structure loads (full-vector, single-element, and the
	-- structure-replicate forms). The data registers are NEON (never webbed);
	-- only the memory operand's GPRs are modeled (base use, writeback RMW).
	ld1=true, ld2=true, ld3=true, ld4=true,
	ld1r=true, ld2r=true, ld3r=true, ld4r=true,
}
local LOAD_PAIR = { ldp=true, ldpsw=true, ldnp=true, ldaxp=true, ldxp=true }

-- Stores: first operand is a source.
local STORES = {
	str=true, strb=true, strh=true, stur=true, sturb=true, sturh=true,
	stlr=true, stlrb=true, stlrh=true,
	-- NEON single-/multi-structure stores (no replicate store forms exist).
	st1=true, st2=true, st3=true, st4=true,
}
local STORE_PAIR = { stp=true, stnp=true }

-- LSE atomics (ARMv8.1) and the store side of the LL/SC family: single-instruction
-- memory RMWs sarcasm models as plain checked accesses (bounds + CanWrite + NATURAL
-- alignment — the hardware faults on unaligned atomics). mnemonic -> kind:
--   "swp" — swp<a|l|al><b|h> <src>, <dst old>, [<Xn>]: use ops[1] (stored
--           source), def ops[2] (old memory value), memory RMW.
--   "ld"  — ld<op><a|l|al><b|h> <src>, <dst old>, [<Xn>] (op = add/clr/eor/set/
--           smax/smin/umax/umin): use ops[1], def ops[2] (old value), memory RMW.
--   "st"  — st<op><l><b|h> <src>, [<Xn>] (the no-return-value aliases of the ld
--           forms): use ops[1], memory RMW, no register def.
--   "cas" — cas<a|l|al><b|h> <cmp>, <src>, [<Xn>]: the compare register is an
--           architectural read-modify-write (read for the compare, receives the
--           old memory value), modeled with the pin mechanism (implicit use+def
--           operand tables) so regalloc keeps use and def in the physical
--           register; src is a use.
--   "casp"— casp<a|l|al> <cmp>, <cmp+1>, <src>, <src+1>, [<Xn>]: compare PAIR
--           (pin, two implicit RMWs) + data PAIR (pinned uses — the hardware
--           encodes each pair as one even register number, so all four explicit
--           registers must render at their written physical numbers).
--   "stxr"/"stxp" — store-exclusive: status-register DEF (wN result), value
--           source(s), memory write (stxr/stlxr + b/h; stxp/stlxp pairs).
-- (The load side — ldxr/ldaxr + b/h, ldxp/ldaxp — rides the plain LOADS/
-- LOAD_PAIR tables above: a checked load needs nothing more.)
local LSE_KIND = {}
for _, v in { "", "a", "l", "al" } do
	for _, w in { "", "b", "h" } do
		LSE_KIND["swp" .. v .. w] = "swp"
		LSE_KIND["cas" .. v .. w] = "cas"
	end
	LSE_KIND["casp" .. v] = "casp"
end
for _, b in { "ldadd", "ldclr", "ldeor", "ldset", "ldsmax", "ldsmin", "ldumax", "ldumin" } do
	for _, v in { "", "a", "l", "al" } do
		for _, w in { "", "b", "h" } do
			LSE_KIND[b .. v .. w] = "ld"
		end
	end
end
for _, b in { "stadd", "stclr", "steor", "stset", "stsmax", "stsmin", "stumax", "stumin" } do
	for _, v in { "", "l" } do
		for _, w in { "", "b", "h" } do
			LSE_KIND[b .. v .. w] = "st"
		end
	end
end
for _, m in { "stxr", "stlxr", "stxrb", "stlxrb", "stxrh", "stlxrh" } do LSE_KIND[m] = "stxr" end
LSE_KIND["stxp"] = "stxp"
LSE_KIND["stlxp"] = "stxp"
arm64.LSE_KIND = LSE_KIND

local COND_BRANCH_ALIASES = {
	beq=true,bne=true,bcs=true,bhs=true,bcc=true,blo=true,bmi=true,bpl=true,
	bvs=true,bvc=true,bhi=true,bls=true,bge=true,blt=true,bgt=true,ble=true,bal=true,
}

local function isGpr(op) return op and op.kind == "reg" and op.rc == "gpr" end

-- First operand that cannot be a label (a register of any kind — xzr/sp included —
-- or anything else non-symbolic), or nil. Used to detect a REGISTER branch target:
-- `b xN`, `cbz xN, xM`, `b.eq xM`, ... A branch target that is not a label is an
-- indirect branch no matter what register class it names.
local function firstNonSym(ops)
	for _, o in ops do
		if o.kind ~= "sym" then return o end
	end
	return nil
end

-- Pointer-authentication instructions (pacia*/pacib*/paciza*/pacizb* and the
-- paciasp/pacialr/pacib1716/... HINT-space aliases, the pacd*/autd* data-pointer
-- siblings, pacga, autia*/autib*/... aliases, xpaci/xpacd/xpaclri) implicitly
-- read+write their operand (x30 for the return-address forms) with a cryptographically
-- authenticated value this model cannot represent: they fall into the unknown-mnemonic
-- fallback, which models ZERO register effects and silently disconnects the register
-- web (x30's, for paciasp/autiasp). clang emits paciasp/autiasp under
-- -mbranch-protection, so reject them cleanly rather than miscompile. Matched by
-- prefix (no non-pauth mnemonic starts with any of these) so every sibling form,
-- current and future, is covered.
local PAUTH_PREFIXES = { "paci", "pacd", "pacg", "auti", "autd", "xpac" }
local function isPauth(m)
	for _, p in PAUTH_PREFIXES do
		if string.sub(m, 1, #p) == p then return true end
	end
	return false
end

-- SVE/SVE2 is out of scope (NEON/AdvSIMD only). SVE registers parse as rc="sve"
-- (z0-31, p0-15, all qualification forms), and a handful of SVE-only mnemonics
-- take no SVE register operand at all (rdvl/addvl/inc*/dec*/cnt*/while*/ptrue/
-- rdffr/...); both are rejected uniformly here, at the same choke point as the
-- pauth rejection. (A label literally named z0/p0 would false-positive; such
-- names are pathological and not generated by compilers.)
local SVE_MNEMONICS = {}
for _, m in {
	"rdvl", "addvl", "addpl",
	"incb", "incd", "inch", "incw", "incp", "decb", "decd", "dech", "decw", "decp",
	"cntb", "cntd", "cnth", "cntw",
	"whilege", "whilegt", "whilehi", "whilehs", "whilele", "whilelo", "whilels", "whilelt",
	"ptrue", "ptrues", "pfalse", "rdffr", "rdffrs", "setffr", "wrffr",
	"ctermeq", "ctermne",
} do SVE_MNEMONICS[m] = true end

local function checkSve(insn)
	local m = string.lower(insn.mnemonic)
	if SVE_MNEMONICS[m] then
		error("sarcasm: SVE is not yet supported (NEON/AdvSIMD only): " .. (insn.raw or m))
	end
	local function rcIsSve(o) return o and o.kind == "reg" and o.rc == "sve" end
	for _, o in (insn.operands or {}) do
		if rcIsSve(o) then
			error("sarcasm: SVE is not yet supported (NEON/AdvSIMD only): " .. (insn.raw or m))
		end
		if o.kind == "reglist" and o.regs[1] and o.regs[1].rc == "sve" then
			error("sarcasm: SVE is not yet supported (NEON/AdvSIMD only): " .. (insn.raw or m))
		end
		if o.kind == "mem" and (rcIsSve(o.base) or rcIsSve(o.index)) then
			error("sarcasm: SVE is not yet supported (NEON/AdvSIMD only): " .. (insn.raw or m))
		end
	end
end

-- svc/hvc/smc enter an exception level with RAW arguments, bypassing Fil-C's syscall
-- argument validation (a genuine memory-safety hole), and have unmodeled control and
-- clobber effects. hlt is privileged. brk/udf stay allowed: they are pure traps.
local UNSUPPORTED_SYSCALL = { svc=true, hvc=true, smc=true }
local UNSUPPORTED_PRIV = { hlt=true }

-- Collect reg uses from a memory operand (base + index), and its writeback def.
-- Writeback bases are recorded in `rmw` (they must occupy one register for use+def).
local function memRegs(op, uses, defs, rmw)
	if op.base and isGpr(op.base) then table.insert(uses, op.base) end
	if op.index and isGpr(op.index) then table.insert(uses, op.index) end
	if op.writeback and op.base and isGpr(op.base) then
		table.insert(defs, op.base)  -- writeback updates base
		if rmw then table.insert(rmw, op.base.num) end
	end
end

-- Classify. Returns:
--   { defs = {regop...}, uses = {regop...},
--     control = "none"|"branch"|"condbranch"|"call"|"return"|"indirect"|"indcall",
--     targets = {label...}, fallsThrough = bool, isMove = {dstIdx,srcIdx} | nil }
function arm64.classify(insn)
	local m = string.lower(insn.mnemonic)
	checkSve(insn)
	if isPauth(m) then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (pointer-authentication instructions have implicit register effects"
			.. " that cannot be modeled)")
	end
	if UNSUPPORTED_SYSCALL[m] then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (a raw supervisor call bypasses Fil-C's syscall argument validation)")
	end
	if UNSUPPORTED_PRIV[m] then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (privileged instructions cannot be made memory-safe)")
	end
	local ops = insn.operands or {}
	local defs, uses = {}, {}
	local rmw = {}  -- regnums that need use+def in the same register (writeback / merge ops)
	local control, targets, fallsThrough = "none", nil, true
	local isMove = nil

	-- `.alloca size, alignment, result` (the `.alloca` directive): operands are in
	-- WRITTEN order. size/alignment: an immediate, a 32/64-bit GPR (including
	-- `fil_` pseudos and frame-slot webs), or a frame-relative spill-slot memory
	-- reference (converted to a slot web by the frame rewrite — provisionally
	-- unmodeled here). result: a 64-bit GPR, a pseudo, or a spill slot. Anything
	-- else (NEON/SVE/zr/sp registers, symbols, heap memory, shifted or writeback
	-- operands, non-64-bit result widths) is a clean rejection.
	if m == "alloca" then
		if not insn.dotAlloca then
			error("sarcasm: `alloca` is only supported as the `.alloca size, alignment, result` directive: " .. (insn.raw or m))
		end
		if #ops ~= 3 then
			error("sarcasm: `.alloca` takes exactly 3 operands (size, alignment, result): " .. (insn.raw or m))
		end
		local function allocaUse(o, what)
			if o.shift then
				error("sarcasm: `.alloca` " .. what .. " has a shift/extend, which is not supported: " .. (insn.raw or m))
			end
			if o.kind == "imm" then return end
			if isGpr(o) then
				if o.width ~= 64 and o.width ~= 32 then
					error("sarcasm: `.alloca` " .. what .. " must be a 32/64-bit register: " .. (insn.raw or m))
				end
				table.insert(uses, o)
				return
			end
			if o.kind == "mem" and not o.writeback and not o.shift and not o.postShift
				and o.base and (o.base.name == "sp" or o.base.name == "x29")
				and (not o.index or o.index.kind == "imm") then return end
			error("sarcasm: `.alloca` " .. what .. " must be an immediate, a GPR (including a pseudoregister), or a frame spill slot: " .. (insn.raw or m))
		end
		allocaUse(ops[1], "size")
		allocaUse(ops[2], "alignment")
		do
			local o = ops[3]
			if o.shift then
				error("sarcasm: `.alloca` result has a shift/extend, which is not supported: " .. (insn.raw or m))
			end
			if isGpr(o) then
				if o.width ~= 64 then
					error("sarcasm: `.alloca` result must be a 64-bit register (a narrower destination would truncate the pointer): " .. (insn.raw or m))
				end
				table.insert(defs, o)
			elseif o.kind == "mem" and not o.writeback and not o.shift and not o.postShift
				and o.base and (o.base.name == "sp" or o.base.name == "x29")
				and (not o.index or o.index.kind == "imm") then
			-- spill-slot result: converted by the frame rewrite; provisionally unmodeled.
			else
				error("sarcasm: `.alloca` result must be a 64-bit register, a pseudoregister, or a frame spill slot: " .. (insn.raw or m))
			end
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none",
			targets = targets, fallsThrough = fallsThrough }
	end

	-- Control flow first. A branch whose TARGET is not a label (`br xN`, `b xN`,
	-- `cbz xN, xM`, `tbz xN, #i, xM`, `b.eq xM`, ...) is an INDIRECT branch: its
	-- target is not a provable label, so it classifies exactly like `br` and the
	-- shared body validation rejects it (a raw register branch cannot be made
	-- memory-safe). Label-target forms are unaffected. (Only `br`/`blr`/`ret` can
	-- take register targets in the ISA; every other branch form is unconditional
	-- about labels — an operand there is garbage that must never render verbatim.)
	if m == "b" then
		local regTarget = firstNonSym(ops)
		if regTarget then
			control, fallsThrough = "indirect", false
			if isGpr(regTarget) then table.insert(uses, regTarget) end
		else
			control, fallsThrough = "branch", false
			targets = {}
			for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		end
	elseif m == "br" then
		control, fallsThrough = "indirect", false
		if isGpr(ops[1]) then table.insert(uses, ops[1]) end
	elseif m == "bl" then
		control = "call"
	elseif m == "blr" then
		control = "indcall"
		if isGpr(ops[1]) then table.insert(uses, ops[1]) end
	elseif m == "ret" then
		-- `ret` (i.e. ret x30) is the only provable return; an explicit non-x30 register
		-- is an indirect return whose target we cannot prove, so reject it instead of
		-- silently dropping the operand (which would return to x30 regardless).
		if isGpr(ops[1]) and ops[1].num ~= 30 then
			error("sarcasm: ret with an explicit register other than x30 (indirect return) "
				.. "is not supported: " .. (insn.raw or "ret " .. (ops[1].name or "?")))
		end
		control, fallsThrough = "return", false
	elseif m == "cbz" or m == "cbnz" then
		-- operand 2 is the branch target.
		local regTarget = (ops[2] and ops[2].kind ~= "sym") and ops[2] or nil
		if regTarget then
			control = "indirect"   -- conditional; fallsThrough stays true (rejected before use)
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			if isGpr(regTarget) then table.insert(uses, regTarget) end
		else
			control = "condbranch"
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			targets = {}
			for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		end
	elseif m == "tbz" or m == "tbnz" then
		-- operands are (reg, bit, target): operand 3 is the branch target.
		local regTarget = (ops[3] and ops[3].kind ~= "sym") and ops[3] or nil
		if regTarget then
			control = "indirect"   -- conditional; fallsThrough stays true (rejected before use)
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			if isGpr(regTarget) then table.insert(uses, regTarget) end
		else
			control = "condbranch"
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			targets = {}
			for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		end
	elseif string.sub(m, 1, 2) == "b." or COND_BRANCH_ALIASES[m] then
		local regTarget = firstNonSym(ops)
		if regTarget then
			-- a conditional branch with a non-label target is not even encodable, but
			-- classify it like any other register branch so it can never slip through
			-- as a label-less (i.e. unvalidated) condbranch.
			control = "indirect"
			if isGpr(regTarget) then table.insert(uses, regTarget) end
		else
			control = "condbranch"
			targets = {}
			for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		end
	end

	if control == "call" or control == "indcall" then
		-- caller-saved clobber is modeled by the transform/regalloc, not here.
		return { defs = defs, uses = uses, control = control, targets = targets,
			fallsThrough = fallsThrough, isMove = isMove }
	end
	if control ~= "none" then
		return { defs = defs, uses = uses, control = control, targets = targets,
			fallsThrough = fallsThrough, isMove = isMove }
	end

	-- Non-control instructions.
	if FLAGS_ONLY[m] then
		for _, o in ops do
			if isGpr(o) then table.insert(uses, o) elseif o.kind == "mem" then memRegs(o, uses, defs, rmw) end
		end
	elseif LOADS[m] then
		if isGpr(ops[1]) then table.insert(defs, ops[1]) end
		for i = 2, #ops do
			local o = ops[i]
			if o.kind == "mem" then memRegs(o, uses, defs, rmw)
			elseif isGpr(o) then table.insert(uses, o) end
		end
	elseif LOAD_PAIR[m] then
		if isGpr(ops[1]) then table.insert(defs, ops[1]) end
		if isGpr(ops[2]) then table.insert(defs, ops[2]) end
		for i = 3, #ops do
			local o = ops[i]
			if o.kind == "mem" then memRegs(o, uses, defs, rmw)
			elseif isGpr(o) then table.insert(uses, o) end
		end
	elseif STORES[m] then
		if isGpr(ops[1]) then table.insert(uses, ops[1]) end
		for i = 2, #ops do
			local o = ops[i]
			if o.kind == "mem" then memRegs(o, uses, defs, rmw)
			elseif isGpr(o) then table.insert(uses, o) end
		end
	elseif STORE_PAIR[m] then
		if isGpr(ops[1]) then table.insert(uses, ops[1]) end
		if isGpr(ops[2]) then table.insert(uses, ops[2]) end
		for i = 3, #ops do
			local o = ops[i]
			if o.kind == "mem" then memRegs(o, uses, defs, rmw)
			elseif isGpr(o) then table.insert(uses, o) end
		end
	elseif GPR_ALL_USES[m] then
		-- GPR -> FP/SIMD conversions (scvtf/ucvtf), vector splat (dup), sysreg writes
		-- (msr) and cache/sys ops: every GPR operand is a source.
		for _, o in ops do
			if isGpr(o) then table.insert(uses, o) elseif o.kind == "mem" then memRegs(o, uses, defs, rmw) end
		end
	elseif m == "fmov" then
		-- fmov moves bits between the GPR and FP/SIMD domains in either direction
		-- (fmov x0, d0 / fmov d0, x0 / fmov w0, s0 / fmov s0, w0 / fmov x0, v0.d[1] /
		-- fmov v0.d[1], x0): a GPR destination (operand 1) is a def, every other GPR
		-- operand is a use.
		if isGpr(ops[1]) then table.insert(defs, ops[1]) end
		for i = 2, #ops do if isGpr(ops[i]) then table.insert(uses, ops[i]) end end
	elseif DST_RMW[m] then
		if isGpr(ops[1]) then table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num) end
		for i = 2, #ops do if isGpr(ops[i]) then table.insert(uses, ops[i]) end end
	elseif LSE_KIND[m] then
		-- LSE/LLSC atomics (see the LSE_KIND table comment above). The memory
		-- operand of these instructions is always a bare [base] (no offset, index
		-- or writeback exists in their encodings); a misplaced/missing one is a
		-- malformed instruction — reject cleanly rather than mis-model.
		local kind = LSE_KIND[m]
		local function memAt(idx)
			local o = ops[idx]
			if not o or o.kind ~= "mem" then
				error("sarcasm: '" .. insn.mnemonic .. "' is malformed (expected register"
					.. " operands followed by one [base] memory operand): " .. (insn.raw or m))
			end
			return o
		end
		if kind == "swp" then
			-- SWP <Ws>, <Wt>, [<Xn>]: Ws is the stored source (use), Wt receives
			-- the old memory value (def).
			if isGpr(ops[2]) then table.insert(defs, ops[2]) end
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			memRegs(memAt(3), uses, defs, rmw)
		elseif kind == "ld" then
			if isGpr(ops[2]) then table.insert(defs, ops[2]) end
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			memRegs(memAt(3), uses, defs, rmw)
		elseif kind == "st" then
			if isGpr(ops[1]) then table.insert(uses, ops[1]) end
			memRegs(memAt(2), uses, defs, rmw)
		elseif kind == "stxr" then
			if isGpr(ops[1]) then table.insert(defs, ops[1]) end  -- status result
			if isGpr(ops[2]) then table.insert(uses, ops[2]) end
			memRegs(memAt(3), uses, defs, rmw)
		elseif kind == "stxp" then
			if isGpr(ops[1]) then table.insert(defs, ops[1]) end  -- status result
			if isGpr(ops[2]) then table.insert(uses, ops[2]) end
			if isGpr(ops[3]) then table.insert(uses, ops[3]) end
			memRegs(memAt(4), uses, defs, rmw)
		else
			-- "cas" / "casp": the compare register (pair) is an architectural
			-- read-modify-write (read for the compare, written with the old
			-- memory value), modeled with the pin mechanism: implicit use+def
			-- operand tables pinned to the written physical register(s) by
			-- codegen's emitPinned, exactly like x86's cmpxchg accumulator.
			-- (Fresh table per implicit occurrence — the lift keys node.opTemp
			-- by table identity, so a def table and a use table for the same
			-- register must never be shared.) The casp data pair is pinned too
			-- (use-only): the hardware encodes each pair as one even register
			-- number, so all four explicit registers must render at their
			-- written physical numbers.
			local pinUses, pinDefs = {}, {}
			local ncmp = (kind == "cas") and 1 or 2
			for i = 1, ncmp do
				local o = ops[i]
				if isGpr(o) then
					local cU = { kind="reg", rc="gpr", num=o.num, width=o.width, name=o.name, implicit=true }
					local cD = { kind="reg", rc="gpr", num=o.num, width=o.width, name=o.name, implicit=true }
					table.insert(uses, cU); table.insert(pinUses, cU)
					table.insert(defs, cD); table.insert(pinDefs, cD)
				end
			end
			local ndat = (kind == "cas") and 1 or 2
			for i = 1, ndat do
				local o = ops[ncmp + i]
				if isGpr(o) then
					if kind == "casp" then
						local dU = { kind="reg", rc="gpr", num=o.num, width=o.width, name=o.name, implicit=true }
						table.insert(uses, dU); table.insert(pinUses, dU)
					else
						table.insert(uses, o)
					end
				end
			end
			memRegs(memAt(ncmp + ndat + 1), uses, defs, rmw)
			local pin = nil
			if #pinUses > 0 then pin = { uses = pinUses, defs = pinDefs } end
			return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
				pin = pin }
		end
	elseif DST_FIRST[m] then
		if isGpr(ops[1]) then table.insert(defs, ops[1]) end
		for i = 2, #ops do
			local o = ops[i]
			if isGpr(o) then table.insert(uses, o) elseif o.kind == "mem" then memRegs(o, uses, defs, rmw) end
		end
		if m == "mov" and isGpr(ops[1]) and isGpr(ops[2]) and #ops == 2 and not ops[2].shift then
			isMove = { dstIdx = 1, srcIdx = 2 }
		end
	else
		-- Unknown mnemonic: conservatively treat first reg as def, rest as uses.
		local first = true
		for _, o in ops do
			if isGpr(o) then
				if first then table.insert(defs, o); first = false else table.insert(uses, o) end
			elseif o.kind == "mem" then memRegs(o, uses, defs, rmw) end
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			isMove = isMove, unknown = true }
	end

	return { defs = defs, uses = uses, rmw = rmw, control = control, targets = targets,
		fallsThrough = fallsThrough, isMove = isMove }
end

-- True if `mnemonic` is a load/store that sarcasm recognizes and models (and can
-- therefore bounds-check). A memory operand on any other mnemonic cannot be proven
-- memory-safe and must be rejected. The LSE/LLSC atomics count: they are modeled
-- single-instruction memory accesses (bounds + CanWrite + natural alignment).
function arm64.isMemInsn(mnemonic)
	local m = string.lower(mnemonic)
	return (LOADS[m] or STORES[m] or LOAD_PAIR[m] or STORE_PAIR[m] or LSE_KIND[m]) and true or false
end

arm64.isGpr = isGpr

-- NZCV condition-flag classification, used for flag liveness at instrumentation points
-- (the transform brackets its cmp/tst-emitting sequences with mrs/msr nzcv when a flag
-- consumer can reach them). A "use" reads NZCV; a "def" writes NZCV without reading
-- it. Instructions that read AND write (ccmp/ccmn, fccmp/fccmpe, adcs/sbcs) count as
-- uses for liveness (the incoming value matters). Everything else is flag-neutral.
local FLAG_USE = {}
for _, m in {
	"csel","csinc","csinv","csneg","cinc","cinv","cneg","cset","csetm","fcsel",
	"adc","sbc",          -- carry-in, no flag write
	"adcs","sbcs",        -- carry-in AND flags written (the read makes it a use) —
	                      -- LOAD-BEARING: dropping these loses the mrs/msr bracket on an
	                      -- instrumented load between a cmp and the adcs/sbcs (pinned by
	                      -- filc/tests/sarcasm-adcs-arm)
	"ccmp","ccmn",        -- read the condition, then write NZCV
	"fccmp","fccmpe",     -- conditional FP compares: documentation/completeness — not
	                      -- load-bearing (any consumer of fccmp's NZCV is itself a flagUse)
} do FLAG_USE[m] = true end

local FLAG_DEF = {}
for _, m in {
	"cmp","cmn","tst","adds","subs","ands","bics","negs",
	"fcmp","fcmpe","adcs","sbcs","ccmp","ccmn",
	"fccmp","fccmpe",     -- like FLAG_USE above: documentation/completeness — not
	                      -- load-bearing (any consumer of fccmp's NZCV is itself a flagUse)
} do FLAG_DEF[m] = true end

-- True iff `op` is the nzcv system-register name operand of an mrs/msr.
local function isNzcvOp(op)
	return op and op.kind == "sym" and string.lower(op.text or "") == "nzcv"
end

function arm64.flagUse(stmt)
	local m = string.lower(stmt.mnemonic)
	if FLAG_USE[m] then return true end
	-- mrs xN, nzcv reads the condition flags (mrs of any other sysreg is flag-neutral).
	if m == "mrs" and isNzcvOp((stmt.operands or {})[2]) then return true end
	-- b.cond (and the beq/bne/... aliases) read NZCV; cbz/cbnz/tbz/tbnz test a
	-- register and are flag-neutral.
	if string.sub(m, 1, 2) == "b." or COND_BRANCH_ALIASES[m] then return true end
	return false
end

function arm64.flagDef(stmt)
	local m = string.lower(stmt.mnemonic)
	if FLAG_DEF[m] then return true end
	-- msr nzcv, xN writes all of NZCV, so incoming flags are dead across it (msr of
	-- any other sysreg is flag-neutral).
	if m == "msr" and isNzcvOp((stmt.operands or {})[1]) then return true end
	return false
end

-- AArch64 has no partial flag writes: NZCV is written all-or-nothing by every
-- flag-setting instruction, and each of them is a full def (FLAG_DEF above). The
-- flag-reading-but-not-writing family (csel/cset/.../adc/sbc/mrs/b.cond) writes
-- nothing, so deleting one can never change any flags. Dead-code elimination
-- therefore needs no complement to flagUse/flagDef here — flagWriteNoDef is always
-- false. (FlagM's cfinv/rmif/setf* are not modeled at all: they fall into the
-- unknown-mnemonic classification, which DCE never deletes.)
function arm64.flagWriteNoDef(stmt) return false end

-- Coarse per-flag masks for the shared liveness fixpoint: AArch64 writes NZCV
-- all-or-nothing (no partial writes), so use reads everything and def kills
-- everything. Bit values are arbitrary distinct bits.
function arm64.flagUseMask(stmt) if arm64.flagUse(stmt) then return 15 end return 0 end
function arm64.flagDefMask(stmt) if arm64.flagDef(stmt) then return 15 end return 0 end

-- NEON width-liveness effects (arm64_codegen.expandFpSaves) -------------------
-- Mirrors x86_64_fp.vecEffects: per vector register NUMBER (0-31), report a USE
-- width in bytes (the instruction reads [0, w)) and a DEF "provided width" (the
-- instruction supplies [0, p); bytes above p flow through from before).
-- Correctness contract (the save/restore can only be wrong by UNDER-saving): it
-- is ALWAYS safe to over-approximate a use width and to report providedWidth 0;
-- NEVER safe to under-approximate a use or to overstate a provided width.
-- Unrecognized mnemonics get exactly that safe default. A64 def semantics make
-- this simple: every scalar (b/h/s/d) or 64-bit-vector write ZEROES the
-- register's upper bits and every 128-bit write covers the register, so a full
-- def always provides 16; only lane-insert forms (ins, element fmov/mov, the
-- single-element structure loads) merge, and those stay on provided 0.

local ELEM_BYTES = { b = 1, h = 2, s = 4, d = 8, q = 16 }
arm64.ELEM_BYTES = ELEM_BYTES

-- Byte width a NEON register operand is READ at: arrangement (lanes*elem) if
-- present, 16 for element references (they flow through the whole register),
-- else the bare-name scalar width (b/h/s/d -> 1/2/4/8, q/v -> 16).
local function neonOpWidth(o)
	if o.arrangement then
		local lanes = o.arrangement.lanes
		if lanes then return lanes * (ELEM_BYTES[o.arrangement.elem] or 16) end
		return 16
	end
	if o.element then return 16 end
	local p = string.sub(o.name or "", 1, 1)
	return ELEM_BYTES[p] or 16
end
arm64.neonOpWidth = neonOpWidth

-- Accumulate/merge mnemonics whose destination is ALSO read (at its own width):
-- fmla/fmls, integer mla/mls, shift-inserts (sli/sri), table merge (tbx), the
-- bitwise select/inserts, the long/widening multiply-accumulates, dot products,
-- abs-diff accumulates, saturating doubling multiply-accumulates. Over-listing
-- here is the safe direction (it over-preserves). SHA/SM3/SM4 crypto is NOT
-- listed and rides the safe default instead (all operands used, no defs).
local VEC_DST_READ = {}
for _, m in {
	"fmla", "fmls", "mla", "mls", "sli", "sri", "ssra", "usra",
	"tbx", "bsl", "bit", "bif",
	"saba", "uaba", "sabal", "sabal2", "uabal", "uabal2",
	"sadalp", "uadalp",
	"smlal", "smlal2", "umlal", "umlal2", "smlsl", "smlsl2", "umlsl", "umlsl2",
	"sqdmlal", "sqdmlal2", "sqdmlsl", "sqdmlsl2",
	"sqrdmlah", "sqrdmlsh",
	"fmlal", "fmlal2", "fmlsl", "fmlsl2",
	"sdot", "udot", "usdot",
	-- AESE/AESD take the round state in their DESTINATION (vaeseq_u8(data, key)
	-- lowers to `aese vD, vK`): the destination is read and rewritten. (The
	-- MixColumns forms aesmc/aesimc are pure single-source and stay pure-write.)
	"aese", "aesd",
	-- saturating accumulates: Vd = sat(Vd + Vn).
	"suqadd", "usqadd",
} do VEC_DST_READ[m] = true end

-- Pure-write mnemonics: destination = f(sources), never read, and (like every
-- full A64 NEON/FP def) provided at 16 bytes. ONLY entries certain to never
-- read their destination belong here — an over-list is an under-save. Anything
-- not listed gets the safe default (all NEON operands used, no defs), so this
-- list is an optimization, not a correctness requirement. Compare/flag forms
-- (fcmp/fcmpe/fccmp/fccmpe) are deliberately absent: they write FLAGS, not
-- their first operand — listing them would sever liveness through that register.
local VEC_PURE_WRITE = {}
for _, m in {
	-- integer vector arithmetic / logic / shifts
	"add", "sub", "mul", "neg", "abs", "sqneg", "sqabs",
	"and", "orr", "eor", "bic", "orn", "eon", "mvn",
	"shl", "sshr", "ushr", "srshr", "urshr", "srshl", "urshl", "sshl", "ushl",
	"sqshl", "uqshl", "sqrshl", "uqrshl", "sqshlu",
	-- widening / narrowing
	"sshll", "sshll2", "ushll", "ushll2", "sxtl", "sxtl2", "uxtl", "uxtl2",
	"shrn", "shrn2", "rshrn", "rshrn2", "xtn", "xtn2",
	"sqxtn", "sqxtn2", "uqxtn", "uqxtn2", "sqxtun", "sqxtun2",
	"saddl", "saddl2", "uaddl", "uaddl2", "ssubl", "ssubl2", "usubl", "usubl2",
	"saddw", "saddw2", "uaddw", "uaddw2", "ssubw", "ssubw2", "usubw", "usubw2",
	"smull", "smull2", "umull", "umull2", "sqdmull", "sqdmull2", "pmull", "pmull2", "pmul",
	-- halving / rounding / saturating arith, min/max
	"shadd", "uhadd", "srhadd", "urhadd", "rhadd", "shsub", "uhsub",
	"sqadd", "uqadd", "sqsub", "uqsub",
	"smin", "umin", "smax", "umax", "sminp", "uminp", "smaxp", "umaxp",
	-- pairwise / reductions
	"addp", "addv", "saddlv", "uaddlv", "saddlp", "uaddlp",
	"sminv", "uminv", "smaxv", "umaxv",
	-- compares (vector result), bit counting, reversals
	"cmeq", "cmge", "cmgt", "cmhi", "cmhs", "cmle", "cmlt", "cmtst",
	"cnt", "clz", "cls", "rbit", "rev16", "rev32", "rev64",
	-- moves / permutes / table
	"movi", "mvni", "dup", "ext", "zip1", "zip2", "uzp1", "uzp2", "trn1", "trn2", "tbl",
	-- AES MixColumns + polynomial multiply + SHA3 logic (pure multi-source forms;
	-- aese/aesd read their destination and live in VEC_DST_READ instead)
	"aesmc", "aesimc", "eor3", "bcax", "rax1", "xar",
	-- FP arithmetic (scalar + vector)
	"fadd", "fsub", "fmul", "fdiv", "fmulx", "fnmul",
	"fmax", "fmin", "fmaxnm", "fminnm", "fabs", "fneg", "fsqrt",
	"frintn", "frintp", "frintm", "frintz", "frinta", "frinti", "frintx",
	"fcvt", "fcvtzs", "fcvtzu", "fcvtas", "fcvtau", "fcvtms", "fcvtmu",
	"fcvtns", "fcvtnu", "fcvtps", "fcvtpu",
	"scvtf", "ucvtf", "fcvtl", "fcvtl2", "fcvtn", "fcvtn2", "fcvtxn", "fcvtxn2",
	"frecpe", "frecps", "frsqrte", "frsqrts", "frecpx",
	"faddp", "fmaxv", "fminv", "fmaxnmv", "fminnmv", "fcsel",
	"urecpe", "ursqrte",
} do VEC_PURE_WRITE[m] = true end

function arm64.vecEffects(stmt)
	local m = string.lower(stmt.mnemonic)
	local ops = stmt.operands or {}
	local uses, defs = {}, {}
	local function addUse(n, w) if n and (uses[n] or 0) < w then uses[n] = w end end
	local function addDef(n, w) if n and (defs[n] or 0) < w then defs[n] = w end end
	local function forEachNeon(o, f)
		if o.kind == "reg" and o.rc == "neon" then f(o)
		elseif o.kind == "reglist" then for _, r in o.regs do f(r) end end
	end

	-- Memory instructions: NEON loads supply their full registers (scalar and
	-- 64-bit forms zero the upper bits); only the single-element lane loads
	-- merge. NEON stores read only the stored bytes (lane stores flow through).
	local mem
	for _, o in ops do if o.kind == "mem" then mem = o end end
	if mem then
		if LOADS[m] or LOAD_PAIR[m] then
			local laneMerge = false
			for _, o in ops do
				if o.kind == "reglist" and o.lane ~= nil then laneMerge = true end
			end
			for _, o in ops do
				forEachNeon(o, function(r)
					if laneMerge then addUse(r.num, 16) else addDef(r.num, 16) end
				end)
			end
			return uses, defs
		end
		if STORES[m] or STORE_PAIR[m] then
			for _, o in ops do
				if o.kind == "reglist" and o.lane ~= nil then
					for _, r in o.regs do addUse(r.num, 16) end
				else
					forEachNeon(o, function(r) addUse(r.num, neonOpWidth(r)) end)
				end
			end
			return uses, defs
		end
		-- non-load/store with a memory operand is rejected elsewhere.
	end

	-- Collect NEON operands.
	local any = false
	for _, o in ops do
		if (o.kind == "reg" and o.rc == "neon") or o.kind == "reglist" then any = true break end
	end
	if not any then return uses, defs end

	-- Lane-insert destinations (ins, element fmov/mov): merge into one lane —
	-- everything flows through (provided 0), and the untouched lanes are read.
	local dst = ops[1]
	local laneDst = (m == "ins") or ((m == "mov" or m == "fmov")
		and dst and dst.kind == "reg" and dst.element ~= nil)
	if laneDst then
		forEachNeon(dst, function(r) addUse(r.num, 16) end)
		for i = 2, #ops do
			forEachNeon(ops[i], function(r) addUse(r.num, neonOpWidth(r)) end)
		end
		return uses, defs
	end

	if VEC_DST_READ[m] then
		forEachNeon(dst, function(r) addDef(r.num, 16); addUse(r.num, neonOpWidth(r)) end)
		for i = 2, #ops do
			forEachNeon(ops[i], function(r) addUse(r.num, neonOpWidth(r)) end)
		end
		return uses, defs
	end

	if VEC_PURE_WRITE[m] or m == "mov" or m == "fmov" then
		forEachNeon(dst, function(r) addDef(r.num, 16) end)
		for i = 2, #ops do
			forEachNeon(ops[i], function(r) addUse(r.num, neonOpWidth(r)) end)
		end
		return uses, defs
	end

	-- Safe default (unknown mnemonic, e.g. SHA/SM3/SM4 crypto): every NEON
	-- operand is read at its width, nothing is provided — liveness flows
	-- through the destination. Never under-saves.
	for _, o in ops do
		forEachNeon(o, function(r) addUse(r.num, neonOpWidth(r)) end)
	end
	return uses, defs
end

-- Register/calling-convention configuration (AAPCS64 + Fil-C fast entrypoint) --------
-- Reserved: x8 (dedicated glue/epilogue scratch), x16/x17 (IP0/IP1 veneer), x18 (platform),
-- x29 (fp), x30 (lr), sp, xzr. Allocatable = the remaining GPRs. Mirrors x86_64_isa.
arm64.ALLOCATABLE = {
	"x0","x1","x2","x3","x4","x5","x6","x7","x9","x10","x11","x12","x13","x14","x15",
	"x19","x20","x21","x22","x23","x24","x25","x26","x27","x28",
}
arm64.CALLEE_SAVED = { x19=true,x20=true,x21=true,x22=true,x23=true,x24=true,x25=true,x26=true,x27=true,x28=true }
arm64.CALLEE_SAVED_ORDER = { "x19","x20","x21","x22","x23","x24","x25","x26","x27","x28" }
arm64.CALLER_SAVED = { x0=true,x1=true,x2=true,x3=true,x4=true,x5=true,x6=true,x7=true,x9=true,x10=true,x11=true,x12=true,x13=true,x14=true,x15=true }
arm64.SCRATCH = "x8"       -- non-allocatable glue scratch
arm64.SCRATCH_NUM = 8
-- caller-saved register numbers clobbered by a call (used to model call defs).
arm64.CALLER_SAVED_NUMS = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 }

return arm64
