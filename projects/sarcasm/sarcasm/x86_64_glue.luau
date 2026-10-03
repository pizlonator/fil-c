--!nonstrict
-- x86_64 Fil-C calling-convention glue (AT&T output): function getter, generic (buffer)
-- entrypoint thunk, FO object + origins, direct-call alias, and callsite resolver thunk.
-- Templates validated against the reference Fil-C clang x86_64 output (hash-filc-x86.s).
-- Data layout is identical to ARM64 (only .xword->.quad, .word->.long).

local glue_x86 = {}

local sig = require("./sig")
local err = require("./err")

local FUNCTION_UPPER_FLAGS = "36873221949095936" -- 0x83 << 48

function glue_x86.getter(out, name)
	local fo = "pizlonatedFO_" .. name
	table.insert(out, "\t.text")
	table.insert(out, "\t.globl\tpizlonated_" .. name)
	table.insert(out, "\t.p2align\t4")
	table.insert(out, "\t.type\tpizlonated_" .. name .. ",@function")
	table.insert(out, "pizlonated_" .. name .. ":")
	table.insert(out, "\tleaq\t" .. fo .. "+16(%rip), %rax")
	table.insert(out, "\tmovq\t%rax, %rdx")
	table.insert(out, "\tretq")
	table.insert(out, ".Lfunc_end_getter_" .. name .. ":")
	table.insert(out, "\t.size\tpizlonated_" .. name .. ", .Lfunc_end_getter_" .. name .. "-pizlonated_" .. name)
	table.insert(out, "")
end

-- Dense fast-CC arg LOCATIONS: (iv[, lo]) words packed consecutively; a scalar arg
-- consumes one slot, a pointer arg two. FLOAT/DOUBLE args consume NO GPR slot: they
-- pass in the vector registers xmm0..xmm7, in declaration order among the FP args
-- alone (a float in the low 32 bits, a double in the full 64), matching pizlonated
-- clang output (long(double,int,float,long): double->xmm0, int->rdx, float->xmm1,
-- long->rcx). Must stay in lockstep with denseArgSlot in x86_64_isa.luau.
-- Clang's x86_64 fast CC has only FOUR dense argument GPRs (rdx, rcx, r8, r9):
-- slots 1..4 below. Slot 5+ travels on the STACK as densely packed 8-byte words in
-- declaration word order (verified against pizlonated clang for 5..14-word
-- signatures, incl. a pointer straddling the r9/stack boundary: its intval rides
-- slot 4 = r9 while its lower is stack word 0). denseArgLoc returns per-argument
-- location tables ivLoc[k]/loLoc[k]: { reg = "%rdx" } or { stack = j } with j the
-- 0-based outgoing stack word index (the word sits at 8*j(%rsp) at the call, i.e.
-- at 8+8*j(%rsp) from the callee's view). FP args are bounded separately: at most
-- 8 (xmm0..xmm7).
local DENSE_ARG_REGS = { "%rdx", "%rcx", "%r8", "%r9" }
local function denseArgLoc(argTypes, argWords)
	local ivLoc, loLoc = {}, {}
	local slot = 1
	local function loc(s)
		if s <= #DENSE_ARG_REGS then return { reg = DENSE_ARG_REGS[s] } end
		return { stack = s - 1 - #DENSE_ARG_REGS }
	end
	for k = 1, argWords do
		if sig.isFpClass(argTypes[k]) then
			-- FP arg: passes in an xmm register, consumes no GPR slot
		elseif argTypes[k] == 7 then   -- TYPE_PTR
			ivLoc[k] = loc(slot)
			loLoc[k] = loc(slot + 1)
			slot += 2
		else
			ivLoc[k] = loc(slot)
			slot += 1
		end
	end
	return ivLoc, loLoc
end

-- FP instruction form + xmm-register name of FP argument k: float = "movss" /
-- "%xmmN" (low 32 bits), double = "movsd" / "%xmmN" (64 bits); N = the argument's
-- 0-based ordinal among the FP args. Also used for the FP return (ordinal 0:
-- xmm0).
local function fpArgForm(argTypes, k)
	local w = sig.fpWidthBytes(argTypes[k])
		or err.plain("internal error: FP register name for a non-FP argument " .. tostring(k))
	local m = 0
	for i = 1, k - 1 do
		if sig.isFpClass(argTypes[i]) then m += 1 end
	end
	return (w == 4 and "movss" or "movsd"), "%xmm" .. tostring(m)
end

-- generic (buffer) entrypoint pizlonated2ET<sig>.
function glue_x86.genericThunk(out, signature, argWords, retWords, retIsPtr, argTypes, retTypes)
	local sym = "pizlonated2ET" .. tostring(signature)
	local argBytes = argWords * 8
	local retBytes = retWords * 8
	local L = ".LBB_2ET" .. signature
	local function e(s) table.insert(out, s) end
	-- dense CC iv/lo locations for arg k (1-based); lo only for pointer args. FP args
	-- have no GPR location (they load straight into their xmm registers).
	local ivLoc, loLoc = denseArgLoc(argTypes, argWords)
	-- outgoing stack words needed (dense words beyond the four CC registers), and
	-- the window size (16-byte multiple so rsp stays 16-aligned at the call).
	local stackWords, winN = 0, 0
	for k = 1, argWords do
		for _, lc in { ivLoc[k], loLoc[k] } do
			if lc and lc.stack then stackWords = math.max(stackWords, lc.stack + 1) end
		end
	end
	if stackWords > 0 then winN = math.ceil(8 * stackWords / 16) * 16 end
	local retFpForm, retFpReg = nil, nil
	if retTypes and retTypes[1] and sig.isFpClass(retTypes[1]) then
		retFpForm, retFpReg = fpArgForm(retTypes, 1)
	end
	e("\t.text")
	e("\t.weak\t" .. sym)
	e("\t.p2align\t4")
	e("\t.type\t" .. sym .. ",@function")
	e(sym .. ":")
	e("\tpushq\t%rbx")
	-- args-size check: actual (rdx) must be >= expected argBytes. Skipped for a zero-arg
	-- signature (argBytes - 1 == -1 would make the unsigned jbe ALWAYS fail — every actual
	-- size is <= -1); clang's zero-arg 2ET thunk has no check either, and the weak symbol
	-- must be link-compatible with clang's (any caller may pass extra argument words).
	if argBytes > 0 then
		e("\tcmpq\t$" .. tostring(argBytes - 1) .. ", %rdx")
		e("\tjbe\t" .. L .. "_fail")
	end
	e("\tmovq\t%rdi, %rbx")
	-- load args from the per-argument buffer slots (iv at 128+8i, lower at 384+8i)
	-- into the densely packed CC registers; FP args load into their xmm registers
	-- (declaration order among the FP args), also from 128+8i (a float loads the
	-- low 4 bytes of the 8-byte slot). Words beyond the four CC registers are
	-- stored into the outgoing-args area at 8*j(%rsp) — exactly clang's placement
	-- (the buffer loads are %rbx-relative, unaffected by the stack adjustment;
	-- %rax is dead scratch here).
	for i = 0, argWords - 1 do
		local k = i + 1
		if sig.isFpClass(argTypes[k]) then
			local form, reg = fpArgForm(argTypes, k)
			e("\t" .. form .. "\t" .. tostring(128 + 8 * i) .. "(%rdi), " .. reg)
		else
			if ivLoc[k].reg then
				e("\tmovq\t" .. tostring(128 + 8 * i) .. "(%rdi), " .. ivLoc[k].reg)
			end
			if loLoc[k] and loLoc[k].reg then
				e("\tmovq\t" .. tostring(384 + 8 * i) .. "(%rdi), " .. loLoc[k].reg)
			end
		end
	end
	if winN > 0 then e("\tsubq\t$" .. tostring(winN) .. ", %rsp") end
	for i = 0, argWords - 1 do
		local k = i + 1
		if not sig.isFpClass(argTypes[k]) then
			if ivLoc[k].stack then
				e("\tmovq\t" .. tostring(128 + 8 * i) .. "(%rbx), %rax")
				e("\tmovq\t%rax, " .. tostring(8 * ivLoc[k].stack) .. "(%rsp)")
			end
			if loLoc[k] and loLoc[k].stack then
				e("\tmovq\t" .. tostring(384 + 8 * i) .. "(%rbx), %rax")
				e("\tmovq\t%rax, " .. tostring(8 * loLoc[k].stack) .. "(%rsp)")
			end
		end
	end
	e("\tcallq\t*(%rsi)")
	-- restore rsp BEFORE the exception-flag test: both paths see a balanced stack
	-- (the flag rides the %al result register, which the add does not touch).
	if winN > 0 then e("\taddq\t$" .. tostring(winN) .. ", %rsp") end
	e("\ttestb\t$1, %al")
	e("\tjne\t" .. L .. "_exc")
	if retWords >= 1 then
		if retFpForm then
			-- FP return: the fast entrypoint left the value in xmm0 (the movss/movsd
			-- form: a float stores the low 4 bytes of the buffer word); the aux word
			-- (the return's "lower") is zero and the ret size stays 8.
			-- (filcc additionally emits `movl $0, 132(%rbx)` for a float return,
			-- zeroing the upper half of the 128-slot buffer word, so this thunk is
			-- not byte-identical to filcc's. The omission is deliberate and
			-- unobserved: every FP reader uses a 4- or 8-byte access — movss/movsd
			-- — so the upper half of the slot is never read.)
			e("\t" .. retFpForm .. "\t" .. retFpReg .. ", 128(%rbx)")
			e("\tmovq\t$0, 384(%rbx)")
		else
			e("\tmovq\t%rdx, 128(%rbx)")
			if retIsPtr then e("\tmovq\t%rcx, 384(%rbx)") else e("\tmovq\t$0, 384(%rbx)") end
		end
	end
	e("\tmovl\t$" .. tostring(retBytes) .. ", %edx")
	e("\tpopq\t%rbx")
	e("\tretq")
	e(L .. "_exc:")
	e("\tpopq\t%rbx")
	e("\tretq")
	e(L .. "_fail:")
	e("\tmovl\t$" .. tostring(argBytes) .. ", %esi")
	e("\tmovq\t%rdx, %rdi")
	e("\txorl\t%edx, %edx")
	e("\tcallq\tfilc_cc_args_check_failure@PLT")
	e("\t.size\t" .. sym .. ", .-" .. sym)
	e("")
end

function glue_x86.foAndOrigins(out, name, signature, numRoots, dbg)
	local fo = "pizlonatedFO_" .. name
	local strSym = ".Lfilc_string_" .. name
	local fileSym = ".Lfilc_filename_" .. name
	local fnOrigin = ".Lfilc_function_origin_" .. name
	local origin = ".Lfilc_origin_" .. name

	table.insert(out, "\t.type\t" .. fo .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t4, 0x0")
	table.insert(out, fo .. ":")
	table.insert(out, "\t.quad\t" .. fo .. "+16")
	table.insert(out, "\t.quad\t(" .. fo .. "+16)+" .. FUNCTION_UPPER_FLAGS)
	table.insert(out, "\t.quad\tpizlonatedFIP" .. tostring(signature) .. "_" .. name)
	table.insert(out, "\t.quad\tpizlonated2ET" .. tostring(signature))
	table.insert(out, "\t.quad\t" .. tostring(signature))
	table.insert(out, "\t.size\t" .. fo .. ", 40")
	table.insert(out, "")

	table.insert(out, "\t.type\t" .. strSym .. ",@object")
	table.insert(out, "\t.section\t.rodata.str1.1,\"aMS\",@progbits,1")
	table.insert(out, strSym .. ":")
	table.insert(out, "\t.asciz\t\"" .. name .. "\"")
	table.insert(out, "\t.size\t" .. strSym .. ", " .. tostring(#name + 1))
	table.insert(out, "")

	if dbg then
		table.insert(out, "\t.type\t" .. fileSym .. ",@object")
		table.insert(out, "\t.section\t.rodata.str1.1,\"aMS\",@progbits,1")
		table.insert(out, fileSym .. ":")
		table.insert(out, "\t.asciz\t\"" .. dbg.filename .. "\"")
		table.insert(out, "\t.size\t" .. fileSym .. ", " .. tostring(#dbg.filename + 1))
		table.insert(out, "")
	end

	table.insert(out, "\t.type\t" .. fnOrigin .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t4, 0x0")
	table.insert(out, fnOrigin .. ":")
	table.insert(out, "\t.quad\t" .. strSym)
	table.insert(out, "\t.quad\t" .. (dbg and fileSym or "0"))
	table.insert(out, "\t.long\t" .. tostring(numRoots))
	table.insert(out, "\t.zero\t4")
	table.insert(out, "\t.quad\t0")
	table.insert(out, "\t.byte\t0")
	table.insert(out, "\t.byte\t0")
	table.insert(out, "\t.byte\t0")
	table.insert(out, "\t.zero\t1")
	table.insert(out, "\t.long\t0")
	table.insert(out, "\t.size\t" .. fnOrigin .. ", 40")
	table.insert(out, "")

	table.insert(out, "\t.type\t" .. origin .. ",@object")
	table.insert(out, "\t.p2align\t3, 0x0")
	table.insert(out, origin .. ":")
	table.insert(out, "\t.quad\t" .. fnOrigin)
	table.insert(out, "\t.long\t" .. tostring(dbg and dbg.funcLine or 0))
	table.insert(out, "\t.long\t0")
	table.insert(out, "\t.size\t" .. origin .. ", 16")
	table.insert(out, "")
end

function glue_x86.accessOrigin(out, sym, size, align, funcName, isWrite, dbg)
	local origin = ".Lfilc_origin_" .. funcName
	if dbg then
		origin = sym .. "_o"
		table.insert(out, "\t.type\t" .. origin .. ",@object")
		table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
		table.insert(out, "\t.p2align\t3, 0x0")
		table.insert(out, origin .. ":")
		table.insert(out, "\t.quad\t.Lfilc_function_origin_" .. funcName)
		table.insert(out, "\t.long\t" .. tostring(dbg.line or 0))
		table.insert(out, "\t.long\t0")
		table.insert(out, "\t.size\t" .. origin .. ", 16")
		table.insert(out, "")
	end
	table.insert(out, "\t.type\t" .. sym .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t4, 0x0")
	table.insert(out, sym .. ":")
	table.insert(out, "\t.long\t" .. tostring(size))
	table.insert(out, "\t.byte\t" .. tostring(align))
	table.insert(out, "\t.byte\t0")
	table.insert(out, "\t.byte\t" .. (isWrite and "1" or "0"))
	table.insert(out, "\t.zero\t1")
	table.insert(out, "\t.quad\t" .. origin)
	table.insert(out, "\t.quad\t" .. origin)
	table.insert(out, "\t.quad\t0")
	table.insert(out, "\t.size\t" .. sym .. ", 32")
	table.insert(out, "")
end

-- A plain 16-byte filc_origin record for the masked-access fail stubs (the
-- runtime's filc_masked_read/write_check_fail and filc_expand/compress fail
-- functions take a filc_origin*, not the 32-byte optimized-access-check origin).
-- Carries the access's line when -g is on, like accessOrigin's generic origin.
function glue_x86.maskedOrigin(out, sym, funcName, dbg)
	table.insert(out, "\t.type\t" .. sym .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t3, 0x0")
	table.insert(out, sym .. ":")
	table.insert(out, "\t.quad\t.Lfilc_function_origin_" .. funcName)
	table.insert(out, "\t.long\t" .. tostring(dbg and dbg.line or 0))
	table.insert(out, "\t.long\t0")
	table.insert(out, "\t.size\t" .. sym .. ", 16")
	table.insert(out, "")
end

function glue_x86.alias(out, name, signature)
	table.insert(out, "\t.globl\tpizlonatedFI" .. tostring(signature) .. "_" .. name)
	table.insert(out, "\t.type\tpizlonatedFI" .. tostring(signature) .. "_" .. name .. ",@function")
	table.insert(out, ".set pizlonatedFI" .. tostring(signature) .. "_" .. name
		.. ", pizlonatedFIP" .. tostring(signature) .. "_" .. name)
end

-- Alias entry labels (tailcall.luau): a `.globl` alias of a function
-- (asm_AES_encrypt for AES_encrypt) shares the function's body and signature,
-- so its getter and direct-call symbol are plain .set aliases — the alias's
-- function object IS the function's.
function glue_x86.getterAlias(out, aliasName, fnName, hidden)
	table.insert(out, "\t.globl\tpizlonated_" .. aliasName)
	if hidden then table.insert(out, "\t.hidden\tpizlonated_" .. aliasName) end
	table.insert(out, ".set pizlonated_" .. aliasName .. ", pizlonated_" .. fnName)
	table.insert(out, "")
end

function glue_x86.callAlias(out, aliasName, fnName, signature, hidden)
	local fiA = "pizlonatedFI" .. tostring(signature) .. "_" .. aliasName
	local fiF = "pizlonatedFI" .. tostring(signature) .. "_" .. fnName
	table.insert(out, "\t.globl\t" .. fiA)
	if hidden then table.insert(out, "\t.hidden\t" .. fiA) end
	table.insert(out, ".set " .. fiA .. ", " .. fiF)
	table.insert(out, "")
end

function glue_x86.originName(funcName) return ".Lfilc_origin_" .. funcName end

-- Data objects (global variables), emitted after all functions. `dos` is the
-- driver's collection result in input order (deterministic). Each DO mirrors
-- FilPizlonator's per-global emission (verified against clang -O2 -S output):
-- the 16-byte header (.quad payloadEnd, .quad flags<<48 — ObjectFlagGlobal at
-- bit 48, ObjectFlagReadonly at bit 49 for const), payload at DO+16 (payload
-- base == capability lower), a leading `.zero A-16` pad when an alignment wider
-- than 16 is required (FilPizlonator's AlignmentOffset trick: payload keeps its
-- alignment), `.LpizlonatedGP_` flight-pointer cache + `.LpizlonatedGS_`
-- slow-init function for writable data (and for exported data, whose getter
-- needs them), and per exported label the `.globl pizlonated_<sym>` getter and
-- the unsafe-export alias `.globl <sym>` + `.set <sym>, <doSym>+<payloadOff>`
-- (`.hidden` propagates to getter and alias).
local DO_FLAGS_W = "281474976710656"   -- 1 << 48 (ObjectFlagGlobal)
local DO_FLAGS_R = "844424930131968"   -- 3 << 48 (Global | Readonly)

-- Exact integer log2 (the alignments are powers of two by construction; the
-- floating-point math.log route can render 2.9999999999999996, which `as`
-- cannot read).
local function log2int(a)
	local p = 0
	while a > 1 do a = math.floor(a / 2); p += 1 end
	return p
end

local function emitDataObject(out, doRec)
	local doSym = doRec.doSym
	table.insert(out, "\t.type\t" .. doSym .. ",@object")
	if doRec.readonly then
		table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	else
		table.insert(out, "\t.data")
	end
	table.insert(out, "\t.p2align\t" .. tostring(log2int(doRec.align)) .. ", 0x0")
	table.insert(out, doSym .. ":")
	if doRec.pad > 0 then table.insert(out, "\t.zero\t" .. tostring(doRec.pad)) end
	table.insert(out, "\t.quad\t" .. doSym .. "+" .. tostring(doRec.totalSize))
	table.insert(out, "\t.quad\t0+" .. (doRec.readonly and DO_FLAGS_R or DO_FLAGS_W))
	table.insert(out, doRec.baseSym .. ":")
	-- payload: user labels and data directives at their collected offsets; gaps
	-- (alignment padding) materialize as .zero
	local cursor = 0
	for _, it in doRec.items do
		if it.off > cursor then
			table.insert(out, "\t.zero\t" .. tostring(it.off - cursor))
			cursor = it.off
		end
		if it.kind == "label" then
			-- An exported label is NOT emitted: the unsafe-export alias
			-- (.set <sym>, <doSym>+<payloadOff>) defines it — defining it twice
			-- (label + alias) is an assembler error.
			if it.name ~= doRec.exportedSym then
				table.insert(out, it.name .. ":")
			end
		elseif it.decoded then
			-- string content, re-emitted as .byte lists (the collector decoded
			-- it, so the byte count is exact by construction); an empty string
			-- emits nothing (its size is 0)
			for p = 1, #it.decoded, 16 do
				local chunk = {}
				for k = p, math.min(p + 15, #it.decoded) do
					table.insert(chunk, tostring(it.decoded[k]))
				end
				table.insert(out, "\t.byte\t" .. table.concat(chunk, ","))
			end
		else
			table.insert(out, "\t" .. it.name .. "\t" .. (it.args or ""))
		end
		cursor += it.size or 0
	end
	if cursor < doRec.payloadSize then
		table.insert(out, "\t.zero\t" .. tostring(doRec.payloadSize - cursor))
	end
	table.insert(out, "\t.size\t" .. doSym .. ", " .. tostring(doRec.totalSize))
	table.insert(out, "")
end

local function emitDataGP(out, doRec)
	table.insert(out, "\t.type\t" .. doRec.gpSym .. ",@object")
	table.insert(out, "\t.local\t" .. doRec.gpSym)
	table.insert(out, "\t.comm\t" .. doRec.gpSym .. ",16,16")
end

local function emitDataSlowInit(out, doRec)
	local gsSym, gpSym, doSym = doRec.gsSym, doRec.gpSym, doRec.doSym
	local doneL = gsSym .. "_done"
	local objAddr = doSym .. (doRec.pad > 0 and ("+" .. tostring(doRec.pad)) or "")
	table.insert(out, "\t.text")
	table.insert(out, "\t.p2align\t4")
	table.insert(out, "\t.type\t" .. gsSym .. ",@function")
	table.insert(out, gsSym .. ":")
	table.insert(out, "\tpushq\t%rbx")
	table.insert(out, "\tmovq\t%rdi, %rbx")
	table.insert(out, "\tleaq\t" .. gpSym .. "(%rip), %rdx")
	table.insert(out, "\tleaq\t" .. objAddr .. "(%rip), %rcx")
	table.insert(out, "\tcallq\tfilc_global_initialization_start@PLT")
	table.insert(out, "\ttestb\t$1, %al")
	table.insert(out, "\tje\t" .. doneL)
	table.insert(out, "\tmovq\t%rbx, %rdi")
	table.insert(out, "\tpopq\t%rbx")
	table.insert(out, "\tjmp\tfilc_global_initialization_end@PLT")
	table.insert(out, doneL .. ":")
	table.insert(out, "\tpopq\t%rbx")
	table.insert(out, "\tretq")
	table.insert(out, "\t.size\t" .. gsSym .. ", .-" .. gsSym)
	table.insert(out, "")
end

local function emitDataGetter(out, doRec, sym)
	local gpSym, gsSym, doSym = doRec.gpSym, doRec.gsSym, doRec.doSym
	local doneL = ".Lglobalgetter_done_" .. sym
	local payloadAddr = doSym .. "+" .. tostring(doRec.pad + 16)
	table.insert(out, "\t.text")
	if doRec.hidden then table.insert(out, "\t.hidden\tpizlonated_" .. sym) end
	table.insert(out, "\t.globl\tpizlonated_" .. sym)
	table.insert(out, "\t.p2align\t4")
	table.insert(out, "\t.type\tpizlonated_" .. sym .. ",@function")
	table.insert(out, "pizlonated_" .. sym .. ":")
	table.insert(out, "\tmovq\t" .. gpSym .. "+8(%rip), %rdx")
	table.insert(out, "\tmovq\t" .. gpSym .. "(%rip), %rax")
	table.insert(out, "\ttestq\t%rax, %rax")
	table.insert(out, "\tsete\t%cl")
	table.insert(out, "\ttestq\t%rdx, %rdx")
	table.insert(out, "\tsete\t%r8b")
	table.insert(out, "\torb\t%cl, %r8b")
	table.insert(out, "\tje\t" .. doneL)
	table.insert(out, "\tpushq\t%rax")
	table.insert(out, "\tcallq\t" .. gsSym)
	table.insert(out, "\tleaq\t" .. payloadAddr .. "(%rip), %rax")
	table.insert(out, "\tmovq\t%rax, %rdx")
	table.insert(out, "\taddq\t$8, %rsp")
	table.insert(out, doneL .. ":")
	table.insert(out, "\tretq")
	table.insert(out, "\t.size\tpizlonated_" .. sym .. ", .-pizlonated_" .. sym)
	table.insert(out, "")
	-- unsafe-export alias: the plain name addresses the payload (so yolo code
	-- and other modules' references resolve), exactly FilPizlonator's
	-- `.filc_unsafe_export` shape.
	if doRec.hidden then table.insert(out, "\t.hidden\t" .. sym) end
	table.insert(out, "\t.globl\t" .. sym)
	table.insert(out, ".set " .. sym .. ", " .. payloadAddr)
	table.insert(out, "")
end

function glue_x86.dataGlobals(out, dos)
	for _, doRec in dos do
		emitDataObject(out, doRec)
		-- Writable DOs need registration (the GC must scan them for pointers),
		-- exported DOs need it for their getter; readonly local data never holds
		-- a capability-tracked pointer, so it emits only the DO.
		if not doRec.readonly or doRec.exportedSym then
			emitDataGP(out, doRec)
			emitDataSlowInit(out, doRec)
		end
		if doRec.exportedSym then
			emitDataGetter(out, doRec, doRec.exportedSym)
		end
	end
end

-- `.section .init` / `.section .fini` fragments: each collected annotated
-- void() call is marshalled as a Fil-C constructor call, emitted back into its
-- section in input order. The .init context runs from _init as plain SysV code
-- — no myth register, possibly before the runtime is initialized — so the call
-- goes through filc_defer_or_run_global_ctor (exactly what filcc emits for a C
-- constructor's .Lfilc_ctor_forwarder): it takes the callee's function-object
-- flight pointer (rdi=intval, rsi=lower), defers to before-main when the
-- runtime is not up yet, and otherwise runs it through the generic (buffer) CC
-- with argc/argv/environ — a void() signature's 2ET thunk accepts and ignores
-- those. A same-file function's flight pointer materializes as a rip-relative
-- lea of its FO payload (intval == lower == pizlonatedFO_<owner>+16, exactly
-- the getter's output); an extern function resolves through its cross-module
-- getter pizlonated_<target> (rax=intval, rdx=lower). The fragments make their
-- calls with rsp exactly as found (no pushes): _init's crti prologue
-- (sub $8 after the call in) leaves rsp 16-byte aligned at fragment entry, so
-- every call site is correctly aligned. A data/signature-mismatch target fails
-- the runtime's function-call check, like any indirect call.
function glue_x86.initSections(out, frags)
	local curSect = nil
	for _, f in frags do
		if f.sect ~= curSect then
			table.insert(out, "\t.section\t" .. f.sect)
			curSect = f.sect
		end
		if f.owner then
			table.insert(out, "\tleaq\tpizlonatedFO_" .. f.owner .. "+16(%rip), %rdi")
			table.insert(out, "\tmovq\t%rdi, %rsi")
		else
			table.insert(out, "\txorl\t%esi, %esi")
			table.insert(out, "\tcallq\tpizlonated_" .. f.target .. "@PLT")
			table.insert(out, "\tmovq\t%rax, %rdi")
			table.insert(out, "\tmovq\t%rdx, %rsi")
		end
		table.insert(out, "\tcallq\tfilc_defer_or_run_global_ctor@PLT")
	end
	if curSect then table.insert(out, "") end
end

-- Callsite resolver thunk pizlonatedFI<sig>_NAME (weak/hidden), validated against the
-- reference Fil-C clang x86_64 output (test3.c -> pizlonatedFI2529_foo). Resolves the
-- callee's FO via pizlonated_NAME, validates it is a function object with the canonical
-- intval and matching signature, then tail-calls the fast entrypoint; on a signature
-- mismatch it marshals through the generic (buffer) CC.
--
-- FP arguments (float/double) ride in xmm0..xmm7 and are NEVER spilled by this thunk:
-- everything between entry and the two call sites — the register saves, the getter
-- (leaq/movq/retq), the object checks and the signature compare — touches GPRs only,
-- so on the fast path the FP args are still in their xmm registers when the tail call
-- consumes them, and on the generic path they are stored into the CC buffer straight
-- from there (each at its DECLARATION index, one 8-byte word; a float stores the low
-- 4 bytes and the aux word at 384+8i is zero). An FP return is loaded back from the
-- buffer word at [myth+128] into xmm0 (movss/movsd form) after the ret-size check;
-- the ret size stays 8.
function glue_x86.callsiteThunk(out, callee, sigDesc)
	local signum = sigDesc.signature
	local sym = "pizlonatedFI" .. tostring(signum) .. "_" .. callee
	local L = ".Lcsr_" .. callee .. "_" .. tostring(signum)
	-- incoming fast-call args arrive densely packed; lo words only for ptr args.
	-- FP args arrive in their xmm registers (fpArgForm) and pass through unmodeled —
	-- ivLoc[k] is nil for them. Dense words beyond the four CC registers arrive on
	-- the STACK at [entry_rsp + 8 + 8*j] (dense word 4+j) — densely packed 8-byte
	-- slots in declaration word order, exactly as the callee's fast entrypoint
	-- expects them, so the direct fast tail call needs NO marshalling for them
	-- (they flow through in place; the getter call between touches no stack above
	-- its own return address).
	local ivLoc, loLoc = denseArgLoc(sigDesc.argTypes, sigDesc.argWords)
	local myth = "%rbx"
	local pool = { "%r12", "%r13", "%r14", "%r15" }
	-- Stashing the incoming CC across the resolver call takes one callee-saved hold
	-- per REGISTER-RESIDENT GPR argument word (two per pointer arg whose intval and
	-- lower both fit in registers: intval + lower). FP args need no hold (they stay
	-- in the xmm registers), and stack-resident words need no hold either (they stay
	-- on the stack). At most four words are register-resident — exactly the pool.
	local nextHold = 1
	local args = {}
	for k = 1, sigDesc.argWords do
		if ivLoc[k] then
			local isPtr = sigDesc.argTypes[k] == 7
			local ent = { isPtr = isPtr }
			if ivLoc[k].reg then ent.iv = pool[nextHold]; nextHold += 1
			else ent.ivStack = ivLoc[k].stack end
			if isPtr then
				if loLoc[k].reg then ent.lo = pool[nextHold]; nextHold += 1
				else ent.loStack = loLoc[k].stack end
			end
			args[k] = ent
		end
	end
	assert(nextHold - 1 <= #pool, "sarcasm: internal error: callsite thunk hold pool overflow")
	local argBytes = sigDesc.argWords * 8
	local retBytes = sigDesc.retWords * 8
	local retFpForm, retFpReg = nil, nil
	if sigDesc.retTypes[1] and sig.isFpClass(sigDesc.retTypes[1]) then
		retFpForm, retFpReg = fpArgForm(sigDesc.retTypes, 1)
	end
	local function e(s) table.insert(out, s) end

	-- push list (must leave rsp 16-byte aligned at the call: retaddr + N pushes, N odd).
	local pushList = { myth }
	for k = 1, sigDesc.argWords do
		if args[k] then
			if args[k].iv then table.insert(pushList, args[k].iv) end
			if args[k].lo then table.insert(pushList, args[k].lo) end
		end
	end
	local filler = (#pushList % 2) == 0
	local function restore()
		if filler then e("\taddq\t$8, %rsp") end
		for i = #pushList, 1, -1 do e("\tpopq\t" .. pushList[i]) end
	end

	e("\t.text")
	e("\t.weak\t" .. sym)
	e("\t.hidden\t" .. sym)
	e("\t.p2align\t4")
	e("\t.type\t" .. sym .. ",@function")
	e(sym .. ":")
	for _, r in pushList do e("\tpushq\t" .. r) end
	if filler then e("\tpushq\t%rax") end
	-- stash incoming CC (myth + register-resident GPR args) into callee-saved holds.
	-- The xmm registers are untouched here (and by everything up to the calls below),
	-- so the FP args ride across the getter call in them; stack-resident argument
	-- words stay on the stack.
	e("\tmovq\t%rdi, " .. myth)
	for k = 1, sigDesc.argWords do
		if args[k] then
			if args[k].iv then e("\tmovq\t" .. ivLoc[k].reg .. ", " .. args[k].iv) end
			if args[k].lo then e("\tmovq\t" .. loLoc[k].reg .. ", " .. args[k].lo) end
		end
	end
	-- resolve FO
	e("\txorl\t%esi, %esi")
	e("\tcallq\tpizlonated_" .. callee .. "@PLT")   -- rax = iv, rdx = lower(FO)
	e("\tmovq\t%rdx, %rsi")                          -- rsi = FO payload
	e("\ttestq\t%rdx, %rdx")
	e("\tje\t" .. L .. "_fail")
	e("\tmovq\t-8(%rsi), %rcx")                       -- aux word (flags)
	e("\tmovabsq\t$0x780000000000000, %rdx")
	e("\tandq\t%rcx, %rdx")
	e("\tmovabsq\t$0x80000000000000, %rdi")
	e("\tcmpq\t%rdi, %rdx")
	e("\tjne\t" .. L .. "_fail")
	e("\tmovabsq\t$0xffffffffffff, %rdx")
	e("\tandq\t%rdx, %rcx")
	e("\tcmpq\t%rcx, %rax")                           -- intval must equal canonical entrypoint
	e("\tjne\t" .. L .. "_fail")
	-- signature. `cmpq $imm32, mem` sign-extends a 32-bit immediate, so signatures
	-- past imm32 (eight doubles -> 8552919316) need the constant materialized first:
	-- movabsq into %r10 (a dead scratch here — the SysV argument registers were
	-- already copied into the hold pool, and %r10 is caller-saved scratch that
	-- nothing past this compare reads), then a register compare.
	if signum <= 2147483647 and signum >= -2147483648 then
		e("\tcmpq\t$" .. tostring(signum) .. ", 16(%rsi)")
	else
		e("\tmovabsq\t$" .. tostring(signum) .. ", %r10")
		e("\tcmpq\t%r10, 16(%rsi)")
	end
	e("\tjne\t" .. L .. "_generic")
	-- direct fast call
	e("\tmovq\t(%rsi), %rax")                         -- fast entrypoint
	e("\tmovq\t" .. myth .. ", %rdi")
	for k = 1, sigDesc.argWords do
		if args[k] then
			if args[k].iv then e("\tmovq\t" .. args[k].iv .. ", " .. ivLoc[k].reg) end
			if args[k].lo then e("\tmovq\t" .. args[k].lo .. ", " .. loLoc[k].reg) end
			-- stack-resident words flow through in place: the tail call keeps rsp
			-- (and the outgoing area above it) exactly as the caller left it.
		end
	end
	restore()
	e("\tjmpq\t*%rax")
	-- generic (buffer) call. A stack-resident incoming word j sits at
	-- [entry_rsp + 8 + 8*j]; rsp here is entry_rsp - 8*#pushList - (filler and 8 or 0),
	-- so it is read back at that offset from the current rsp (%rax is dead scratch).
	e(L .. "_generic:")
	local stackAdj = 8 * #pushList + (filler and 8 or 0)
	for k = 1, sigDesc.argWords do
		if sig.isFpClass(sigDesc.argTypes[k]) then
			-- FP arg: still live in its xmm register (nothing clobbered the xmm
			-- file); store it at its declaration index (a float = 4-byte store into
			-- the 8-byte slot) with a zero aux word.
			local form, reg = fpArgForm(sigDesc.argTypes, k)
			e("\t" .. form .. "\t" .. reg .. ", " .. tostring(128 + 8 * (k - 1)) .. "(" .. myth .. ")")
			e("\tmovq\t$0, " .. tostring(384 + 8 * (k - 1)) .. "(" .. myth .. ")")
		else
			if args[k].iv then
				e("\tmovq\t" .. args[k].iv .. ", " .. tostring(128 + 8 * (k - 1)) .. "(" .. myth .. ")")
			else
				e("\tmovq\t" .. tostring(stackAdj + 8 + 8 * args[k].ivStack) .. "(%rsp), %rax")
				e("\tmovq\t%rax, " .. tostring(128 + 8 * (k - 1)) .. "(" .. myth .. ")")
			end
			if args[k].lo then
				e("\tmovq\t" .. args[k].lo .. ", " .. tostring(384 + 8 * (k - 1)) .. "(" .. myth .. ")")
			elseif args[k].loStack then
				e("\tmovq\t" .. tostring(stackAdj + 8 + 8 * args[k].loStack) .. "(%rsp), %rax")
				e("\tmovq\t%rax, " .. tostring(384 + 8 * (k - 1)) .. "(" .. myth .. ")")
			else
				e("\tmovq\t$0, " .. tostring(384 + 8 * (k - 1)) .. "(" .. myth .. ")")
			end
		end
	end
	e("\tmovl\t$" .. tostring(argBytes) .. ", %edx")
	e("\tmovq\t" .. myth .. ", %rdi")
	e("\tcallq\t*8(%rsi)")                            -- generic entrypoint
	e("\ttestb\t$1, %al")
	e("\tjne\t" .. L .. "_exc")
	if sigDesc.retWords >= 1 then
		e("\tcmpq\t$" .. tostring(retBytes - 1) .. ", %rdx")
		e("\tjbe\t" .. L .. "_retfail")
		if retFpForm then
			-- FP return: read the buffer word back into xmm0 (a float = 4-byte load)
			e("\t" .. retFpForm .. "\t128(" .. myth .. "), " .. retFpReg)
		else
			e("\tmovq\t128(" .. myth .. "), %rdx")
			if sigDesc.retIsPtr then e("\tmovq\t384(" .. myth .. "), %rcx") end
		end
	end
	e(L .. "_done:")
	restore()
	e("\tretq")
	e(L .. "_exc:")
	e("\tjmp\t" .. L .. "_done")
	e(L .. "_fail:")
	e("\tmovq\t%rax, %rdi")
	e("\tcallq\tfilc_check_function_call_fail@PLT")
	if sigDesc.retWords >= 1 then
		e(L .. "_retfail:")
		e("\tmovl\t$" .. tostring(retBytes) .. ", %esi")
		e("\tmovq\t%rdx, %rdi")
		e("\txorl\t%edx, %edx")
		e("\tcallq\tfilc_cc_rets_check_failure@PLT")
	end
	e("\t.size\t" .. sym .. ", .-" .. sym)
	e("")
end

return glue_x86
