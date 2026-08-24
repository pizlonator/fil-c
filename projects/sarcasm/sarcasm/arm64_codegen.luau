--!nonstrict
-- ARM64 codegen: the per-arch instruction emitter used by the shared GIMSO transform
-- (transform.luau). Same interface as x86_64_codegen — static config (calling convention,
-- operand analysis) plus make(ctx) returning neutral "micro-ops" the shared transform
-- composes into access checks, invisicap loads/stores, calls, pollchecks and allocations.

local build = require("./build")
local isa = require("./arm64_isa")

local M = {}

M.classify = isa.classify
M.baseMnemonic = function(m) return string.lower(m) end
M.precolorName = function(num) return "x" .. num end
M.ALLOCATABLE = isa.ALLOCATABLE

-- Calling convention. Fast entrypoint: x0=myth; args are packed DENSELY into the SIX
-- registers x2..x7 (a pointer-class arg occupies two consecutive slots -- intval, lower;
-- a scalar-class arg occupies one slot, intval only), matching pizlonated clang output.
-- Arguments beyond the sixth word are passed on the stack, which sarcasm does not
-- marshal. ret w0=flags, x1=iv, x2=lo. Runtime calls (AAPCS): x0,x1,x2,...; result x0.
-- yolo params arrive in x{k-1}.
local DENSE_ARG_REGS = { 2, 3, 4, 5, 6, 7 }
local function denseArgSlot(argTypes, k)
	local slot = 1
	for i = 1, k - 1 do
		slot += (argTypes[i] == 7) and 2 or 1   -- 7 = TYPE_PTR (see sig.luau)
	end
	return slot
end
M.cc = {
	myth = 0, retFlag = 0, retIv = 1, retLo = 2,
	argIv = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k)] end,
	argLo = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k) + 1] end,  -- ptr args only
	yoloArg = function(k) return k - 1 end,
	rtArg = function(k) return k - 1 end,
	rtRet = 0,
}

-- Callsite signature validation (called by the shared transform for each `;! sig` call
-- annotation). The arm64 fast CC packs argument words densely into the SIX registers
-- x2..x7 (a pointer arg = 2 words); wider arguments are passed on the stack, which
-- sarcasm does not yet marshal. So a call needs at most 6 register argument words.
function M.checkCallsiteSig(cs, callee)
	local words = 0
	for k = 1, cs.argWords do words += (cs.argTypes[k] == 7) and 2 or 1 end
	if words > 6 then
		error("sarcasm: call to '" .. callee .. "': >6 register argument words not yet supported"
			.. " (this call's signature needs " .. tostring(words) .. ")")
	end
end

-- Static operand analysis -------------------------------------------------
function M.memOperand(stmt)
	for _, op in stmt.operands do if op.kind == "mem" then return op end end
	return nil
end

-- Fixed-width scalar mnemonics: byte size of the access. Width-varying forms
-- (ldr/str/ldp/stp/atomics) derive their size from the data register below.
local ACCESS = {
	ldrb=1, strb=1, ldrsb=1, ldurb=1, sturb=1, ldursb=1,
	ldarb=1, stlrb=1, ldaprb=1, ldapurb=1, ldapursb=1, ldxrb=1, ldaxrb=1,
	ldrh=2, strh=2, ldrsh=2, ldurh=2, sturh=2, ldursh=2,
	ldarh=2, stlrh=2, ldaprh=2, ldapurh=2, ldapursh=2, ldxrh=2, ldaxrh=2,
	ldrsw=4, ldursw=4, ldapursw=4,
}

-- Byte size of a NEON/FP scalar register from its name prefix (b/h/s/d/q/v).
local function neonSize(r)
	local p = string.sub(r.name or "", 1, 1)
	if p == "b" then return 1 elseif p == "h" then return 2 elseif p == "s" then return 4
	elseif p == "d" then return 8 else return 16 end  -- q / v
end

-- Byte size of one data element of the instruction (first register operand):
-- 4 for wN/sN, 8 for xN/dN, 1/2 for bN/hN, 16 for qN/vN.
local function elemSize(r)
	if r and r.kind == "reg" then
		if r.rc == "neon" then return neonSize(r) end
		if r.width == 32 then return 4 end
	end
	return 8
end

-- Access size and required alignment for a modeled load/store. Sizes follow the data
-- register class (q-reg accesses are 16 bytes, q-reg pairs 32, ldnp/stnp/ldxp/ldaxp are
-- 16, sub-word scalar forms 1/2/4). Alignment matches what pizlonated clang itself
-- demands for the same accesses: no alignment check for accesses of 8 bytes or fewer
-- (the architecture tolerates unaligned scalar and NEON accesses), 8-byte alignment
-- for wider (16/32-byte) accesses.
function M.accessSizeAlign(stmt)
	local m = string.lower(stmt.mnemonic)
	local ops = stmt.operands
	local sz = ACCESS[m]
	if not sz then
		if m == "ldpsw" then
			sz = 8                      -- pair of signed words
		elseif m == "ldp" or m == "stp" or m == "ldnp" or m == "stnp"
			or m == "ldxp" or m == "ldaxp" then
			sz = elemSize(ops[1]) * 2   -- register pair: two elements
		else
			sz = elemSize(ops[1])       -- single-register forms
		end
	end
	return sz, (sz >= 16) and 8 or 1
end

-- Fixed-size alloca base: `mov rd, sp` -> 0, `add rd, sp, #imm` -> imm, else nil.
function M.allocaRegionBase(stmt)
	local m = string.lower(stmt.mnemonic)
	local ops = stmt.operands
	if m == "mov" and ops[2] and ops[2].name == "sp" then return 0 end
	if m == "add" and ops[2] and ops[2].name == "sp" and ops[3] and ops[3].kind == "imm" then
		return ops[3].value or tonumber(ops[3].text)
	end
	return nil
end

-- `alloca size` size operand: `sub sp, sp, X` -> X is operands[3].
function M.allocaSizeOperand(stmt) return stmt.operands[3] end
-- store value operand: `str Xs, [mem]` -> Xs is operands[1] (source-first).
function M.isStoreInsn(stmt) return string.sub(string.lower(stmt.mnemonic), 1, 2) == "st" end

function M.isMemInsn(mnem) return isa.isMemInsn(mnem) end

-- Condition-flag (NZCV) classification for flag liveness at instrumentation points.
M.flagUse = isa.flagUse
M.flagDef = isa.flagDef

-- `;! load ptr` / `;! store ptr` are valid ONLY on a scalar 64-bit load/store
-- (ldr/ldur/str/stur with a 64-bit GPR data register): the invisicap sequence moves
-- exactly one (intval, lower) pair per access. Everything else — pairs, 32-bit or
-- sub-word forms, atomics (whose ordering semantics would be lost), literal loads,
-- NEON data registers, xzr destinations/values, non-memory instructions, and
-- stack-frame bases (sp/x29 are virtualized as slots; there is no heap capability to
-- load/store through) — is rejected here, at the annotation site, instead of crashing
-- or silently corrupting later.
function M.validatePtrAccess(stmt, what)
	local m = string.lower(stmt.mnemonic)
	local raw = stmt.raw or m
	local function bad(why)
		error("sarcasm: ;! " .. what .. " ptr is only valid on a 64-bit scalar "
			.. (what == "load" and "load (ldr/ldur xN, [...])" or "store (str/stur xN, [...])")
			.. ": " .. why .. ": " .. raw)
	end
	local want = (what == "load") and { ldr = true, ldur = true } or { str = true, stur = true }
	if not want[m] then
		if isa.isMemInsn(m) then
			bad("annotated instruction is '" .. m
				.. "' (pairs, sub-word forms and atomics cannot carry a capability)")
		end
		bad("annotated instruction is not a " .. what)
	end
	local mem = M.memOperand(stmt)
	if not mem then
		bad("instruction has no memory operand (a literal-pool load cannot carry a capability)")
	end
	local val = stmt.operands[1]
	if not val or val.kind ~= "reg" or val.rc ~= "gpr" or val.width ~= 64 then
		bad("the " .. (what == "load" and "destination" or "stored value")
			.. " is not a 64-bit GPR register")
	end
	if mem.base and (mem.base.name == "sp" or mem.base.name == "x29") then
		bad("a stack-frame-relative access cannot be an annotated pointer " .. what)
	end
end

-- Reject instructions that forge raw addresses to symbols. adrp/adr to a symbol make a
-- global's address as a plain integer (the capability is lost, so every later access
-- would trap at runtime — and any jump table it points at is rejected as data anyway);
-- `ldr xN, sym` reads a literal-pool entry whose data sarcasm does not emit (the label
-- would dangle). `ldr xN, =constant` stays accepted: GNU as materializes the constant
-- pool in OUR output .text.
function M.checkForgedAddress(stmt)
	local m = string.lower(stmt.mnemonic)
	local ops = stmt.operands
	if m == "adrp" or m == "adr" then
		local op2 = ops[2]
		if op2 and op2.kind == "sym" then
			error("sarcasm: '" .. m .. "' to symbol '" .. op2.text
				.. "' forges a raw address to a global (globals cannot be made memory-safe"
				.. " automatically): " .. (stmt.raw or m))
		end
	elseif m == "ldr" then
		local op2 = ops[2]
		if op2 and op2.kind == "sym" and string.sub(op2.text, 1, 1) ~= "=" then
			error("sarcasm: literal-pool load from symbol '" .. op2.text
				.. "' is not supported (literal pool data cannot be made memory-safe"
				.. " automatically): " .. (stmt.raw or m))
		end
	end
end

-- ARM condition suffixes for the neutral cmp/test conditions.
local BCOND = { ult = "b.lo", uge = "b.hs", ugt = "b.hi", ule = "b.ls", eq = "b.eq", ne = "b.ne" }

function M.make(ctx)
	local emit, allocTemp, pColored, temps = ctx.emit, ctx.allocTemp, ctx.pColored, ctx.temps
	local pf = ctx.pf
	local cg = { cc = M.cc }

	local function rt(t, w) return build.rt(t, w) end
	local function br(cc_, lbl) emit(build.insn(cc_, { build.sym(lbl) }, { control = "condbranch", targets = {lbl} })) end

	function cg.move(dst, src, coalesce)
		emit(build.insn("mov", { rt(dst), rt(src) },
			{ defs = {dst}, uses = {src}, move = coalesce and { dst = dst, src = src } or nil }))
	end
	function cg.moveImm(dst, v) emit(build.insn("mov", { rt(dst), build.imm(v) }, { defs = {dst} })) end
	function cg.moveZero(dst) emit(build.insn("mov", { rt(dst), build.xzr() }, { defs = {dst} })) end
	function cg.movBig(dst, hex)
		-- movz/movk chain. Rarely needed: ARM expresses the masks the transform uses as
		-- bitmask immediates directly (see testMaskBranch/maskLow48), so this is a fallback.
		local v = tonumber(hex) or 0
		emit(build.insn("mov", { rt(dst), build.imm(v % 65536) }, { defs = {dst} }))
		for s = 16, 48, 16 do
			local part = math.floor(v / 2 ^ s) % 65536
			if part ~= 0 then
				emit(build.insn("movk", { rt(dst), build.withShift(build.imm(part), "lsl", s) },
					{ defs = {dst}, uses = {dst} }))
			end
		end
	end

	-- loads/stores: ldur/stur for negative (unscaled) offsets, ldr/str otherwise; ldrb for byte.
	-- The nil guards are defense-in-depth: a `;! load ptr`/`;! store ptr` annotation on an
	-- instruction without a usable 64-bit GPR value operand (e.g. a NEON register) funnels
	-- nil here; reject cleanly instead of rendering a nil register.
	local function checkValTemp(t, what)
		if t == nil then
			error("sarcasm: " .. what .. " has no usable 64-bit GPR value register "
				.. "(a pointer load/store annotation requires a plain GPR load/store)")
		end
	end
	function cg.load(dst, base, off, size)
		checkValTemp(dst, "annotated pointer load")
		checkValTemp(base, "annotated pointer load")
		local m = (size == 1) and "ldrb" or ((off < 0) and "ldur" or "ldr")
		local w = (size == 8) and 64 or 32
		emit(build.insn(m, { rt(dst, w), build.mem(rt(base), off ~= 0 and off or nil) }, { defs = {dst}, uses = {base} }))
	end
	function cg.loadIndexed(dst, base, idx, scale, size)
		checkValTemp(dst, "annotated pointer load")
		checkValTemp(base, "annotated pointer load")
		checkValTemp(idx, "annotated pointer load")
		local m = (size == 1) and "ldrb" or "ldr"
		local w = (size == 8) and 64 or 32
		local idxOp = rt(idx)
		if scale and scale > 1 then build.withShift(idxOp, "lsl", math.floor(math.log(scale) / math.log(2))) end
		emit(build.insn(m, { rt(dst, w), build.mem(rt(base), idxOp) }, { defs = {dst}, uses = {base, idx} }))
	end
	function cg.store(src, base, off)
		checkValTemp(src, "annotated pointer store")
		checkValTemp(base, "annotated pointer store")
		local m = (off < 0) and "stur" or "str"
		emit(build.insn(m, { rt(src), build.mem(rt(base), off ~= 0 and off or nil) }, { uses = {src, base} }))
	end
	function cg.storeIndexed(src, base, idx, scale)
		checkValTemp(src, "annotated pointer store")
		checkValTemp(base, "annotated pointer store")
		checkValTemp(idx, "annotated pointer store")
		local idxOp = rt(idx)
		if scale and scale > 1 then build.withShift(idxOp, "lsl", math.floor(math.log(scale) / math.log(2))) end
		emit(build.insn("str", { rt(src), build.mem(rt(base), idxOp) }, { uses = {src, base, idx} }))
	end
	-- dst = base + off + idx*scale  (add/sub; base/idx are temps, may be nil)
	function cg.effAddr(dst, base, off, idx, scale)
		if idx then
			local idxOp = rt(idx)
			if scale and scale > 1 then build.withShift(idxOp, "lsl", math.floor(math.log(scale) / math.log(2))) end
			emit(build.insn("add", { rt(dst), rt(base), idxOp }, { defs = {dst}, uses = {base, idx} }))
		elseif off and off ~= 0 then
			local op = off > 0 and "add" or "sub"
			emit(build.insn(op, { rt(dst), rt(base), build.imm(math.abs(off)) }, { defs = {dst}, uses = {base} }))
		else
			cg.move(dst, base)
		end
	end
	function cg.addImm(dst, src, imm) emit(build.insn("add", { rt(dst), rt(src), build.imm(imm) }, { defs = {dst}, uses = {src} })) end
	function cg.subImm(dst, src, imm) emit(build.insn("sub", { rt(dst), rt(src), build.imm(imm) }, { defs = {dst}, uses = {src} })) end
	function cg.subReg(dst, a, b) emit(build.insn("sub", { rt(dst), rt(a), rt(b) }, { defs = {dst}, uses = {a, b} })) end
	function cg.andImmInPlace(dst, mask)
		local m = (mask == -2) and "0xfffffffffffffffe" or mask
		emit(build.insn("and", { rt(dst), rt(dst), build.imm(m) }, { defs = {dst}, uses = {dst} }))
	end
	function cg.maskLow48(dst) emit(build.insn("and", { rt(dst), rt(dst), build.imm("0xffffffffffff") }, { defs = {dst}, uses = {dst} })) end

	-- Save/restore NZCV around an instrumented sequence when the program's flags are
	-- live across the insertion point (see transform.luau's flag liveness scan). The
	-- temp is an ordinary virtual register; regalloc owns it (mrs/msr are cheap and it
	-- may spill freely — sp is stable inside the FIP body).
	function cg.saveFlags()
		local t = allocTemp({ role = "flags" })
		emit(build.insn("mrs", { rt(t), build.sym("nzcv") }, { defs = {t} }))
		return t
	end
	function cg.restoreFlags(t)
		emit(build.insn("msr", { build.sym("nzcv"), rt(t) }, { uses = {t} }))
	end

	function cg.branchZero(reg, lbl) emit(build.insn("cbz", { rt(reg), build.sym(lbl) }, { control = "condbranch", targets = {lbl}, uses = {reg} })) end
	function cg.branchNonZero(reg, lbl) emit(build.insn("cbnz", { rt(reg), build.sym(lbl) }, { control = "condbranch", targets = {lbl}, uses = {reg} })) end
	function cg.cmpBranch(a, b, cond, lbl)
		emit(build.insn("cmp", { rt(a), rt(b) }, { uses = {a, b} })); br(BCOND[cond], lbl)
	end
	function cg.cmpImmBranch(a, imm, cond, lbl)
		emit(build.insn("cmp", { rt(a), build.imm(imm) }, { uses = {a} })); br(BCOND[cond], lbl)
	end
	function cg.testMaskBranch(reg, mask, cond, lbl, width)
		emit(build.insn("tst", { rt(reg, width), build.imm(mask) }, { uses = {reg} })); br(cond == "eq" and "b.eq" or "b.ne", lbl)
	end
	function cg.jump(lbl) emit(build.insn("b", { build.sym(lbl) }, { control = "branch", targets = {lbl}, fallsThrough = false })) end
	function cg.label(name) emit(build.label(name)) end

	function cg.call(sym, uses, clobbers, indirect)
		if indirect then
			emit(build.insn("blr", { rt(sym) }, { control = "call", uses = uses, defs = clobbers }))
		else
			emit(build.insn("bl", { build.sym(sym) }, { control = "call", uses = uses, defs = clobbers }))
		end
	end
	function cg.callerSavedClobbers()
		local clob = {}
		for _, rn in isa.CALLER_SAVED_NUMS do table.insert(clob, pColored(rn)) end
		return clob
	end
	function cg.leaGlobal(destPhys, sym)
		emit(build.insn("adrp", { build.rp(destPhys), build.sym(sym) }, { defs = { pColored(destPhys) } }))
		emit(build.insn("add", { build.rp(destPhys), build.rp(destPhys), build.sym(sym, "lo12") },
			{ defs = { pColored(destPhys) }, uses = { pColored(destPhys) } }))
	end

	function cg.computeAddr(node, mem)
		if mem == nil then
			error("sarcasm: instruction has no memory operand to make memory-safe: "
				.. (node.stmt and node.stmt.raw or "?"))
		end
		local baseTemp = node.opTemp[mem.base]
		if baseTemp == nil then
			-- The base register carries no value we can check (sp/x29/xzr or an
			-- unmodeled register): there is no capability to bounds-check against.
			error("sarcasm: memory operand base '"
				.. ((mem.base and mem.base.name) or "?")
				.. "' has no usable value (cannot prove safety): "
				.. (node.stmt and node.stmt.raw or "?"))
		end
		local indexTemp = (mem.index and mem.index.kind == "reg") and node.opTemp[mem.index] or nil
		if mem.index and mem.index.kind == "reg" and indexTemp == nil then
			error("sarcasm: memory operand index '"
				.. (mem.index.name or "?")
				.. "' has no usable value (cannot prove safety): "
				.. (node.stmt and node.stmt.raw or "?"))
		end
		if mem.index and mem.index.kind ~= "imm" and mem.index.kind ~= "reg" then
			-- e.g. a symbolic :lo12: offset folded into the brackets: the checked
			-- address would not match the accessed address.
			error("sarcasm: memory operand with a symbolic index cannot be made memory-safe: "
				.. (node.stmt and node.stmt.raw or "?"))
		end
		local capLo
		if baseTemp and pf.isPtr[baseTemp] then capLo = pf.lowerTemp[baseTemp]
		elseif indexTemp and pf.isPtr[indexTemp] then capLo = pf.lowerTemp[indexTemp] end
		local effTemp = baseTemp
		if mem.writeback == "post" then
			effTemp = baseTemp
		elseif mem.index and mem.index.kind == "imm" then
			local off = mem.index.value or tonumber(mem.index.text) or 0
			if off ~= 0 then
				effTemp = allocTemp({ role = "eff" })
				local op = off > 0 and "add" or "sub"
				emit(build.insn(op, { rt(effTemp), rt(baseTemp), build.imm(math.abs(off)) }, { defs = {effTemp}, uses = {baseTemp} }))
			end
		elseif indexTemp then
			effTemp = allocTemp({ role = "eff" })
			local idxOp = rt(indexTemp)
			if mem.shift then build.withShift(idxOp, mem.shift.op, mem.shift.amount) end
			emit(build.insn("add", { rt(effTemp), rt(baseTemp), idxOp }, { defs = {effTemp}, uses = {baseTemp, indexTemp} }))
		end
		return effTemp, capLo
	end

	function cg.markingStateNonzeroBranch(lbl)
		local msp = allocTemp({ role = "scratch" })
		emit(build.insn("adrp", { rt(msp), build.sym("filc_current_marking_state", "got") }, { defs = {msp} }))
		emit(build.insn("ldr", { rt(msp), build.mem(rt(msp), build.sym("filc_current_marking_state", "got_lo12")) }, { defs = {msp}, uses = {msp} }))
		local msv = allocTemp({ role = "scratch" })
		emit(build.insn("ldr", { rt(msv, 32), build.mem(rt(msp), 0) }, { defs = {msv}, uses = {msp} }))
		emit(build.insn("cbnz", { rt(msv, 32), build.sym(lbl) }, { control = "condbranch", targets = {lbl}, uses = {msv} }))
	end
	function cg.exceptionBranch(lbl)
		emit(build.insn("tbnz", { build.rp(M.cc.retFlag, 32), build.imm(0), build.sym(lbl) },
			{ control = "condbranch", targets = {lbl}, uses = { pColored(M.cc.retFlag) } }))
	end
	function cg.clearRetFlag()
		emit(build.insn("mov", { build.rp(M.cc.retFlag, 32), build.wzr() }, { defs = { pColored(M.cc.retFlag) } }))
	end

	function cg.failStub(list, lbl, ivTemp, loTemp, acoSym)
		local a1, a2, a3 = M.cc.rtArg(1), M.cc.rtArg(2), M.cc.rtArg(3)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(ivTemp) }, { defs = { pColored(a1) }, uses = {ivTemp} }))
		if loTemp then
			table.insert(list, build.insn("mov", { build.rp(a2), rt(loTemp) }, { defs = { pColored(a2) }, uses = {loTemp} }))
		else
			table.insert(list, build.insn("mov", { build.rp(a2), build.xzr() }, { defs = { pColored(a2) } }))
		end
		table.insert(list, build.insn("adrp", { build.rp(a3), build.sym(acoSym) }, { defs = { pColored(a3) } }))
		table.insert(list, build.insn("add", { build.rp(a3), build.rp(a3), build.sym(acoSym, "lo12") }, { defs = { pColored(a3) }, uses = { pColored(a3) } }))
		table.insert(list, build.insn("bl", { build.sym("filc_optimized_access_check_fail") },
			{ control = "call", uses = { pColored(a1), pColored(a2), pColored(a3) } }))
	end

	-- Redirect an sp-relative address computation landing inside a fixed alloca region to the
	-- allocation pointer (gcc -O0 re-derives the buffer address per use). Returns true if it
	-- replaced the node; a direct stack-relative *access* into a region is rejected.
	function cg.regionRedirect(node, regionOf)
		local m = string.lower(node.stmt.mnemonic)
		local ops = node.stmt.operands
		if m == "add" and ops[2] and ops[2].name == "sp" and ops[3] and ops[3].kind == "imm"
			and ops[1] and ops[1].name ~= "sp" then
			local off = ops[3].value or tonumber(ops[3].text)
			local r = regionOf(off)
			if r then
				local rd = node.defTemps[1]
				cg.addImm(rd, r.ptrTemp, off - r.base)
				pf.isPtr[rd] = true; pf.lowerTemp[rd] = r.loTemp
				return true
			end
		end
		if m == "mov" and ops[2] and ops[2].name == "sp" and ops[1] and ops[1].name ~= "sp" then
			local r = regionOf(0)
			if r then
				local rd = node.defTemps[1]
				cg.move(rd, r.ptrTemp, true)
				pf.isPtr[rd] = true; pf.lowerTemp[rd] = r.loTemp
				return true
			end
		end
		local mem = M.memOperand(node.stmt)
		if mem and mem.base and (mem.base.name == "sp" or mem.base.name == "x29") and not mem.writeback then
			local off = (mem.index and mem.index.kind == "imm") and (mem.index.value or tonumber(mem.index.text) or 0) or 0
			if regionOf(off) then
				error("sarcasm: direct stack-relative memory access into an alloca region is not supported: " .. node.stmt.raw)
			end
		end
		return false
	end

	-- passthrough special-casing: ARM has none (no cltq / lea-with-mem); return false.
	function cg.lowerSpecial(node, mem) return false end
	-- a memory operand may only appear on a load/store we model; anything else is unmodeled.
	function cg.checkMemInsn(node, mem)
		if not M.isMemInsn(node.stmt.mnemonic) then
			error("sarcasm: memory operand on non-load/store instruction '"
				.. node.stmt.mnemonic .. "' cannot be made memory-safe: " .. node.stmt.raw)
		end
		-- Writeback index must be a plain immediate (register/shift post-index forms are
		-- not part of the modeled subset — on arm64 they only exist for SIMD ld1/st1).
		if mem.writeback and ((mem.index and mem.index.kind ~= "imm") or mem.postShift) then
			error("sarcasm: memory operand with non-immediate writeback index cannot be made memory-safe: "
				.. node.stmt.raw)
		end
		-- A shift/extend inside the brackets only makes sense with a register index.
		if mem.shift and not (mem.index and mem.index.kind == "reg") then
			error("sarcasm: memory operand has a shift/extend but no index register: " .. node.stmt.raw)
		end
	end

	return cg
end

return M
