--!nonstrict
-- ARM64 frame policy for the shared frame preprocessor (frame.luau). Provides frame-size
-- analysis and the per-statement rewrite: dropping AAPCS frame setup, virtualizing
-- sp/x29-relative slots as pseudo-registers, and rejecting stack-address escapes.

local sig = require("./sig")
local err = require("./err")

local arm64_frame = {}

-- Slot virtualization mirrors the x86_64 backend: a stack slot is keyed by its byte
-- offset and shared across access widths (a narrow store and a wider reload of the same
-- offset hit one web, exactly like one memory cell). Since ARM has no partial-width
-- register moves, sub-width accesses are virtualized with explicit extension/insertion:
--   strb w0, [sp, #7]  ->  bfi  xslot7, x0, #0, #8   (preserves upper bits, like memory)
--   ldrb w0, [sp, #7]  ->  uxtb w0, wslot7
--   ldrsb x0, [sp, #7] ->  sxtb x0, wslot7
--   ldrsw x0, [sp, #4] ->  sxtw x0, wslot4

local function isSp(op)
	return op and op.kind == "reg" and (op.name == "sp" or op.name == "wsp")
end
local function isFrameBase(op)
	return op and op.kind == "reg" and (op.name == "sp" or op.name == "x29")
end
local function immVal(op)
	if not op then return 0 end
	if op.kind == "imm" then
		local v = op.value or tonumber(op.text) or 0
		-- A shifted immediate counts at its architectural value: `sub sp, sp, #10, lsl #12`
		-- allocates 40960 bytes, not 10 (how clang/gcc emit large frames).
		local sh = op.shift
		if sh and sh.op == "lsl" and sh.amount then v = v * (2 ^ sh.amount) end
		return v
	end
	return 0
end

-- Callee-saved 64-bit GPR (x19..x28, plus fp x29 and lr x30): the registers an AAPCS
-- prologue/epilogue saves and restores.
local function isSavedReg(op)
	return op and op.kind == "reg" and op.rc == "gpr" and op.width == 64
		and op.num and op.num >= 19 and op.num <= 30
end

-- NEON/FP frame slots (PHASE 2): FP/SIMD registers pass through verbatim (they
-- are never register-allocated), but their STACK accesses must still be modeled.
-- A NEON stack slot virtualizes into the same raw-byte GPR slot webs the GPR
-- mechanism uses, via explicit GPR<->vector moves (the slot is just bytes):
--   str q0, [sp, #16]  ->  fmov xslot16, d0 ; fmov xslot24, v0.d[1]
--   ldr q0, [sp, #16]  ->  fmov d0, xslot16 ; fmov v0.d[1], xslot24
--                          (the FMOV vector-element encoding only exists for the
--                          top half, so the low 64 bits move as the d register)
--   str d8, [sp, #16]  ->  fmov xslot16, d8        (AAPCS callee-saved spill!)
--   str s0, [sp, #16]  ->  fmov wslot16, s0        (32-bit slot view, like str w0)
--   str h0/b0          ->  umov + bfi (narrow store preserves neighboring bytes)
--   ldr s0/h0/b0       ->  fmov s0, wslot (a scalar write zeroes the register's
--                          upper bits, exactly like the ldr it replaces)
-- Vector pairs (stp/ldp/stnp/ldnp of s/d/q) virtualize per element, and the
-- AAPCS callee-saved NEON prologue/epilogue forms (`str d8, [sp, #-32]!` /
-- `ldr d8, [sp], #32`, `stp d8, d9, [sp, #-16]!` / `ldp d8, d9, [sp], #16`)
-- virtualize at their final-sp-relative offset (tracked via ctx.cumAdj, since
-- the writeback's own sp adjustment is frame setup we drop). NEON forms that
-- cannot be virtualized (multi-structure ld2-4/st2-4, replicate ld1r-4r, and
-- lane-indexed single-element forms on a stack base) are rejected cleanly.

-- Byte width of a bare NEON scalar register (b/h/s/d/q/v name prefix).
local function neonScalarWidth(r)
	local p = string.sub(r.name or "", 1, 1)
	if p == "b" then return 1 elseif p == "h" then return 2 elseif p == "s" then return 4
	elseif p == "d" then return 8 else return 16 end  -- q / v
end

-- A v-element operand (v0.d[1] / v3.b[0] / ...) for a synthesized fmov/umov.
local function vElemReg(num, elem, idx)
	return { kind = "reg", rc = "neon", num = num, width = 128,
		name = "v" .. tostring(num) .. "." .. elem .. "[" .. tostring(idx) .. "]",
		element = { elem = elem, index = idx } }
end

-- Fresh scratch GPR number for the h/b store extract (never collides with slot
-- pseudo-registers, which are always positive: see frame.luau's SLOT_BASE).
local function fscratchNum(ctx)
	ctx.fscratchN = (ctx.fscratchN or 0) + 1
	return -(2000 + ctx.fscratchN)
end
local function gprNum(num, width)
	return { kind = "reg", rc = "gpr", num = num, width = width,
		name = (width == 64 and "x" or "w") .. tostring(num) }
end

-- Sub-width loads virtualized as an extending move out of the slot.
local SUB_LOAD = {
	ldrb   = { size = 1, ext = "uxtb" }, ldurb  = { size = 1, ext = "uxtb" },
	ldrsb  = { size = 1, ext = "sxtb" }, ldursb = { size = 1, ext = "sxtb" },
	ldrh   = { size = 2, ext = "uxth" }, ldurh  = { size = 2, ext = "uxth" },
	ldrsh  = { size = 2, ext = "sxth" }, ldursh = { size = 2, ext = "sxth" },
	ldrsw  = { size = 4, ext = "sxtw" }, ldursw = { size = 4, ext = "sxtw" },
	-- Acquire/exclusive loads model as their plain extending form (stack slots are
	-- thread-local, so the ordering semantics are immaterial here).
	ldarb  = { size = 1, ext = "uxtb" }, ldarh  = { size = 2, ext = "uxth" },
	ldxrb  = { size = 1, ext = "uxtb" }, ldaxrb = { size = 1, ext = "uxtb" },
	ldxrh  = { size = 2, ext = "uxth" }, ldaxrh = { size = 2, ext = "uxth" },
	ldapurb = { size = 1, ext = "uxtb" }, ldapurh = { size = 2, ext = "uxth" },
	ldapursb = { size = 1, ext = "sxtb" }, ldapursh = { size = 2, ext = "sxth" },
	ldapursw = { size = 4, ext = "sxtw" },
}

-- Sub-width stores virtualized as a bitfield insert into the slot (byte size).
local SUB_STORE = {
	strb = 1, sturb = 1, stlrb = 1,
	strh = 2, sturh = 2, stlrh = 2,
}

-- Full-width forms: size comes from the data register width (w=4, x=8).
local FULL_LOAD = { ldr = true, ldur = true, ldar = true, ldapr = true, ldapur = true,
	ldxr = true, ldaxr = true }
local FULL_STORE = { str = true, stur = true, stlr = true }
local PAIR_LOAD = { ldp = true, ldnp = true, ldxp = true, ldaxp = true }
local PAIR_STORE = { stp = true, stnp = true }

-- Pre-scan: total frame size (sum of sp decrements), fp offset (x29 = sp + fpOffset),
-- and whether x29 is established as a frame pointer at all (it may be an ordinary GPR,
-- see the sarcasm-rbpgpr-arm test). The usesFp result lets the transform normalize
-- x29-relative alloca bases/offsets into the sp-relative coordinate space (mirroring
-- x86_64's rbp handling) without mistaking an x29-as-GPR function for a framed one.
-- The 4th return is the FP/SIMD frame info (arm64's counterpart of x86_64's fpInfo):
-- { bytes = 0 (arm64 virtualizes NEON slots instead of materializing clusters),
--   saveBytes = the vector-save reservation for the injected-call save/restore —
--   16 bytes per v0..v31 slot when the function touches any NEON register, else 0 }.
-- `sigDesc` (optional) is the function's entry signature: a float/double argument or
-- an FP return rides in the v registers EVEN when the body never names a NEON
-- register (FP args are typically consumed by a call, an FP return produced, without
-- any NEON instruction in the body), so the vector-save reservation must be active —
-- otherwise an injected runtime call (pollcheck slow path, allocation, ptr-store
-- barrier) could silently clobber an FP argument.
function arm64_frame.analyzeFrame(body, sigDesc)
	local frameSize, fpOffset, usesFp = 0, 0, false
	local usesNeon = false
	if sigDesc then
		if sigDesc.retTypes[1] and sig.isFpClass(sigDesc.retTypes[1]) then
			usesNeon = true
		else
			for _, t in (sigDesc.argTypes or {}) do
				if sig.isFpClass(t) then
					usesNeon = true
					break
				end
			end
		end
	end
	for _, st in body do
		if st.kind == "insn" then
			local m = string.lower(st.mnemonic)
			local ops = st.operands
			if m == "sub" and ops[1] and ops[1].name == "sp" and ops[2] and ops[2].name == "sp" then
				frameSize += math.abs(immVal(ops[3]))
			elseif m == "mov" and ops[1] and ops[1].name == "x29" and ops[2] and ops[2].name == "sp" then
				fpOffset = 0
				usesFp = true
			elseif m == "add" and ops[1] and ops[1].name == "x29" and ops[2] and ops[2].name == "sp" then
				fpOffset = immVal(ops[3])
				usesFp = true
			else
				for _, o in ops do
					if o.kind == "mem" and o.base and o.base.name == "sp" and o.writeback == "pre" then
						frameSize += math.abs(immVal(o.index))
					end
				end
			end
			if not usesNeon then
				for _, o in ops do
					if (o.kind == "reg" and o.rc == "neon") or o.kind == "reglist" then
						usesNeon = true
						break
					end
				end
			end
		end
	end
	local fpInfo = { bytes = 0, saveBytes = usesNeon and (32 * 16) or 0 }
	return frameSize, fpOffset, usesFp, fpInfo
end

-- The vector-save reservation arm64 uses when a function's v registers must be
-- preserved across an injected runtime call (16 bytes per v0..v31 slot). The
-- transform forces this reservation for FP-signature functions even when the body
-- touches no NEON register (see transform.luau).
arm64_frame.VEC_SAVE_BYTES = 32 * 16

-- The signed sp adjustment a statement performs, for ctx.cumAdj (the cumulative
-- total of sp adjustments seen so far in body order): `sub sp` allocates (+),
-- `add sp` releases (-), a writeback memory operand applies sp := sp +/- index
-- (signed), and `mov sp, x29` restores absolutely to sp0 - frameSize + fpOffset
-- ("reset"). Used to place NEON callee-saved writeback saves at their
-- final-sp-relative slot offset.
local function spAdjustmentOf(st)
	local m = string.lower(st.mnemonic)
	local ops = st.operands
	if (m == "sub" or m == "add") and ops[1] and isSp(ops[1]) and ops[2] and isSp(ops[2])
		and ops[3] and ops[3].kind == "imm" then
		local v = immVal(ops[3])
		return (m == "sub") and v or -v
	end
	if m == "mov" and ops[1] and isSp(ops[1]) and ops[2] and ops[2].name == "x29" then
		return "reset"
	end
	local adj = 0
	for _, o in ops do
		if o.kind == "mem" and o.base and o.base.name == "sp" and o.writeback then
			adj = adj - immVal(o.index)
		end
	end
	return adj
end

-- Emit a constructed statement (carrying the original raw text for diagnostics).
local function emitInsn(ctx, raw, mnemonic, operands)
	ctx.emit({ kind = "insn", mnemonic = mnemonic, operands = operands, raw = raw })
end

-- True if `op` is a NEON data register (bare b/h/s/d/q/v scalar) or any
-- register-list (NEON structure) operand.
local function isNeonData(op)
	return (op and op.kind == "reg" and op.rc == "neon") or (op and op.kind == "reglist")
end

-- A "d"-form (low 64 bits) operand of a vector register, for the q forms: the
-- FMOV vector-element encoding only exists for .d[1] (the top half), so the
-- low half moves as `fmov xN, dK` / `fmov dK, xN`.
local function dFormReg(num)
	return { kind = "reg", rc = "neon", num = num, width = 64, name = "d" .. tostring(num) }
end

-- Virtualize a NEON scalar store of `src` (b/h/s/d/q reg) at slot offset off.
-- `st` is the source statement (threaded for diagnostics: checkBounds location).
local function neonStore(ctx, off, src, raw, st)
	local sz = neonScalarWidth(src)
	ctx.checkBounds(off, sz, true, raw, nil, st)
	if sz == 16 then
		emitInsn(ctx, raw, "fmov", { ctx.slotReg(off, 64), dFormReg(src.num) })
		emitInsn(ctx, raw, "fmov", { ctx.slotReg(off + 8, 64), vElemReg(src.num, "d", 1) })
	elseif sz == 8 then
		emitInsn(ctx, raw, "fmov", { ctx.slotReg(off, 64), src })
	elseif sz == 4 then
		emitInsn(ctx, raw, "fmov", { ctx.slotReg(off, 32), src })
	else
		-- h/b: bitfield insert of the low 16/8 bits, preserving the slot's other
		-- bytes exactly like a narrow memory store (the GPR strh/strb mechanism).
		local sn = fscratchNum(ctx)
		emitInsn(ctx, raw, "umov", { gprNum(sn, 32), vElemReg(src.num, (sz == 2) and "h" or "b", 0) })
		emitInsn(ctx, raw, "bfi", { ctx.slotReg(off, 64), gprNum(sn, 64),
			{ kind = "imm", text = "0", value = 0 },
			{ kind = "imm", text = tostring(sz * 8), value = sz * 8 } })
	end
end

-- Virtualize a NEON scalar load into `dst` (b/h/s/d/q reg) at slot offset off.
local function neonLoad(ctx, off, dst, raw, st)
	local sz = neonScalarWidth(dst)
	ctx.checkBounds(off, sz, false, raw, nil, st)
	if sz == 16 then
		emitInsn(ctx, raw, "fmov", { dFormReg(dst.num), ctx.slotReg(off, 64) })
		emitInsn(ctx, raw, "fmov", { vElemReg(dst.num, "d", 1), ctx.slotReg(off + 8, 64) })
	elseif sz == 8 then
		emitInsn(ctx, raw, "fmov", { dst, ctx.slotReg(off, 64) })
	elseif sz == 4 then
		emitInsn(ctx, raw, "fmov", { dst, ctx.slotReg(off, 32) })
	else
		-- h/b: extend out of the slot, then a scalar fmov — a scalar register
		-- write zeroes the upper bits of the vector register, exactly matching
		-- the ldr h/b it replaces.
		local sn = fscratchNum(ctx)
		emitInsn(ctx, raw, (sz == 2) and "uxth" or "uxtb", { gprNum(sn, 32), ctx.slotReg(off, 32) })
		emitInsn(ctx, raw, "fmov", { { kind = "reg", rc = "neon", num = dst.num, width = 32,
			name = "s" .. tostring(dst.num) }, gprNum(sn, 32) })
	end
end

-- Virtualize a NEON pair (s/d/q element stride e) load/store at [off, off+2e).
local function neonPair(ctx, off, r1, r2, isStore, raw, st)
	local e1, e2 = neonScalarWidth(r1), neonScalarWidth(r2)
	if e1 ~= e2 then
		err.at(st, "NEON register pair with mixed element widths cannot be virtualized: " .. raw)
	end
	if isStore then
		neonStore(ctx, off, r1, raw, st)
		neonStore(ctx, off + e1, r2, raw, st)
	else
		neonLoad(ctx, off, r1, raw, st)
		neonLoad(ctx, off + e1, r2, raw, st)
	end
end

-- Virtualize a single-structure full-vector ld1/st1 ({v0.16b} / {v0.8b} / ...).
-- Per-register bytes are 16 (q semantics) or 8 (d semantics; the ld1 also
-- zeroes the upper half, which the fmov d-form reproduces).
local function neonFullVector(ctx, off, rl, isStore, raw, st)
	local arr = rl.regs[1].arrangement
	local per = arr and arr.lanes and (arr.lanes * ({ b = 1, h = 2, s = 4, d = 8 })[arr.elem] or 0) or 0
	if per ~= 8 and per ~= 16 then
		err.at(st, "NEON stack access with an unsupported arrangement cannot be virtualized: " .. raw)
	end
	local n = rl.regs[1].num
	local asReg = { kind = "reg", rc = "neon", num = n, width = per * 8,
		name = ((per == 16) and "q" or "d") .. tostring(n) }
	if isStore then neonStore(ctx, off, asReg, raw, st) else neonLoad(ctx, off, asReg, raw, st) end
end

function arm64_frame.rewrite(st, ctx)
	local emit = ctx.emit
	local m = string.lower(st.mnemonic)
	local ops = st.operands

	-- Cumulative sp-adjustment tracking (see spAdjustmentOf): maintained for
	-- EVERY statement in body order so NEON callee-saved writeback saves can be
	-- placed at their final-sp-relative offset.
	local cumBefore = ctx.cumAdj or 0
	do
		local adj = spAdjustmentOf(st)
		if adj == "reset" then ctx.cumAdj = ctx.frameSize - ctx.fpOffset
		else ctx.cumAdj = cumBefore + adj end
	end

	-- `.alloca size, alignment, result`: convert frame-relative (spill-slot) memory
	-- operands to slot pseudo-registers in place, keeping the mnemonic. Only plain
	-- base+immediate slot references convert (sp or x29 base, no writeback, no shift,
	-- no register index); anything else is a clean rejection.
	if m == "alloca" then
		local araw = st.raw or m
		local anewops = {}
		for oi, o in ops do
			if o.kind ~= "mem" then table.insert(anewops, o)
			else
				if o.writeback or o.shift or o.postShift then
					err.at(st, "`.alloca` spill-slot operand must be a plain [sp, #off]/[x29, #off] reference: " .. araw)
				end
				if o.index and o.index.kind ~= "imm" then
					err.at(st, "indexed `.alloca` spill-slot operand is not supported: " .. araw)
				end
				if not isFrameBase(o.base) then
					err.at(st, "`.alloca` operand is not a register, immediate, or frame spill slot: " .. araw)
				end
				local abase = (o.base.name == "sp") and 0 or ctx.fpOffset
				local aoff = abase + immVal(o.index)
				ctx.checkBounds(aoff, 8, oi == 3, araw, nil, st)
				table.insert(anewops, ctx.slotReg(aoff, 64))
			end
		end
		emit({ kind = "insn", mnemonic = "alloca", operands = anewops, raw = st.raw, line = st.line, dotAlloca = true })
		return
	end

	-- (PHASE 2 removed the blanket NEON/FP rejection that lived here. NEON/FP
	-- registers now pass through; their stack accesses virtualize below; SVE is
	-- rejected in arm64_isa.classify, which validateBody runs on every
	-- instruction before this pass.)

	-- sp-relative accesses that fall inside a fixed-size alloca region are redirected to the
	-- allocation by the transform; pass them through untouched here.
	local function immVal2(op) return (op and op.kind == "imm") and (op.value or tonumber(op.text) or 0) or nil end
	if m == "add" and ops[2] and ops[2].name == "sp" and ops[1] and ops[1].name ~= "sp" then
		local off = immVal2(ops[3])
		if off and ctx.regionOf(off) then emit(st); return end
	end
	if m == "mov" and ops[2] and ops[2].name == "sp" and ops[1] and ops[1].name ~= "sp" then
		if ctx.regionOf(0) then emit(st); return end
	end
	do
		local mem
		for _, o in ops do if o.kind == "mem" then mem = o end end
		if mem and isFrameBase(mem.base) and not mem.writeback then
			local base = (mem.base.name == "sp") and 0 or ctx.fpOffset
			if ctx.regionOf(base + immVal(mem.index)) then emit(st); return end
		end
	end

	local raw = st.raw or m

	-- sp adjustments and fp setup. Constant sp (re)size (`sub/add sp, sp, #imm`) and the
	-- fp setup (`mov x29, sp` / `add x29, sp, #N`) are frame boilerplate we drop; a
	-- register-valued sp adjustment is a dynamic stack allocation, which sarcasm
	-- does not model on the stack pointer (allocate with the `.alloca` directive,
	-- which never touches sp).
	if (m == "sub" or m == "add") and ops[1] and isSp(ops[1]) then
		if ops[2] and isSp(ops[2]) and ops[3] and ops[3].kind == "imm" then return end
		err.at(st, "dynamic sp adjustment is not supported (allocate with the `.alloca` directive, which never touches sp): " .. raw)
	end
	if (m == "mov" or m == "add") and ops[1] and ops[1].name == "x29" then return end
	if m == "mov" and ops[1] and isSp(ops[1]) then
		if ops[2] and ops[2].name == "x29" then return end  -- `mov sp, x29` (epilogue restore)
		err.at(st, "write to sp from a register is not supported (allocate with the `.alloca` directive, which never touches sp): " .. raw)
	end

	local mem = nil
	for _, o in ops do if o.kind == "mem" then mem = o end end
	if mem and isFrameBase(mem.base) then
		-- Writeback forms (`[sp, #-N]!` / `[sp], #N`): only the AAPCS prologue/epilogue
		-- pair save/restore of callee-saved registers is pure frame setup we may drop.
		-- A plain `str/ldr x30` (or x29) is NOT dropped: in a leaf function gcc reuses
		-- x30 (lr) as a general register and spills it, so those are real values to
		-- virtualize. Anything else with base writeback is a mid-function sp move that
		-- sarcasm's frame model cannot track — reject it rather than silently dropping
		-- the memory access (and the sp update).
		if mem.writeback then
			if mem.base.name == "sp" and (m == "stp" or m == "ldp")
				and isSavedReg(ops[1]) and isSavedReg(ops[2])
				and mem.index and mem.index.kind == "imm"
				and ((m == "stp") == (immVal(mem.index) < 0)) then
				return  -- stp rA, rB, [sp, #-N]! / ldp rA, rB, [sp], #N  (rA,rB in x19..x30)
			end
			-- NEON callee-saved save/restore writeback forms (`str d8, [sp, #-32]!`
			-- / `ldr d8, [sp], #32`, `stp d8, d9, [sp, #-16]!` / `ldp d8, d9,
			-- [sp], #16`): unlike the GPR pairs these cannot just be DROPPED —
			-- vector registers pass through verbatim, so the caller's d8-d15
			-- values must round-trip through slot webs. The access sits at its
			-- final-sp-relative offset (the writeback's own sp adjustment is
			-- frame setup we drop): pre-index stores allocate (negative index),
			-- post-index loads release (positive index).
			if mem.base.name == "sp" and isNeonData(ops[1]) then
				local isStore = (m == "str" or m == "stur" or m == "stp" or m == "stnp")
				local isLoad = (m == "ldr" or m == "ldur" or m == "ldp" or m == "ldnp")
				local amt = (mem.index and mem.index.kind == "imm") and immVal(mem.index) or nil
				if not amt or not (isStore or isLoad) then
					err.at(st, "frame-relative NEON access with base writeback is not supported "
						.. "(only callee-saved str/ldr/stp/ldp prologue/epilogue forms can be modeled): " .. raw)
				end
				if not ((isStore and mem.writeback == "pre" and amt < 0)
					or (isLoad and mem.writeback == "post" and amt > 0)) then
					err.at(st, "frame-relative NEON access with base writeback is not supported "
						.. "(only callee-saved str/ldr/stp/ldp prologue/epilogue forms can be modeled): " .. raw)
				end
				local off = ctx.frameSize - cumBefore + ((mem.writeback == "pre") and amt or 0)
				if (m == "str" or m == "stur") and ops[1].kind == "reg" then
					neonStore(ctx, off, ops[1], raw, st)
				elseif (m == "ldr" or m == "ldur") and ops[1].kind == "reg" then
					neonLoad(ctx, off, ops[1], raw, st)
				elseif (m == "stp" or m == "stnp") and isNeonData(ops[2]) then
					neonPair(ctx, off, ops[1], ops[2], true, raw, st)
				elseif (m == "ldp" or m == "ldnp") and isNeonData(ops[2]) then
					neonPair(ctx, off, ops[1], ops[2], false, raw, st)
				else
					err.at(st, "frame-relative NEON access with base writeback cannot be "
						.. "virtualized (register-list/lane forms are not supported here): " .. raw)
				end
				return
			end
			err.at(st, "frame-relative memory access with base writeback is not supported "
				.. "(only callee-saved stp/ldp prologue/epilogue forms can be modeled): " .. raw)
		end

		-- Only plain base+immediate slots are virtualizable (no index register).
		if mem.index and mem.index.kind ~= "imm" then
			err.at(st, "indexed stack-relative access is not supported on arm64: " .. raw)
		end
		local base = (mem.base.name == "sp") and 0 or ctx.fpOffset
		local off = base + immVal(mem.index)

		-- NEON/FP stack slots virtualize into the raw-byte GPR slot webs via
		-- explicit GPR<->vector moves (see the note at the top of this file).
		if isNeonData(ops[1]) then
			if (m == "str" or m == "stur") and ops[1].kind == "reg" then
				neonStore(ctx, off, ops[1], raw, st)
			elseif (m == "ldr" or m == "ldur") and ops[1].kind == "reg" then
				neonLoad(ctx, off, ops[1], raw, st)
			elseif (m == "stp" or m == "stnp") and isNeonData(ops[2]) then
				neonPair(ctx, off, ops[1], ops[2], true, raw, st)
			elseif (m == "ldp" or m == "ldnp") and isNeonData(ops[2]) then
				neonPair(ctx, off, ops[1], ops[2], false, raw, st)
			elseif (m == "st1" or m == "ld1") and ops[1].kind == "reglist"
				and #ops[1].regs == 1 and ops[1].lane == nil then
				neonFullVector(ctx, off, ops[1], m == "st1", raw, st)
			else
				-- Multi-structure (ld2-4/st2-4), replicate (ld1r-4r) and
				-- lane-indexed single-element stack accesses cannot be
				-- virtualized — reject cleanly rather than mis-model them.
				err.at(st, "NEON stack access cannot be virtualized "
					.. "(multi-structure, replicate and lane-indexed forms are not"
					.. " supported on a frame base; only plain NEON scalar, pair and"
					.. " single-structure full-vector stack slots are): " .. raw)
			end
			return
		end

		local function dataReg(op, what)
			if op and op.kind == "reg" and (op.rc == "gpr" or op.rc == "zr") then return op end
			err.at(st, "cannot virtualize frame-relative " .. what
				.. " (unsupported data register): " .. raw)
		end
		local function mov(dst, src)
			emit({ kind = "insn", mnemonic = "mov", operands = { dst, src }, raw = st.raw })
		end

		if FULL_STORE[m] then
			local src = dataReg(ops[1], "store")
			ctx.checkBounds(off, (src.width == 32) and 4 or 8, true, raw, nil, st)
			mov(ctx.slotReg(off, src.width), src)
		elseif SUB_STORE[m] then
			local src = dataReg(ops[1], "store")
			local sz = SUB_STORE[m]
			ctx.checkBounds(off, sz, true, raw, nil, st)
			-- bfi inserts the low sz bytes, preserving the slot's upper bits exactly
			-- like a narrow memory store preserves the neighboring bytes.
			local src64
			if src.rc == "zr" then
				src64 = { kind = "reg", rc = "zr", width = 64, name = "xzr" }
			else
				src64 = { kind = "reg", rc = "gpr", num = src.num, width = 64, name = "x" .. tostring(src.num) }
			end
			emit({ kind = "insn", mnemonic = "bfi",
				operands = { ctx.slotReg(off, 64), src64,
					{ kind = "imm", text = "0", value = 0 },
					{ kind = "imm", text = tostring(sz * 8), value = sz * 8 } },
				raw = st.raw })
		elseif FULL_LOAD[m] then
			local dst = dataReg(ops[1], "load")
			ctx.checkBounds(off, (dst.width == 32) and 4 or 8, false, raw, nil, st)
			mov(dst, ctx.slotReg(off, dst.width))
		elseif SUB_LOAD[m] then
			local dst = dataReg(ops[1], "load")
			local t = SUB_LOAD[m]
			ctx.checkBounds(off, t.size, false, raw, nil, st)
			emit({ kind = "insn", mnemonic = t.ext, operands = { dst, ctx.slotReg(off, 32) }, raw = st.raw })
		elseif PAIR_STORE[m] then
			local s1, s2 = dataReg(ops[1], "pair store"), dataReg(ops[2], "pair store")
			local e = (s1.width == 32) and 4 or 8  -- element stride: w-pairs pack at 4 bytes
			ctx.checkBounds(off, e * 2, true, raw, nil, st)
			mov(ctx.slotReg(off, s1.width), s1)
			mov(ctx.slotReg(off + e, s2.width), s2)
		elseif PAIR_LOAD[m] then
			local d1, d2 = dataReg(ops[1], "pair load"), dataReg(ops[2], "pair load")
			local e = (d1.width == 32) and 4 or 8
			ctx.checkBounds(off, e * 2, false, raw, nil, st)
			mov(d1, ctx.slotReg(off, d1.width))
			mov(d2, ctx.slotReg(off + e, d2.width))
		elseif m == "ldpsw" then
			local d1, d2 = dataReg(ops[1], "pair load"), dataReg(ops[2], "pair load")
			ctx.checkBounds(off, 8, false, raw, nil, st)
			emit({ kind = "insn", mnemonic = "sxtw", operands = { d1, ctx.slotReg(off, 32) }, raw = st.raw })
			emit({ kind = "insn", mnemonic = "sxtw", operands = { d2, ctx.slotReg(off + 4, 32) }, raw = st.raw })
		else
			-- Not a load/store sarcasm models (prfm, cas, stxr, ld1/st1, ...): pass the
			-- statement through; the transform cleanly rejects a memory operand it
			-- cannot make memory-safe.
			emit(st)
		end
		return
	end

	-- Reject taking the address of the stack frame (potential escape). Note x29 is NOT
	-- treated as a frame register here: like the x86 backend's rbp-as-GPR handling, input
	-- code may use x29 as an ordinary register when it never establishes a frame pointer
	-- (see the sarcasm-rbpgpr-arm test); only sp is unambiguously the frame.
	if (m == "add" or m == "mov" or m == "sub") then
		for _, o in ops do
			if isSp(o) and ops[1] and not isSp(ops[1]) then
				ctx.rejectEscape(raw, st)
			end
		end
	end

	emit(st)
end

return arm64_frame
