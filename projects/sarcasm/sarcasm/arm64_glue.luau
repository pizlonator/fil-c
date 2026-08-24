--!nonstrict
-- Emits the Fil-C calling-convention "glue" for an exported function: the function
-- getter, the FO (function object), the generic (buffer) entrypoint thunk, the
-- per-function origin objects, and the direct-call alias. These templates are
-- validated against the live runtime (see ABI-NOTES.md / hash.yolo.s).

local glue = {}

local FUNCTION_UPPER_FLAGS = "36873221949095936" -- 0x83 << 48

-- Dense fast-CC arg registers: (iv[, lo]) pairs packed consecutively from x2; a scalar
-- arg consumes one slot, a pointer arg two. Matches pizlonated clang output. The arm64
-- fast CC has SIX dense argument registers (x2..x7) -- wider arguments go on the stack,
-- which sarcasm does not marshal; signatures needing more than 6 register argument
-- words are rejected.
local DENSE_ARG_REGS = { 2, 3, 4, 5, 6, 7 }
local function denseArgRegs(argTypes, argWords)
	local ivReg, loReg = {}, {}
	local slot = 1
	for k = 1, argWords do
		ivReg[k] = DENSE_ARG_REGS[slot]
		if argTypes[k] == 7 then   -- TYPE_PTR
			loReg[k] = DENSE_ARG_REGS[slot + 1]
			slot += 2
		else
			slot += 1
		end
	end
	return ivReg, loReg
end

-- Number of fast-CC register argument words a signature needs (ptr args = 2).
local function argRegWords(argTypes, argWords)
	local words = 0
	for k = 1, argWords do words += (argTypes[k] == 7) and 2 or 1 end
	return words
end

-- Function getter: returns the FO flight pointer (intval in x0, lower in x1).
function glue.getter(out, name: string)
	local fo = "pizlonatedFO_" .. name
	table.insert(out, "\t.text")
	table.insert(out, "\t.globl\tpizlonated_" .. name)
	table.insert(out, "\t.p2align\t2")
	table.insert(out, "\t.type\tpizlonated_" .. name .. ",@function")
	table.insert(out, "pizlonated_" .. name .. ":")
	table.insert(out, "\tadrp\tx0, " .. fo .. "+16")
	table.insert(out, "\tadd\tx0, x0, :lo12:" .. fo .. "+16")
	table.insert(out, "\tmov\tx1, x0")
	table.insert(out, "\tret")
	table.insert(out, ".Lfunc_end_getter_" .. name .. ":")
	table.insert(out, "\t.size\tpizlonated_" .. name .. ", .Lfunc_end_getter_" .. name .. "-pizlonated_" .. name)
	table.insert(out, "")
end

-- Generic (buffer-based) entrypoint thunk pizlonated2ET<sig>. Marshals args from the
-- thread CC buffers into the register CC, calls the fast entrypoint, marshals the
-- return value back. argWords/retWords count 8-byte words. retIsPtr controls whether
-- the return lower is copied (else zeroed).
function glue.genericThunk(out, signature: number, argWords: number, retWords: number, retIsPtr: boolean, argTypes)
	assert(argRegWords(argTypes, argWords) <= 6, "sarcasm: >6 register argument words not yet supported")
	local sym = "pizlonated2ET" .. tostring(signature)
	local argBytes = argWords * 8
	local retBytes = retWords * 8
	local lend = ".LBB_2ET" .. signature
	-- dense CC iv/lo registers for arg k (1-based); lo only for pointer args.
	local ivReg, loReg = denseArgRegs(argTypes, argWords)
	table.insert(out, "\t.text")
	table.insert(out, "\t.weak\t" .. sym)
	table.insert(out, "\t.p2align\t2")
	table.insert(out, "\t.type\t" .. sym .. ",@function")
	table.insert(out, sym .. ":")
	table.insert(out, "\tstp\tx30, x19, [sp, #-16]!")
	table.insert(out, "\tcmp\tx2, #" .. tostring(argBytes))
	table.insert(out, "\tb.lo\t" .. lend .. "_fail")
	-- load args from the per-argument buffer slots (iv at 128+8i, lower at 384+8i)
	-- into the densely packed CC registers.
	for i = 0, argWords - 1 do
		table.insert(out, "\tldr\tx" .. tostring(ivReg[i + 1]) .. ", [x0, #" .. tostring(128 + 8 * i) .. "]")
		if loReg[i + 1] then
			table.insert(out, "\tldr\tx" .. tostring(loReg[i + 1]) .. ", [x0, #" .. tostring(384 + 8 * i) .. "]")
		end
	end
	table.insert(out, "\tmov\tx19, x0")
	table.insert(out, "\tldr\tx8, [x1]")
	table.insert(out, "\tblr\tx8")
	table.insert(out, "\ttbnz\tw0, #0, " .. lend .. "_exc")
	if retWords >= 1 then
		table.insert(out, "\tstr\tx1, [x19, #128]")
	end
	table.insert(out, "\tmov\tw1, #" .. tostring(retBytes))
	if retWords >= 1 then
		if retIsPtr then
			table.insert(out, "\tstr\tx2, [x19, #384]")
		else
			table.insert(out, "\tstr\txzr, [x19, #384]")
		end
	end
	table.insert(out, lend .. "_ret:")
	table.insert(out, "\tand\tw0, w0, #0x1")
	table.insert(out, "\tldp\tx30, x19, [sp], #16")
	table.insert(out, "\tret")
	table.insert(out, lend .. "_exc:")
	table.insert(out, "\tb\t" .. lend .. "_ret")
	table.insert(out, lend .. "_fail:")
	table.insert(out, "\tmov\tx0, x2")
	table.insert(out, "\tmov\tw1, #" .. tostring(argBytes))
	table.insert(out, "\tmov\tx2, xzr")
	table.insert(out, "\tbl\tfilc_cc_args_check_failure")
	table.insert(out, "\t.size\t" .. sym .. ", .-" .. sym)
	table.insert(out, "")
end

-- The FO object plus per-function origin objects.
-- numRoots = number of GC pointer-root slots in the frame (matches origin count field).
-- `dbg`, when present, is { filename = <string>, funcLine = <number> } and makes the
-- function origin reference a filename string and carry the function's source line.
function glue.foAndOrigins(out, name: string, signature: number, numRoots: number, dbg)
	local fo = "pizlonatedFO_" .. name
	local strSym = ".Lfilc_string_" .. name
	local fileSym = ".Lfilc_filename_" .. name
	local fnOrigin = ".Lfilc_function_origin_" .. name
	local origin = ".Lfilc_origin_" .. name

	table.insert(out, "\t.type\t" .. fo .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t4, 0x0")
	table.insert(out, fo .. ":")
	table.insert(out, "\t.xword\t" .. fo .. "+16")
	table.insert(out, "\t.xword\t(" .. fo .. "+16)+" .. FUNCTION_UPPER_FLAGS)
	table.insert(out, "\t.xword\tpizlonatedFIP" .. tostring(signature) .. "_" .. name)
	table.insert(out, "\t.xword\tpizlonated2ET" .. tostring(signature))
	table.insert(out, "\t.xword\t" .. tostring(signature))
	table.insert(out, "\t.size\t" .. fo .. ", 40")
	table.insert(out, "")

	table.insert(out, "\t.type\t" .. strSym .. ",@object")
	table.insert(out, "\t.section\t.rodata.str1.4,\"aMS\",@progbits,1")
	table.insert(out, "\t.p2align\t2, 0x0")
	table.insert(out, strSym .. ":")
	table.insert(out, "\t.asciz\t\"" .. name .. "\"")
	table.insert(out, "\t.size\t" .. strSym .. ", " .. tostring(#name + 1))
	table.insert(out, "")

	-- optional filename string (only with -g)
	if dbg then
		table.insert(out, "\t.type\t" .. fileSym .. ",@object")
		table.insert(out, "\t.section\t.rodata.str1.4,\"aMS\",@progbits,1")
		table.insert(out, "\t.p2align\t2, 0x0")
		table.insert(out, fileSym .. ":")
		table.insert(out, "\t.asciz\t\"" .. dbg.filename .. "\"")
		table.insert(out, "\t.size\t" .. fileSym .. ", " .. tostring(#dbg.filename + 1))
		table.insert(out, "")
	end

	table.insert(out, "\t.type\t" .. fnOrigin .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t4, 0x0")
	table.insert(out, fnOrigin .. ":")
	table.insert(out, "\t.xword\t" .. strSym)
	table.insert(out, "\t.xword\t" .. (dbg and fileSym or "0"))
	table.insert(out, "\t.word\t" .. tostring(numRoots))
	table.insert(out, "\t.zero\t4")
	-- personality_getter = NULL (no landing pads in sarcasm-generated code), but
	-- can_throw/can_catch must be 1: Fil-C unwinding walks the filc_frame chain
	-- (filc_native__Unwind_RaiseException phase 1) and fatals on any frame whose
	-- function origin is !can_catch, or !can_throw without a personality — which
	-- would make a C++ exception through a sarcasm frame terminate instead of
	-- reaching the caller's handler. The exception itself propagates through the
	-- fast-CC return flag, which the FIP's post-call tbnz/exception epilogue
	-- already forwards (matching clang's no-personality frames: can_throw=1,
	-- can_catch=1, personality NULL). has_setjmps = 0, num_stack_auxes = 0.
	table.insert(out, "\t.xword\t0")
	table.insert(out, "\t.byte\t1")   -- can_throw
	table.insert(out, "\t.byte\t1")   -- can_catch
	table.insert(out, "\t.byte\t0")   -- has_setjmps
	table.insert(out, "\t.zero\t1")
	table.insert(out, "\t.word\t0")   -- num_stack_auxes
	table.insert(out, "\t.size\t" .. fnOrigin .. ", 40")
	table.insert(out, "")

	table.insert(out, "\t.type\t" .. origin .. ",@object")
	table.insert(out, "\t.p2align\t3, 0x0")
	table.insert(out, origin .. ":")
	table.insert(out, "\t.xword\t" .. fnOrigin)
	table.insert(out, "\t.word\t" .. tostring(dbg and dbg.funcLine or 0))
	table.insert(out, "\t.word\t0")
	table.insert(out, "\t.size\t" .. origin .. ", 16")
	table.insert(out, "")
end

-- An optimized-access-check origin (one per distinct access site).
-- size = access size in bytes, align = required alignment, isWrite marks a store.
-- `dbg`, when present, is { line = <number> } and gives this access its own source-line
-- origin (rather than sharing the function origin).
function glue.accessOrigin(out, sym: string, size: number, align: number, funcName: string, isWrite: boolean?, dbg)
	local origin = ".Lfilc_origin_" .. funcName
	if dbg then
		-- per-access origin referencing the function origin, with this access's line
		origin = sym .. "_o"
		table.insert(out, "\t.type\t" .. origin .. ",@object")
		table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
		table.insert(out, "\t.p2align\t3, 0x0")
		table.insert(out, origin .. ":")
		table.insert(out, "\t.xword\t.Lfilc_function_origin_" .. funcName)
		table.insert(out, "\t.word\t" .. tostring(dbg.line or 0))
		table.insert(out, "\t.word\t0")
		table.insert(out, "\t.size\t" .. origin .. ", 16")
		table.insert(out, "")
	end
	table.insert(out, "\t.type\t" .. sym .. ",@object")
	table.insert(out, "\t.section\t.data.rel.ro,\"aw\",@progbits")
	table.insert(out, "\t.p2align\t4, 0x0")
	table.insert(out, sym .. ":")
	table.insert(out, "\t.word\t" .. tostring(size))
	table.insert(out, "\t.byte\t" .. tostring(align))
	table.insert(out, "\t.byte\t0")
	table.insert(out, "\t.byte\t" .. (isWrite and "1" or "0"))
	table.insert(out, "\t.zero\t1")
	table.insert(out, "\t.xword\t" .. origin)
	table.insert(out, "\t.xword\t" .. origin)
	table.insert(out, "\t.xword\t0")
	table.insert(out, "\t.size\t" .. sym .. ", 32")
	table.insert(out, "")
end

-- The direct-call alias pizlonatedFI<sig>_<name> = pizlonatedFIP<sig>_<name>.
function glue.alias(out, name: string, signature: number)
	table.insert(out, "\t.globl\tpizlonatedFI" .. tostring(signature) .. "_" .. name)
	table.insert(out, "\t.type\tpizlonatedFI" .. tostring(signature) .. "_" .. name .. ",@function")
	table.insert(out, ".set pizlonatedFI" .. tostring(signature) .. "_" .. name
		.. ", pizlonatedFIP" .. tostring(signature) .. "_" .. name)
end

function glue.originName(funcName: string): string
	return ".Lfilc_origin_" .. funcName
end

-- Emit a weak/hidden callsite resolver thunk pizlonatedFI<sig>_NAME for a called
-- external function. It resolves the callee's function object via the pizlonated_NAME
-- getter, validates that it is a function object whose intval is the canonical
-- entrypoint and whose signature matches, then tail-calls the fast entrypoint. On a
-- signature mismatch it marshals through the generic (buffer) calling convention. On a
-- non-function object it raises filc_check_function_call_fail. (Template: clang's
-- pizlonatedFI<sig>_NAME in test3.c output.)
function glue.callsiteThunk(out, callee: string, sigDesc)
	local sig = sigDesc.signature
	local sym = "pizlonatedFI" .. tostring(sig) .. "_" .. callee
	local L = ".Lcsr_" .. callee .. "_" .. tostring(sig)

	-- incoming fast-call args arrive densely packed in x2..x7; lo registers only for
	-- pointer args.
	local ivReg, loReg = denseArgRegs(sigDesc.argTypes, sigDesc.argWords)

	-- register save plan: x19 = myth; then per-arg iv (+ lo if pointer) from x20 up.
	-- Stashing the incoming CC across the resolver call takes one callee-saved hold per
	-- argument word (two per pointer arg: intval + lower). The CC has at most 6 argument
	-- words (x2..x7), so the holds top out at x20..x25, well within x19..x28; a signature
	-- needing more than 6 words goes on the stack, which sarcasm does not marshal --
	-- reject it cleanly.
	local holdWords = argRegWords(sigDesc.argTypes, sigDesc.argWords)
	if holdWords > 6 then
		error("sarcasm: callsite thunk for '" .. callee .. "': >6 register argument words not yet supported"
			.. " (this call's signature needs " .. tostring(holdWords) .. ")")
	end
	-- The fast tail call loads the canonical entrypoint into the first register past the
	-- dense argument words (x2..x(1+holdWords)) -- exactly pizlonated clang's choice
	-- (x4 for 2 words, x6 for 4, x8 for 6) -- so it can never clobber an argument.
	local entryReg = 2 + holdWords
	local myth = 19
	local nextReg = 20
	local args = {}  -- k -> { iv = reg, lo = reg|nil, isPtr = bool }
	for k = 1, sigDesc.argWords do
		local isPtr = sigDesc.argTypes[k] == 7
		local ent = { iv = nextReg, isPtr = isPtr }
		nextReg += 1
		if isPtr then ent.lo = nextReg; nextReg += 1 end
		args[k] = ent
	end
	local saved = { myth }
	for k = 1, sigDesc.argWords do
		table.insert(saved, args[k].iv)
		if args[k].lo then table.insert(saved, args[k].lo) end
	end
	local total = math.floor((8 * (1 + #saved) + 15) / 16) * 16
	local argBytes = sigDesc.argWords * 8
	local retBytes = sigDesc.retWords * 8

	local function e(s) table.insert(out, s) end
	e("\t.text")
	e("\t.weak\t" .. sym)
	e("\t.hidden\t" .. sym)
	e("\t.p2align\t2")
	e("\t.type\t" .. sym .. ",@function")
	e(sym .. ":")
	e("\tsub\tsp, sp, #" .. total)
	e("\tstr\tx30, [sp, #0]")
	for i, r in saved do e("\tstr\tx" .. r .. ", [sp, #" .. (8 * i) .. "]") end
	-- save incoming CC registers into callee-saved
	e("\tmov\tx" .. myth .. ", x0")
	for k = 1, sigDesc.argWords do
		e("\tmov\tx" .. args[k].iv .. ", x" .. ivReg[k])
		if args[k].lo then e("\tmov\tx" .. args[k].lo .. ", x" .. loReg[k]) end
	end
	-- resolve the function object
	e("\tbl\tpizlonated_" .. callee)
	e("\tcbz\tx1, " .. L .. "_fail")
	e("\tldur\tx8, [x1, #-8]")
	e("\tmov\tx10, #36028797018963968")          -- 0x80000000000000
	e("\tand\tx9, x8, #0x780000000000000")
	e("\tcmp\tx9, x10")
	e("\tb.ne\t" .. L .. "_fail")
	e("\tand\tx8, x8, #0xffffffffffff")
	e("\tcmp\tx0, x8")
	e("\tb.ne\t" .. L .. "_fail")
	e("\tldr\tx8, [x1, #16]")                      -- signature
	-- Signature compare. `cmp x8, #imm` only takes a 12-bit immediate, so mirror what
	-- pizlonated clang emits for larger signatures: a single movz into w9 when the
	-- signature fits in 16 bits (e.g. 12777), a sub/cmp pair splitting off the top
	-- 12-bit chunk when the high part fits (e.g. 194714 = 47<<12 | 2202), and
	-- movz/movk otherwise (e.g. 23561750). x8/x9 are dead scratch at this point.
	if sig <= 4095 then
		e("\tcmp\tx8, #" .. tostring(sig))
	elseif sig <= 65535 then
		e("\tmov\tw9, #" .. tostring(sig))
		e("\tcmp\tx8, x9")
	elseif math.floor(sig / 4096) <= 4095 then
		e("\tsub\tx8, x8, #" .. tostring(math.floor(sig / 4096)) .. ", lsl #12")
		e("\tcmp\tx8, #" .. tostring(sig % 4096))
	else
		e("\tmov\tw9, #" .. tostring(sig % 65536))
		e("\tmovk\tw9, #" .. tostring(math.floor(sig / 65536)) .. ", lsl #16")
		e("\tcmp\tx8, x9")
	end
	e("\tb.ne\t" .. L .. "_generic")
	-- direct fast call
	e("\tmov\tx0, x" .. myth)
	for k = 1, sigDesc.argWords do
		e("\tmov\tx" .. ivReg[k] .. ", x" .. args[k].iv)
		if args[k].lo then e("\tmov\tx" .. loReg[k] .. ", x" .. args[k].lo) end
	end
	for i, r in saved do e("\tldr\tx" .. r .. ", [sp, #" .. (8 * i) .. "]") end
	e("\tldr\tx" .. entryReg .. ", [x1]")
	e("\tldr\tx30, [sp, #0]")
	e("\tadd\tsp, sp, #" .. total)
	e("\tbr\tx" .. entryReg)
	-- generic buffer call
	e(L .. "_generic:")
	for k = 1, sigDesc.argWords do
		e("\tstr\tx" .. args[k].iv .. ", [x" .. myth .. ", #" .. (128 + 8 * (k - 1)) .. "]")
		if args[k].lo then
			e("\tstr\tx" .. args[k].lo .. ", [x" .. myth .. ", #" .. (384 + 8 * (k - 1)) .. "]")
		else
			e("\tstr\txzr, [x" .. myth .. ", #" .. (384 + 8 * (k - 1)) .. "]")
		end
	end
	e("\tmov\tx0, x" .. myth)
	e("\tmov\tw2, #" .. argBytes)
	e("\tldr\tx8, [x1, #8]")
	e("\tblr\tx8")
	e("\ttbnz\tw0, #0, " .. L .. "_exc")
	if sigDesc.retWords >= 1 then
		e("\tcmp\tx1, #" .. tostring(retBytes))
		e("\tb.lo\t" .. L .. "_retfail")
		e("\tldr\tx1, [x" .. myth .. ", #128]")
		if sigDesc.retIsPtr then e("\tldr\tx2, [x" .. myth .. ", #384]") end
	end
	e(L .. "_done:")
	for i, r in saved do e("\tldr\tx" .. r .. ", [sp, #" .. (8 * i) .. "]") end
	e("\tand\tw0, w0, #0x1")
	e("\tldr\tx30, [sp, #0]")
	e("\tadd\tsp, sp, #" .. total)
	e("\tret")
	e(L .. "_exc:")
	e("\tb\t" .. L .. "_done")
	e(L .. "_fail:")
	e("\tbl\tfilc_check_function_call_fail")
	if sigDesc.retWords >= 1 then
		e(L .. "_retfail:")
		e("\tmov\tx0, x1")
		e("\tmov\tw1, #" .. retBytes)
		e("\tmov\tx2, xzr")
		e("\tbl\tfilc_cc_rets_check_failure")
	end
	e("\t.size\t" .. sym .. ", .-" .. sym)
	e("")
end

return glue
