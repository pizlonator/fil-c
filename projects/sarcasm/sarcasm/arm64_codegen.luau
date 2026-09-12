--!nonstrict
-- ARM64 codegen: the per-arch instruction emitter used by the shared GIMSO transform
-- (transform.luau). Same interface as x86_64_codegen — static config (calling convention,
-- operand analysis) plus make(ctx) returning neutral "micro-ops" the shared transform
-- composes into access checks, invisicap loads/stores, calls, pollchecks and allocations.

local build = require("./build")
local isa = require("./arm64_isa")
local sig = require("./sig")
local regalloc = require("./regalloc")

local M = {}

M.classify = isa.classify
M.baseMnemonic = function(m) return string.lower(m) end
M.precolorName = function(num) return "x" .. num end
M.ALLOCATABLE = isa.ALLOCATABLE

-- Strict raw-body validation (validateBody in transform.luau): unannotated calls,
-- tail calls, unknown/misplaced `;!` annotations, in-body data, and forged
-- addresses are rejected BEFORE any rewriting (indirect CALLS with a signature
-- annotation are supported on arm64 like on x86_64; the shared annotation walk
-- rejects the unsupported indirect shapes). ARM64-only: the x86_64 backend
-- deliberately keeps its historical (looser) acceptance.
M.strictBodyValidation = true

-- This backend implements the float/double signature classes (the transform
-- marshals FP arguments through the v registers and FP returns through v0). The
-- x86_64 backend rejects every FP class at signature validation instead.
M.fpSignatures = true

-- Calling convention. Fast entrypoint: x0=myth; GPR-class args are packed DENSELY
-- into the SIX registers x2..x7 (a pointer-class arg occupies two consecutive slots
-- -- intval, lower; a scalar-class arg occupies one slot, intval only), matching
-- pizlonated clang output. FLOAT/DOUBLE args do NOT consume GPR slots: they pass in
-- the vector registers v0..v7 in declaration order among the FP args alone (float in
-- the low 32 bits = sN, double in the full dN — two floats never pack one register),
-- exactly like pizlonated clang (verified: long(double,int,float,long) places the
-- double in v0, the int in x2, the float in v1, the long in x3). Arguments beyond the
-- sixth GPR word are passed on the stack, which sarcasm does not marshal. FP args
-- beyond the eighth (v0..v7) are likewise rejected. ret w0=flags, x1=iv, x2=lo, and
-- an FP return rides in v0 (s0/d0). An INDIRECT call passes the callee's function-
-- object lower in x1 (fo) — the same register the CC returns the result intval /
-- ret-size in (they are live on disjoint sides of the call, exactly like x86_64's
-- rsi/rdx overlap with the SysV argument registers). Runtime calls (AAPCS):
-- x0,x1,x2,...; result x0.  yolo params arrive in x{k-1} counting only GPR args (FP
-- yolo params arrive in their v registers and are not modeled as GPR webs).
local DENSE_ARG_REGS = { 2, 3, 4, 5, 6, 7 }
local function denseArgSlot(argTypes, k)
	local slot = 1
	for i = 1, k - 1 do
		if sig.isFpClass(argTypes[i]) then
			-- FP arg (float/double): passes in a v register, consumes no GPR slot
		elseif argTypes[i] == 7 then
			slot += 2                             -- 7 = TYPE_PTR (see sig.luau)
		else
			slot += 1
		end
	end
	return slot
end
-- v-register index (0-based) of the FP arg at declaration position k: the count of
-- FP args before it (FP args allocate v0..v7 in declaration order among FP args).
local function fpArgOrdinal(argTypes, k)
	local m = 0
	for i = 1, k - 1 do
		if sig.isFpClass(argTypes[i]) then m += 1 end
	end
	return m
end
M.cc = {
	myth = 0, retFlag = 0, retIv = 1, retLo = 2,
	fo = 1,                                       -- x1: callee function-object lower at an indirect call
	argIv = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k)] end,
	argLo = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k) + 1] end,  -- ptr args only
	-- number of dense argument words that fit in registers; arm64 keeps its 6-word
	-- limit (words beyond the sixth are still rejected), so no signature that
	-- reaches the transform ever has a stack-resident dense word here.
	argRegCount = 6,
	-- 0-based dense word index of argument k's intval word (a pointer's lower word
	-- is at denseSlot+1). FP args consume no dense words.
	denseSlot = function(argTypes, k) return denseArgSlot(argTypes, k) - 1 end,
	-- yolo (AAPCS) integer-argument registers: arm64's callsite/entry checks cap
	-- every signature at 3 integer args / 6 words, so SysV stack args never arise.
	yoloRegCount = 6,
	-- yolo input-asm register for argument k. FP args pass in v registers (their
	-- v-reg index is fpArgOrdinal) and consume no yolo GPR: arg k's yolo GPR
	-- register is its 0-based ordinal among the NON-FP args (nil for an FP arg).
	yoloArg = function(k, argTypes)
		if argTypes and sig.isFpClass(argTypes[k]) then return nil end
		local n = 0
		if argTypes then
			for i = 1, k - 1 do
				if not sig.isFpClass(argTypes[i]) then n += 1 end
			end
		else
			n = k - 1
		end
		return n
	end,
	fpArgOrdinal = fpArgOrdinal,                  -- v-reg index (0-based) of FP arg k
	rtArg = function(k) return k - 1 end,
	rtRet = 0,
	rtRetLo = 1,                                  -- x1 (filc_ptr return: lower word)
}

-- Callsite signature validation (called by the shared transform for each `;! sig` call
-- annotation). The arm64 fast CC packs GPR argument words densely into the SIX
-- registers x2..x7 (a pointer arg = 2 words); wider arguments are passed on the stack,
-- which sarcasm does not yet marshal. So a call needs at most 6 register argument
-- words. Float/double args are supported on arm64 (they pass in v0..v7, in
-- declaration order among FP args, and the return rides in v0), so at most 8 of them
-- are allowed; long double and the vector classes are rejected by the arch-aware
-- signature class check (same rejection shape as x86_64, which rejects every FP
-- class).
function M.checkCallsiteSig(cs, callee)
	sig.checkSupportedClasses(cs, "call to '" .. callee .. "'", { arch = "arm64" })
	local words, fpArgs = 0, 0
	for k = 1, cs.argWords do
		if sig.isFpClass(cs.argTypes[k]) then
			fpArgs += 1
		else
			words += (cs.argTypes[k] == 7) and 2 or 1
		end
	end
	if words > 6 then
		error("sarcasm: call to '" .. callee .. "': >6 register argument words not yet supported"
			.. " (this call's signature needs " .. tostring(words) .. ")")
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

-- NEON single-/multi-structure load/store family: mnemonic -> {min, max}
-- register count (ld1/st1 take 1-4 structures, ld2-4/st2-4 and the replicate
-- loads ld1r..ld4r take exactly N; no replicate stores exist).
local MS_STRUCTS = {
	ld1 = { 1, 4 }, ld2 = { 2, 2 }, ld3 = { 3, 3 }, ld4 = { 4, 4 },
	st1 = { 1, 4 }, st2 = { 2, 2 }, st3 = { 3, 3 }, st4 = { 4, 4 },
	ld1r = { 1, 1 }, ld2r = { 2, 2 }, ld3r = { 3, 3 }, ld4r = { 4, 4 },
}

-- Access byte width of a NEON structure load/store. The data operand is a
-- reglist: full structures are N_structs x (lanes x elem) bytes (legal
-- arrangements give 8 or 16 bytes per register), single-element forms are
-- N_structs x elem bytes, replicate forms (ld1r..ld4r) load ONE element per
-- structure (N_structs x elem bytes). Anything unmodelable is a clean reject —
-- never an under-check.
local function msAccessBytes(stmt, counts, raw)
	local rl = stmt.operands[1]
	local mnem = string.lower(stmt.mnemonic)
	if not rl or rl.kind ~= "reglist" or #rl.regs < counts[1] or #rl.regs > counts[2] then
		error("sarcasm: NEON structure load/store register list does not match '"
			.. mnem .. "' (cannot size the access): " .. raw)
	end
	local nst = #rl.regs
	local r0 = rl.regs[1]
	if rl.lane ~= nil then
		-- single-element: {v0.b}[3] / {v0.h, v1.h}[1] — the element size comes
		-- from the arrangement qualifier (the lane index sits outside the braces).
		local el = (r0.element and r0.element.elem) or (r0.arrangement and r0.arrangement.elem)
		if not el then
			error("sarcasm: NEON single-element structure access without an element"
				.. " qualifier cannot be sized: " .. raw)
		end
		return nst * (isa.ELEM_BYTES[el] or 16)
	end
	if r0.element then
		error("sarcasm: malformed NEON structure register list (cannot size the access): " .. raw)
	end
	local arr = r0.arrangement
	if not arr or not arr.elem then
		error("sarcasm: NEON structure access without an arrangement qualifier"
			.. " cannot be sized: " .. raw)
	end
	local elemB = isa.ELEM_BYTES[arr.elem] or 16
	if string.sub(mnem, -1) == "r" then
		-- replicate: one element per structure, splatted across the destination.
		return nst * elemB
	end
	if not arr.lanes then
		error("sarcasm: NEON structure access without a lane count cannot be sized: " .. raw)
	end
	local per = arr.lanes * elemB
	if per ~= 8 and per ~= 16 then
		error("sarcasm: NEON structure arrangement gives a non-8/16-byte register width"
			.. " (" .. tostring(per) .. " bytes; not a legal shape): " .. raw)
	end
	for i = 2, nst do
		local a2 = rl.regs[i].arrangement
		if not a2 or not a2.lanes or a2.lanes * (isa.ELEM_BYTES[a2.elem] or 16) ~= per then
			error("sarcasm: mixed NEON structure arrangements cannot be sized: " .. raw)
		end
	end
	return nst * per
end

-- Atomics requiring NATURAL alignment: the hardware faults on unaligned addresses
-- for the LSE RMW family, the load/store-exclusive family, and the (non-RCpc)
-- acquire/release forms — so an unaligned one must produce the filc alignment
-- trap, never a raw hardware Alignment fault. (ldapr/ldapur* are RCpc and
-- tolerate unaligned, like plain loads.)
local NATURAL_ALIGN = {}
for m in pairs(isa.LSE_KIND) do NATURAL_ALIGN[m] = true end
for _, m in { "ldxr", "ldaxr", "ldxrb", "ldaxrb", "ldxrh", "ldaxrh", "ldxp", "ldaxp",
	"ldar", "ldarb", "ldarh", "stlr", "stlrb", "stlrh" } do NATURAL_ALIGN[m] = true end

-- Atomic forms whose ONLY addressing mode is a bare [base] (no offset, index or
-- writeback encoding exists): the LSE family, the exclusive loads/stores, and
-- the acquire/release forms (the ldapur* unscaled RCpc forms excepted — they
-- take a simm9 offset).
local BARE_MEM_ATOMIC = {}
for m in pairs(isa.LSE_KIND) do BARE_MEM_ATOMIC[m] = true end
for _, m in { "ldxr", "ldaxr", "ldxrb", "ldaxrb", "ldxrh", "ldaxrh", "ldxp", "ldaxp",
	"ldar", "ldarb", "ldarh", "ldapr", "ldaprb", "ldaprh",
	"stlr", "stlrb", "stlrh" } do BARE_MEM_ATOMIC[m] = true end

-- Access byte size of a modeled atomic: b/h-suffixed forms are fixed-width, the
-- pair forms (casp, stxp, ldxp/ldaxp) are double the element width, stxr/stlxr's
-- data register is ops[2] (ops[1] is the wN status), everything else takes the
-- first data register's width (w=4, x=8).
local function atomicAccessSize(stmt, m)
	if string.sub(m, -1) == "b" then return 1 end
	if string.sub(m, -1) == "h" then return 2 end
	local ops = stmt.operands
	local kind = isa.LSE_KIND[m]
	if kind == "casp" then return elemSize(ops[1]) * 2 end
	if kind == "stxp" then return elemSize(ops[2]) * 2 end
	if kind == "stxr" then return elemSize(ops[2]) end
	if m == "ldxp" or m == "ldaxp" then return elemSize(ops[1]) * 2 end
	return elemSize(ops[1])
end

-- Access size and required alignment for a modeled load/store. Sizes follow the data
-- register class (q-reg accesses are 16 bytes, q-reg pairs 32, ldnp/stnp/ldxp/ldaxp are
-- 16, sub-word scalar forms 1/2/4). Alignment matches what pizlonated clang itself
-- demands for the same accesses: no alignment check for accesses of 8 bytes or fewer
-- (the architecture tolerates unaligned scalar and NEON accesses), 8-byte alignment
-- for wider (16/32-byte) accesses — EXCEPT the atomics (LSE RMWs, the exclusive
-- family, ldar/stlr & co), which fault on unaligned addresses and therefore demand
-- NATURAL alignment (size == alignment).
function M.accessSizeAlign(stmt)
	local m = string.lower(stmt.mnemonic)
	local ops = stmt.operands
	if NATURAL_ALIGN[m] then
		local sz = atomicAccessSize(stmt, m)
		return sz, sz
	end
	local sz = ACCESS[m]
	if not sz then
		local counts = MS_STRUCTS[m]
		if counts then
			sz = msAccessBytes(stmt, counts, stmt.raw or m)
		elseif m == "ldpsw" then
			sz = 8                      -- pair of signed words
		elseif m == "ldp" or m == "stp" or m == "ldnp" or m == "stnp" then
			sz = elemSize(ops[1]) * 2   -- register pair: two elements
		else
			sz = elemSize(ops[1])       -- single-register forms
		end
	end
	return sz, (sz >= 16) and 8 or 1
end

-- store value operand: `str Xs, [mem]` -> Xs is operands[1] (source-first).
-- The LSE RMWs and store-exclusives also write memory (whatever their mnemonic
-- prefix — swp/ldadd/cas don't start with "st"), so they get the CanWrite check
-- and the store-class trap origin like every other heap write.
function M.isStoreInsn(stmt)
	local m = string.lower(stmt.mnemonic)
	if isa.LSE_KIND[m] then return true end
	return string.sub(m, 1, 2) == "st"
end

function M.isMemInsn(mnem) return isa.isMemInsn(mnem) end

-- Condition-flag (NZCV) classification for flag liveness at instrumentation points.
M.flagUse = isa.flagUse
M.flagDef = isa.flagDef
M.flagWriteNoDef = isa.flagWriteNoDef
M.flagUseMask = isa.flagUseMask
M.flagDefMask = isa.flagDefMask

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
		error("sarcasm: `" .. what .. " ptr` is only valid on a 64-bit scalar "
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

-- Ptr-annotation shape classification (used by the shared transform's annotation
-- walk; validatePtrAccess above is the validateBody-time rejection with detailed
-- reasons). Returns "load" (`ldr xN, [..]`), "store" (`str xN, [..]`), or "other".
-- arm64 has no single-instruction memory-destination RMW in its accepted set
-- (checkMemInsn rejects them), so "rmw" never occurs here.
function M.ptrAccessKind(stmt)
	local ops = stmt.operands or {}
	if not M.memOperand(stmt) then return "other" end
	local m = string.lower(stmt.mnemonic)
	local r = ops[1]
	if r and r.kind == "reg" and (r.width or 64) == 64 then
		if m == "ldr" then return "load" end
		if m == "str" then return "store" end
	end
	return "other"
end

-- LSE RMWs supported for `;! atomic load store ptr` on a pointer slot, mapped to
-- the plain arm64 ALU op the runtime-CAS-loop lowering re-executes on the loaded
-- intval (the capability rides through unchanged — see transform.luau's
-- emitAtomicLoadStorePtrLse). swp maps to "mov" — the marker the transform keys
-- on to lower swp to filc_xchg_ptr_with_manual_tracking instead: the new value
-- IS the source (regardless of the old), so it must carry the SOURCE's
-- capability, which the same-lower CAS loop cannot express. The min/max forms
-- are NOT in the table and are rejected (no single capability-preserving ALU op
-- expresses them). The st<op> aliases (no old-value destination) share the
-- table with "" and "l" variants.
local ATOMIC_PTR_RMW = {}
for base, alu in pairs({ ldadd = "add", ldset = "orr", ldeor = "eor", ldclr = "bic" }) do
	for _, v in { "", "a", "l", "al" } do ATOMIC_PTR_RMW[base .. v] = alu end
end
for _, v in { "", "a", "l", "al" } do ATOMIC_PTR_RMW["swp" .. v] = "mov" end
for base, alu in pairs({ stadd = "add", stset = "orr", steor = "eor", stclr = "bic" }) do
	for _, v in { "", "l" } do ATOMIC_PTR_RMW[base .. v] = alu end
end

-- The lowering plan for an annotated pointer RMW (nil when unsupported): the ALU
-- op to re-execute on the loaded intval plus the LSE kind (the transform derives
-- the operand roles from it: swp and ld<op> are ops[1]=source, ops[2]=old-value
-- destination; st<op> has no old-value destination).
function M.atomicRmwPlan(mnemonic)
	local alu = ATOMIC_PTR_RMW[mnemonic]
	if not alu then return nil end
	return { alu = alu, kind = isa.LSE_KIND[mnemonic] }
end

-- Shape validation for the atomic/pointer-RMW annotations (`;! atomic load ptr`,
-- `;! atomic store ptr`, `;! atomic ptr`, `;! load store ptr`,
-- `;! atomic load store ptr`) on arm64. Mirrors the x86_64 contracts
-- (x86_64_codegen.ptrAtomicShape) with arm64 instruction shapes:
--   "atomicload"  — ldr/ldur/ldar/ldapr xN, [...] — runtime atomic pointer load
--   "atomicstore" — str/stur/stlr xN, [...] — runtime atomic pointer store
--   "cas"         — `;! atomic ptr` on cas<a|l|al> xA, xB, [xM] (64-bit only) —
--                   pointer compare-exchange through the runtime; NZCV is
--                   recomputed as (expected − old) so a following b.eq/cset
--                   behaves like x86's cmpxchg flags
--   "atomicrmw"   — `;! atomic load store ptr` on a 64-bit LSE RMW (ldadd/ldset/
--                   ldeor/ldclr/swp or the st<op> aliases) — a real atomic RMW:
--                   ldadd/ldset/ldeor/ldclr via the runtime compare-exchange
--                   loop (the instruction is already atomic, so this is the x86
--                   `lock` form's analog); swp via a single
--                   filc_xchg_ptr_with_manual_tracking call (the new value is
--                   the source operand itself, so it carries the SOURCE's
--                   capability — filcc's C11 atomic_exchange lowering)
-- `;! load store ptr` (the NON-atomic single-instruction form) is rejected:
-- arm64 has no non-atomic memory-destination RMW instruction; the ldr+op+str
-- sequence with separate `;! load ptr` / `;! store ptr` annotations covers the
-- same (each access checked; capability preserved through the slot).
function M.ptrAtomicShape(ann, stmt)
	local raw = stmt.raw or stmt.mnemonic or "?"
	local m = string.lower(stmt.mnemonic)
	local ops = stmt.operands or {}
	local mem = M.memOperand(stmt)

	local function valReg(idx, what)
		local r = ops[idx]
		if not r or r.kind ~= "reg" or r.rc ~= "gpr" or r.width ~= 64 then
			error("sarcasm: `" .. ann .. "` requires a 64-bit GPR " .. what
				.. " (pointer slots are 8 bytes): " .. raw)
		end
		return r
	end
	-- The hardware atomic forms (ldar/ldapr/stlr/cas/LSE) take a bare [base];
	-- the plain ldr/str forms allow a register/immediate index (the runtime
	-- call receives the effective address). Writeback is rejected everywhere:
	-- no atomic form has it.
	local function checkMemShape(allowIndex)
		if not mem then
			error("sarcasm: `" .. ann .. "` requires a memory operand: " .. raw)
		end
		if mem.writeback then
			error("sarcasm: `" .. ann .. "` does not support writeback addressing"
				.. " (no atomic form exists; split the address update from the access): " .. raw)
		end
		if not allowIndex and (mem.index or mem.shift) then
			error("sarcasm: `" .. ann .. "` on '" .. m .. "' requires a plain [base]"
				.. " memory operand (no offset/index exists in its encoding): " .. raw)
		end
		if not mem.base or mem.base.rc ~= "gpr" then
			error("sarcasm: `" .. ann .. "` requires a GPR base register"
				.. " (sp/x29 frame slots cannot carry a capability): " .. raw)
		end
	end

	if ann == "atomic load ptr" then
		if not ({ ldr = true, ldur = true, ldar = true, ldapr = true })[m] then
			error("sarcasm: `atomic load ptr` requires a plain 64-bit scalar load"
				.. " (ldr/ldur/ldar/ldapr xN, [...]): " .. raw)
		end
		valReg(1, "destination")
		checkMemShape(m == "ldr" or m == "ldur")
		return "atomicload"
	end
	if ann == "atomic store ptr" then
		if not ({ str = true, stur = true, stlr = true })[m] then
			error("sarcasm: `atomic store ptr` requires a plain 64-bit scalar store"
				.. " (str/stur/stlr xN, [...]): " .. raw)
		end
		valReg(1, "stored value")
		checkMemShape(m == "str" or m == "stur")
		return "atomicstore"
	end
	if ann == "atomic ptr" then
		local kind = isa.LSE_KIND[m]
		if kind == "casp" then
			error("sarcasm: " .. m .. " cannot operate on pointers (a 16-byte"
				.. " compare-exchange cannot update an invisicap (intval, lower) pair"
				.. " atomically): " .. raw)
		end
		if kind ~= "cas" then
			error("sarcasm: `atomic ptr` is only supported on cas (pointer"
				.. " compare-exchange): " .. raw)
		end
		if string.sub(m, -1) == "b" or string.sub(m, -1) == "h" then
			error("sarcasm: `atomic ptr` requires the 64-bit cas form"
				.. " (pointer slots are 8 bytes): " .. raw)
		end
		valReg(1, "compare/expected register")
		valReg(2, "new-value register")
		checkMemShape(false)
		return "cas"
	end
	if ann == "load store ptr" then
		error("sarcasm: `load store ptr` is not supported on ARM64: arm64 has no"
			.. " non-atomic single-instruction memory read-modify-write; write the"
			.. " load, op and store as separate instructions annotated `load ptr`"
			.. " and `store ptr` (the capability rides through the slot), or use"
			.. " `atomic load store ptr` on an LSE RMW for the atomic form: " .. raw)
	end
	-- `;! atomic load store ptr`: pointer RMW via the runtime compare-exchange loop.
	if not isa.LSE_KIND[m] then
		error("sarcasm: `atomic load store ptr` requires an LSE atomic"
			.. " read-modify-write instruction (ldadd/ldset/ldeor/ldclr/swp; arm64 has"
			.. " no plain memory-destination ALU RMW): " .. raw)
	end
	if not ATOMIC_PTR_RMW[m] then
		error("sarcasm: `atomic load store ptr` is not supported on '" .. m
			.. "' (supported pointer RMWs: ldadd/ldset/ldeor/ldclr/swp and the st<op>"
			.. " aliases; cas wants `atomic ptr`): " .. raw)
	end
	if string.sub(m, -1) == "b" or string.sub(m, -1) == "h" then
		error("sarcasm: `atomic load store ptr` requires the 64-bit (x-register)"
			.. " form (pointer slots are 8 bytes): " .. raw)
	end
	valReg(1, "operand")
	if isa.LSE_KIND[m] ~= "st" then valReg(2, "operand") end
	checkMemShape(false)
	return "atomicrmw"
end

-- ARM condition suffixes for the neutral cmp/test conditions.
local BCOND = { ult = "b.lo", uge = "b.hs", ugt = "b.hi", ule = "b.ls", eq = "b.eq", ne = "b.ne" }

function M.make(ctx)
	local emit, allocTemp, pColored, temps = ctx.emit, ctx.allocTemp, ctx.pColored, ctx.temps
	local pf = ctx.pf
	-- casNewInRegs: filc_strong_cas_ptr_with_manual_tracking's eighth argument
	-- word (new_value) marshals in x6/x7 under AAPCS64 — the whole (myth, slot,
	-- offset, expected, new) call fits in x0..x7, so arm64 needs no stack
	-- marshalling (x86_64 SysV spills new_value to the stack via pushStackPtr).
	local cg = { cc = M.cc, casNewInRegs = true }

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
	function cg.addReg(dst, a, b) emit(build.insn("add", { rt(dst), rt(a), rt(b) }, { defs = {dst}, uses = {a, b} })) end
	function cg.andReg(dst, a, b) emit(build.insn("and", { rt(dst), rt(a), rt(b) }, { defs = {dst}, uses = {a, b} })) end
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
	-- Compare against an arbitrary 64-bit immediate and branch. ARM's `cmp` immediate
	-- is only 12 bits (optionally shifted), so a wider immediate must be widened —
	-- mirroring arm64_glue.callsiteThunk's signature compare:
	--   imm <= 4095              plain `cmp xN, #imm; b.cond`
	--   imm <= 65535             `mov wT, #imm` into a scratch, then cmp-register
	--   high bits fit 12         `sub T, a, #(imm>>12), lsl #12` into a FRESH temp (the
	--                            compared register may be live — e.g. the ret-size
	--                            check reads cc.retIv), then `cmp T, #(imm&4095)`
	--   imm < 2^32               `mov wT, #imm16` + `movk wT, #imm>>16, lsl 16` into a
	--                            scratch, then cmp-register
	--   otherwise                movBig materializes the full constant into a scratch
	--                            (16-bit movz/movk chunks into an x register, so the
	--                            lsl 32/48 chunks encode), then cmp-register
	-- (The sub/cmp pair compares a - (imm & ~4095) against imm & 4095, which decides
	-- the relation exactly; it only engages below 2^24, where both 12-bit halves fit.
	-- Signature numbers with 8 FP arguments reach past 2^32 — e.g. 8552919316 for
	-- double(double x8) — which is what the movBig form is for. Every temp here is
	-- freshly allocated, so the compared register — possibly live, e.g. the ret-size
	-- check's cc.retIv — is never clobbered.)
	function cg.cmpImmBranchWidened(a, imm, cond, lbl)
		imm = math.floor(tonumber(imm)
			or error("sarcasm: internal error: bad compare immediate " .. tostring(imm)))
		if imm <= 4095 then
			cg.cmpImmBranch(a, imm, cond, lbl)
		elseif imm <= 65535 then
			local t = allocTemp({ role = "scratch" })
			emit(build.insn("mov", { rt(t, 32), build.imm(imm) }, { defs = { t } }))
			cg.cmpBranch(a, t, cond, lbl)
		elseif math.floor(imm / 4096) <= 4095 then
			local t = allocTemp({ role = "scratch" })
			emit(build.insn("sub", { rt(t), rt(a), build.withShift(build.imm(math.floor(imm / 4096)), "lsl", 12) },
				{ defs = { t }, uses = { a } }))
			cg.cmpImmBranch(t, imm % 4096, cond, lbl)
		elseif imm < 4294967296 then
			local t = allocTemp({ role = "scratch" })
			emit(build.insn("mov", { rt(t, 32), build.imm(imm % 65536) }, { defs = { t } }))
			emit(build.insn("movk", { rt(t, 32), build.withShift(build.imm(math.floor(imm / 65536)), "lsl", 16) },
				{ defs = { t }, uses = { t } }))
			cg.cmpBranch(a, t, cond, lbl)
		else
			local t = allocTemp({ role = "scratch" })
			cg.movBig(t, imm)
			cg.cmpBranch(a, t, cond, lbl)
		end
	end
	-- dst = src & mask, where mask is a 64-bit constant (hex string or number):
	-- materialize the mask with movz/movk (ARM bitmask immediates cannot express every
	-- constant, e.g. the FUNCTION-type mask 0x780000000000000 is fine but the general
	-- case is not) and use the three-operand and. x86_64 uses its two-operand and —
	-- the shared indirect-call sequence's FUNCTION-type test.
	function cg.andConst(dst, src, mask)
		cg.movBig(dst, mask)
		emit(build.insn("and", { rt(dst), rt(dst), rt(src) },
			{ defs = { dst }, uses = { dst, src } }))
	end
	function cg.testMaskBranch(reg, mask, cond, lbl, width)
		emit(build.insn("tst", { rt(reg, width), build.imm(mask) }, { uses = {reg} })); br(cond == "eq" and "b.eq" or "b.ne", lbl)
	end
	function cg.jump(lbl) emit(build.insn("b", { build.sym(lbl) }, { control = "branch", targets = {lbl}, fallsThrough = false })) end
	function cg.label(name) emit(build.label(name)) end

	function cg.call(sym, uses, clobbers, indirect)
		if indirect then
			local n = build.insn("blr", { rt(sym) }, { control = "call", uses = uses, defs = clobbers })
			emit(n)
			return n
		else
			local n = build.insn("bl", { build.sym(sym) }, { control = "call", uses = uses, defs = clobbers })
			emit(n)
			return n
		end
	end
	function cg.callerSavedClobbers()
		local clob = {}
		for _, rn in isa.CALLER_SAVED_NUMS do table.insert(clob, pColored(rn)) end
		return clob
	end

	-- The fast-path indirect call: call the entrypoint loaded into targetTemp.
	-- `uses` carries every register live at the call (myth, the callee's function-
	-- object lower, the target temp and the marshalled argument registers);
	-- `clobbers` are the caller-saved registers the callee may destroy. The target
	-- temp is an ordinary virtual temp, so regalloc colors it around the clobbers
	-- (it is read AT the blr; never assume the reserved x8 scratch). Returns the
	-- emitted node (the transform tags it with the FP-arg vector uses when the
	-- callsite signature has float/double arguments — see fpVecUses in
	-- expandFpSaves).
	function cg.emitIndirectCallInsn(targetTemp, uses, clobbers)
		local n = build.insn("blr", { rt(targetTemp) },
			{ control = "call", uses = uses, defs = clobbers })
		emit(n)
		return n
	end

	-- The generic (buffer-CC) indirect call: call the callee's generic entrypoint at
	-- [lower+8]. ARM has no memory-indirect call, so load it into a fresh temp and
	-- blr it (x86_64 uses `call *8(%lower)` — see x86_64_codegen).
	function cg.emitGenericIndirectCall(fpLoTemp, uses, clobbers)
		local entryT = allocTemp({ role = "scratch" })
		cg.load(entryT, fpLoTemp, 8, 8)
		local allUses = {}
		for _, t in uses do table.insert(allUses, t) end
		table.insert(allUses, entryT)
		local n = build.insn("blr", { rt(entryT) },
			{ control = "call", uses = allUses, defs = clobbers })
		emit(n)
		return n
	end

	-- FP argument/return marshalling atoms (arm64 fast CC: FP args in v0..v7 in
	-- declaration order among FP args; FP return in v0). These synthesize str/ldr
	-- with a NAMED NEON register operand (regalloc never models vector registers —
	-- the same shape expandFpSaves' save/restore movs use) plus a colored GPR base
	-- temp for the address. The nodes carry a `vecStmt` pseudo-statement so
	-- expandFpSaves' width-liveness sees the vector register effect (a store USES
	-- the FP argument value — it must stay live across any injected call between
	-- the argument's definition and the call; a load DEFINES the FP return value).
	local function neonOperand(name, widthBytes)
		return { kind = "reg", rc = "neon", num = tonumber(string.sub(name, 2)),
			width = widthBytes * 8, name = name }
	end

	-- Register name of FP argument #ordinal (0-based) of the given class:
	-- "d0".."d7" for double, "s0".."s7" for float (the low 32 bits of the v reg).
	function cg.fpArgRegName(class, ordinal)
		local w = sig.fpWidthBytes(class)
			or error("sarcasm: internal error: fpArgRegName on a non-FP class " .. tostring(class))
		return (w == 4 and "s" or "d") .. tostring(ordinal)
	end

	-- Store FP argument #ordinal (class = float/double) into the CC buffer word at
	-- [mythT, #offset] (a float writes the low 4 bytes of the 8-byte slot, exactly
	-- like filcc's generic marshalling).
	function cg.storeFpArg(class, ordinal, mythTemp, offset)
		local w = sig.fpWidthBytes(class)
		local op = neonOperand(cg.fpArgRegName(class, ordinal), w)
		local n = build.insn("str", { op, build.mem(rt(mythTemp), offset) },
			{ uses = { mythTemp } })
		n.vecStmt = { mnemonic = "str", operands = n.operands }
		emit(n)
		return n
	end

	-- Load the FP return value (class = float/double) from the CC buffer word at
	-- [mythT, #offset] into v0 (s0/d0).
	function cg.loadFpRet(class, mythTemp, offset)
		local w = sig.fpWidthBytes(class)
		local op = neonOperand(cg.fpArgRegName(class, 0), w)
		local n = build.insn("ldr", { op, build.mem(rt(mythTemp), offset) },
			{ uses = { mythTemp } })
		n.vecStmt = { mnemonic = "ldr", operands = n.operands }
		emit(n)
		return n
	end

	-- Fail stubs for the shared indirect-call sequence (appended at function end).
	-- filc_check_function_call_fail takes the flight pointer (x0 = intval, x1 =
	-- lower); filc_cc_rets_check_failure takes (x0 = actual size, w1 = expected size,
	-- x2 = 0) — the same argument marshalling filcc's own thunks use. Both are
	-- noreturn plain `bl`s (no PLT reloc on ARM).
	--
	-- NB: on arm64 the ret-size register (cc.retIv = x1) is ALSO the second runtime
	-- argument register, so the rets stub must copy the ACTUAL size out of x1 BEFORE
	-- materializing the expected size into w1 (x86_64's actual-size register rdx is
	-- rtArg(3), disjoint from its rsi = rtArg(2), so it can move in either order).
	function cg.emitIndcallFailStub(list, lbl, ivTemp, loTemp)
		local a1, a2 = M.cc.rtArg(1), M.cc.rtArg(2)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(ivTemp) },
			{ defs = { pColored(a1) }, uses = { ivTemp } }))
		table.insert(list, build.insn("mov", { build.rp(a2), rt(loTemp) },
			{ defs = { pColored(a2) }, uses = { loTemp } }))
		-- appended to the list (the stubs flush at function end), never emitted here
		table.insert(list, build.insn("bl", { build.sym("filc_check_function_call_fail") },
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2) } }))
	end
	function cg.emitRetsFailStub(list, lbl, actualTemp, expectedBytes)
		local a1, a2, a3 = M.cc.rtArg(1), M.cc.rtArg(2), M.cc.rtArg(3)
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a1), rt(actualTemp) },
			{ defs = { pColored(a1) }, uses = { actualTemp } }))
		table.insert(list, build.insn("mov", { build.rp(a2, 32), build.imm(expectedBytes) },
			{ defs = { pColored(a2) } }))
		table.insert(list, build.insn("mov", { build.rp(a3, 32), build.imm(0) },
			{ defs = { pColored(a3) } }))
		table.insert(list, build.insn("bl", { build.sym("filc_cc_rets_check_failure") },
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3) } }))
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
		-- `//! use capability xN` (stashed on the node by the transform;
		-- `//! new capability xN` is a deprecated alias): the access is
		-- guarded by the NAMED register's capability, not the base-first
		-- default (validated: the named web carries a capability).
		local capSel = node.useCapTemp or node.newCapTemp
		if capSel and pf.lowerTemp[capSel] then
			capLo = pf.lowerTemp[capSel]
		elseif baseTemp and pf.isPtr[baseTemp] then capLo = pf.lowerTemp[baseTemp]
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
			{ control = "call", noreturnCall = true, uses = { pColored(a1), pColored(a2), pColored(a3) } }))
	end

	-- Redirect an sp-relative address computation landing inside a fixed alloca region to the
	-- allocation pointer (gcc -O0 re-derives the buffer address per use). Returns true if it
	-- replaced the node; a direct stack-relative *access* into a region is rejected.
	-- All matching happens in ONE sp-relative coordinate space: an x29 (frame-pointer)
	-- base is normalized via fpOffset when x29 is the frame pointer (see
	-- arm64_frame.analyzeFrame), exactly like x86_64's rbp normalization (stackOff in
	-- x86_64_codegen.regionRedirect).
	local fctx = ctx.frameCtx
	local function stackOff(baseName, imm)
		if baseName == "sp" then return imm end
		if baseName == "x29" and fctx and fctx.usesFp then return fctx.fpOffset + imm end
		return nil  -- not a stack base (e.g. x29 used as an ordinary GPR)
	end
	function cg.regionRedirect(node, regionOf)
		local m = string.lower(node.stmt.mnemonic)
		local ops = node.stmt.operands
		if m == "add" and ops[3] and ops[3].kind == "imm"
			and ops[1] and ops[1].name ~= "sp" then
			local off = ops[2] and stackOff(ops[2].name, ops[3].value or tonumber(ops[3].text))
			local r = off and regionOf(off)
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
				-- sp+0 redirected into the region: buffer + (0 - base), NOT buffer + 0
				-- (the old cg.move form was correct only for a region based at sp+0).
				cg.addImm(rd, r.ptrTemp, -r.base)
				pf.isPtr[rd] = true; pf.lowerTemp[rd] = r.loTemp
				return true
			end
		end
		local mem = M.memOperand(node.stmt)
		if mem and mem.base and not mem.writeback
			and (mem.index == nil or mem.index.kind == "imm") then
			local off = stackOff(mem.base.name,
				(mem.index and mem.index.kind == "imm")
					and (mem.index.value or tonumber(mem.index.text) or 0) or 0)
			if off and regionOf(off) then
				error("sarcasm: direct stack-relative memory access into an alloca region is not supported: " .. node.stmt.raw)
			end
		end
		return false
	end

	-- FP/SIMD save/restore around sarcasm-injected RETURNING runtime calls (the
	-- pollcheck slow path, filc_allocate, the ptr-store aux-ensure/barrier slow
	-- paths). The source program never saw these calls, so its live NEON values
	-- would be silently clobbered by them: AAPCS64 makes v0-v7 and v16-v31 fully
	-- caller-saved (and the Fil-C runtime uses NEON — it is built with a full
	-- NEON toolchain), while v8-v15's low 64 bits are callee-saved (the C callee
	-- preserves those itself, so only their upper halves are ever at risk).
	--
	-- Save/restore is LIVENESS- AND WIDTH-AWARE, mirroring the x86_64 design:
	-- cg.fpSave/fpRestore emit PLACEHOLDER nodes (kind "fpsave"/"fprestore"),
	-- and cg.expandFpSaves — called from transformFunction after the whole body
	-- is emitted, before regalloc — runs a backward per-vector-register WIDTH
	-- liveness over the emitted node stream (semantics from isa.vecEffects) and
	-- replaces each placeholder with exactly the str/ldr the analysis proves
	-- live at that point: a v-reg live only in its low 4 bytes is saved as sN,
	-- 8 as dN, 16 as qN; a v-reg dead across the call emits nothing. v8-v15 are
	-- saved only when live at MORE than 8 bytes (their low 64 bits survive the
	-- call). User calls (the pizlonatedFI* thunks) KILL caller-saved vector
	-- state (v8-v15 degrade to their callee-saved low 8 bytes); the injected
	-- filc_* runtime calls are TRANSPARENT (the placeholder pair preserves the
	-- state). A GPR-only function emits NOTHING (vecSaveBytes == 0: fpSave/
	-- fpRestore emit no placeholders and the hook below is a cheap no-op scan).
	--
	-- The save area sits past the GC root area of the synthesized frame
	-- (reserved by arm64_render.computeLayout; 16 bytes per v-register, slot
	-- for register n at slotBase + 16*n — the reservation is fixed regardless
	-- of how many movs the liveness elides, exactly like x86_64). The expanded
	-- movs carry no temps (named fixed operands; regalloc never models NEON).
	local fpInfo = fctx and fctx.fp
	local vecSaveBytes = (fpInfo and fpInfo.saveBytes) or 0
	function cg.fpSave()
		if vecSaveBytes == 0 then return end  -- no vector state the runtime could clobber
		emit({ kind = "fpsave" })
	end
	function cg.fpRestore()
		if vecSaveBytes == 0 then return end
		emit({ kind = "fprestore" })
	end

	-- Post-transform expansion (arm64): replaces every fpsave/fprestore
	-- placeholder in `out` (spliced in place) with the exact vector save/restore
	-- the width-liveness analysis proves live at that point. `numRoots` is the
	-- final GC root count (the transform passes it; the slot base must match
	-- arm64_render.computeLayout and the spillBase computation in sarcasm.luau).
	function cg.expandFpSaves(out, _fctx, numRoots)
		numRoots = numRoots or 0
		local any = false
		for _, node in out do
			if node.kind == "fpsave" or node.kind == "fprestore" then any = true break end
		end
		if not any then return end
		if vecSaveBytes == 0 then
			-- No vector state: placeholders expand to nothing (belt-and-braces;
			-- fpSave above already emits none in that case).
			for i = #out, 1, -1 do
				if out[i].kind == "fpsave" or out[i].kind == "fprestore" then table.remove(out, i) end
			end
			return
		end

		-- CFG-shaped instruction list parallel to `out` (mirrors the x86_64
		-- hook): labels define blocks; control comes from node.control or the
		-- lifted classification node.c. Calls: injected filc_* runtime calls are
		-- TRANSPARENT (the placeholder pair around them preserves the state);
		-- every other call (pizlonatedFI* user thunks) CLOBBERS v0-v7/v16-v31
		-- and the upper halves of v8-v15 (AAPCS64: d8-d15's low 64 bits are
		-- callee-saved and flow through).
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
					local sym = node.operands and node.operands[1]
					local txt = (sym and sym.kind == "sym") and sym.text or ""
					if not node.stmt and string.sub(txt, 1, 5) == "filc_" then
						d.transparent = true
					else
						d.killAll = true
						-- An FP-argument call CONSUMES the FP argument values the
						-- source program placed in v0..v7 (per AAPCS64 they ride the
						-- call in the vector registers): they are live UP TO the call
						-- even though the call clobbers them. Tagged by the transform
						-- (fpVecUses = { [vregNum] = widthBytes }); applied after the
						-- killAll below so the consumed args are live-before the call
						-- (and get saved around an INJECTED call sitting between the
						-- argument's definition and this call).
						d.fpUses = node.fpVecUses
					end
				elseif node.stmt then
					d.uses, d.defs = isa.vecEffects(node.stmt)
				elseif node.vecStmt then
					-- synthesized FP marshalling (cg.storeFpArg / cg.loadFpRet): the
					-- same vector effects as the user instruction it stands for (a
					-- buffer store USES the FP argument register; a buffer load
					-- DEFINES the FP return register)
					d.uses, d.defs = isa.vecEffects(node.vecStmt)
				end
				table.insert(insns, d)
			else
				-- fpsave/fprestore placeholders, prologue/epilogue/rootstore/raw:
				-- plain fall-through nodes, no vector effect.
				table.insert(insns, { fallsThrough = true })
			end
		end

		-- Backward width-liveness (identical transfer to x86_64, plus the AAPCS
		-- killAll rule): a def with providedWidth p kills liveness at or below
		-- p; a use at width u adds it.
		local function applyNode(d, live)
			if d.killAll then
				for n in pairs(live) do
					if n < 8 or n > 15 then live[n] = nil
					elseif live[n] > 8 then live[n] = 8 end
				end
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

		-- Expand each placeholder from its local liveness state. The slot base
		-- must match computeLayout/spillBase EXACTLY: [16 + 8*numRoots (+8 pad
		-- when the root count is odd) .. +vecSaveBytes) is the vector-save area;
		-- register n's slot is at slotBase + 16*n.
		local pad = (numRoots % 2 == 1) and 8 or 0
		local slotBase = 16 + 8 * numRoots + pad
		local SAVE_NAME = { [1] = "b", [2] = "h", [4] = "s", [8] = "d", [16] = "q" }
		local function emitPairMovs(isRestore, live, into)
			for n = 0, 31 do
				local w = live[n]
				-- v8-v15's low 64 bits are callee-saved (the C callee preserves
				-- them itself): only their upper halves ever need saving.
				if w and n >= 8 and n <= 15 and w <= 8 then w = nil end
				if w then
					local rn = SAVE_NAME[w]
					if not rn then
						error("sarcasm: internal error: bad live vector width " .. tostring(w))
					end
					local vreg = { kind = "reg", rc = "neon", num = n, width = w * 8, name = rn .. n }
					local slot = build.mem(build.sp(), slotBase + 16 * n)
					if isRestore then
						table.insert(into, build.insn("ldr", { vreg, slot }, {}))
					else
						table.insert(into, build.insn("str", { vreg, slot }, {}))
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

	-- passthrough special-casing: ARM has none (no cltq / lea-with-mem); return false.
	function cg.lowerSpecial(node, mem) return false end

	-- Implicit-GPR pinning (the cas/casp compare register pair; casp's data pair
	-- is a use-only pin). The compare register of an LSE compare-and-swap is an
	-- architectural read-modify-write — read for the compare, written with the
	-- old memory value — so its use and def webs must occupy the physical
	-- register the assembly text names. classify models those effects with
	-- implicit def/use operand tables (node.c.pin); emitPinned copies the
	-- reaching webs into the physical registers before the instruction and the
	-- physical results back out into the fresh def webs after it, exactly like
	-- x86_64's emitPinned for the cmpxchg accumulator. The instruction renders
	-- the pinned explicit operands as their fixed physical registers; regalloc
	-- colors everything else AROUND them via the precolored def/use temps.
	-- (casp's pairs must render at their written even/odd physical numbers —
	-- the hardware encodes a pair as one even register number — so all four
	-- explicit registers are pinned.)
	-- Called by the transform wherever a plain node would be emitted; returns
	-- false for unpinned nodes.
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
		-- pin-in: every modeled use of a pinned register gets its reaching web
		-- copied into the physical register (same-web duplicates collapse: the
		-- lift keys use webs by register number).
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
				table.insert(newops, build.rp(o.num, o.width))
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
		})
		-- pin-out: every modeled def of a pinned register gets the physical
		-- register copied out into its fresh web.
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

	-- Fail stub for an alignment failure the optimized-access-check origin cannot
	-- express (an alignment wider than a word — casp/stxp/ldxp's 16): the
	-- origin's recorded alignment is word-clamped, so
	-- filc_optimized_access_check_fail could not attribute an in-bounds,
	-- writable, word-aligned-but-not-fully-aligned pointer to ANY failure and
	-- would hit its "Should not be reached" assert (a pas crash, not a clean
	-- trap). filc_check_aligned_access_fail takes the TRUE alignment as a plain
	-- argument and reports it ("alignment requirement of 16 bytes not met during
	-- write") via the usual filc safety panic. Machine-level args: (ptr iv, ptr
	-- lower, bytes, alignment, kind, ptr_name iv, ptr_name lower) — kind is
	-- filc_access_kind (0=read, 1=write); AAPCS64 marshals all seven words in
	-- x0..x6 (a NULL ptr_name iv makes the runtime ignore the name, so the
	-- lower's value is never consulted — write 0 anyway).
	function cg.alignFailStub(list, lbl, ivTemp, loTemp, size, align, isWrite)
		local a = {}
		for k = 1, 7 do a[k] = M.cc.rtArg(k) end
		table.insert(list, build.label(lbl))
		table.insert(list, build.insn("mov", { build.rp(a[1]), rt(ivTemp) }, { defs = { pColored(a[1]) }, uses = {ivTemp} }))
		if loTemp then
			table.insert(list, build.insn("mov", { build.rp(a[2]), rt(loTemp) }, { defs = { pColored(a[2]) }, uses = {loTemp} }))
		else
			table.insert(list, build.insn("mov", { build.rp(a[2]), build.xzr() }, { defs = { pColored(a[2]) } }))
		end
		table.insert(list, build.insn("mov", { build.rp(a[3]), build.imm(size) }, { defs = { pColored(a[3]) } }))
		table.insert(list, build.insn("mov", { build.rp(a[4]), build.imm(align) }, { defs = { pColored(a[4]) } }))
		table.insert(list, build.insn("mov", { build.rp(a[5]), build.imm(isWrite and 1 or 0) }, { defs = { pColored(a[5]) } }))
		table.insert(list, build.insn("mov", { build.rp(a[6]), build.xzr() }, { defs = { pColored(a[6]) } }))
		table.insert(list, build.insn("mov", { build.rp(a[7]), build.xzr() }, { defs = { pColored(a[7]) } }))
		table.insert(list, build.insn("bl", { build.sym("filc_check_aligned_access_fail") },
			{ control = "call", noreturnCall = true, uses = { pColored(a[1]), pColored(a[2]), pColored(a[3]),
				pColored(a[4]), pColored(a[5]), pColored(a[6]), pColored(a[7]) } }))
	end

	-- a memory operand may only appear on a load/store we model; anything else is unmodeled.
	function cg.checkMemInsn(node, mem)
		local mnem = string.lower(node.stmt.mnemonic)
		if not M.isMemInsn(mnem) then
			error("sarcasm: memory operand on non-load/store instruction '"
				.. node.stmt.mnemonic .. "' cannot be made memory-safe: " .. node.stmt.raw)
		end
		if BARE_MEM_ATOMIC[mnem] and (mem.index or mem.shift or mem.writeback) then
			error("sarcasm: '" .. mnem .. "' takes a plain [base] memory operand"
				.. " (no offset, index or writeback exists in its encoding): " .. node.stmt.raw)
		end
		if mem.writeback then
			-- The NEON structure family (ld1-4/st1-4/ld1r-4r) has post-index forms
			-- only: an immediate or a REGISTER index (the base advances by it; the
			-- register is modeled as a use, the base as an RMW, by memRegs). Every
			-- other modeled load/store must use a plain immediate writeback index.
			local msRegPost = MS_STRUCTS[mnem] and mem.writeback == "post"
				and mem.index and mem.index.kind == "reg"
			if not msRegPost and ((mem.index and mem.index.kind ~= "imm") or mem.postShift) then
				error("sarcasm: memory operand with non-immediate writeback index cannot be made memory-safe: "
					.. node.stmt.raw)
			end
			-- The structure family has no pre-index encoding.
			if MS_STRUCTS[mnem] and mem.writeback == "pre" then
				error("sarcasm: NEON structure load/store with pre-index writeback"
					.. " is not part of the modeled subset: " .. node.stmt.raw)
			end
		end
		-- A shift/extend inside the brackets only makes sense with a register index.
		if mem.shift and not (mem.index and mem.index.kind == "reg") then
			error("sarcasm: memory operand has a shift/extend but no index register: " .. node.stmt.raw)
		end
	end

	-- Pure trap for a bad dynamic `.alloca` alignment: `brk #0` raises
	-- deterministically in every build configuration.
	function cg.trap()
		emit(build.insn("brk", { build.imm(0) }, { control = "none", fallsThrough = false, noreturnCall = true }))
	end

	return cg
end

return M
