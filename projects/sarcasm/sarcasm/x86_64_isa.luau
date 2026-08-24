--!nonstrict
-- x86_64 instruction semantics: classify a parsed (dest-first, syntax-normalized)
-- instruction into register defs/uses and control flow. Operands come from x86parse
-- (already normalized to destination-first, so ops[1] is the destination).

local x86 = {}

-- Normalize a mnemonic to its base operation (strip AT&T size suffix; unify movz*/movs*).
local BASE = {}
for _, m in { "mov","add","sub","and","or","xor","sal","shl","sar","shr","cmp","test",
	"lea","imul","inc","dec","neg","not","push","pop","adc","sbb","mul","idiv","div","ror","rol",
	"rcr","rcl","bswap","xchg","xadd","cmpxchg","movbe" } do
	BASE[m] = m
	BASE[m .. "b"] = m; BASE[m .. "w"] = m; BASE[m .. "l"] = m; BASE[m .. "q"] = m
end
local FIXED = {
	movabs = "mov", movabsq = "mov",
	movzx = "movzx", movsx = "movsx", movsxd = "movsx",
	movzbl = "movzx", movzbw = "movzx", movzbq = "movzx", movzwl = "movzx", movzwq = "movzx",
	movsbl = "movsx", movsbw = "movsx", movsbq = "movsx", movswl = "movsx", movswq = "movsx",
	movslq = "movsx",
	cltq = "cltq", cdqe = "cltq", cqo = "cqo", cqto = "cqo",
	endbr64 = "nop", nop = "nop", ud2 = "ud2", ret = "ret", retq = "ret", leave = "leave",
	call = "call", callq = "call", jmp = "jmp", jmpq = "jmp",
}
function x86.baseMnemonic(m)
	m = string.lower(m)
	if FIXED[m] then return FIXED[m] end
	if BASE[m] then return BASE[m] end
	-- conditional jumps: je/jne/jb/jae/ja/jbe/jl/jge/jg/jle/js/jns/jc/jnc/jz/jnz/jo/jno/jp/jnp
	if string.match(m, "^j%a+$") then return "jcc" end
	if string.match(m, "^set%a+$") then return "setcc" end
	if string.match(m, "^cmov%a+$") then return "cmov" end
	return m
end

local ALU2 = { add=true, sub=true, ["and"]=true, ["or"]=true, xor=true, sal=true, shl=true,
	sar=true, shr=true, imul=true, adc=true, sbb=true, ror=true, rol=true, rcr=true, rcl=true }
local FLAGS_ONLY = { cmp=true, test=true }
-- Unary read-modify-write ops on their single operand (inc/dec/neg/not; bswap r/m is the
-- same shape: it reads and writes exactly one register — bswap only has register forms).
local DST_ONLY = { inc=true, dec=true, neg=true, ["not"]=true, bswap=true }
-- condition -> whether it's an unsigned/eq branch we generate; classify only needs control.
local COND_JMP = { jcc = true }

local function isGpr(op) return op and op.kind == "reg" and op.rc == "gpr" end

local function memRegs(op, uses)
	if op.base and isGpr(op.base) then table.insert(uses, op.base) end
	if op.index and isGpr(op.index) then table.insert(uses, op.index) end
end

-- loop/loope/loopne (and the loopz/loopnz aliases) and jrcxz/jecxz/jcxz are conditional
-- branches whose implicit rcx decrement/test (and, for loope/loopne, ZF read) this model
-- does not track: `loop` falls into the unknown-mnemonic fallback (control="none"), which
-- silently drops the back-edge from the CFG, and the jrcx family matches the jcc pattern,
-- which models a flags read instead of the rcx read. Compilers haven't emitted these in
-- decades, so reject them outright rather than misclassify them.
local UNSUPPORTED_LOOP = {
	loop=true, loope=true, loopne=true, loopz=true, loopnz=true,
	jrcxz=true, jecxz=true, jcxz=true,
}

-- Instructions with implicit register effects this model cannot represent: implicit
-- defs/uses of rax/rdx/rcx/rbx (lahf/sahf on ah, rdtsc/rdtscp/rdpmc on edx:eax, cpuid
-- on rax/rbx/rcx/rdx, xgetbv on edx:eax, xlat on al/rbx, the aaa/daa/... BCD adjust ops
-- on ax) or an implicit rsp RMW (pushf/popf). The unknown-mnemonic fallback models ZERO
-- register effects for these (they take no GPR operands), silently disconnecting the
-- affected register webs — a verified miscompile under register pressure — so reject
-- them like the loop family. xadd/cmpxchg (implicit rax RMW for cmpxchg) are
-- additionally read-modify-write atomics, which sarcasm does not model. xchg is a
-- BOTH-operands RMW (and an implicit-lock atomic with a memory operand); movbe is an
-- unmodeled memory access plus a byte swap.
local UNSUPPORTED_IMPLICIT = {
	lahf=true, sahf=true,
	pushf=true, pushfq=true, pushfw=true, popf=true, popfq=true, popfw=true,
	rdtsc=true, rdtscp=true, rdpmc=true, cpuid=true, xgetbv=true,
	xlat=true, xlatb=true,
	aaa=true, aas=true, daa=true, das=true, aam=true, aad=true,
	xadd=true, cmpxchg=true, cmpxchg8b=true, cmpxchg16b=true,
	xchg=true, movbe=true,
}

-- Instruction PREFIXES (rep/repe/repz/repne/repnz/lock/xacquire/xrelease) parse as a
-- mnemonic of their own (the real opcode follows as "operands", which the model then
-- ignores): `rep movsb` would classify as a zero-effect unknown mnemonic and execute
-- with garbage rsi/rdi/rcx, and `lock xaddq %rax,(%rdi)` would lose its memory RMW.
-- String ops are equally unmodelable bare: even without a prefix, movs/stos/lods/
-- scas/cmps/ins/outs implicitly read-modify-write rsi/rdi (and rcx under rep). Real
-- compilers DO emit `rep movsb`/`rep stosb` (memcpy/memset at -Os or with ERMS), so
-- reject both the prefixes and every standalone string-op form with a clear message.
local UNSUPPORTED_PREFIX_STRING = {
	rep=true, repe=true, repz=true, repne=true, repnz=true,
	lock=true, xacquire=true, xrelease=true,
	movs=true, movsb=true, movsw=true, movsl=true, movsq=true,
	stos=true, stosb=true, stosw=true, stosl=true, stosq=true,
	lods=true, lodsb=true, lodsw=true, lodsl=true, lodsq=true,
	scas=true, scasb=true, scasw=true, scasl=true, scasq=true,
	cmps=true, cmpsb=true, cmpsw=true, cmpsl=true, cmpsq=true,
	ins=true, insb=true, insw=true, insl=true, insd=true,
	outs=true, outsb=true, outsw=true, outsl=true, outsd=true,
}

-- syscall/sysenter enter the kernel with RAW arguments, bypassing Fil-C's syscall
-- argument validation (a genuine memory-safety hole, like arm64 svc); int/into do the
-- same through a software interrupt, and hlt is privileged. int3/ud2 stay allowed:
-- they are pure traps (breakpoints/unreachable) with no kernel entry.
local UNSUPPORTED_SYS = {
	syscall=true, sysenter=true, int=true, into=true, hlt=true,
}

-- Classify. Returns { defs, uses, rmw, control, targets, fallsThrough }.
function x86.classify(insn)
	local m = x86.baseMnemonic(insn.mnemonic)
	local low = string.lower(insn.mnemonic)
	if UNSUPPORTED_LOOP[low] then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (use an explicit dec/cmp + conditional branch)")
	end
	if UNSUPPORTED_IMPLICIT[low] or UNSUPPORTED_IMPLICIT[m] then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (implicit register effects cannot be modeled)")
	end
	if UNSUPPORTED_PREFIX_STRING[low] then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (rep/lock prefixes and string instructions have implicit register effects"
			.. " that cannot be modeled; use explicit load/store loops)")
	end
	if UNSUPPORTED_SYS[low] then
		error("sarcasm: '" .. insn.mnemonic .. "' is not supported"
			.. " (a raw syscall or privileged instruction bypasses Fil-C's runtime validation)")
	end
	local ops = insn.operands or {}
	local defs, uses, rmw = {}, {}, {}
	local control, targets, fallsThrough = "none", nil, true

	local function useOperand(o)
		if isGpr(o) then table.insert(uses, o) elseif o and o.kind == "mem" then memRegs(o, uses) end
	end

	if m == "jmp" then
		control, fallsThrough = "branch", false
		targets = {}
		for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		if ops[1] and (isGpr(ops[1]) or (ops[1].kind == "mem")) then control = "indirect"; useOperand(ops[1]) end
	elseif m == "jcc" then
		control = "condbranch"; targets = {}
		for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
	elseif m == "call" then
		control = "call"
		if ops[1] and (isGpr(ops[1]) or ops[1].kind == "mem") then control = "indcall"; useOperand(ops[1]) end
	elseif m == "ret" then
		control, fallsThrough = "return", false
	elseif m == "nop" or m == "ud2" or m == "leave" then
		-- no gpr effects we model (leave restores rsp/rbp; frame handled elsewhere)
	elseif m == "cltq" or m == "cqo" then
		table.insert(defs, { kind="reg", rc="gpr", num=0, width=64, name="rax" })
		table.insert(uses, { kind="reg", rc="gpr", num=0, width=64, name="rax" })
		table.insert(rmw, 0)
	elseif FLAGS_ONLY[m] then
		for _, o in ops do useOperand(o) end
	elseif ALU2[m] then
		-- dst = dst OP src : ops[1] is def+use, rest are uses
		if isGpr(ops[1]) then table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		elseif ops[1] and ops[1].kind == "mem" then memRegs(ops[1], uses) end
		for i = 2, #ops do useOperand(ops[i]) end
	elseif DST_ONLY[m] then
		if isGpr(ops[1]) then table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		elseif ops[1] and ops[1].kind == "mem" then memRegs(ops[1], uses) end
	elseif m == "mov" or m == "movzx" or m == "movsx" or m == "lea" or m == "cmov" or m == "setcc" then
		-- ops[1] def, ops[2..] use (mem regs used); a store (mem dst) uses the value instead.
		if isGpr(ops[1]) then table.insert(defs, ops[1])
		elseif ops[1] and ops[1].kind == "mem" then memRegs(ops[1], uses) end
		if (ops[1] and ops[1].kind == "mem") and isGpr(ops[2]) then table.insert(uses, ops[2]) end
		if m == "cmov" and isGpr(ops[1]) then table.insert(uses, ops[1]); table.insert(rmw, ops[1].num) end
		for i = 2, #ops do
			local o = ops[i]
			if o and o.kind == "mem" then memRegs(o, uses)
			elseif isGpr(o) and not (ops[1] and ops[1].kind == "mem") then table.insert(uses, o) end
		end
	elseif m == "push" then
		useOperand(ops[1])
	elseif m == "pop" then
		if isGpr(ops[1]) then table.insert(defs, ops[1]) end
	else
		-- unknown: conservative (first reg def, rest use)
		local first = true
		for _, o in ops do
			if isGpr(o) then
				if first then table.insert(defs, o); first = false else table.insert(uses, o) end
			elseif o and o.kind == "mem" then memRegs(o, uses) end
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true, unknown = true }
	end

	return { defs = defs, uses = uses, rmw = rmw, control = control, targets = targets, fallsThrough = fallsThrough }
end

x86.isGpr = isGpr

-- RFLAGS condition-flag classification, used for flag liveness at instrumentation
-- points (the transform brackets its cmp/test-emitting sequences with pushf/popf when
-- a flag consumer can reach them). A "use" reads RFLAGS (jcc/setcc/cmov, adc/sbb
-- carry-in); a "def" writes RFLAGS without reading it. adc/sbb read AND write and
-- count as uses. `not` and lea/mov/push/pop do not affect flags.
-- rcr/rcl read CF (carry-in) and are only PARTIAL writers (CF/OF written, the rest
-- preserved — like ror/rol), so they are uses but NOT defs. (lahf/sahf/pushf/popf
-- would need entries here, but they are REJECTED at classify — implicit register
-- effects — before any flag-liveness scan can see them; the scan only ever walks
-- program statements, never the transform's own emitted pushfq/popfq brackets.)
function x86.flagUse(stmt)
	local m = x86.baseMnemonic(stmt.mnemonic)
	if m == "jcc" or m == "setcc" or m == "cmov" then return true end
	if m == "adc" or m == "sbb" then return true end
	if m == "rcr" or m == "rcl" then return true end
	return false
end

-- FULL RFLAGS writers: after one of these, no flag-reader can observe a flag value
-- that was set before it, so incoming flags are dead. PARTIAL writers are NOT defs:
-- inc/dec write OF/SF/ZF/AF/PF but PRESERVE CF (so `cmp; inc; jb` still reads the
-- cmp's carry), and ror/rol write CF/OF but preserve SF/ZF/AF/PF — classifying them
-- as defs would let an inserted flag-clobbering check corrupt a live preserved flag.
-- neg IS a full def (it writes CF: set iff the operand was non-zero, plus the rest).
-- adc/sbb read AND write: they are defs AND uses (the use dominates for liveness).
-- mul/imul/div/idiv leave some flags architecturally UNDEFINED (clobbered, never
-- preserved), so a reader across them cannot be relying on the incoming value.
-- (popf/popfq would be full defs, but they are rejected at classify like pushf above.)
local FLAG_FULL_DEF = {
	add=true, sub=true, ["and"]=true, ["or"]=true, xor=true,
	sal=true, shl=true, sar=true, shr=true,
	adc=true, sbb=true, imul=true, neg=true,
	cmp=true, test=true,
	mul=true, idiv=true, div=true,
}

function x86.flagDef(stmt)
	return FLAG_FULL_DEF[x86.baseMnemonic(stmt.mnemonic)] == true
end

-- Register/calling-convention configuration (SysV AMD64 + Fil-C fast entrypoint) -------
-- Reserved: rsp (stack), r11 (dedicated glue/epilogue scratch, like ARM x8).
-- Allocatable = the other 14 GPRs. rbp is allocatable (we synthesize rsp-relative frames).
x86.ALLOCATABLE = { "rax","rcx","rdx","rbx","rbp","rsi","rdi","r8","r9","r10","r12","r13","r14","r15" }
x86.CALLEE_SAVED = { rbx=true, rbp=true, r12=true, r13=true, r14=true, r15=true }
x86.CALLEE_SAVED_ORDER = { "rbx","rbp","r12","r13","r14","r15" }  -- push order (r15 pushed last)
x86.CALLER_SAVED = { rax=true, rcx=true, rdx=true, rsi=true, rdi=true, r8=true, r9=true, r10=true }
x86.SCRATCH = "r11"       -- non-allocatable glue scratch
x86.SCRATCH_NUM = 11
-- caller-saved register numbers clobbered by a call (used to model call defs).
x86.CALLER_SAVED_NUMS = { 0, 1, 2, 6, 7, 8, 9, 10 }

-- Fil-C fast-entrypoint calling convention (register numbers).
-- Args are packed DENSELY (a pointer-class arg occupies two consecutive slots --
-- intval, lower; a scalar-class arg occupies one slot, intval only), matching
-- pizlonated clang output. Clang's x86_64 fast CC has only FOUR dense argument
-- registers (rdx, rcx, r8, r9); arguments beyond the fourth word are passed on the
-- stack. sarcasm does not marshal stack arguments -- it rejects signatures needing
-- more than 4 register argument words (see sarcasm.luau / x86_64_codegen.luau), so
-- only the first four slots below are ever used.
local DENSE_ARG_REGS = { 2, 1, 8, 9, 10, 11 }  -- rdx, rcx, r8, r9, (r10, r11: unreachable)
local function denseArgSlot(argTypes, k)
	local slot = 1
	for i = 1, k - 1 do
		slot += (argTypes[i] == 7) and 2 or 1   -- 7 = TYPE_PTR (see sig.luau)
	end
	return slot
end
x86.CC = {
	myth = 7,       -- rdi
	fo = 6,         -- rsi
	argIv = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k)] end,
	argLo = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k) + 1] end,  -- ptr args only
	retFlag = 0,    -- al (rax)
	retIv = 2,      -- rdx
	retLo = 1,      -- rcx
}
-- SysV C-call argument registers: rdi, rsi, rdx, rcx, r8, r9.
x86.SYSV = { 7, 6, 2, 1, 8, 9 }

return x86
