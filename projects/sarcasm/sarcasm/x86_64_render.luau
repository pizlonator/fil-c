--!nonstrict
-- x86_64 rendering + frame policy for the shared emit loop (emit.luau). Renders IR nodes
-- (dest-first operand model) back to assembly text in the requested syntax ("att"/"intel"),
-- substituting register allocations, and synthesizes the Fil-C prologue/epilogue/frame.
-- Interface used by emit.emitFip: makeColorNum / usedCalleeSaved / computeLayout / header /
-- prologue / epilogue / rootStore / renderInsn / isSelfMove.

local x86 = require("./x86_64_isa")
local x86parse = require("./x86_64_parse")
local fp = require("./x86_64_fp")

local x86_64_render = {}

local SUFFIX = { [8] = "b", [16] = "w", [32] = "l", [64] = "q" }

-- mnemonics that take an AT&T size suffix when synthesized (bare)
local SUFFIXABLE = {
	mov=true, add=true, sub=true, ["and"]=true, ["or"]=true, xor=true, cmp=true, test=true,
	-- adc/sbb: same story as add/sub (their only width-less rendering is imm+mem),
	-- but they were missing until locked imm+mem RMWs exposed it: a lifted
	-- `lock adc QWORD PTR [rdi], 3` has NO register operand to imply a width, so
	-- the bare AT&T rendering (`lock adc $3, (%rbx)`) assembled as gas's silent
	-- l-default (32-bit `f0 83 /2 ib` = adcl) while the injected bounds check
	-- covered the requested 8 bytes — a silent width-changing miscompile.
	adc=true, sbb=true,
	shl=true, shr=true, sal=true, sar=true, imul=true, inc=true, dec=true, neg=true, ["not"]=true,
	-- the one-operand mul/div family: an Intel PTR-sized memory form (`div QWORD
	-- PTR [rdi]`) carries its width only in mem.size, and a bare AT&T rendering
	-- would assemble as the l-default — synthesize the suffix like cmp/test.
	div=true, idiv=true, mul=true,
}

-- xadd/cmpxchg ALWAYS render with an explicit size suffix: gas cannot infer the
-- width of `xadd (%rdi), %rsi` / `cmpxchg (%rdi), %rsi` (the memory operand
-- comes first) and rejects the bare form — including when the input was legal
-- Intel syntax (`xadd [rdi], rsi`). The width comes from the register operand
-- (always present: xadd/cmpxchg are r/m, r) or the memory size.
local FORCE_SUFFIX = { xadd = true, cmpxchg = true }

-- Bare (unsuffixed) x87 mnemonic + a sized memory operand -> the suffixed AT&T
-- mnemonic encoding that size. gas's default width for an unsuffixed x87 mnemonic
-- (fld -> flds) silently changes the access width of a PTR-sized Intel memory form
-- (`fld QWORD PTR [rdi]` must render fldl; rendered bare it assembles to a 4-byte
-- flds while being bounds-checked at 8 — a silent width-changing miscompile).
-- Combinations the ISA does not have (fst with a tbyte operand, fiadd with a qword,
-- ...) are rejected cleanly. Form matrix verified against `as`: the float family
-- has m32/m64 forms (plus m80 for fld/fstp only); the int family has m16/m32 forms
-- (plus m64 for fild/fistp/fisttp only).
local X87_MEM_FORM = {}
for _, m in { "fadd", "fsub", "fsubr", "fmul", "fdiv", "fdivr", "fcom", "fcomp" } do
	X87_MEM_FORM[m] = { [4] = m .. "s", [8] = m .. "l" }
end
for _, m in { "fiadd", "fisub", "fisubr", "fimul", "fidiv", "fidivr", "ficom", "ficomp" } do
	X87_MEM_FORM[m] = { [2] = m .. "s", [4] = m .. "l" }
end
X87_MEM_FORM.fld = { [4] = "flds", [8] = "fldl", [10] = "fldt" }
X87_MEM_FORM.fst = { [4] = "fsts", [8] = "fstl" }            -- fst has no tbyte form
X87_MEM_FORM.fstp = { [4] = "fstps", [8] = "fstpl", [10] = "fstpt" }
X87_MEM_FORM.fild = { [2] = "filds", [4] = "fildl", [8] = "fildq" }
X87_MEM_FORM.fist = { [2] = "fists", [4] = "fistl" }         -- fist has no qword form
X87_MEM_FORM.fistp = { [2] = "fistps", [4] = "fistpl", [8] = "fistpq" }
X87_MEM_FORM.fisttp = { [2] = "fisttps", [4] = "fisttpl", [8] = "fisttpq" }

-- The integer-source cvt family has the same gas-default-width trap: a bare AT&T
-- `cvtsi2ss (%rdi), %xmm0` assembles as m32 (cvtsi2ssl). When an Intel PTR size
-- selected the m32/m64 form, synthesize the suffix so the emitted encoding matches
-- the bounds-checked width (the fp table's cvtIntW width function is
-- annotation-driven). A PTR size that is not a real form (e.g. WORD PTR) is
-- rejected cleanly rather than under-checked.
local CVT_INT_MEM_FORM = {
	cvtsi2ss = { [4] = "cvtsi2ssl", [8] = "cvtsi2ssq" },
	cvtsi2sd = { [4] = "cvtsi2sdl", [8] = "cvtsi2sdq" },
	vcvtusi2ss = { [4] = "vcvtusi2ssl", [8] = "vcvtusi2ssq" },
	vcvtusi2sd = { [4] = "vcvtusi2sdl", [8] = "vcvtusi2sdq" },
}

-- Bare `crc32` with a sized (Intel PTR, or materialized frame slot) memory
-- source: gas would otherwise default the width from the destination register,
-- silently changing the crc source width (`crc32 eax, BYTE PTR [rdi]` must read
-- 1 byte, not 4). Synthesize the suffix that encodes the annotation.
local CRC32_MEM_FORM = { crc32 = { [1] = "crc32b", [2] = "crc32w", [4] = "crc32l", [8] = "crc32q" } }

local function regName(num, width) return x86parse.regName(num, width) end
x86_64_render.regName = regName

-- physical number for an operand (fixed, tempRef, or lifted opTemp).
local function physOf(node, op, colorNum)
	if not op or op.kind ~= "reg" then return nil end
	if op.fixedNum ~= nil then return op.fixedNum end
	if op.tempRef ~= nil then return colorNum and colorNum(op.tempRef) end
	if node.opTemp and node.opTemp[op] ~= nil then return colorNum and colorNum(node.opTemp[op]) end
	return nil
end

local function renderOperandRaw(node, op, colorNum, syntax)
	local function numOf(o)
		if o.fixedNum ~= nil then return o.fixedNum end
		if o.tempRef ~= nil then return colorNum and colorNum(o.tempRef) end
		if node.opTemp then local t = node.opTemp[o]; if t ~= nil then return colorNum and colorNum(t) end end
		return nil
	end
	-- name for a memory base/index register: the allocated physical register, or the
	-- original name when the operand was never modeled (e.g. a raw %rsp base passed
	-- through by a memory-neutral instruction).
	local function baseRegName(o)
		local n = numOf(o)
		if n ~= nil then return regName(n, 64) end
		return (o and o.name) or "?"
	end
	if op.kind == "reg" then
		if op.rc == "rip" then return syntax == "att" and "%rip" or "rip" end
		-- A high-byte operand (%ah/%ch/%dh/%bh) on a pinned instruction: it names
		-- bits 8-15 of the pinned physical register (emitPinned tags it), so the
		-- name renders verbatim — the colored low byte would be wrong.
		if op.highByte then
			return (syntax == "att" and "%" or "") .. op.highByte
		end
		local num = numOf(op)
		local name = (num ~= nil) and regName(num, op.width or 64) or op.name
		return syntax == "att" and ("%" .. name) or name
	elseif op.kind == "imm" then
		return syntax == "att" and ("$" .. op.text) or op.text
	elseif op.kind == "sym" then
		local t = op.text
		if op.reloc == "plt" then t = t .. "@PLT"
		elseif op.reloc == "gotpcrel" then t = t .. "@GOTPCREL" end
		return t
	elseif op.kind == "mem" then
		local ripBase = op.base and op.base.kind == "reg" and op.base.rc == "rip"
		local disp = ""
		if op.dispSym then
			disp = op.dispSym
			if op.dispReloc == "gotpcrel" then disp = disp .. "@GOTPCREL"
			elseif op.dispReloc == "plt" then disp = disp .. "@PLT" end
		end
		if syntax == "att" then
			local dtext = disp
			if op.dispVal and op.dispVal ~= 0 then
				dtext = (dtext ~= "" and dtext or "") .. (op.dispVal < 0 and tostring(op.dispVal) or ("+" .. op.dispVal))
				if disp == "" then dtext = tostring(op.dispVal) end
			end
			if ripBase then return dtext .. "(%rip)" end
			if op.index then
				local b = op.base and ("%" .. baseRegName(op.base)) or ""
				return dtext .. "(" .. b .. ",%" .. baseRegName(op.index) .. "," .. tostring(op.scale or 1) .. ")"
			end
			if not op.base then
				-- A displacement-only memory operand (no base, no index): classify
				-- rejects it as an absolute address, but the renderer must never
				-- crash on one regardless (it renders as the bare displacement).
				return dtext
			end
			return dtext .. "(%" .. baseRegName(op.base) .. ")"
		else
			if ripBase then return "[rip + " .. disp .. "]" end
			local parts = {}
			if op.base then table.insert(parts, baseRegName(op.base)) end
			if op.index then table.insert(parts, baseRegName(op.index) .. "*" .. tostring(op.scale or 1)) end
			if disp ~= "" then table.insert(parts, disp) end
			if op.dispVal and op.dispVal ~= 0 then
				table.insert(parts, (op.dispVal < 0 and tostring(op.dispVal)) or ("+" .. tostring(op.dispVal)))
			end
			-- (A displacement-only operand with no base and no index is rejected by
			-- classify as an absolute address; the renderer still never crashes on it.)
			local body = parts[1] or ""
			for k = 2, #parts do
				local p = parts[k]
				if string.sub(p, 1, 1) == "+" or string.sub(p, 1, 1) == "-" then body = body .. " " .. string.sub(p,1,1) .. " " .. string.sub(p,2)
				else body = body .. " + " .. p end
			end
			return "[" .. body .. "]"
		end
	end
	return "?"
end

-- AVX512 decorators ({%k1}{z}, {1to16}, ...) ride along on the operand and are
-- re-rendered verbatim after the register/memory text.
local function renderOperand(node, op, colorNum, syntax)
	local s = renderOperandRaw(node, op, colorNum, syntax)
	if op.deco then s = s .. op.deco end
	return s
end
x86_64_render.renderOperand = renderOperand

-- operation size (bytes) for a synthesized instruction (widest reg, or mem.size).
-- Lifted nodes carry their operands in node.stmt.operands (only synthesized nodes
-- have node.operands), so look at both — this is what sizes the AT&T suffix of a
-- reg-less lifted form (e.g. a materialized `movl $5, O(%rsp)` frame-slot store).
local function opSize(node, colorNum)
	local w = nil
	for _, op in ((node.stmt and node.stmt.operands) or node.operands or {}) do
		if op.kind == "reg" and op.rc ~= "rip" then w = math.max(w or 0, op.width or 64)
		elseif op.kind == "mem" and op.size then w = math.max(w or 0, op.size * 8) end
	end
	return w or 64
end

local INTEL_PTR = { [1]="BYTE PTR", [2]="WORD PTR", [4]="DWORD PTR", [8]="QWORD PTR" }

-- rel8-reachable trampoline for the rel8-ONLY implicit-counter branches
-- (`loop`/`loopq`/`loopl` decrement-and-branch and `jrcxz`/`jecxz` branch-on-counter-
-- zero re-emit verbatim and have NO rel32/abs encoding). Sarcasm's instrumentation —
-- the pollcheck at a back-edge label, the per-access bounds checks — routinely pushes
-- the branch target beyond +-127 bytes, and gas then rejects the emitted file with an
-- opaque "value of ... too large for field of 1 byte" error from the temp .yolo.s.
-- Sarcasm cannot predict the post-instrumentation displacement (the instrumentation is
-- injected between the branch and its target by earlier stages), so EVERY such branch
-- is rewritten at render time, unconditionally, to jump through a nearby trampoline:
--
--	loop	.Lsarctramp_<fn>_<n>t    # the original instruction, unchanged semantics
--	jmp	.Lsarctramp_<fn>_<n>s    # skip past the stub
--.Lsarctramp_<fn>_<n>t:
--	jmp	<original target>        # rel32 — always reachable
--.Lsarctramp_<fn>_<n>s:
--
-- The trampoline (stub) label sits 2 bytes ahead of the branch — always within rel8
-- reach — the skip jmp lands past the stub, and the stub's `jmp T` can be rel32. NO
-- flag-affecting instruction is added: jmp preserves all flags and loop/jrcxz/jecxz
-- do not read flags, so both the taken and the fall-through path see the same flags as
-- before (a dec/jne rewrite is REJECTED as an approach: it would change the flags the
-- fall-through path observes). Fall-through semantics are unchanged, and the IR-level
-- instruction plus its original target stay exactly as they are — validateBody,
-- reachability, liveness, the counter web's rcx use/def/RMW + fixedReg precolor and
-- the pollcheck-at-back-edge machinery (which lives at the TARGET label, saving and
-- restoring physical rcx around filc_pollcheck_slow) all still see the original
-- branch; only the final emitted text changes. <n> is a per-function statement-order
-- counter (deterministic: renderInsn runs once per FIP node, in emission order), the
-- function name keeps sites of different functions apart, and the counter is bumped
-- past any name a user label of this function already uses.
local trampFn = nil      -- current function's plain name (set by header)
local trampCounter = 0   -- per-function statement-order trampoline counter
local trampUsed = nil    -- label names already defined in the current function

-- ---------------------------------------------------------------------------
-- .byte emission for instructions the SYSTEM ASSEMBLER may not know.
--
-- sarcasm emits the temp .yolo.s to whatever `as` is installed; baselines as old
-- as binutils 2.38 (Ubuntu 22.04) do not know SHA512-NI (vsha512*) or SM3/SM4
-- (vsm3*/vsm4*), both of which OpenSSL perlasm uses (the sha512 SHA512-NI body
-- reaches sarcasm as .byte encodings; sm3/sm4 arrive spelled). These mnemonics
-- are therefore emitted as their VEX 3-byte encodings in a `.byte` directive —
-- byte-identical semantics on every assembler, and deterministic output (the
-- registers are all fixed at render time: vector registers are never
-- reallocated, and a memory source's base/index resolve through opTemp like
-- any rendered operand). The spelled instruction rides along as a comment for
-- readability.
--
-- Encodings (SDM / binutils i386-opc.tbl):
--   vsha512rnds2 ymm1, ymm2, xmm3/m128:  VEX.256.F2.0F38.W0 CB /r   (AT&T: src2, src1, dst)
--   vsha512msg1  ymm1, ymm2, xmm3/m128:  VEX.256.F2.0F38.W0 CC /r
--   vsha512msg2  ymm1, ymm2, xmm3/m128:  VEX.256.F2.0F38.W0 CD /r
--   vsm3msg1     xmm1, xmm2, xmm3/m128:  VEX.128.66.0F38.W0 DA /r
--   vsm3msg2     xmm1, xmm2, xmm3/m128:  VEX.128.66.0F38.W0 DB /r
--   vsm3rnds2    xmm1, xmm2, xmm3/m128, imm8: VEX.128.66.0F3A.W0 DE /r ib (AT&T: imm, src2, src1, dst)
--   vsm4key4     xmm1, xmm2, xmm3/m128:  VEX.128.F3.0F38.W0 DA /r
--   vsm4rnds4    xmm1, xmm2, xmm3/m128:  VEX.128.F3.0F38.W0 DB /r
-- Per-mnemonic encoding shape (verified against binutils 2.42's i386-opc.tbl
-- and disassembler). `twoOp`: the vvvv field is unused (encoded as ~0) and the
-- operand list is { dst, src2 }; otherwise it is { dst, src1, src2 } with
-- vvvv = src1. `src2Class`: the register class of the rm source (vsha512msg2's
-- source is a full ymm/m256; every other rm source here is xmm/m128 — sm4 ymm
-- forms take ymm/m256). `classes`: the allowed destination classes (SM4 has
-- both VEX.128 and VEX.256 forms; L follows the destination class).
local BYTE_EMIT = {
	vsha512rnds2 = { map = 2, pp = 3, L = 1, op = 0xCB, src2Class = "xmm",
		classes = { ymm = true } },
	vsha512msg1 = { map = 2, pp = 3, L = 1, op = 0xCC, twoOp = true, src2Class = "xmm",
		classes = { ymm = true } },
	vsha512msg2 = { map = 2, pp = 3, L = 1, op = 0xCD, twoOp = true, src2Class = "ymm",
		classes = { ymm = true } },
	vsm3msg1 = { map = 2, pp = 0, L = 0, op = 0xDA, src2Class = "xmm",
		classes = { xmm = true } },
	vsm3msg2 = { map = 2, pp = 1, L = 0, op = 0xDA, src2Class = "xmm",
		classes = { xmm = true } },
	vsm3rnds2 = { map = 3, pp = 1, L = 0, op = 0xDE, imm = true, src2Class = "xmm",
		classes = { xmm = true } },
	vsm4key4 = { map = 2, pp = 2, L = 0, op = 0xDA, src2Class = "xmm",
		classes = { xmm = true, ymm = true } },
	vsm4rnds4 = { map = 2, pp = 3, L = 0, op = 0xDA, src2Class = "xmm",
		classes = { xmm = true, ymm = true } },
}

-- Resolve a (possibly temp-carrying) register operand to its physical number.
local function byteEmitNumOf(node, op, colorNum)
	if op.fixedNum ~= nil then return op.fixedNum end
	if op.tempRef ~= nil then return colorNum and colorNum(op.tempRef) end
	if node.opTemp then local t = node.opTemp[op]; if t ~= nil then return colorNum and colorNum(t) end end
	return nil
end

-- Encode a memory operand as ModRM [+SIB] [+disp] bytes appended to `out`
-- (regField is the ModRM.reg value 0-7). Returns false when the shape is not
-- encodable here (a missing base register number).
local function byteEmitMem(out, node, mem, regField, colorNum, raw)
	local base = mem.base and byteEmitNumOf(node, mem.base, colorNum) or nil
	local index = (mem.index and mem.index.kind == "reg")
		and byteEmitNumOf(node, mem.index, colorNum) or nil
	if not base then
		error("sarcasm: internal error: cannot resolve the memory base register of '"
			.. (raw or "?") .. "' for .byte emission")
	end
	local scale = mem.scale or 1
	local disp = mem.dispVal or 0
	local baseLow = base % 8
	local needSib = index ~= nil or baseLow == 4
	-- mod: 0 (disp0, baseLow ~= 5), 1 (disp8), 2 (disp32)
	local mod
	if disp == 0 and baseLow ~= 5 then mod = 0
	elseif disp >= -128 and disp <= 127 then mod = 1
	else mod = 2 end
	local rm = needSib and 4 or baseLow
	table.insert(out, mod * 64 + (regField % 8) * 8 + rm)
	if needSib then
		local scaleBits = (scale == 1 and 0) or (scale == 2 and 1) or (scale == 4 and 2) or 3
		local indexField = index and (index % 8) or 4
		table.insert(out, scaleBits * 64 + indexField * 8 + baseLow)
	end
	if mod == 1 then
		table.insert(out, disp % 256)
	elseif mod == 2 then
		local d = disp % 4294967296
		table.insert(out, d % 256)
		table.insert(out, math.floor(d / 256) % 256)
		table.insert(out, math.floor(d / 65536) % 256)
		table.insert(out, math.floor(d / 16777216) % 256)
	end
	return true
end

-- Emit a BYTE_EMIT instruction node as `.byte ...` (with the spelled form as a
-- comment), or nil when it does not apply.
local function byteEmitInsn(node, colorNum, syntax, low)
	local spec = BYTE_EMIT[low]
	if not spec then return nil end
	local ops = node.stmt and node.stmt.operands or node.operands or {}
	local raw = node.stmt and node.stmt.raw or low
	-- dest-first operand shape: three-operand { dst, src1, src2[, imm] } or
	-- two-operand { dst, src2 } (vsha512msg1/msg2); src2 may be memory.
	local dst, src1, src2
	if spec.twoOp then
		dst, src2 = ops[1], ops[2]
	else
		dst, src1, src2 = ops[1], ops[2], ops[3]
	end
	if not (dst and dst.kind == "reg" and src2) or (not spec.twoOp and not src1) then
		error("sarcasm: '" .. low .. "' requires register destination/source operands: "
			.. raw)
	end
	local dstClass = string.sub(dst.name or "", 1, 3)
	if not spec.classes[dstClass] or dst.num > 15 then
		local allowed = {}
		for c in spec.classes do table.insert(allowed, c) end
		table.sort(allowed)
		error("sarcasm: '" .. low .. "' requires a " .. table.concat(allowed, "/")
			.. "0-15 destination register (a wider vector class is not encodable): " .. raw)
	end
	-- VEX.L follows the destination class (SM4 has both forms; every other
	-- family here is single-class, so this matches spec.L for them).
	local vexL = (dstClass == "ymm") and 1 or 0
	if not spec.twoOp
		and (string.sub(src1.name or "", 1, 3) ~= dstClass or src1.num > 15) then
		error("sarcasm: '" .. low .. "' requires a " .. dstClass .. "0-15 source"
			.. " register (its VEX encoding cannot name another class): " .. raw)
	end
	-- the rm source class: ymm/m256 for the sm4 ymm forms and vsha512msg2,
	-- xmm/m128 otherwise.
	local src2Class = (dstClass == "ymm" and (low == "vsm4key4" or low == "vsm4rnds4"))
		and "ymm" or spec.src2Class
	local bytes = { 0xC4 }
	local regExt = (dst.num >= 8) and 0 or 128        -- R̅: clear when extended
	local bExt = 32                                    -- B̅: set when rm < 8
	local xExt = 64                                    -- X̅: set when index < 8/none
	if src2.kind == "reg" then
		if string.sub(src2.name or "", 1, 3) ~= src2Class or src2.num > 15 then
			error("sarcasm: '" .. low .. "' requires a " .. src2Class .. "0-15"
				.. " source register (its VEX encoding cannot name another class): " .. raw)
		end
		if src2.num >= 8 then bExt = 0 end
	else
		local bn = src2.base and byteEmitNumOf(node, src2.base, colorNum) or 0
		local xn = (src2.index and src2.index.kind == "reg")
			and byteEmitNumOf(node, src2.index, colorNum) or nil
		if bn and bn >= 8 then bExt = 0 end
		if xn and xn >= 8 then xExt = 0 end
	end
	table.insert(bytes, regExt + xExt + bExt + spec.map)
	-- vvvv: the second source for the three-operand forms, ~0 (unused) for the
	-- two-operand forms.
	local vvvvField = spec.twoOp and 15 or (15 - src1.num)
	table.insert(bytes, vvvvField * 8 + vexL * 4 + spec.pp)
	table.insert(bytes, spec.op)
	if src2.kind == "reg" then
		table.insert(bytes, 0xC0 + (dst.num % 8) * 8 + (src2.num % 8))
	else
		byteEmitMem(bytes, node, src2, dst.num % 8, colorNum, raw)
	end
	if spec.imm then
		local immOp = ops[4]
		local v = immOp and immOp.kind == "imm" and (immOp.value or tonumber(immOp.text)) or nil
		if not v then
			error("sarcasm: '" .. low .. "' requires an immediate operand: " .. raw)
		end
		table.insert(bytes, v % 256)
	end
	local text = "\t.byte " .. table.concat(bytes, ",")
	-- readability: the spelled instruction as a comment (gas-style: # comment)
	local spelled = {}
	for _, o in ops do table.insert(spelled, renderOperand(node, o, colorNum, syntax)) end
	if syntax == "att" and #spelled >= 2 then
		local rev = {}
		for k = #spelled, 1, -1 do table.insert(rev, spelled[k]) end
		spelled = rev
	end
	return text .. "\t# " .. low .. " " .. table.concat(spelled, ",")
end

local function trampNames()
	while true do
		local t = ".Lsarctramp_" .. (trampFn or "?") .. "_" .. trampCounter .. "t"
		local s = ".Lsarctramp_" .. (trampFn or "?") .. "_" .. trampCounter .. "s"
		trampCounter += 1
		if not (trampUsed[t] or trampUsed[s]) then return t, s end
	end
end

-- Is this an implicit-counter branch that must go through the trampoline? Only the
-- supported forms reach the renderer (loopb/loopw and the ZF-consuming loop variants
-- and jcxz are rejected at classify), but the shape is checked defensively: a label
-- operand is required (a loop/jcxz with no label target never gets here — validateBody
-- rejects it first).
local function isCounterBranch(low)
	return string.match(low, "^loop[bwlq]?$") ~= nil or low == "jrcxz" or low == "jecxz"
end

function x86_64_render.renderInsn(node, colorNum, syntax, rc)
	local mnem = node.stmt and node.stmt.mnemonic or node.mnemonic
	local ops = node.stmt and node.stmt.operands or node.operands or {}
	local low = string.lower(mnem)

	-- Instructions the system assembler may not know (SHA512-NI, SM3/SM4):
	-- emitted as their VEX encodings in a .byte directive (see BYTE_EMIT).
	do
		local be = byteEmitInsn(node, colorNum, syntax, low)
		if be then return be end
	end

	-- Displacement fixups for two synthesized node kinds whose %rsp-relative
	-- offsets are only known once the frame layout is (they cannot be baked into
	-- the operand at transform time):
	--   * inArgWord — an INCOMING fast-CC stack-argument load (cg.loadInArg): the
	--     word sits at [entry_rsp + 8 + 8*w] (above the return address), i.e. at
	--     [rsp + 8*#calleeSaved + layout.total + 8 + 8*w] at the body frame base
	--     in the STATIC (alignA <= 16) case. Under DYNAMIC alignment (alignA >
	--     16) the body rsp is the post-`and` rsp (pre-and rsp minus 0..A-1 slack,
	--     entry-dependent), so no static rsp-relative displacement can address
	--     the incoming words: the load is emitted as a two-move sequence through
	--     its own destination register
	--       mov saveOff(%rsp), dst ; mov postPushDelta(dst), dst
	--     where saveOff(%rsp) holds the prologue-saved post-push rsp (captured
	--     into r11 before the `sub`, i.e. entry_rsp - 8*#calleeSaved) and
	--     postPushDelta is the word's offset from THAT base: 8*#calleeSaved +
	--     8 + 8*w (above the return address). x86_64-only (arm64 has no
	--     dynamic frame alignment).
	--   * spillShift — a spill reload/store regalloc inserted inside an outgoing
	--     stack-arg window (cg.beginStackArgs/endStackArgs), where rsp was dropped
	--     by N: re-base the slot by the window's shift so it addresses the same
	--     frame slot (see sarcasm.luau's rewriteSpills).
	-- Dynamic-align incoming-argument loads cannot use a static displacement.
	local layoutA = (rc and rc.layout and rc.layout.alignA) or 16
	if node.inArgWord and rc and rc.layout and layoutA > 16 then
		local saveOff = rc.layout.saveOff
		assert(saveOff ~= nil,
			"sarcasm: internal error: dynamic frame without saveOff for inArgWord")
		-- The prologue saves the POST-PUSH rsp (entry_rsp - 8*#calleeSaved,
		-- captured before the `sub`), so the word's offset from the saved base
		-- is 8*#calleeSaved + 8 + 8*w -- notably WITHOUT layout.total (the
		-- static displacement's total accounts for the `sub`, which the saved
		-- base predates).
		local postPushDelta = 8 * #rc.calleeSaved + 8 + 8 * node.inArgWord
		-- loadInArg builds its mem with dispVal 0, but honor any base dispVal.
		-- NO-INDEX ASSUMPTION: loadInArg builds build.memx(build.rp(4)) with no
		-- index operand, and nothing between cg.loadInArg and renderInsn adds
		-- one — the second move addresses (base + postPushDelta) with no scale.
		-- Reject (a clean `sarcasm:` compile failure, not an internal-error
		-- assert) rather than silently ignoring an index (which would address
		-- the wrong word): user assembly never builds inArgWord nodes directly,
		-- so an index here means a compiler-side provenance change, and failing
		-- closed with the input blamed keeps the failure actionable.
		for _, o in ops do
			if o.kind == "mem" and o.base and o.base.fixedNum == 4 then
				if o.index ~= nil then
					error("sarcasm: inArgWord memory operand has an index register"
						.. " (entry stack-argument loads address base + word"
						.. " with no scale)")
				end
				postPushDelta += (o.dispVal or 0)
				break
			end
		end
		-- inArgWord nodes are emitted only by the entry-CC unpack (transform.luau,
		-- before any call's beginStackArgs window opens), and rewriteSpills
		-- (sarcasm.luau) stamps spillShift only on spill reload/store nodes it
		-- creates INSIDE such a window — never on a pre-existing entry mov —
		-- and the FIP packer propagates spillShift only across spillReload/
		-- spillStore rebuilds. So spillShift can never co-occur with inArgWord
		-- by construction, and no user input can produce the combination (entry
		-- unpack sites and window interiors are disjoint node provenances).
		-- Coexistence (not co-occurrence) of the two features in one function
		-- IS exercised: filc/tests/sarcasm-frame-and64-stackargs-spill-att runs
		-- incoming stack-argument loads (inArgWord, dynamic-align two-move form)
		-- alongside an outgoing stack-argument call whose marshalling spills
		-- inside the window (spillShift re-basing) — proving the loud reject
		-- below has no false positives on real spill traffic. Reject loudly
		-- (a clean `sarcasm:` compile failure, not a crash) rather than
		-- silently mis-rendering, so any future provenance change fails closed.
		if node.spillShift ~= nil then
			error("sarcasm: inArgWord inside outgoing stack-arg window"
				.. " (entry stack-argument load colliding with a call's"
				.. " stack-argument area)")
		end
		local dstOp = ops[1]
		assert(dstOp and dstOp.kind == "reg",
			"sarcasm: internal error: inArgWord node without reg destination")
		local dstNum = physOf(node, dstOp, colorNum)
		assert(dstNum ~= nil,
			"sarcasm: internal error: inArgWord destination has no color")
		-- Width-aware emission. The FIRST load moves the full 64-bit saved rsp
		-- base (a truncated base would address garbage), so it is always a
		-- 64-bit move into the full destination register; the SECOND load (the
		-- actual argument word) matches the destination width. DSTW AUDIT
		-- (2026-09): cg.loadInArg builds its destination with build.rt(dst) —
		-- no width argument, so build.luau stamps width 64 — and all three
		-- callers (transform.luau's entry-CC unpack x2, the yolo-base fill)
		-- pass a bare temp id, never a narrowed operand. Stack words are 8
		-- bytes each (marshalFastArgs packs at 8*j), so only the 64-bit form
		-- is reachable — the former 32-bit branch was dead code and is
		-- removed. A width-mismatched `movq ..., %eax` (or an `eax` with
		-- QWORD PTR) must never be emitted, so any non-64 width still fails
		-- loudly.
		local dstW = dstOp.width or 64
		if dstW ~= 64 then
			error("sarcasm: inArgWord destination width "
				.. tostring(dstW) .. " (entry stack-argument loads are 64-bit)")
		end
		local dstStr = renderOperand(node, dstOp, colorNum, syntax)
		local dstBase = regName(dstNum, 64)
		local dstFull = (syntax == "att" and "%" or "") .. dstBase
		if syntax == "att" then
			local l1 = "\tmovq\t" .. tostring(saveOff) .. "(%rsp), " .. dstFull
			local l2 = "\tmovq\t" .. tostring(postPushDelta) .. "(%" .. dstBase .. "), " .. dstStr
			return l1 .. "\n" .. l2
		else
			local s1 = saveOff == 0 and "[rsp]"
				or "[rsp + " .. tostring(saveOff) .. "]"
			local l1 = "\tmov\t" .. dstFull .. ", QWORD PTR " .. s1
			local l2 = "\tmov\t" .. dstStr .. ", QWORD PTR [" .. dstBase
				.. " + " .. tostring(postPushDelta) .. "]"
			return l1 .. "\n" .. l2
		end
	end
	local dispDelta = 0
	if node.inArgWord and rc and rc.layout then
		dispDelta = rc.layout.total + 8 * #rc.calleeSaved + 8 + 8 * node.inArgWord
	end
	if node.spillShift then dispDelta += node.spillShift end
	if dispDelta ~= 0 then
		local adjusted = {}
		for _, o in ops do
			if o.kind == "mem" and o.base and o.base.fixedNum == 4 then
				local c = {}
				for k, v in o do c[k] = v end
				c.dispVal = (c.dispVal or 0) + dispDelta
				table.insert(adjusted, c)
			else
				table.insert(adjusted, o)
			end
		end
		ops = adjusted
		-- renderOperandRaw looks up temps through node.opTemp keyed by operand
		-- identity; the cloned operand carries the same tempRef field, so only a
		-- node.opTemp-mapped operand would lose its temp — clone carefully: the
		-- memory operands these node kinds carry never use opTemp (their base is
		-- the fixed rsp), so this is safe.
	end

	-- Bare multi-byte padding NOPs do not assemble (gas rejects the bare mnemonic
	-- with "invalid instruction suffix for `nop'"), and the operand-ful padding
	-- forms (nopw 0x0(%rax,%rax,1)) are zero-effect encodings that render verbatim.
	-- Normalize the bare forms to plain `nop` so the emitted file always assembles.
	if (low == "nopw" or low == "nopl") and #ops == 0 then
		return "\tnop"
	end

	-- The rel8 trampoline for loop/jrcxz/jecxz (see the long comment above). Emitted
	-- as one renderInsn result: emitFip joins the output lines with newlines, so the
	-- five-line form lands verbatim in the assembly.
	if trampUsed and node.stmt and isCounterBranch(low)
		and ops[1] and ops[1].kind == "sym" then
		local tgt = renderOperand(node, ops[1], colorNum, syntax)
		local t, s = trampNames()
		return "\t" .. low .. "\t" .. t .. "\n"
			.. "\tjmp\t" .. s .. "\n"
			.. t .. ":\n"
			.. "\tjmp\t" .. tgt .. "\n"
			.. s .. ":"
	end

	-- A validated `rep`/`repe`/`repz` prefix (parser tag; x86_64_isa.classify
	-- allows it only on the modeled string ops plus `rep ret`/`rep nop`)
	-- re-emits ahead of the mnemonic, normalized to `rep` (the flavors are
	-- semantically identical where they are accepted).
	local repPfx = ((node.stmt and node.stmt.repPrefix) or node.repPrefix) and "rep " or ""

	-- A validated `lock` prefix (parser tag; x86_64_isa.classify allows it only
	-- on exactly-modeled memory-destination RMWs) re-emits ahead of the mnemonic.
	local lockPfx = ((node.stmt and node.stmt.lockPrefix) or node.lockPrefix) and "lock " or ""

	-- control-only forms: an indirect call/jump renders its target operand with
	-- the AT&T `*`. The marker comes from a synthesized node's `indirect` flag
	-- (the transform's indirect-call sequence), from the PARSED operand's own
	-- `*` (x86_64_parse records it in op.indirect), or implicitly: a call/jmp
	-- whose operand is a register or memory operand is ALWAYS indirect (x86 has
	-- no direct register/memory call or jmp — direct forms take a symbol), which
	-- also covers Intel-syntax input (`jmp rax` has no `*` to record). Losing
	-- the marker renders `jmp %rax`, which clang's integrated assembler rejects
	-- and gas only warns about.
	local ctlBase = x86.baseMnemonic(mnem)
	local op1 = ops[1]
	local indir = node.indirect or (op1 and op1.indirect)
		or (op1 and (op1.kind == "reg" or op1.kind == "mem"))
	if indir and (ctlBase == "call" or ctlBase == "jmp") then
		local tgt = renderOperand(node, op1, colorNum, syntax)
		return "\t" .. low .. "\t" .. (syntax == "att" and ("*" .. tgt) or tgt)
	end

	-- AT&T size suffix. movzx/movsx need an explicit source+dest size pair (movzbl, ...)
	-- since bare `movzx` with a memory source is ambiguous in AT&T; derive it from the
	-- source size (AT&T suffix letter or BYTE/WORD PTR) and the destination register width.
	local outMnem = mnem
	local base = x86.baseMnemonic(mnem)
	if syntax == "att" then
		local sizedForm = X87_MEM_FORM[low] or CVT_INT_MEM_FORM[low] or CRC32_MEM_FORM[low]
		if sizedForm then
			-- Bare x87 / cvt-int mnemonic with a sized (Intel PTR, or materialized
			-- frame slot) memory operand: emit the suffix encoding that size (see
			-- the table comments). Already-suffixed input (fldl, cvtsi2ssq) is not
			-- table-keyed and stays untouched.
			local memsz = nil
			for _, op in ops do
				if op.kind == "mem" and op.size then memsz = op.size break end
			end
			if memsz then
				local f = sizedForm[memsz]
				if not f then
					error("sarcasm: " .. low .. " has no " .. tostring(memsz)
						.. "-byte memory form: " .. (node.stmt and node.stmt.raw or mnem))
				end
				outMnem = f
			end
		elseif FORCE_SUFFIX[low] then
			-- xadd/cmpxchg: gas cannot infer the width of a bare mnemonic (see the
			-- table comment) — always render an explicit size suffix.
			local w = 0
			for _, op in ops do
				if op.kind == "reg" and op.rc ~= "rip" then w = math.max(w, op.width or 64)
				elseif op.kind == "mem" and op.size then w = math.max(w, op.size * 8) end
			end
			outMnem = low .. (SUFFIX[w > 0 and w or 64] or "q")
		elseif (base == "movzx" or base == "movsx") and string.match(low, "^mov[zs][bwlq][bwlq]$") then
			outMnem = low  -- already a full AT&T form (movzbl, movslq, ...): render verbatim
		elseif base == "movzx" or base == "movsx" then
			local letter = string.match(low, "^mov[zs]([bw])")
			local srcBytes = letter and ({ b = 1, w = 2 })[letter] or nil
			if not srcBytes then for _, op in ops do if op.kind == "mem" and op.size then srcBytes = op.size end end end
			if not srcBytes and ops[2] and ops[2].kind == "reg" and ops[2].rc ~= "rip" then
				srcBytes = math.floor((ops[2].width or 16) / 8)
			end
			srcBytes = srcBytes or 1
			local dstW = 32
			for _, op in ops do if op.kind == "reg" and op.rc ~= "rip" then dstW = op.width or 64; break end end
			outMnem = "mov" .. (base == "movzx" and "z" or "s") .. SUFFIX[srcBytes * 8] .. SUFFIX[dstW]
		elseif node.synth then
			if low == "lea" then outMnem = "leaq"
			elseif SUFFIXABLE[low] then
				outMnem = low .. (SUFFIX[opSize(node, colorNum)] or "q")
			end
		elseif SUFFIXABLE[low] then
			-- Lifted node with a bare mnemonic (Intel input) and no register operand to imply
			-- the operand width (e.g. `cmp $0, 8(%rdx)`): AT&T needs an explicit size suffix.
			local hasReg = false
			for _, op in ops do if op.kind == "reg" and op.rc ~= "rip" then hasReg = true; break end end
			if not hasReg then outMnem = low .. (SUFFIX[opSize(node, colorNum)] or "q") end
		end
		-- Bare narrowing convert with a sized memory operand: gas needs the
		-- source-width suffix (vcvtpd2psx/y, vcvtpd2phz, ...) — the bare rendering
		-- is ambiguous and fails assembly.
		if outMnem == mnem then
			local memsz = nil
			for _, op in ops do
				if op.kind == "mem" and op.size then memsz = op.size break end
			end
			local ns = memsz and fp.narrowSuffix(low, memsz)
			if ns then outMnem = low .. ns end
		end
	end

	-- determine a memory size for intel PTR prefixes (imm+mem ambiguity)
	local memSize = nil
	do
		local w = 0
		local hasMem = false
		for _, op in ops do
			if op.kind == "reg" and op.rc ~= "rip" then w = math.max(w, op.width or 64)
			elseif op.kind == "mem" then hasMem = true; if op.size then w = math.max(w, op.size*8) end end
		end
		if hasMem and syntax == "intel" then memSize = math.floor((w > 0 and w or 64) / 8) end
	end

	local rendered = {}
	for _, op in ops do
		local s = renderOperand(node, op, colorNum, syntax)
		if op.kind == "mem" and syntax == "intel" and memSize and INTEL_PTR[memSize] then
			s = INTEL_PTR[memSize] .. " " .. s
		end
		table.insert(rendered, s)
	end
	if #rendered == 0 then
		-- Zero-operand string ops render in the OUTPUT syntax (x86 output is
		-- always AT&T): the Intel dword spellings `movsd`/`stosd` become the
		-- AT&T `movsl`/`stosl` (a rendered `stosd` would die in `as` — it is
		-- not an AT&T form at all — and `movsd` is canonically `movsl`
		-- there). An `movsd` WITH operands is the FP scalar-double move and
		-- never reaches this branch.
		if syntax == "att" then
			if low == "stosd" then outMnem = "stosl"
			elseif low == "movsd" then outMnem = "movsl" end
		end
		return "\t" .. repPfx .. lockPfx .. outMnem
	end
	-- Operands are stored destination-first (Intel order). AT&T lists them source-first.
	if syntax == "att" and #rendered >= 2 then
		local rev = {}
		for k = #rendered, 1, -1 do table.insert(rev, rendered[k]) end
		rendered = rev
	end
	return "\t" .. repPfx .. lockPfx .. outMnem .. "\t" .. table.concat(rendered, ", ")
end

function x86_64_render.isSelfMove(node, colorNum)
	local mnem = node.stmt and node.stmt.mnemonic or node.mnemonic
	if not mnem then return false end
	local low = string.lower(mnem)
	if low ~= "mov" and low ~= "movq" then return false end
	-- a 32-bit `movl %eax, %eax` is NOT a no-op — it zero-extends into %rax. Only a full
	-- 64-bit reg-reg move to the same register is a true no-op we may drop.
	local ops = node.stmt and node.stmt.operands or node.operands
	if not ops or #ops ~= 2 then return false end
	if ops[1].kind ~= "reg" or ops[2].kind ~= "reg" then return false end
	if (ops[1].width or 64) ~= 64 or (ops[2].width or 64) ~= 64 then return false end
	local a = physOf(node, ops[1], colorNum)
	local b = physOf(node, ops[2], colorNum)
	return a ~= nil and a == b
end

-- Frame layout -----------------------------------------------------------
-- [0,16) prev+origin; [16, 16+8*numRoots) the GC root slots — this placement is an
-- ABI requirement: the Fil-C GC reads a frame's roots as frame->lowers[index], i.e.
-- immediately after the parent+origin header (filc_runtime.h's filc_frame). Then the
-- reserved vector-save area and the materialized FP/SIMD frame-slot clusters (whose
-- analyzeFrame-assigned offsets are relative to base 16 and get shifted past the root
-- area by transform once numRoots is known), then the materialized incoming yolo
-- stack-argument area (x86_64 only — yoloBytes; fed by the entry unpack, read by
-- FP/vector argument reads), then regalloc spill slots. The FP/SIMD+yolo region
-- starts on a 16-byte boundary (8 bytes of padding after the root slots when
-- numRoots is odd) so the clusters' and the yolo area's mod-16 alignment residues
-- (movaps-class accesses) survive the shift. The body rsp is 16-aligned by
-- default. When alignReq > 16 (a materialized cluster needs vmovdqa-class
-- alignment, proved aligned by the taint scan), the prologue dynamically aligns
-- the frame (sub total, and $-A) and saves the pre-alignment rsp at saveOff
-- for the epilogue to restore; static sizing cannot align since the entry
-- rsp's mod-A residue is dynamic (only mod-16 is fixed by the ABI).
local function computeLayout(calleeSaved, numRoots, spillBytes, fpSaveBytes, fpBytes, yoloBytes, alignReq)
	spillBytes = spillBytes or 0
	fpSaveBytes = fpSaveBytes or 0
	fpBytes = fpBytes or 0
	yoloBytes = yoloBytes or 0
	local A = alignReq or 16
	if A < 16 then A = 16 end
	local p = #calleeSaved
	local rootBase = 16
	local fpPadBase = ((fpSaveBytes + fpBytes + yoloBytes) > 0 and numRoots % 2 == 1) and 8 or 0
	-- Widen the pad so the root-area shift (8*numRoots + fpPad, mirrored by the
	-- transform's frameSlot shift) is a multiple of the frame alignment: the
	-- materialized clusters' alignment residues survive it. At the default
	-- 16-byte alignment the historical pad already qualifies, so outputs are
	-- unchanged there.
	local fpPad = fpPadBase
	if A > 16 then
		while ((8 * numRoots + fpPad) % A) ~= 0 do fpPad += 8 end
	end
	local spillBase = 16 + 8 * numRoots + fpPad + fpSaveBytes + fpBytes + yoloBytes
	local base = spillBase + spillBytes  -- prev+origin+roots(+pad+save+fp+yolo+spills)
	local N, total, saveOff = base, base, nil
	if A > 16 then
		-- Dynamic alignment: the content (base bytes) sits at the aligned rsp,
		-- the pre-alignment rsp is saved 8 bytes into the cluster area (base0
		-- = 16 + saveBytes, shifted past the roots by 8*numRoots + fpPad --
		-- the frameSlot shift, a multiple of A by the widened pad above), and
		-- A slack bytes cover the rounding.
		N = base
		total = base + A
		saveOff = 16 + fpSaveBytes + 8 * numRoots + fpPad
	else
		-- Maintain 16-byte alignment at call sites: (8 retaddr + 8*p pushes + N) % 16 == 0.
		local target = ((p % 2) == 0) and 8 or 0
		N = base
		while (N % 16) ~= target do N += 1 end
		total = N
	end
	return { total = total, alignA = A, saveOff = saveOff,
		rootBase = rootBase, spillBase = spillBase, pushed = calleeSaved }
end
x86_64_render.computeLayout = computeLayout

-- x86 color name -> physical number (via classifyReg).
function x86_64_render.makeColorNum(color)
	return function(t)
		local c = color[t]
		if not c then return nil end
		local r = x86parse.classifyReg(c)
		return r and r.num or nil
	end
end

-- Used callee-saved GPRs in canonical push order.
function x86_64_render.usedCalleeSaved(color)
	local usedCallee = {}
	for _, c in color do usedCallee[c] = true end
	local calleeSaved = {}
	for _, r in x86.CALLEE_SAVED_ORDER do if usedCallee[r] then table.insert(calleeSaved, r) end end
	return calleeSaved
end

-- prologue/epilogue/rootStore emit in the requested syntax.
local function att_(rc) return rc.syntax == "att" end

function x86_64_render.header(rc)
	local out, sigName = rc.out, rc.sigName
	-- Trampoline state for this function (see the rel8-trampoline comment above):
	-- statement-order counter restarts at 0 and the used-name set collects every
	-- label this function's FIP body defines, PLUS every user label of EVERY
	-- function in the file (rc.tr.fileUserLabels, collected by the driver):
	-- .Lsarctramp_* names are FILE-GLOBAL assembly symbols, so a user label in
	-- one function named exactly like another function's generated trampoline
	-- name would collide in the object (gas: "symbol ... is already defined",
	-- plus a knock-on rel8 error). Generated names of OTHER functions cannot
	-- collide with this function's — each embeds its own function name — so the
	-- per-function statement-order counter stays deterministic.
	trampFn = rc.tr.name
	trampCounter = 0
	trampUsed = {}
	for n in (rc.tr.fileUserLabels or {}) do trampUsed[n] = true end
	for _, n in rc.tr.fipNodes do
		if n.kind == "label" then trampUsed[n.name] = true end
	end
	table.insert(out, "\t.text")
	table.insert(out, "\t.globl\t" .. sigName)
	table.insert(out, "\t.p2align\t4")
	table.insert(out, "\t.type\t" .. sigName .. ",@function")
	table.insert(out, sigName .. ":")
end

function x86_64_render.prologue(rc)
	local out, tr, layout = rc.out, rc.tr, rc.layout
	local att = att_(rc)
	local SC = x86.SCRATCH  -- r11
	local mq = att and "movq" or "mov"
	local function ln(s) table.insert(out, s) end
	local function I(v) return att and ("$" .. v) or tostring(v) end
	for _, r in rc.calleeSaved do ln("\t" .. (att and "pushq" or "push") .. "\t" .. (att and ("%" .. r) or r)) end
	local alignA = layout.alignA or 16
	if alignA > 16 then
		-- Dynamically aligned frame (a materialized cluster needs vmovdqa-class
		-- alignment): the entry rsp's mod-A residue is dynamic, so align at
		-- runtime and save the pre-alignment rsp for the epilogue to restore.
		-- layout.total covers the content plus A slack bytes for the rounding.
		ln("\t" .. (att and "movq" or "mov") .. "\t" .. (att and ("%rsp, %" .. SC) or (SC .. ", rsp")))
		ln("\t" .. (att and "subq" or "sub") .. "\t" .. (att and (I(layout.total) .. ", %rsp") or ("rsp, " .. layout.total)))
		ln("\t" .. (att and "andq" or "and") .. "\t" .. (att and ("$-" .. alignA .. ", %rsp") or ("rsp, -" .. alignA)))
		ln("\t" .. mq .. "\t" .. (att and ("%" .. SC .. ", " .. layout.saveOff .. "(%rsp)") or ("QWORD PTR [rsp + " .. layout.saveOff .. "], " .. SC)))
	else
		ln("\t" .. (att and "subq" or "sub") .. "\t" .. (att and (I(layout.total) .. ", %rsp") or ("rsp, " .. layout.total)))
	end
	-- stack-overflow check: fail when rsp <= limit (limit at [myth=rdi]).
	if att then
		ln("\tcmpq\t%rsp, (%rdi)")
		ln("\tjae\tfilc_stack_overflow_failure@PLT")
	else
		ln("\tcmp\trsp, QWORD PTR [rdi]")
		ln("\tjbe\tfilc_stack_overflow_failure@PLT")
	end
	-- push filc_frame at (rsp): prev, origin; myth->top_frame = rsp.
	ln("\t" .. mq .. "\t" .. (att and ("16(%rdi), %" .. SC) or (SC .. ", QWORD PTR [rdi + 16]")))
	ln("\t" .. mq .. "\t" .. (att and ("%" .. SC .. ", (%rsp)") or ("QWORD PTR [rsp], " .. SC)))
	ln("\t" .. mq .. "\t" .. (att and ("%rsp, 16(%rdi)") or ("QWORD PTR [rdi + 16], rsp")))
	ln("\t" .. (att and "leaq" or "lea") .. "\t" .. (att and (tr.originSym .. "(%rip), %" .. SC) or (SC .. ", [rip + " .. tr.originSym .. "]")))
	ln("\t" .. mq .. "\t" .. (att and ("%" .. SC .. ", 8(%rsp)") or ("QWORD PTR [rsp + 8], " .. SC)))
	-- Zero-initialize every GC root slot. Root slots are written lazily by rootStore (arg
	-- lowers at entry; load-ptr / alloca / call-result lowers after those points), but the
	-- frame declares all of them from the start — so any safepoint (pollcheck, allocation,
	-- call) that runs before a slot's first rootStore would otherwise expose stack garbage
	-- to the GC as a live lower. Matches clang, which emits `movq $0, slot(%rsp)` for its
	-- lowers in the prologue.
	for i = 0, (tr.numRoots or 0) - 1 do
		local off = layout.rootBase + 8 * i
		ln("\t" .. mq .. "\t" .. (att and ("$0, " .. off .. "(%rsp)") or ("QWORD PTR [rsp + " .. off .. "], 0")))
	end
end

function x86_64_render.epilogue(rc)
	local out, layout = rc.out, rc.layout
	local att = att_(rc)
	local SC = x86.SCRATCH
	local mq = att and "movq" or "mov"
	local mythName = regName(rc.mythNum, 64)
	local function ln(s) table.insert(out, s) end
	local function I(v) return att and ("$" .. v) or tostring(v) end
	-- pop frame: myth->top_frame = frame.prev  (scratch = r11, preserves rax/rdx/rcx)
	ln("\t" .. mq .. "\t" .. (att and ("(%rsp), %" .. SC) or (SC .. ", QWORD PTR [rsp]")))
	ln("\t" .. mq .. "\t" .. (att and ("%" .. SC .. ", 16(%" .. mythName .. ")") or ("QWORD PTR [" .. mythName .. " + 16], " .. SC)))
	local alignA2 = layout.alignA or 16
	if alignA2 > 16 then
		-- Dynamically aligned frame: restore the pre-alignment rsp saved by the
		-- prologue (the rounding is lossy, so the static total cannot restore
		-- it); the callee-saved pops below then match the prologue pushes.
		ln("\t" .. mq .. "\t" .. (att and (layout.saveOff .. "(%rsp), %rsp") or ("rsp, QWORD PTR [rsp + " .. layout.saveOff .. "]")))
	else
		ln("\t" .. (att and "addq" or "add") .. "\t" .. (att and (I(layout.total) .. ", %rsp") or ("rsp, " .. layout.total)))
	end
	for i = #rc.calleeSaved, 1, -1 do ln("\t" .. (att and "popq" or "pop") .. "\t" .. (att and ("%" .. rc.calleeSaved[i]) or rc.calleeSaved[i])) end
end

function x86_64_render.rootStore(rc, node)
	local att = att_(rc)
	local mq = att and "movq" or "mov"
	local off = rc.layout.rootBase + 8 * node.index
	local rn = regName(rc.colorNum(node.temp), 64)
	table.insert(rc.out, "\t" .. mq .. "\t" .. (att and ("%" .. rn .. ", " .. off .. "(%rsp)") or ("QWORD PTR [rsp + " .. off .. "], " .. rn)))
end

return x86_64_render
