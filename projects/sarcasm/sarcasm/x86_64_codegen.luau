--!nonstrict
-- x86_64 codegen: the per-arch instruction emitter used by the shared GIMSO transform
-- (transform.luau). It exposes (a) static config — calling convention, operand analysis —
-- and (b) make(ctx) returning a `cg` of neutral "micro-ops" the shared transform composes
-- into access checks, invisicap loads/stores, calls, pollchecks and allocations. Output is
-- always AT&T (see x86_64_render); the IR nodes are dest-first.

local build = require("./build")
local isa = require("./x86_64_isa")
local xparse = require("./x86_64_parse")
local sig = require("./sig")
local fp = require("./x86_64_fp")
local regalloc = require("./regalloc")

local M = {}

-- ISA passthrough ---------------------------------------------------------
-- High-byte register names (%ah/%ch/%dh/%bh): emitPinned keeps an operand with
-- one of these names verbatim (it names bits 8-15 of the pinned physical
-- register; see emitPinned).
local HIGH_BYTE_NAME = { ah = true, ch = true, dh = true, bh = true }

M.classify = isa.classify
M.baseMnemonic = isa.baseMnemonic
M.flagUse = isa.flagUse
M.flagDef = isa.flagDef
M.flagWriteNoDef = isa.flagWriteNoDef
M.flagUseMask = isa.flagUseMask
M.flagDefMask = isa.flagDefMask
M.precolorName = function(num) return xparse.regName(num, 64) end
M.ALLOCATABLE = isa.ALLOCATABLE

-- Strict raw-body validation (validateBody in transform.luau): unannotated direct
-- calls, tail branches, non-label branch targets (`jmp *%reg` / `jmp *mem` classify
-- as indirect branches), unknown/misplaced `;!` annotations, and in-body data are
-- rejected BEFORE any rewriting — exactly like arm64. (Historically the x86_64
-- backend kept a looser acceptance: an unannotated `call foo` was passed through
-- raw, which miscompiled calls to same-file annotated callees — they landed in the
-- callee's FIP body without myth marshalling — and could never link against C
-- callees, since filcc emits no bare `foo` symbol. The `;! load ptr`/`store ptr`
-- early shape validation stays with the shared annotation walk's ptrAccessKind
-- checks: this backend defines no validatePtrAccess hook.)
M.strictBodyValidation = true

-- This backend implements the float/double signature classes (the transform
-- marshals FP arguments through the xmm registers and FP returns through xmm0).
-- See the FP hooks below (storeFpArg / loadFpRet / fpVecUses in expandFpSaves)
-- and the FP-signature save-area forcing (x86_64_frame.analyzeFrame +
-- x86_64_frame.VEC_SAVE_BYTES).
M.fpSignatures = true

-- This backend implements SysV/yolo STACK ARGUMENTS: at ENTRY the 7th and later
-- integer-class arguments (which arrive at 8+8i(%rsp), above the return address)
-- are redirected to virtual slot webs / a materialized frame area fed from the
-- incoming fast-CC words (x86_64_frame + transform's entry unpack), and at
-- CALLSITES the outgoing stack arguments are ordinary frame slots the transform
-- marshals into the outgoing stack-arg window.
M.yoloStackArgs = true

-- This backend implements GLOBAL VARIABLES: the driver collects same-file data
-- (.rodata/.data/.bss blocks and .comm/.lcomm) into Fil-C data objects (DOs) and
-- threads them to the transform; rip-relative references to same-file data are
-- lowered automatically (lea seeds / checked direct accesses), and `#! global
-- ptr` materializes an EXTERN global's flight pointer through its getter.
M.globalSupport = true

-- This backend implements CROSS-FUNCTION JUMPS (tailcall.luau): tail branches to
-- signature-annotated function entries (B1, rewritten to an annotated call plus
-- an epilogue jump by the driver's cross-function-jump pass) and mid-body
-- shared-tail joins (B2, region cloning). validateBody gates its annotated-jump
-- acceptance on this flag (arm64 rejects those forms "not yet supported").
M.crossFunctionJumps = true

-- Calling convention (numbers are register encodings; see x86_64_isa).
-- Fast entrypoint: rdi=myth, (rdx,rcx)=arg0, (r8,r9)=arg1; ret al=flags, rdx=iv, rcx=lo.
-- Runtime (SysV) calls: rdi,rsi,rdx,... ; result in rax.  yolo params arrive in SysV regs.
-- A runtime filc_ptr result (the atomic pointer operations) comes back as rax=intval,
-- rdx=lower (plain SysV: two consecutive integer return words).
M.cc = {
	myth = isa.CC.myth, retFlag = isa.CC.retFlag, retIv = isa.CC.retIv, retLo = isa.CC.retLo,
	fo = isa.CC.fo,                               -- rsi (callee function-object lower at a call)
	argIv = isa.CC.argIv, argLo = isa.CC.argLo,
	argRegCount = isa.CC.argRegCount,             -- 4 (rdx,rcx,r8,r9); beyond: stack words
	denseSlot = isa.CC.denseSlot,                 -- 0-based dense word index of arg k's iv
	-- yolo input-asm register for GPR argument k: its 1-based ordinal among the
	-- NON-FP args (FP args pass in the xmm registers and return nil here; the 7th
	-- and later integer-class args, which live on the stack at entry, return nil too).
	yoloArg = isa.CC.yoloArg,
	yoloRegCount = isa.CC.yoloRegCount,           -- 6 (rdi..r9); beyond: yolo stack args
	fpArgOrdinal = isa.CC.fpArgOrdinal,           -- xmm index (0-based) of FP arg k
	rtArg = function(k) return isa.SYSV[k] end,   -- runtime-call arg registers
	rtRet = 0,                                    -- rax
	rtRetLo = 2,                                  -- rdx (filc_ptr return: lower word)
}

-- Callsite signature validation (called by the shared transform for each `;! sig` call
-- annotation). The x86_64 fast CC packs GPR argument words densely into the FOUR
-- registers rdx,rcx,r8,r9 (a pointer arg = 2 words); GPR words beyond the fourth
-- travel on the stack as densely packed 8-byte slots (verified against pizlonated
-- clang), and sarcasm marshals them (transform.luau's outgoing stack-arg window) —
-- so a call is no longer bounded in its GPR argument words. Float/double args are
-- supported (they pass in xmm0..xmm7, in declaration order among the FP args, and
-- the return rides in xmm0), so at most 8 of them are allowed; long double and the
-- vector classes are rejected by the arch-aware signature class check (same rejection
-- shape as arm64).
function M.checkCallsiteSig(cs, callee)
	sig.checkSupportedClasses(cs, "call to '" .. callee .. "'", { arch = "x86_64" })
	local fpArgs = 0
	for k = 1, cs.argWords do
		if sig.isFpClass(cs.argTypes[k]) then fpArgs += 1 end
	end
	if fpArgs > 8 then
		error("sarcasm: call to '" .. callee .. "': >8 floating-point arguments not yet supported"
			.. " (this call's signature needs " .. tostring(fpArgs) .. ")")
	end
end

-- Static operand analysis -------------------------------------------------
function M.memOperand(stmt)
	for _, op in stmt.operands do if op.kind == "mem" then return op end end
	return nil
end

local WIDTH_LETTER = { b = 1, w = 2, l = 4, q = 8 }
function M.accessSizeAlign(stmt)
	local mnem = string.lower(stmt.mnemonic)
	-- FP/SIMD/x87 instructions get their access width from the knowledge module
	-- (authoritative — it cleanly rejects an Intel PTR size annotation that
	-- contradicts the ISA-fixed width).
	local fw, fa = fp.accessWidth(mnem, stmt.operands or {}, stmt.raw)
	if fw then
		-- The access check's alignment is capped at the word size, exactly like the
		-- Fil-C compiler (FilPizlonator's buildCheck does std::min(Alignment,
		-- WordSize)): the runtime's optimized-access-check origin cannot express
		-- larger alignments (filc_runtime.c asserts check_origin->alignment <=
		-- FILC_WORD_SIZE). A movdqa-class access to an address that is word-aligned
		-- but not vector-aligned passes the check and then faults in hardware,
		-- same as compiled code — the bounds check is unaffected.
		if fa > 8 then fa = 8 end
		return fw, fa
	end
	-- Defense in depth: an FP-class instruction with a memory operand whose width
	-- is unknowable must never fall back to a default width (fp.classify normally
	-- already rejected it during the lift).
	do
		local hasMem = false
		for _, op in (stmt.operands or {}) do if op.kind == "mem" then hasMem = true break end end
		if hasMem and fp.hasFpReg(stmt) then
			error("sarcasm: cannot determine the memory access width of '" .. mnem
				.. "' (cannot prove memory safety): " .. (stmt.raw or mnem))
		end
	end
	local zs = string.match(mnem, "^mov[zs]([bw])")  -- movzbl/movswq/... source width letter
	if zs then local s = WIDTH_LETTER[zs]; return s, 1 end
	local mem
	for _, op in stmt.operands do if op.kind == "mem" then mem = op end end
	-- cmpxchg8b/cmpxchg16b: the ISA-fixed width is in the mnemonic itself (there
	-- is no size suffix and no register form); the sole operand is memory. An
	-- Intel PTR size annotation CONTRADICTING the ISA-fixed width is rejected
	-- like any other annotation lie. (cmpxchg16b's 16-byte alignment requirement
	-- is hardware-enforced; checking it here keeps a misaligned address a clean
	-- filc trap rather than a #GP.)
	if mnem == "cmpxchg8b" or mnem == "cmpxchg16b" then
		local w = (mnem == "cmpxchg16b") and 16 or 8
		if mem and mem.size and mem.size ~= w then
			error("sarcasm: PTR size annotation (" .. tostring(mem.size)
				.. " bytes) contradicts the memory access width (" .. tostring(w)
				.. " bytes) of '" .. mnem .. "': " .. (stmt.raw or mnem))
		end
		return w, w
	end
	local base = isa.baseMnemonic(mnem)
	if (base == "movzx" or base == "movsx") and mem and mem.size then return mem.size, 1 end
	-- GPR access: the width is register-defined (AT&T suffix / first GPR operand
	-- width). An Intel PTR size annotation CONTRADICTING that width is a lie — the
	-- emitted instruction encodes the register width regardless (`mov WORD PTR
	-- [rdi], rax` stores 8 bytes, not 2) — so reject rather than under-size the
	-- bounds check. Size-driven (movzx/movsx) and memory-neutral (lea) forms and
	-- no-GPR-operand forms (push/pop/call/jmp, inc/dec/neg/not mem-only) take
	-- their width from the annotation/suffix legitimately (gprAccessRegWidth is
	-- nil for them).
	local regW = isa.gprAccessRegWidth(stmt)
	if mem and mem.size and regW and mem.size ~= regW then
		error("sarcasm: PTR size annotation (" .. tostring(mem.size)
			.. " bytes) contradicts the memory access width (" .. tostring(regW)
			.. " bytes) of '" .. mnem .. "': " .. (stmt.raw or mnem))
	end
	local sz = (mem and mem.size) or regW or 8
	-- GPR scalar accesses need only hardware-required alignment (x86 tolerates
	-- misaligned scalar loads/stores), exactly like Fil-C compiled code:
	-- FilPizlonator's needToCheckAlignment is false for integer/float/double
	-- types, so a scalar access is checked with alignment 1, while pointers
	-- are checked with alignment 8 (pointer loads/stores ride their own
	-- invisicap sequences, not this path). A `lock`-prefixed access is
	-- atomic, and the compiler checks atomics (min(ABIAlign, Alignment)), so
	-- those keep natural alignment. (cmpxchg16b's hardware 16-byte requirement
	-- is handled above and never reaches here.)
	if stmt.lockPrefix then return sz, sz end
	return sz, 1
end


-- Ptr-annotation shape validation (the shared transform rejects `;! load ptr` /
-- `;! store ptr` on anything that is not the matching plain access — a mismatch
-- used to be a SILENT MISCOMPILE: the annotated instruction was replaced by an
-- invisicap load/store, dropping the original operation). Returns:
--   "load"  — a plain 8-byte GPR load  (`movq (mem), %reg64` / Intel `mov r64, [mem]`)
--   "store" — a plain 8-byte GPR store (`movq %reg64, (mem)`)
--   "rmw"   — a memory-destination read-modify-write (ALU mem-RMW, xadd, cmpxchg,
--             adcx/adox/crc32, cmpxchg8b/16b): needs `;! load store ptr` instead
--   "other" — anything else (no memory operand, FP/SIMD, imm store, narrow mov, ...)
function M.ptrAccessKind(stmt)
	local ops = stmt.operands or {}
	local mem = M.memOperand(stmt)
	if not mem then return "other" end
	if isa.isMemRmw(stmt) then return "rmw" end
	local base = isa.baseMnemonic(string.lower(stmt.mnemonic))
	if base == "mov" and #ops == 2 then
		if ops[1] == mem and isa.isGpr(ops[2]) and (ops[2].width or 64) == 64 then return "store" end
		if ops[2] == mem and isa.isGpr(ops[1]) and (ops[1].width or 64) == 64 then return "load" end
	end
	return "other"
end

-- The memory-destination RMW operations supported on pointer slots (8-byte forms):
-- the lockable ALU family plus the unary inc/dec/neg/not. (Shifts/rotates/imul are
-- memory-destination RMWs too, but a shift/rotate/multiply of a pointer intval is
-- not a capability-preserving operation sarcasm implements.)
local PTR_RMW_OPS = { add = true, adc = true, ["and"] = true, ["or"] = true, sbb = true,
	sub = true, xor = true, inc = true, dec = true, neg = true, ["not"] = true }

-- Shape validation for the Phase-2 atomic/pointer-RMW annotations (`;! atomic load ptr`,
-- `;! atomic store ptr`, `;! atomic ptr`, `;! load store ptr`,
-- `;! atomic load store ptr`). Returns the sequence kind for the shared transform:
--   "atomicload" / "atomicstore" — runtime atomic pointer load/store (plain movq forms)
--   "cas"       — `;! atomic ptr` on cmpxchgq (pointer compare-exchange)
--   "rmw"       — `;! load store ptr` on a supported 8-byte mem-RMW (non-atomic)
--   "atomicrmw" — `;! atomic load store ptr` on a supported 8-byte mem-RMW
-- Anything else raises a clean sarcasm: error. (cmpxchg8b/cmpxchg16b are rejected
-- earlier, in the shared transform, for EVERY ptr-family annotation.)
function M.ptrAtomicShape(ann, stmt)
	local raw = stmt.raw or stmt.mnemonic or "?"
	if ann == "atomic load ptr" or ann == "atomic store ptr" then
		local want = (ann == "atomic load ptr") and "load" or "store"
		if M.ptrAccessKind(stmt) ~= want then
			error("sarcasm: `" .. ann .. "` requires a plain 8-byte GPR " .. want
				.. " instruction: " .. raw)
		end
		return (ann == "atomic load ptr") and "atomicload" or "atomicstore"
	end
	local base = isa.baseMnemonic(string.lower(stmt.mnemonic))
	local ops = stmt.operands or {}
	if ann == "atomic ptr" then
		if base ~= "cmpxchg" then
			error("sarcasm: `atomic ptr` is only supported on cmpxchg (pointer"
				.. " compare-exchange): " .. raw)
		end
		if not (ops[1] and ops[1].kind == "mem") then
			error("sarcasm: `atomic ptr` requires cmpxchg with a memory destination"
				.. " (the register form has no capability slot to exchange against): " .. raw)
		end
		if #ops ~= 2 or not isa.isGpr(ops[2]) then
			error("sarcasm: `atomic ptr` requires the cmpxchg mem, reg form"
				.. " (a GPR new-value operand): " .. raw)
		end
		local w = M.accessSizeAlign(stmt)
		if w ~= 8 then
			error("sarcasm: `atomic ptr` requires the 8-byte (q) cmpxchg form"
				.. " (pointer slots are 8 bytes): " .. raw)
		end
		return "cas"
	end
	-- `;! load store ptr` / `;! atomic load store ptr`: pointer RMWs.
	if M.ptrAccessKind(stmt) ~= "rmw" then
		error("sarcasm: `" .. ann .. "` requires a memory-destination"
			.. " read-modify-write instruction: " .. raw)
	end
	if base == "cmpxchg" then
		error("sarcasm: `" .. ann .. "` on cmpxchg; pointer compare-exchange needs"
			.. " `atomic ptr`: " .. raw)
	end
	if string.match(string.lower(stmt.mnemonic), "^xadd[bwlq]?$") then
		error("sarcasm: `" .. ann .. "` is not supported on xadd: its source register"
			.. " is also a destination (it would receive the old pointer value, needing a"
			.. " pointer def + lower — use a `atomic ptr` cmpxchg loop instead): " .. raw)
	end
	if not PTR_RMW_OPS[base] then
		error("sarcasm: `" .. ann .. "` is not supported on '" .. string.lower(stmt.mnemonic)
			.. "' (supported pointer RMWs: add/adc/and/or/sbb/sub/xor, inc/dec/neg/not): " .. raw)
	end
	-- Operand shape: the unary forms are mem-only; the binary forms are
	-- mem, reg|imm. A second MEMORY operand is not encodable in x86 for these
	-- (a gas error) and would sneak an unchecked heap read into the re-emitted
	-- register operation — reject it here.
	if PTR_RMW_OPS[base] and (base == "inc" or base == "dec" or base == "neg" or base == "not") then
		if #ops ~= 1 then
			error("sarcasm: `" .. ann .. "` on " .. base .. " requires the one-operand"
				.. " memory form: " .. raw)
		end
	elseif #ops ~= 2 or not (isa.isGpr(ops[2]) or ops[2].kind == "imm") then
		error("sarcasm: `" .. ann .. "` requires a GPR register or immediate source"
			.. " operand (x86 has no mem-to-mem " .. base .. " form): " .. raw)
	end
	local w = M.accessSizeAlign(stmt)
	if w ~= 8 then
		error("sarcasm: `" .. ann .. "` requires the 8-byte (q) form"
			.. " (pointer slots are 8 bytes): " .. raw)
	end
	if ann == "load store ptr" and stmt.lockPrefix then
		error("sarcasm: `lock` on a `load store ptr` RMW is a lie (the sequence is a"
			.. " non-atomic load+op+store); use `atomic load store ptr` for an atomic"
			.. " pointer RMW: " .. raw)
	end
	return (ann == "load store ptr") and "rmw" or "atomicrmw"
end
-- AVX512 {k}-masked / AVX2 vector-masked memory access detection. Returns the
-- fp.maskedAccessInfo metadata (elem/lanes/vecsize/contiguous/extract) plus the
-- mask source for THIS instruction — { maskReg = 1-7 } for a {%kN} writemask,
-- { maskVecOp = <operand> } for the AVX2 vmaskmov/vpmaskmov mask vector — or nil
-- when the instruction is not a supported masked access (an unmasked form, a
-- broadcast memory operand — which rides the plain element-width check — or an
-- unsupported masked form, which fp.classify already rejected during the lift).
function M.maskedAccessInfo(stmt)
	local mnem = string.lower(stmt.mnemonic)
	local ops = stmt.operands or {}
	local mi = fp.maskedAccessInfo(mnem, ops)
	if not mi then return nil end
	if mi.extract == "k" then
		local maskReg = nil
		for _, o in ops do
			if o.deco then
				local kn = string.match(o.deco, "{%%?k([0-7])}")
				if kn then maskReg = tonumber(kn) break end
			end
		end
		if not maskReg then return nil end  -- unmasked form: plain full-width check
		mi.maskReg = maskReg
		return mi
	end
	-- movmsk-extract forms (vmaskmovps/pd, vpmaskmovd/q): inherently masked; the
	-- mask is the (dest-first) second operand's vector register, present on both
	-- the load form (dest, mask, mem) and the store form (mem, mask, src).
	if not M.memOperand(stmt) then return nil end
	mi.maskVecOp = ops[2]
	return mi
end

-- a store writes memory when the (dest-first) first operand is the memory operand.
-- Single-memory-operand FP/x87 instructions break that positional rule: the lone
-- memory operand can be a source (fldl, faddl, fcoml, fxrstor, ldmxcsr, ...), so
-- decide those from the mnemonic — the stores are exactly the fst*/fist*/fbstp/
-- fnst*/fxsave/stmxcsr families, plus the unprivileged system-table stores
-- sgdt/sidt/sldt/str/smsw (all single-memory-operand stores the fp table claims;
-- vstmxcsr is the VEX alias of stmxcsr and reaches here by name).
function M.isStoreInsn(stmt)
	local ops = stmt.operands
	if ops[1] and ops[1].kind == "mem" then
		-- cmp/test with a memory FIRST operand are READ-only (they write flags,
		-- not memory): classifying them as stores would demand a CanWrite check
		-- and spuriously trap reads of read-only objects (sha256's
		-- `cmpb $0,3(%rbp)` against the K256 table).
		local b = isa.baseMnemonic(string.lower(stmt.mnemonic))
		if b == "cmp" or b == "test" then return false end
		-- One-operand multiply/divide: the memory operand is the multiplicand /
		-- divisor — a READ (the product/quotient goes to the implicit accumulator
		-- registers, never to memory). Classifying it as a store would demand a
		-- CanWrite check and spuriously trap reads of read-only operands
		-- (ecp_nistz256's `mulq 0(%rsi)`).
		if #ops == 1 and (b == "mul" or b == "imul" or b == "div" or b == "idiv") then
			return false
		end
		if #ops == 1 then
			local mnem = string.lower(stmt.mnemonic)
			if fp.lookup(mnem) then
				return mnem == "fbstp" or mnem == "fxsave" or mnem == "fxsave64"
					or mnem == "stmxcsr" or mnem == "vstmxcsr"
					or mnem == "sgdt" or mnem == "sidt"
					or mnem == "sldt" or mnem == "str" or mnem == "smsw"
					or string.match(mnem, "^fst") ~= nil
					or string.match(mnem, "^fist") ~= nil
					or string.match(mnem, "^fnst") ~= nil
			end
		end
		return true
	end
	return false
end

-- x86 conditions for the neutral cmp/test conditions used by the shared transform.
local JCC = { ult = "jb", uge = "jae", ugt = "ja", ule = "jbe", eq = "je", ne = "jne" }

function M.make(ctx)
	local emit, allocTemp, pColored, temps = ctx.emit, ctx.allocTemp, ctx.pColored, ctx.temps
	local pf = ctx.pf
	local cg = { cc = M.cc }

	local function rt(t, w) return build.rt(t, w) end
	local function branch(cc_, lbl)
		emit(build.insn(cc_, { build.sym(lbl) }, { control = "condbranch", targets = {lbl} }))
	end

	function cg.move(dst, src, coalesce)
		emit(build.insn("mov", { rt(dst), rt(src) },
			{ defs = {dst}, uses = {src}, move = coalesce and { dst = dst, src = src } or nil }))
	end
	function cg.moveImm(dst, v) emit(build.insn("mov", { rt(dst), build.imm(v) }, { defs = {dst} })) end
	function cg.moveZero(dst) emit(build.insn("mov", { rt(dst), build.imm(0) }, { defs = {dst} })) end
	function cg.movBig(dst, hex) emit(build.insn("mov", { rt(dst), build.imm(hex) }, { defs = {dst} })) end

	function cg.load(dst, base, off, size)
		-- byte loads zero-extend into a 32-bit dest (mirrors ARM `ldrb`); wider loads are plain.
		local mnem = (size == 1) and "movzbl" or "mov"
		local w = (size == 1) and 32 or (size and size * 8 or 64)
		emit(build.insn(mnem, { rt(dst, w),
			build.memx(rt(base), { dispVal = off ~= 0 and off or nil }) }, { defs = {dst}, uses = {base} }))
	end
	function cg.loadIndexed(dst, base, idx, scale, size)
		emit(build.insn("mov", { rt(dst, size and size * 8 or 64),
			build.memx(rt(base), { index = rt(idx), scale = scale or 1 }) }, { defs = {dst}, uses = {base, idx} }))
	end
	function cg.store(src, base, off)
		emit(build.insn("mov", { build.memx(rt(base), { dispVal = off ~= 0 and off or nil }), rt(src) },
			{ uses = {src, base} }))
	end
	function cg.storeIndexed(src, base, idx, scale)
		emit(build.insn("mov", { build.memx(rt(base), { index = rt(idx), scale = scale or 1 }), rt(src) },
			{ uses = {src, base, idx} }))
	end
	-- dst = base + off + idx*scale (lea; base/idx are temps, may be nil)
	function cg.effAddr(dst, base, off, idx, scale)
		local uses, mopts = {}, { dispVal = (off ~= 0) and off or nil }
		if base then table.insert(uses, base) end
		if idx then mopts.index = rt(idx); mopts.scale = scale or 1; table.insert(uses, idx) end
		emit(build.insn("lea", { rt(dst), build.memx(base and rt(base) or nil, mopts) }, { defs = {dst}, uses = uses }))
	end
	function cg.addImm(dst, src, imm)  -- dst = src + imm
		emit(build.insn("lea", { rt(dst), build.memx(rt(src), { dispVal = imm }) }, { defs = {dst}, uses = {src} }))
	end
	function cg.subImm(dst, src, imm)  -- dst = src - imm
		emit(build.insn("lea", { rt(dst), build.memx(rt(src), { dispVal = -imm }) }, { defs = {dst}, uses = {src} }))
	end
	function cg.subReg(dst, a, b)      -- dst = a - b
		cg.move(dst, a)
		emit(build.insn("sub", { rt(dst), rt(b) }, { defs = {dst}, uses = {dst, b} }))
	end
	function cg.addReg(dst, a, b)      -- dst = a + b
		cg.move(dst, a)
		emit(build.insn("add", { rt(dst), rt(b) }, { defs = {dst}, uses = {dst, b} }))
	end
	function cg.andReg(dst, a, b)      -- dst = a & b
		cg.move(dst, a)
		emit(build.insn("and", { rt(dst), rt(b) }, { defs = {dst}, uses = {dst, b} }))
	end
	function cg.andImmInPlace(dst, mask)  -- dst = dst & mask
		emit(build.insn("and", { rt(dst), build.imm(mask) }, { defs = {dst}, uses = {dst} }))
	end
	function cg.maskLow48(dst)  -- dst = dst & 0xffffffffffff  (shl 16; shr 16 clears top 16)
		emit(build.insn("shl", { rt(dst), build.imm(16) }, { defs = {dst}, uses = {dst} }))
		emit(build.insn("shr", { rt(dst), build.imm(16) }, { defs = {dst}, uses = {dst} }))
	end

	function cg.branchZero(reg, lbl)
		emit(build.insn("test", { rt(reg), rt(reg) }, { uses = {reg} })); branch("je", lbl)
	end
	function cg.branchNonZero(reg, lbl)
		emit(build.insn("test", { rt(reg), rt(reg) }, { uses = {reg} })); branch("jne", lbl)
	end
	function cg.cmpBranch(a, b, cond, lbl)
		emit(build.insn("cmp", { rt(a), rt(b) }, { uses = {a, b} })); branch(JCC[cond], lbl)
	end
	function cg.cmpImmBranch(a, imm, cond, lbl)
		emit(build.insn("cmp", { rt(a), build.imm(imm) }, { uses = {a} })); branch(JCC[cond], lbl)
	end
	-- Compare against an arbitrary 64-bit immediate and branch. x86_64's cmp takes a
	-- sign-extended imm32, which covers most of what the transform compares, but FP
	-- signatures push signature numbers past imm32 (eight doubles -> 8552919316),
	-- where `cmpq $imm64, mem/reg` does not exist — GNU as and clang's IAS both
	-- reject the encoding. Widen, mirroring arm64_glue.callsiteThunk's intent:
	--   imm fits sign-extended imm32   plain `cmpq $imm, a`
	--   otherwise                      movBig materializes the full constant into a
	--                                  freshly allocated temp, then cmp-register
	-- (`movq $imm64, %reg` assembles as the movabs form in both assemblers; the
	-- transform's movBig renders it the same way — see its use for the FUNCTION-type
	-- mask 0x780000000000000). The temp is fresh, so the compared register —
	-- possibly live, e.g. the ret-size check's cc.retIv — is never clobbered.)
	function cg.cmpImmBranchWidened(a, imm, cond, lbl)
		imm = math.floor(tonumber(imm)
			or error("sarcasm: internal error: bad compare immediate " .. tostring(imm)))
		if imm <= 2147483647 and imm >= -2147483648 then
			cg.cmpImmBranch(a, imm, cond, lbl)
		else
			local t = allocTemp({ role = "scratch" })
			cg.movBig(t, imm)
			cg.cmpBranch(a, t, cond, lbl)
		end
	end
	-- dst = src & mask, where mask is a 64-bit constant (hex string or number): x86_64
	-- materializes the mask and uses the two-operand and (arm64 uses movz/movk plus a
	-- three-operand and — the shared indirect-call sequence's FUNCTION-type test).
	function cg.andConst(dst, src, mask)
		cg.movBig(dst, mask)
		emit(build.insn("and", { rt(dst), rt(src) },
			{ defs = { dst }, uses = { dst, src } }))
	end
	-- branch if (reg & mask) cond 0   (cond "eq"/"ne"). x86 `test` only takes a 32-bit
	-- (sign-extended) immediate, so a mask that doesn't fit imm32 must go through a register.
	function cg.testMaskBranch(reg, mask, cond, lbl, width)
		local mval = (type(mask) == "number") and mask or tonumber(mask)
		local big = (mval == nil) or mval > 0x7fffffff or mval < -0x80000000
		if big then
			local m = allocTemp({ role = "scratch" }); cg.movBig(m, mask)
			emit(build.insn("test", { rt(reg), rt(m) }, { uses = {reg, m} }))
		else
			emit(build.insn("test", { rt(reg, width), build.imm(mask) }, { uses = {reg} }))
		end
		branch(cond == "eq" and "je" or "jne", lbl)
	end
	function cg.jump(lbl)
		emit(build.insn("jmp", { build.sym(lbl) }, { control = "branch", targets = {lbl}, fallsThrough = false }))
	end
	function cg.label(name) emit(build.label(name)) end

	function cg.call(sym, uses, clobbers, indirect)
		local node = build.insn("call", { build.sym(sym, indirect and nil or "plt") },
			{ control = "call", uses = uses, defs = clobbers })
		if indirect then node.indirect = true end
		emit(node)
		return node
	end
	function cg.callerSavedClobbers()
		local clob = {}
		for _, rn in isa.CALLER_SAVED_NUMS do table.insert(clob, pColored(rn)) end
		return clob
	end

	-- The fast-path indirect call: call the entrypoint loaded into targetTemp.
	-- `uses` carries every register live at the call (myth, fo, the target temp and
	-- the marshalled argument registers); `clobbers` are the caller-saved registers
	-- the callee may destroy. x86_64 renders this as `call *%reg` (op.indirect marks
	-- it for the renderer).
	function cg.emitIndirectCallInsn(targetTemp, uses, clobbers)
		local node = build.insn("call", { build.rt(targetTemp) },
			{ control = "call", uses = uses, defs = clobbers })
		node.indirect = true
		emit(node)
		return node
	end

	-- The generic (buffer-CC) indirect call: call the callee's generic entrypoint at
	-- [lower+8] — x86_64 can take the target straight from memory (`call *8(%lower)`);
	-- arm64 loads it into a fresh temp first (see arm64_codegen).
	function cg.emitGenericIndirectCall(fpLoTemp, uses, clobbers)
		local node = build.insn("call", { build.memx(build.rt(fpLoTemp), { dispVal = 8 }) },
			{ control = "call", uses = uses, defs = clobbers })
		node.indirect = true
		emit(node)
		return node
	end

	-- FP argument/return marshalling atoms (x86_64 fast CC: FP args in xmm0..xmm7 in
	-- declaration order among FP args; FP return in xmm0). These synthesize movss/movsd
	-- with a NAMED vector register operand (regalloc never models vector registers —
	-- the same shape expandFpSaves' save/restore movs use) plus a colored GPR base
	-- temp for the address. The nodes carry a `vecStmt` pseudo-statement so
	-- expandFpSaves' width-liveness sees the vector register effect (a buffer store
	-- USES the FP argument value — it must stay live across any injected call between
	-- the argument's definition and the call; a buffer load DEFINES the FP return
	-- value). Operands are dest-first (the renderer's AT&T reversal turns
	-- {mem, xmmN} into `movss %xmmN, OFF(%rMYTH)` and {xmmN, mem} into
	-- `movss OFF(%rMYTH), %xmmN`).

	-- Register name of FP argument #ordinal (0-based) of the given class:
	-- "xmm0".."xmm7" for both classes (float = low 32 bits, double = 64 bits —
	-- the instruction mnemonic selects the width).
	function cg.fpArgRegName(class, ordinal)
		local w = sig.fpWidthBytes(class)
			or error("sarcasm: internal error: fpArgRegName on a non-FP class " .. tostring(class))
		return "xmm" .. tostring(ordinal)
	end

	-- Store FP argument #ordinal (class = float/double) into the CC buffer word at
	-- [mythT + offset] (a float writes the low 4 bytes of the 8-byte slot, exactly
	-- like filcc's generic marshalling).
	function cg.storeFpArg(class, ordinal, mythTemp, offset)
		local w = sig.fpWidthBytes(class)
		local op = { kind = "reg", rc = "xmm", num = ordinal, width = w * 8,
			name = cg.fpArgRegName(class, ordinal) }
		local mem = build.memx(build.rt(mythTemp), { dispVal = offset })
		mem.size = w  -- lets the renderer size Intel PTR forms; AT&T rendering ignores it
		local n = build.insn("mov" .. (w == 4 and "ss" or "sd"), { mem, op },
			{ uses = { mythTemp } })
		n.vecStmt = { mnemonic = n.mnemonic, operands = n.operands }
		emit(n)
		return n
	end

	-- Load the FP return value (class = float/double) from the CC buffer word at
	-- [mythT + offset] into xmm0.
	function cg.loadFpRet(class, mythTemp, offset)
		local w = sig.fpWidthBytes(class)
		local op = { kind = "reg", rc = "xmm", num = 0, width = w * 8,
			name = cg.fpArgRegName(class, 0) }
		local mem = build.memx(build.rt(mythTemp), { dispVal = offset })
		mem.size = w
		local n = build.insn("mov" .. (w == 4 and "ss" or "sd"), { op, mem },
			{ uses = { mythTemp } })
		n.vecStmt = { mnemonic = n.mnemonic, operands = n.operands }
		emit(n)
		return n
	end

	-- Fail stubs for the shared indirect-call sequence (appended at function end).
	-- filc_check_function_call_fail takes the flight pointer (arg0 = intval, arg1 =
	-- lower); filc_cc_rets_check_failure takes (arg0 = actual size, arg1 = expected
	-- size, arg2 = 0). Both are noreturn.
	function cg.emitIndcallFailStub(list, lbl, ivTemp, loTemp)
		local a1, a2 = M.cc.rtArg(1), M.cc.rtArg(2)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(ivTemp) },
			{ defs = { pColored(a1) }, uses = { ivTemp } }))
		table.insert(list, build.insn("mov", { build.rp(a2), rt(loTemp) },
			{ defs = { pColored(a2) }, uses = { loTemp } }))
		-- appended to the list (the stubs flush at function end), never emitted here
		table.insert(list, build.insn("call", { build.sym("filc_check_function_call_fail", "plt") },
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2) } }))
	end
	function cg.emitRetsFailStub(list, lbl, actualTemp, expectedBytes)
		local a1, a2, a3 = M.cc.rtArg(1), M.cc.rtArg(2), M.cc.rtArg(3)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a2, 32), build.imm(expectedBytes) },
			{ defs = { pColored(a2) } }))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(actualTemp) },
			{ defs = { pColored(a1) }, uses = { actualTemp } }))
		table.insert(list, build.insn("mov", { build.rp(a3, 32), build.imm(0) },
			{ defs = { pColored(a3) } }))
		table.insert(list, build.insn("call", { build.sym("filc_cc_rets_check_failure", "plt") },
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3) } }))
	end
	function cg.leaGlobal(destPhys, sym)  -- destPhys is a physical reg num
		emit(build.insn("lea", { build.rp(destPhys), build.memx({ kind = "reg", rc = "rip" }, { dispSym = sym }) },
			{ defs = { pColored(destPhys) } }))
	end

	-- Synthesized rip-relative lea / load against a symbol (global-variable
	-- materialization: payload bases, GP-cell loads). dst is a temp; off is an
	-- optional constant displacement.
	function cg.leaRipSym(dst, sym, off)
		emit(build.insn("lea", { rt(dst), build.memx({ kind = "reg", rc = "rip" },
			{ dispSym = sym, dispVal = (off and off ~= 0) and off or nil }) }, { defs = { dst } }))
	end
	function cg.loadRipSym(dst, sym, off)
		emit(build.insn("mov", { rt(dst), build.memx({ kind = "reg", rc = "rip" },
			{ dispSym = sym, dispVal = (off and off ~= 0) and off or nil }) }, { defs = { dst } }))
	end

	-- Stack marshalling for a runtime call with a trailing filc_ptr argument past
	-- the six SysV argument registers (filc_strong_cas_ptr_with_manual_tracking's
	-- new_value): the words must sit at [rsp]=intval, [rsp+8]=lower AT the call.
	-- The values are stored into the red zone FIRST (-16/-8(%rsp), clobber-free per
	-- the SysV ABI) and rsp is then dropped over them — this order keeps every
	-- temp-using instruction (the two stores, incl. any spill reloads regalloc
	-- inserts for them) on the UNADJUSTED rsp that the frame layout is computed
	-- against; only the call itself (whose def/use temps are all precolored,
	-- never spill-rewritten) executes inside the adjusted window. popStackPtr
	-- restores rsp right after the call (before fpRestore, whose frame slots are
	-- also rsp-relative).
	function cg.pushStackPtr(ivTemp, loTemp)
		emit(build.insn("mov", { build.memx(build.rp(4), { dispVal = -16 }), rt(ivTemp) },
			{ uses = { ivTemp } }))
		emit(build.insn("mov", { build.memx(build.rp(4), { dispVal = -8 }), rt(loTemp) },
			{ uses = { loTemp } }))
		emit(build.insn("sub", { build.rp(4), build.imm(16) }, {}))
	end
	function cg.popStackPtr()
		emit(build.insn("add", { build.rp(4), build.imm(16) }, {}))
	end

	-- INCOMING stack-argument load (entry side of the >4-word fast CC): dense GPR
	-- argument word (argRegCount + wordIdx) arrives at [entry_rsp + 8 + 8*wordIdx]
	-- (above the return address, in the caller's outgoing-args area — read-only).
	-- The load renders AFTER the prologue, when rsp sits at the body frame base,
	-- so its displacement depends on the final frame layout (callee-saved pushes +
	-- layout.total): the node carries inArgWord and x86_64_render.renderInsn
	-- computes the real displacement at render time (emit.luau passes it rc).
	function cg.loadInArg(dst, wordIdx)
		local n = build.insn("mov", { rt(dst), build.memx(build.rp(4), { dispVal = 0 }) },
			{ defs = { dst } })
		n.inArgWord = wordIdx
		emit(n)
	end

	-- A plain disp(%rsp) frame-slot load with no modeled base temp (the GC root
	-- slots live at FIXED frame offsets — 16 + 8*index, an ABI constant — so the
	-- displacement needs no layout fixup). Used by the callsite marshalling to
	-- re-read a pointer argument's capability lower from its root slot instead of
	-- keeping the lower web live up to the call.
	function cg.loadFrameSlot(dst, off)
		emit(build.insn("mov", { rt(dst), build.memx(build.rp(4), { dispVal = off }) },
			{ defs = { dst } }))
	end

	-- OUTGOING stack-argument window (callsite side of the >4-word fast CC): dense
	-- argument words beyond argRegCount must sit at 8*j(%rsp) AT the call, densely
	-- packed in declaration word order (pizlonated clang's exact placement). rsp
	-- is dropped over the outgoing area BEFORE the marshalling stores and restored
	-- right after the call (before the exception-flag test, whose flag rides the
	-- %al result register, which add/sub do not touch). The marshalling stores (and
	-- any spill reloads regalloc inserts for their sources) then execute INSIDE the
	-- adjusted window: rewriteSpills stamps every spill node it creates there with
	-- the window's shift (node.spillShift), which renderInsn adds back so the
	-- reload/store still addresses the same frame slot. Only the call itself (whose
	-- def/use temps are all precolored, never spill-rewritten) and the two
	-- adjustments carry no shifted temps. N is a multiple of 16 so rsp stays
	-- 16-byte aligned at the call (a clang-compiled callee reads incoming word
	-- pairs with movaps).
	function cg.beginStackArgs(N)
		local n = build.insn("sub", { build.rp(4), build.imm(N) }, {})
		n.shiftBegin = N
		emit(n)
	end
	function cg.endStackArgs(N)
		local n = build.insn("add", { build.rp(4), build.imm(N) }, {})
		n.shiftEnd = true
		emit(n)
	end
	-- Store one outgoing argument word at 8*j(%rsp) inside the window.
	function cg.storeStackArg(src, off)
		emit(build.insn("mov", { build.memx(build.rp(4), { dispVal = off }), rt(src) },
			{ uses = { src } }))
	end
	-- Store a zero outgoing argument word (a pointer argument whose web carries no
	-- capability marshals a null lower, exactly like emitCapMove's moveZero).
	function cg.storeStackArgZero(off)
		local slot = build.memx(build.rp(4), { dispVal = off })
		slot.size = 8  -- lets the renderer synthesize the movq suffix for the reg-less form
		emit(build.insn("mov", { slot, build.imm(0) }, {}))
	end

	-- Save/restore EFLAGS around an instrumented sequence when the program's flags
	-- are live across it (transform.luau's flag-liveness scan; the arm64 backend
	-- does the same with mrs/msr nzcv). x86 has no flags-to-register move, so the
	-- flags round-trip through the stack: pushfq, then pop into a VIRTUAL temp
	-- (ordinary regalloc-owned temp — it may spill freely), and the mirror image to
	-- restore. Each pair is stack-neutral ON ITS OWN, so an out-of-bounds fail path
	-- taken between the two pairs (the check's branch to the fail stub, which skips
	-- the restore) still leaves rsp exactly where the body keeps it — a bare
	-- pushfq/popfq bracket would misalign the stack by 8 on that path and crash the
	-- fail stub's runtime call. The temp is live across any injected runtime call in
	-- the bracketed sequence, so regalloc gives it a callee-saved register or spills
	-- it (the call's precolored caller-saved defs interfere with it).
	function cg.saveFlags()
		local t = allocTemp({ role = "flags" })
		emit(build.insn("pushfq", {}, {}))
		emit(build.insn("pop", { rt(t) }, { defs = {t} }))
		return t
	end
	function cg.restoreFlags(t)
		emit(build.insn("push", { rt(t) }, { uses = {t} }))
		emit(build.insn("popfq", {}, {}))
	end

	-- effective address + capability selection for a memory operand.
	local dataGlobals = ctx.globals
	function cg.computeAddr(node, mem)
		-- A symbolic displacement (RIP-relative global access, `sym(%rip)`) gets its
		-- capability from the symbol's data object when the driver collected one
		-- (same-file global): the effective address and the payload-base lower
		-- materialize with rip-relative leas, and the original instruction
		-- re-renders verbatim (its dispSym is the user label at its payload
		-- offset). Anything else is rejected with a pointer to `#! global ptr`.
		if mem.dispSym then
			local desc = dataGlobals and not mem.dispReloc and not mem.index
				and mem.base and mem.base.rc == "rip"
				and dataGlobals[mem.dispSym] or nil
			if desc then
				local effTemp = allocTemp({ role = "eff" })
				emit(build.insn("lea", { rt(effTemp), build.memx({ kind = "reg", rc = "rip" },
					{ dispSym = mem.dispSym, dispVal = mem.dispVal }) }, { defs = { effTemp } }))
				local loT = allocTemp({ role = "globallo" })
				emit(build.insn("lea", { rt(loT), build.memx({ kind = "reg", rc = "rip" },
					{ dispSym = desc.baseSym }) }, { defs = { loT } }))
				return effTemp, loT
			end
			error("sarcasm: memory access with a symbolic address cannot be bounds-checked"
				.. " (if '" .. mem.dispSym .. "' is a Fil-C global defined in another module,"
				.. " annotate the instruction with `#! global ptr`): " .. (node.stmt.raw or "?"))
		end
		local baseTemp = mem.base and node.opTemp[mem.base] or nil
		local indexTemp = (mem.index and mem.index.kind == "reg") and node.opTemp[mem.index] or nil
		local capLo
		-- `#! use capability %reg` (stashed on the node by the transform;
		-- `#! new capability %reg` is a deprecated alias): the access is
		-- guarded by the NAMED register's capability, not the base-first
		-- default (validated: the named web carries a capability).
		-- The effective address is still computed from base+index+disp as
		-- written — only the capability source changes.
		local capSel = node.useCapTemp or node.newCapTemp
		if capSel and pf.lowerTemp[capSel] then
			capLo = pf.lowerTemp[capSel]
		elseif baseTemp and pf.isPtr[baseTemp] then capLo = pf.lowerTemp[baseTemp]
		elseif indexTemp and pf.isPtr[indexTemp] then capLo = pf.lowerTemp[indexTemp] end
		local disp = mem.dispVal or 0
		local effTemp = baseTemp
		if indexTemp or disp ~= 0 or not baseTemp then
			effTemp = allocTemp({ role = "eff" })
			cg.effAddr(effTemp, baseTemp, disp, indexTemp, mem.scale or 1)
		end
		return effTemp, capLo
	end

	-- store-barrier marking-state test: branch to `lbl` if FUGC marking is active.
	function cg.markingStateNonzeroBranch(lbl)
		local msp = allocTemp({ role = "scratch" })
		emit(build.insn("mov", { rt(msp), build.memx({ kind = "reg", rc = "rip" },
			{ dispSym = "filc_current_marking_state", dispReloc = "gotpcrel" }) }, { defs = {msp} }))
		local msv = allocTemp({ role = "scratch" })
		emit(build.insn("mov", { rt(msv, 32), build.memx(rt(msp), {}) }, { defs = {msv}, uses = {msp} }))
		emit(build.insn("test", { rt(msv, 32), rt(msv, 32) }, { uses = {msv} })); branch("jne", lbl)
	end
	-- branch to exception epilogue if the call's return flag bit 0 is set (al bit0).
	function cg.exceptionBranch(lbl)
		emit(build.insn("test", { build.rp(M.cc.retFlag, 8), build.imm(1) }, { uses = { pColored(M.cc.retFlag) } }))
		branch("jne", lbl)
	end

	function cg.clearRetFlag()
		emit(build.insn("mov", { build.rp(M.cc.retFlag, 32), build.imm(0) }, { defs = { pColored(M.cc.retFlag) } }))
	end
	-- Redirect an rsp/rbp-relative address computation landing inside a fixed alloca region
	-- to the allocation pointer (gcc -O0 re-derives the buffer address per use). Returns
	-- true if it replaced the node; a direct stack-relative *access* into a region is
	-- rejected (mirrors arm64's regionRedirect). All matching happens in rsp-relative
	-- frame offsets: an rbp-relative displacement d is normalized to frameSize + d (rbp =
	-- rsp + frameSize when it is the frame pointer).
	local fctx = ctx.frameCtx

	-- FP/SIMD save/restore around sarcasm-injected RETURNING runtime calls (the pollcheck
	-- slow path, filc_allocate, the ptr-store slow paths, the Phase-2 atomic pointer
	-- load/store/compare-exchange runtime calls). The source program never saw
	-- these calls, so its live xmm/ymm/zmm values would be silently clobbered by them:
	-- SysV makes every FP/SIMD register caller-saved. libpizlo itself is compiled
	-- SSE2-only (pizfix/lib/libpizlo.a contains xmm instructions but zero ymm/zmm),
	-- but the injected-call paths can ALSO reach libyolocimpl.so, whose string/memory
	-- routines are EVEX-compiled (__memcpy_evex_unaligned_erms, __strcmp_evex, ...:
	-- thousands of zmm16-31 and k0-7 uses). Those registers are caller-saved scratch
	-- for the runtime, so live xmm/ymm/zmm16-31 and k0-7 state is at risk exactly
	-- like xmm0-15, and the save/restore below covers every vector register the body
	-- names (32 slots when any xmm/ymm/zmm >= 16 appears, 16 otherwise) plus k0-7
	-- (saved unconditionally when the body uses any opmask register — vecRegOf
	-- deliberately keeps k-regs out of the liveness model). mm and x87 state remain
	-- unreachable for these paths.
	--
	-- Save/restore is LIVENESS- AND WIDTH-AWARE: cg.fpSave/fpRestore below emit
	-- PLACEHOLDER nodes (kind "fpsave"/"fprestore"), and cg.expandFpSaves — called
	-- from transformFunction AFTER the whole body is emitted but BEFORE the
	-- frameSlot numRoots shift — runs a backward per-vector-register WIDTH
	-- liveness over the emitted node stream and replaces each placeholder with
	-- exactly the movs the analysis proves live at that point: a register live
	-- only in its low 32 bits is saved with movss, 64 bits with movsd, 128 with
	-- movdqu, 256 with vmovdqu, 512 with vmovdqu64; a register dead across the
	-- call emits nothing. The width semantics come from fp.vecEffects
	-- (x86_64_fp): each instruction reports, per vector register, a use width
	-- and a "provided width" — the width up to which it PROVIDES values
	-- (written or architecturally zeroed), with bits above flowing through
	-- from before. E.g. a movss LOAD provides 16 bytes (writes 4, zeroes
	-- [4,16)), so a later 16-byte read (movaps store) sees the movss's zeros
	-- — liveness records 16 bytes live, and the save/restore preserves the
	-- zeros; an addsd provides 8 (the low qword result) while the upper bytes
	-- flow through. User calls (the pizlonatedFI* thunks) KILL all vector
	-- state (SysV: values live across a call were already broken in the source
	-- program); the injected filc_* runtime calls are TRANSPARENT (the
	-- placeholder pair around them preserves the state).
	--
	-- A nil vecBytes (a GPR-only function, or one using only k-opmask/x87/mm
	-- registers) emits NOTHING — same zero-cost behavior as before. For x87/mm
	-- state this is sound NOT because "an SSE2-only runtime cannot clobber
	-- x87" — libpizlo.a DOES contain x87 instructions (the fldt/fstpt in the
	-- zmath long-double wrappers) — but because the only callees these
	-- injected-call paths can reach (filc_pollcheck_slow, filc_allocate, the
	-- two store-barrier slow paths filc_object_ensure_aux_ptr_outline /
	-- filc_store_barrier_for_lower_slow, and the Phase-2 atomic pointer
	-- operations filc_load_ptr_atomic_with_manual_tracking_outline /
	-- filc_store_ptr_atomic_outline / filc_strong_cas_ptr_with_manual_tracking
	-- — whose internal ensure-aux/box-allocation/barrier paths are the same
	-- allocation/barrier code the first four already cover) never execute
	-- those wrappers. k-opmask and zmm16-31 state CAN be clobbered on these
	-- paths whenever they reach libyolocimpl's EVEX-compiled routines; both
	-- are covered by the extended save (see the expandFpSaves comment).
	--
	-- The save area sits above the GC root area of the synthesized frame
	-- (reserved by x86_64_render.computeLayout) at the ORIGINAL 16*vecBytes
	-- reservation — the optimization only elides/narrows the movs, never the
	-- reservation; slot i stays at 16+i*vecBytes(%rsp) here — relative to base
	-- 16, like the materialized clusters — and transform shifts it past the
	-- root area once numRoots is known (the frameSlot tag is what the shift
	-- looks for). The unaligned forms (movss/movsd/movdqu/vmovdqu/vmovdqu64)
	-- need no alignment beyond the frame's guaranteed 16-byte body alignment,
	-- and each is at or below the ISA level the function's own widest register
	-- class already requires (a live 32-byte width implies the function uses
	-- ymm registers, so vecBytes is 32 and AVX encodings are fair game;
	-- likewise 64/AVX512F). The vector registers render by name (fixed
	-- operands; the register allocator never models xmm-class registers); the
	-- slot base is a fixed %rsp operand (4 = rsp).
	local fpInfo = fctx and fctx.fp
	local vecBytes = fpInfo and fpInfo.vecBytes or nil
	if not vecBytes and fpInfo and (fpInfo.saveBytes or 0) > 0 then
		-- The save area was forced by an FP SIGNATURE (entry args/return in the xmm
		-- registers, or an FP callsite — see x86_64_frame.analyzeFrame and
		-- transform.luau) even though the body names no vector register: the save/
		-- restore machinery must still be ACTIVE, or an FP argument web would be
		-- silently clobbered by an injected runtime call (the pollcheck slow path,
		-- filc_allocate, the ptr-store barriers). The live widths in this situation
		-- are at most 8 bytes (float/double), i.e. within the xmm class.
		vecBytes = 16
	end
	function cg.fpSave()
		if not vecBytes then return end  -- no vector state the runtime could clobber
		emit({ kind = "fpsave" })
	end
	function cg.fpRestore()
		if not vecBytes then return end
		emit({ kind = "fprestore" })
	end

	-- Post-transform expansion (x86_64 only; the arm64 backend has no such
	-- hook — its fpSave/fpRestore are no-ops and emit no placeholders).
	-- Replaces every fpsave/fprestore placeholder in `out` (spliced in place)
	-- with the exact vector save/restore movs the width-liveness analysis
	-- proves live at that point. Runs pre-regalloc; the expanded movs carry
	-- no temps (named fixed operands), exactly like the old unconditional
	-- save/restore did.
	function cg.expandFpSaves(out, _fctx)
		local any = false
		for _, node in out do
			if node.kind == "fpsave" or node.kind == "fprestore" then any = true break end
		end
		if not any then return end
		if not vecBytes then
			-- No vector state: placeholders expand to nothing (fpSave above
			-- already emits none in that case; this is belt-and-braces).
			for i = #out, 1, -1 do
				if out[i].kind == "fpsave" or out[i].kind == "fprestore" then table.remove(out, i) end
			end
			return
		end

		-- CFG-shaped instruction list parallel to `out` (mirrors the
		-- control-flow reading in sarcasm.luau's buildRaInsns/packSpillSlots):
		-- labels define blocks; branch/call/ret control comes from
		-- node.control or the lifted classification node.c. The per-node
		-- vector effect is memoized here (uses/defs width maps from
		-- fp.vecEffects, or a kill/transparent tag for calls).
		local insns = {}
		for idx, node in out do
			if node.kind == "label" then
				table.insert(insns, { isLabelDef = node.name, fallsThrough = true })
			elseif node.kind == "insn" then
				local control = node.control or (node.c and node.c.control) or "none"
				local targets = node.targets or (node.c and node.c.targets)
				local ft = node.fallsThrough
				if ft == nil then ft = node.c and node.c.fallsThrough end
				if ft == nil then ft = true end
				local d = { targets = targets, fallsThrough = ft }
				if control == "call" or control == "indcall" then
					-- Injected filc_* runtime calls are TRANSPARENT: the
					-- placeholder pair around them preserves the state, so
					-- liveness flows straight through. Every other call —
					-- synth calls to the pizlonatedFI* user thunks, or a
					-- passthrough user call that rode the lift — clobbers
					-- every caller-saved vector register (all of them, per
					-- SysV): a value "live" across such a call was already
					-- broken in the source program, so killing is sound and
					-- precise.
					local sym = node.operands and node.operands[1]
					local txt = (sym and sym.kind == "sym") and sym.text or ""
					if not node.stmt and (string.sub(txt, 1, 5) == "filc_" or node.globalGetter) then
						-- Injected global-getter / slow-init calls (globalGetter) are
						-- transparent like the filc_* runtime calls: the fpSave/
						-- fpRestore pair around them preserves the state.
						d.transparent = true
					else
						d.killAll = true
						-- An FP-argument call CONSUMES the FP argument values the
						-- source program placed in xmm0..xmm7 (per the fast CC
						-- they ride the call in the vector registers): they are
						-- live UP TO the call even though the call clobbers
						-- them. Tagged by the transform (fpVecUses =
						-- { [xmmNum] = widthBytes }); applied after the killAll
						-- below so the consumed args are live-before the call
						-- (and get saved around an INJECTED call sitting between
						-- the argument's definition and this call).
						d.fpUses = node.fpVecUses
					end
				elseif node.stmt then
					d.uses, d.defs = fp.vecEffects(node.stmt)
				elseif node.vecStmt then
					-- synthesized FP marshalling (cg.storeFpArg / cg.loadFpRet): the
					-- same vector effects as the user instruction it stands for (a
					-- buffer store USES the FP argument register; a buffer load
					-- DEFINES the FP return register)
					d.uses, d.defs = fp.vecEffects(node.vecStmt)
				end
				table.insert(insns, d)
			else
				-- fpsave/fprestore placeholders, prologue/epilogue/rootstore/
				-- raw: plain fall-through nodes, no vector effect.
				table.insert(insns, { fallsThrough = true })
			end
		end

		-- Backward width-liveness: state = vector reg num -> live width in
		-- bytes (4/8/16/32/64); join at merges = per-register max. Transfer
		-- (backward over an instruction): a def with providedWidth p kills
		-- liveness at or below p (the instruction supplies those bytes
		-- itself); bytes above p flow through. A use at width u adds liveness:
		--   live[n] = max(u, live[n] > p and live[n] or 0)
		-- — an RMW (addsd reads AND writes xmm0) thereby counts its own read.
		local function applyNode(d, live)
			if d.killAll then
				for n in pairs(live) do live[n] = nil end
				-- FP args consumed by the call (see the fpVecUses comment above):
				-- live up to the call even though the call clobbers them.
				if d.fpUses then
					for n, u in pairs(d.fpUses) do
						if (live[n] or 0) < u then live[n] = u end
					end
				end
				return
			end
			if d.transparent or not d.uses then return end
			for n, p in pairs(d.defs) do
				if (live[n] or 0) <= p then live[n] = nil end
			end
			for n, u in pairs(d.uses) do
				if (live[n] or 0) < u then live[n] = u end
			end
		end

		local blocks = regalloc.buildCFG(insns)
		local liveIn, liveOut = {}, {}
		for bi in ipairs(blocks) do liveIn[bi] = {}; liveOut[bi] = {} end
		local function stateEq(a, b)
			for n, w in pairs(a) do if b[n] ~= w then return false end end
			for n, w in pairs(b) do if a[n] ~= w then return false end end
			return true
		end
		local changed = true
		while changed do
			changed = false
			for bi = #blocks, 1, -1 do
				local b = blocks[bi]
				local newOut = {}
				for _, s in b.succ do
					for n, w in pairs(liveIn[s]) do
						if (newOut[n] or 0) < w then newOut[n] = w end
					end
				end
				local live = {}
				for n, w in pairs(newOut) do live[n] = w end
				for k = #b.insns, 1, -1 do applyNode(insns[b.insns[k]], live) end
				if not stateEq(newOut, liveOut[bi]) or not stateEq(live, liveIn[bi]) then
					changed = true
					liveOut[bi] = newOut
					liveIn[bi] = live
				end
			end
		end
		-- per-node live-after (backward within block)
		local liveAfterNode = {}
		for bi, b in blocks do
			local live = {}
			for n, w in pairs(liveOut[bi]) do live[n] = w end
			for k = #b.insns, 1, -1 do
				liveAfterNode[b.insns[k]] = live
				local nl = {}
				for n, w in pairs(live) do nl[n] = w end
				live = nl
				applyNode(insns[b.insns[k]], live)
			end
		end

		-- Expand each placeholder from its local liveness state (the pair
		-- around an injected call naturally agrees: only GPR marshalling movs
		-- and the transparent call intervene). Registers 0-15 always save with
		-- the legacy/VEX mnemonics; registers 16-31 (live only when the body
		-- names them, i.e. saveSlots == 32) must use their EVEX forms — gas has
		-- no VEX encodings for xmm16-31, so movdqu/vmovdqu/movss/movsd are
		-- unsupported there and vmovdqu64/vmovss/vmovsd take over. Opmask
		-- registers k0-7 have no liveness model (vecRegOf ignores them), so
		-- when the body uses any of them ALL of them are saved — over-saving
		-- is sound, and this is the only way to be safe against EVEX-compiled
		-- runtime code (libyolocimpl's _evex routines clobber k0-7 freely).
		local SAVE_FORM = { [4] = "movss", [8] = "movsd", [16] = "movdqu",
			[32] = "vmovdqu", [64] = "vmovdqu64" }
		local SAVE_CLASS = { [4] = "xmm", [8] = "xmm", [16] = "xmm", [32] = "ymm", [64] = "zmm" }
		local EVEX_FORM = { [4] = "vmovss", [8] = "vmovsd", [16] = "vmovdqu64",
			[32] = "vmovdqu64", [64] = "vmovdqu64" }
		local saveSlots = fpInfo.saveSlots or 16
		local usesK = fpInfo.usesK
		local function emitPairMovs(isRestore, live, into)
			for n = 0, saveSlots - 1 do
				local w = live[n]
				if w then
					local mnem = (n > 15) and EVEX_FORM[w] or SAVE_FORM[w]
					if not mnem or w > vecBytes then
						error("sarcasm: internal error: bad live vector width "
							.. tostring(w) .. " (vecBytes " .. tostring(vecBytes) .. ")")
					end
					local vreg = { kind = "reg", rc = "xmm", num = n, width = w * 8, name = SAVE_CLASS[w] .. n }
					local slot = build.memx(build.rp(4), { dispVal = 16 + n * vecBytes })
					-- Synthesized-frame slot: transform shifts its dispVal
					-- past the GC root area (roots must occupy
					-- [16, 16+8*numRoots) for the Fil-C GC).
					slot.frameSlot = true
					if isRestore then
						table.insert(into, build.insn(mnem, { vreg, slot }, {}))
					else
						table.insert(into, build.insn(mnem, { slot, vreg }, {}))
					end
				end
			end
			if usesK then
				-- The opmask area sits right after the vector slots
				-- (16 + saveSlots*vecBytes + j*8); same frameSlot shift.
				for j = 0, 7 do
					local kreg = { kind = "reg", rc = "xmm", num = j, width = 64, name = "k" .. j }
					local slot = build.memx(build.rp(4), { dispVal = 16 + saveSlots * vecBytes + j * 8 })
					slot.frameSlot = true
					if isRestore then
						table.insert(into, build.insn("kmovq", { kreg, slot }, {}))
					else
						table.insert(into, build.insn("kmovq", { slot, kreg }, {}))
					end
				end
			end
		end
		local expanded = {}
		for idx, node in out do
			if node.kind == "fpsave" then
				emitPairMovs(false, liveAfterNode[idx] or {}, expanded)
			elseif node.kind == "fprestore" then
				emitPairMovs(true, liveAfterNode[idx] or {}, expanded)
			else
				table.insert(expanded, node)
			end
		end
		for i = #out, 1, -1 do out[i] = nil end
		for _, node in expanded do table.insert(out, node) end
	end

	local function stackOff(base, disp)
		if not base then return nil end
		if base.rc == "sp" then return disp end
		-- rbp-relative offsets normalize with the frame rewrite's own base (rbp =
		-- rsp + rbpNorm; 0 for the rbp-at-the-bottom layout) — NOT the bare frame
		-- size, which only agrees at the rbp-at-the-top layout.
		if fctx and fctx.usesFp and base.name == "rbp" then
			return (fctx.rbpNorm or fctx.frameSize) + disp
		end
		return nil  -- not a stack base (e.g. rbp as an ordinary GPR with frame-pointer omission)
	end
	-- The +8 rule for a clone statement's rsp-relative operand at a static depth
	-- (the return-address compensation — localcall.luau; keyed exactly like the
	-- frame rewrite's slot/region coordinates).
	local function cloneStackOff(node, base, disp)
		local off = stackOff(base, disp)
		if off and base.rc == "sp" and node.stmt and node.stmt.localClone then
			local d = node.stmt.x86_depth
			if d ~= nil and type(d) ~= "table" and d ~= "dyn" and fctx and fctx.callerDepth then
				off = off + fctx.callerDepth - d - 8
			end
		end
		return off
	end
	function cg.regionRedirect(node, regionOf)
		-- A materialized FP frame-slot operand (disp(%rsp) into sarcasm's synthesized
		-- frame) is NOT an input-frame access; its positive offset can numerically
		-- coincide with an alloca-region frame offset. Never redirect/reject it.
		local fsmem = M.memOperand(node.stmt)
		if fsmem and fsmem.frameSlot then return false end
		local m = isa.baseMnemonic(string.lower(node.stmt.mnemonic))
		local ops = node.stmt.operands
		-- Region membership, reproducing the frame rewrite exactly: inside an
		-- {anchored} region scope (the statement's depth is the region's table) raw
		-- displacements are that region's coordinates; inside a {dynreg} scope rsp =
		-- region base + bias, so the region offset is bias + disp (-8 for a clone —
		-- the +8 rule) and membership is unconditional (the runtime bounds check
		-- covers the dynamic extent); plain "dyn" has no valid coordinates;
		-- everything else matches the static regions (normalized frame coordinates —
		-- rbp-relative offsets are static at any depth). Returns the region and the
		-- REGION-RELATIVE offset of the access.
		local function nodeRegion(off, isSpBase)
			local d = node.stmt.x86_depth
			if isSpBase and not node.stmt.x86_prologue and type(d) == "table" then
				if d.anchored then
					-- A clone statement keys 8 lower (the +8 rule — the return-address
					-- word sits between the clone's rsp and the caller's frame, so the
					-- same physical slot is spelled 8 higher inside a clone). The frame
					-- slot model (slotKeyOf) and the dynreg branch below already key it
					-- this way; the anchored region must agree (rsaz_512_mul's
					-- `leaq 8(%rsp),%rdi` t[] base in the __rsaz_512_mul clone — it must
					-- redirect to region+0, not region+8).
					local ao = off
					if node.stmt.localClone then ao = ao - 8 end
					if ao >= d.anchored.base and ao < d.anchored.base + d.anchored.size then
						return d.anchored, ao - d.anchored.base
					end
					return nil
				end
				if d.dynreg then
					local rl = off + d.dynreg.bias
					if node.stmt.localClone then rl = rl - 8 end
					return d.dynreg, rl
				end
				return nil
			end
			local r = regionOf(off, false)
			if r then return r, off - r.base end
			return nil
		end
		-- Define the redirected pointer's lower: the ptrflow-SEEDED lower temp (shared
		-- by every copy of the region pointer) is defined from the region's own lower;
		-- an unseeded def simply shares it.
		local function seedRegionLower(rd, r)
			pf.isPtr[rd] = true
			local lo = pf.lowerTemp[rd]
			if lo and lo ~= r.loTemp then
				cg.move(lo, r.loTemp, true)
			else
				pf.lowerTemp[rd] = r.loTemp
			end
		end
		if m == "lea" then
			local mem = M.memOperand(node.stmt)
			if mem and not mem.dispSym and mem.base then
				local off = cloneStackOff(node, mem.base, mem.dispVal or 0)
				local r, reloff = nil, nil
				if off then r, reloff = nodeRegion(off, mem.base and mem.base.rc == "sp") end
				if r then
					local rd = node.defTemps[1]
					if mem.index and mem.index.kind == "reg" then
						-- Indexed region lea (`lea D(%rsp,%idx,s),%rd`): the
						-- single-step spelling of the sanctioned two-step
						-- idiom (`lea D(%rsp),%t` + `lea (%t,%idx,s),%rd`).
						-- Membership keys on the static displacement exactly
						-- like the two-step's first lea; the dynamic index is
						-- covered by the runtime bounds checks at every use
						-- (fail-closed: an out-of-region index traps there).
						-- The value rides the region pointer with the index
						-- preserved, and the result shares the region's
						-- capability — bit-identical behavior to the two-step
						-- form (same address, same lower).
						local idxT = node.opTemp and node.opTemp[mem.index] or nil
						if idxT then
							cg.effAddr(rd, r.ptrTemp, reloff, idxT, mem.scale or 1)
							seedRegionLower(rd, r)
							return true
						end
						-- No temp for the index (unmodeled register class):
						-- fall through to the ordinary checked path below.
					else
						cg.addImm(rd, r.ptrTemp, reloff)
						seedRegionLower(rd, r)
						return true
					end
				end
			end
		end
		if m == "mov" and ops[1] and ops[1].kind == "reg" and ops[1].rc ~= "sp"
			and ops[2] and ops[2].rc == "sp" then
			local r, reloff = nodeRegion(cloneStackOff(node, { kind = "reg", rc = "sp" }, 0), true)
			if r then
				local rd = node.defTemps[1]
				-- rsp+0 redirected into the region: buffer + (0 - base), NOT buffer + 0.
				cg.addImm(rd, r.ptrTemp, reloff)
				seedRegionLower(rd, r)
				return true
			end
		end
		local mem = M.memOperand(node.stmt)
		if mem and not mem.dispSym then
			local off = cloneStackOff(node, mem.base, mem.dispVal or 0)
			local r, reloff = nil, nil
			if off then r, reloff = nodeRegion(off, mem.base and mem.base.rc == "sp") end
			if r then
				-- A DIRECT stack-relative access into a region (fixed, or dynamic —
				-- indexed forms included): rewrite the memory operand to ride the
				-- region pointer (the region-relative offset off the region temp)
				-- and return false so the walk lowers it through the ordinary
				-- checked path (bounds at the access's width/alignment, CanWrite
				-- for stores — GC buffers are writable). The frame rewrite proved
				-- membership (static for fixed regions; the exact rsp = base +
				-- bias identity for dynamic ones — the runtime bounds check
				-- covers the dynamic extent); the redirect keeps real bounds.
				local newBase = rt(r.ptrTemp)
				mem.base = newBase
				mem.dispVal = reloff
				node.opTemp[newBase] = r.ptrTemp
				table.insert(node.useTemps, r.ptrTemp)
				if node.c and node.c.uses then table.insert(node.c.uses, newBase) end
			end
		end
		-- A memory access THROUGH a saved-rsp carrier register that lands inside the
		-- current {anchored} region scope (the poly1305 frame-anchor shape: the
		-- pre-alloca `leaq -88(%rsp), %r11` parks entry_rsp - 88; the subq-anchored
		-- region base is entry_rsp - scopeDepth exactly, so the access addresses
		-- region offset scopeDepth - carrier depth + disp). The frame rewrite
		-- proved membership and passed the statement through (st.x86_regionCarrier);
		-- retarget the operand to the region temp exactly like the direct forms.
		if mem and not mem.dispSym and mem.base and mem.base.kind == "reg"
			and not mem.index and node.stmt.x86_regionCarrier then
			local e = node.stmt.x86_smap and node.stmt.x86_smap[mem.base.name]
			local d = node.stmt.x86_depth
			if e and not e.redefined and not e.slot and not e.roff
				and type(d) == "table" and d.anchored and d.scopeDepth then
				local r = d.anchored
				local reloff = d.scopeDepth - e.depth + (mem.dispVal or 0) - r.base
				if reloff >= 0 and reloff < r.size then
					local newBase = rt(r.ptrTemp)
					mem.base = newBase
					mem.dispVal = reloff
					node.opTemp[newBase] = r.ptrTemp
					table.insert(node.useTemps, r.ptrTemp)
					if node.c and node.c.uses then table.insert(node.c.uses, newBase) end
				end
			end
		end
		return false
	end

	-- passthrough special-casing: cltq -> renameable movslq on the rax web; lea does not
	-- access memory (its [mem] is an address computation), so pass it through unchecked.
	-- Memory-neutral hints (prefetch*) also pass through with no access check.
	function cg.lowerSpecial(node, mem)
		-- A pinned instruction (div/mul/cmpxchg/xchg/shift-by-%cl/high-byte forms)
		-- is emitted by emitPinned with its pin moves — never rewrite it here (the
		-- setcc widening does not apply to a high-byte setcc, which the pin model
		-- already makes exact; the other rewrites never see a pinned node).
		if node.c and node.c.pin then return false end
		-- A materialized FP frame-slot operand is a real disp(%rsp) into sarcasm's
		-- own synthesized frame: the access is always in-bounds (the frame contains
		-- its own slots), so it needs no access check — pass the instruction
		-- through untouched. (`;! load/store ptr` on a stack access was already
		-- rejected by the frame rewrite; keep a defensive copy here.)
		if mem and mem.frameSlot then
			local ann = node.stmt.annotation
			if ann and sig.PTR_ANNOTATIONS[ann] then
				error("sarcasm: `" .. ann .. "` on a stack-frame access is not supported"
					.. " (frame slots are virtualized/materialized, not real memory): "
					.. (node.stmt.raw or "?"))
			end
			emit(node); return true
		end
		local base = isa.baseMnemonic(node.stmt.mnemonic)
		local fpe = fp.lookup(string.lower(node.stmt.mnemonic))
		if fpe and fpe.noCheck then emit(node); return true end
		-- Multi-byte padding NOPs (nop/endbr64/nopw/nopl) are zero-effect, and a
		-- padding operand like 0x0(%rax,%rax,1) is a dummy encoding hint, NOT a
		-- memory access: pass through with no access check (the frame rewrite
		-- already declined to virtualize the operand; without this the dummy mem
		-- operand would be bounds-checked as a real access below).
		if base == "nop" then emit(node); return true end
		-- fnstsw/fstsw with a register destination write ONLY %ax (the encoding is
		-- hardwired): pin the access to physical rax and copy out, rather than
		-- letting regalloc rename the operand into an invalid `fnstsw %dx`.
		-- The 16-bit write is PARTIAL (bits 16-63 of the enclosing register are
		-- preserved), and classify models the destination web as an RMW: pin the
		-- reaching web IN as well, so the preserved bits are the web's own
		-- (a copy-out alone would leave whatever regalloc happened to park in
		-- physical rax above the new %ax — the partial-write miscompile).
		if (base == "fnstsw" or base == "fstsw") and node.stmt.operands[1]
			and node.stmt.operands[1].kind == "reg" and node.stmt.operands[1].rc == "gpr" then
			local t = node.defTemps and node.defTemps[1]
			local tu = node.useTemps and node.useTemps[1]
			if tu then cg.move(pColored(0), tu, true) end
			emit(build.insn(node.stmt.mnemonic, { build.rp(0, 16) }, { defs = { pColored(0) }, uses = { pColored(0) } }))
			if t then cg.move(t, pColored(0), true) end
			return true
		end
		if base == "cltq" then
			local t = node.defTemps and node.defTemps[1]
			if t then emit(build.insn("movslq", { rt(t, 64), rt(t, 32) }, { defs = {t}, uses = {t} })) else emit(node) end
			return true
		elseif base == "cbw" then
			local t = node.defTemps and node.defTemps[1]
			if t then emit(build.insn("movsbw", { rt(t, 16), rt(t, 8) }, { defs = {t}, uses = {t} })) else emit(node) end
			return true
		elseif base == "cwde" then
			local t = node.defTemps and node.defTemps[1]
			if t then emit(build.insn("movswl", { rt(t, 32), rt(t, 16) }, { defs = {t}, uses = {t} })) else emit(node) end
			return true
		elseif base == "setcc" then
			-- SETcc writes exactly the LOW BYTE of its destination register, but the
			-- web model has no subregister view (webs are keyed by register number):
			-- classify models a register-destination setcc as a FULL-WIDTH def of the
			-- destination web. Make the hardware match the model by appending a
			-- zero-extension of the low byte — the same movzbl/movzx the manual idiom
			-- (used by every pre-existing setcc test) writes by hand: the 32-bit
			-- destination write zeroes bits 32-63, so after the pair the whole web
			-- holds 0/1, which is exactly what the full-width def claims. The
			-- extension writes no flags, so the setcc's condition codes stay live for
			-- any consumer (jcc/setcc/cmov) that follows. A MEMORY destination is a
			-- complete 8-bit store — nothing to widen; it takes the normal
			-- (bounds-checked) path untouched. A frame-SLOT pseudo-register
			-- destination (a virtualized `setcb 8(%rsp)`) is the same shape: the
			-- single byte is the whole access — widening it would zero the slot's
			-- other seven bytes, which the input program never wrote. classify
			-- models the slot destination as an RMW (a partial write), and the
			-- literal setcc is already exact. (A %ah high-byte destination never
			-- reaches here: classify models it as a pinned RMW of the enclosing
			-- register's web, and the pin guard above routes it to emitPinned.)
			local d = node.stmt.operands[1]
			if d and d.kind == "reg" and d.rc == "gpr" and not d.isSlot then
				local t = node.defTemps and node.defTemps[1]
				if not t then
					error("sarcasm: internal error: setcc destination has no web temp: "
						.. (node.stmt.raw or node.stmt.mnemonic))
				end
				emit(node)
				-- Both operands reference the SAME web temp as the setcc's def, so
				-- regalloc colors them identically and the pair reads/writes one
				-- colored register (the RMW shape cltq's rewrite already uses). The
				-- renderer emits `movzbl %Xb, %eX` (AT&T) / `movzx eX, Xb` (Intel).
				emit(build.insn("movzx", { rt(t, 32), rt(t, 8) }, { defs = {t}, uses = {t} }))
				return true
			end
			return false
		elseif base == "lea" then
			emit(node); return true
		end
		return false
	end

	-- Implicit-GPR pinning (div/idiv/mul/1-op imul, mulx, cmpxchg, cqo/cdq/cwd,
	-- and the zero-operand implicit-only family cpuid/rdtsc/rdtscp/xgetbv/
	-- rdpkru/wrpkru/monitor/mwait).
	-- These instructions read and/or write physical rax/rdx beyond what their
	-- explicit operands say; classify names those effects with implicit def/use
	-- operand tables (recorded in node.c.pin), so the lift's web analysis
	-- connects them to the surrounding code. We then pin the implicit registers
	-- to the physical registers with synthesized moves AROUND the instruction —
	-- exactly the way the fnstsw->rax pin and the calling-convention boundary
	-- copies work: before the instruction, copy the CURRENT webs of the
	-- implicit-use registers into their physical registers; after it, copy the
	-- physical registers back out into the fresh webs that the lift made the
	-- reaching defs for subsequent reads of rax/rdx. The instruction keeps only
	-- its explicit operands in the emitted text; its def/use temp lists (what
	-- regalloc sees) reference the precolored physical temps for the implicit
	-- registers, so the register allocator colors everything else AROUND the
	-- fixed rax/rdx effects. Regalloc coalescing makes the pin moves free in
	-- the common case (the user just wrote rax/rdx).
	--
	-- An explicit operand that NAMES a pinned register (cmpxchg %rsi, %rax;
	-- mulxq %rsi, %rdx, %rax; idiv %rdx) is rendered as the FIXED physical
	-- register: renaming it would desync it from the implicit pin (its web's
	-- value flows through the physical register via the pin-in/pin-out copies,
	-- so the semantics stay correct either way).
	--
	-- Called by the transform wherever a plain node would be emitted (with or
	-- without a preceding access check); returns false for unpinned nodes.
	function cg.emitPinned(node)
		local c = node.c
		local pin = c and c.pin
		if not pin then return false end
		local pinnedNums = {}
		for _, r in (pin.uses or {}) do pinnedNums[r.num] = true end
		for _, r in (pin.defs or {}) do pinnedNums[r.num] = true end
		-- private opTemp copy (spill rewriting mutates it in place)
		local opT = {}
		for k, v in (node.opTemp or {}) do opT[k] = v end
		-- pin-in: every modeled use of a pinned register (an implicit table or an
		-- explicit operand naming the register) gets its reaching web copied into
		-- the physical register. (Same-web duplicates collapse: the lift keys use
		-- webs by register number.)
		local moved = {}
		for _, r in (c.uses or {}) do
			if pinnedNums[r.num] and not moved[r.num] then
				moved[r.num] = true
				local t = opT[r]
				if t then cg.move(pColored(r.num), t, true) end
			end
		end
		-- the pinned instruction itself
		local defTemps, useTemps, newops = {}, {}, {}
		for _, o in (node.stmt.operands or {}) do
			if o.kind == "reg" and o.rc == "gpr" and pinnedNums[o.num] then
				-- A HIGH-BYTE operand (%ah/%ch/%dh/%bh) must render its own name: it
				-- names bits 8-15 of the pinned physical register, which is exactly
				-- where the pin landed the enclosing web. build.rp alone would
				-- render the LOW byte of the physical register (regName(num, 8) has
				-- no high-byte spelling — silently turning %ah into %al), and the
				-- original operand table cannot be kept either (it may be a
				-- classified table whose opTemp entry renders the colored low
				-- byte). Tag the fixed register with the high-byte name; the
				-- renderer honors it.
				if o.width == 8 and HIGH_BYTE_NAME[o.name or ""] then
					local rp = build.rp(o.num, o.width)
					rp.highByte = o.name
					table.insert(newops, rp)
				else
					table.insert(newops, build.rp(o.num, o.width))
				end
			else
				table.insert(newops, o)
			end
		end
		for _, r in (c.uses or {}) do
			local t = pinnedNums[r.num] and pColored(r.num) or opT[r]
			if t then table.insert(useTemps, t) end
		end
		for _, r in (c.defs or {}) do
			local t = pinnedNums[r.num] and pColored(r.num) or opT[r]
			if t then table.insert(defTemps, t) end
		end
		emit({
			kind = "insn", synth = true, mnemonic = node.stmt.mnemonic,
			operands = newops, opTemp = opT,
			defTemps = defTemps, useTemps = useTemps,
			control = "none", fallsThrough = true,
			lockPrefix = node.stmt.lockPrefix,  -- `lock cmpxchg...` rides through the pin
		})
		-- pin-out: every modeled def of a pinned register gets the physical
		-- register copied out into its fresh web (the reaching def for all
		-- subsequent reads of that register).
		local outDone = {}
		for _, r in (c.defs or {}) do
			local t = opT[r]
			if pinnedNums[r.num] and t and not outDone[t] then
				outDone[t] = true
				cg.move(t, pColored(r.num), true)
			end
		end
		return true
	end
	function cg.checkMemInsn(node, mem) end  -- x86: any memory operand is a real access

	-- Extract the lane-enable mask of a supported masked memory access into a GPR
	-- temp, truncated to exactly the access's lane count (info.lanes), for the
	-- mask-refined bounds slow paths. The mask registers (k0-7 / the mask vector)
	-- are never modeled by the web analysis — they ride the instruction verbatim —
	-- so this reads the architectural mask directly at the access. (kmovw zero-
	-- extends its 16-bit result; kmovd/kmovq cover the 32/64-lane forms. N<16
	-- needs an explicit AND. The vmovmsk forms yield exactly N sign bits.)
	function cg.maskToGpr(dstTemp, info)
		if info.maskReg then
			local N = info.lanes
			local mnem = (N <= 16) and "kmovw" or (N <= 32) and "kmovd" or "kmovq"
			local kop = { kind = "reg", rc = "xmm", num = info.maskReg,
				name = "k" .. info.maskReg, width = 16 }
			emit(build.insn(mnem, { rt(dstTemp, (N == 64) and 64 or 32), kop },
				{ defs = { dstTemp } }))
			if N < 16 then cg.andImmInPlace(dstTemp, (2 ^ N) - 1) end
		else
			local mnem = info.movmskPd and "vmovmskpd" or "vmovmskps"
			emit(build.insn(mnem, { rt(dstTemp, 32), info.maskVecOp }, { defs = { dstTemp } }))
		end
	end

	-- one access-check-fail stub, appended to `list` (arch-specific arg regs + rip origin).
	function cg.failStub(list, lbl, ivTemp, loTemp, acoSym)
		local a1, a2, a3 = M.cc.rtArg(1), M.cc.rtArg(2), M.cc.rtArg(3)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(ivTemp) }, { defs = { pColored(a1) }, uses = {ivTemp} }))
		if loTemp then
			table.insert(list, build.insn("mov", { build.rp(a2), rt(loTemp) }, { defs = { pColored(a2) }, uses = {loTemp} }))
		else
			table.insert(list, build.insn("mov", { build.rp(a2), build.imm(0) }, { defs = { pColored(a2) } }))
		end
		table.insert(list, build.insn("lea", { build.rp(a3), build.memx({ kind = "reg", rc = "rip" }, { dispSym = acoSym }) },
			{ defs = { pColored(a3) } }))
		table.insert(list, build.insn("call", { build.sym("filc_optimized_access_check_fail", "plt") },
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3) } }))
	end

	-- Fail stub for an alignment failure the optimized-access-check origin cannot
	-- express (an alignment wider than a word — cmpxchg16b's 16): the origin's
	-- recorded alignment is word-clamped, so filc_optimized_access_check_fail
	-- could not attribute an in-bounds, writable, word-aligned-but-not-fully-
	-- aligned pointer to ANY failure and would hit its "Should not be reached"
	-- assert (a pas crash, not a clean trap). filc_check_aligned_access_fail
	-- takes the TRUE alignment as a plain argument and reports it ("alignment
	-- requirement of 16 bytes not met during write") via the usual filc safety
	-- panic. Machine-level args: (ptr iv, ptr lower, bytes, alignment, kind,
	-- ptr_name iv, ptr_name lower) — kind is filc_access_kind (0=read, 1=write);
	-- the 7th word (ptr_name's lower) is marshalled on the stack at [rsp] AT the
	-- call, exactly like pushStackPtr (a NULL ptr_name iv makes the runtime
	-- ignore the name, so the lower's value is never consulted — write 0 anyway).
	function cg.alignFailStub(list, lbl, ivTemp, loTemp, size, align, isWrite)
		local a1, a2, a3 = M.cc.rtArg(1), M.cc.rtArg(2), M.cc.rtArg(3)
		local a4, a5, a6 = M.cc.rtArg(4), M.cc.rtArg(5), M.cc.rtArg(6)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(ivTemp) }, { defs = { pColored(a1) }, uses = {ivTemp} }))
		if loTemp then
			table.insert(list, build.insn("mov", { build.rp(a2), rt(loTemp) }, { defs = { pColored(a2) }, uses = {loTemp} }))
		else
			table.insert(list, build.insn("mov", { build.rp(a2), build.imm(0) }, { defs = { pColored(a2) } }))
		end
		table.insert(list, build.insn("mov", { build.rp(a3), build.imm(size) }, { defs = { pColored(a3) } }))
		table.insert(list, build.insn("mov", { build.rp(a4), build.imm(align) }, { defs = { pColored(a4) } }))
		table.insert(list, build.insn("mov", { build.rp(a5), build.imm(isWrite and 1 or 0) }, { defs = { pColored(a5) } }))
		table.insert(list, build.insn("mov", { build.rp(a6), build.imm(0) }, { defs = { pColored(a6) } }))
		local nameLoSlot = build.memx(build.rp(4), { dispVal = -16 })
		nameLoSlot.size = 8  -- lets the renderer synthesize the movq suffix for the reg-less form
		table.insert(list, build.insn("mov", { nameLoSlot, build.imm(0) }, {}))
		table.insert(list, build.insn("sub", { build.rp(4), build.imm(16) }, {}))
		table.insert(list, build.insn("call", { build.sym("filc_check_aligned_access_fail", "plt") },
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3),
				pColored(a4), pColored(a5), pColored(a6) } }))
	end

	-- Fail stub for a mask-refined bounds failure of a supported AVX512 {k}-masked
	-- or AVX2 vmaskmov/vpmaskmov access: marshals the runtime's dedicated masked
	-- fail function — exactly the calls the Fil-C compiler emits for the masked
	-- intrinsics (FilPizlonator's lowerIntrinsicAccess), and noreturn like every
	-- fail stub (no exception check, no vector save: it panics).
	--   fixed-position: filc_masked_read/write_check_fail(iv, lower, mask, vecsize, origin)
	--   contiguous:     filc_expand_read/compress_write_check_fail(iv, lower, mask,
	--                     elemsize, vecsize, origin)
	-- (a filc_ptr is two consecutive argument words, intval then lower; the mask
	-- is the truncated lane-enable value the slow path computed).
	function cg.maskedFailStub(list, lbl, ivTemp, loTemp, maskTemp, info, isWrite, originSym)
		local a1, a2, a3 = M.cc.rtArg(1), M.cc.rtArg(2), M.cc.rtArg(3)
		local a4, a5, a6 = M.cc.rtArg(4), M.cc.rtArg(5), M.cc.rtArg(6)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(ivTemp) },
			{ defs = { pColored(a1) }, uses = {ivTemp} }))
		table.insert(list, build.insn("mov", { build.rp(a2), rt(loTemp) },
			{ defs = { pColored(a2) }, uses = {loTemp} }))
		table.insert(list, build.insn("mov", { build.rp(a3), rt(maskTemp) },
			{ defs = { pColored(a3) }, uses = {maskTemp} }))
		local sym
		if info.contiguous then
			table.insert(list, build.insn("mov", { build.rp(a4), build.imm(info.elem) },
				{ defs = { pColored(a4) } }))
			table.insert(list, build.insn("mov", { build.rp(a5), build.imm(info.vecsize) },
				{ defs = { pColored(a5) } }))
			table.insert(list, build.insn("lea", { build.rp(a6), build.memx({ kind = "reg", rc = "rip" },
				{ dispSym = originSym }) }, { defs = { pColored(a6) } }))
			sym = isWrite and "filc_compress_write_check_fail" or "filc_expand_read_check_fail"
			table.insert(list, build.insn("call", { build.sym(sym, "plt") },
				{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3),
					pColored(a4), pColored(a5), pColored(a6) } }))
		else
			table.insert(list, build.insn("mov", { build.rp(a4), build.imm(info.vecsize) },
				{ defs = { pColored(a4) } }))
			table.insert(list, build.insn("lea", { build.rp(a5), build.memx({ kind = "reg", rc = "rip" },
				{ dispSym = originSym }) }, { defs = { pColored(a5) } }))
			sym = isWrite and "filc_masked_write_check_fail" or "filc_masked_read_check_fail"
			table.insert(list, build.insn("call", { build.sym(sym, "plt") },
				{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3),
					pColored(a4), pColored(a5) } }))
		end
	end

	-- Pure trap for a bad dynamic `.alloca` alignment (nonzero power-of-two check
	-- failed): `ud2` raises SIGILL deterministically in every build configuration
	-- (no runtime hook, no assert-behavior dependency).
	function cg.trap()
		emit(build.insn("ud2", {}, { control = "none", fallsThrough = false, noreturnCall = true }))
	end

	return cg
end

return M
