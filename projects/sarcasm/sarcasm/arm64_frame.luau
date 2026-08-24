--!nonstrict
-- ARM64 frame policy for the shared frame preprocessor (frame.luau). Provides frame-size
-- analysis and the per-statement rewrite: dropping AAPCS frame setup, virtualizing
-- sp/x29-relative slots as pseudo-registers, and rejecting stack-address escapes.

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

-- Pre-scan: total frame size (sum of sp decrements) and fp offset (x29 = sp + fpOffset).
function arm64_frame.analyzeFrame(body)
	local frameSize, fpOffset = 0, 0
	for _, st in body do
		if st.kind == "insn" then
			local m = string.lower(st.mnemonic)
			local ops = st.operands
			if m == "sub" and ops[1] and ops[1].name == "sp" and ops[2] and ops[2].name == "sp" then
				frameSize += math.abs(immVal(ops[3]))
			elseif m == "mov" and ops[1] and ops[1].name == "x29" and ops[2] and ops[2].name == "sp" then
				fpOffset = 0
			elseif m == "add" and ops[1] and ops[1].name == "x29" and ops[2] and ops[2].name == "sp" then
				fpOffset = immVal(ops[3])
			else
				for _, o in ops do
					if o.kind == "mem" and o.base and o.base.name == "sp" and o.writeback == "pre" then
						frameSize += math.abs(immVal(o.index))
					end
				end
			end
		end
	end
	return frameSize, fpOffset, false
end

function arm64_frame.rewrite(st, ctx)
	local emit = ctx.emit
	local m = string.lower(st.mnemonic)
	local ops = st.operands

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
	-- register-valued sp adjustment is a dynamic allocation that only the transform's
	-- annotated-alloca path may model (annotated statements never reach this pass).
	if (m == "sub" or m == "add") and ops[1] and isSp(ops[1]) then
		if ops[2] and isSp(ops[2]) and ops[3] and ops[3].kind == "imm" then return end
		error("sarcasm: dynamic sp adjustment without an `;! alloca size` annotation is not supported: " .. raw)
	end
	if (m == "mov" or m == "add") and ops[1] and ops[1].name == "x29" then return end
	if m == "mov" and ops[1] and isSp(ops[1]) then
		if ops[2] and ops[2].name == "x29" then return end  -- `mov sp, x29` (epilogue restore)
		error("sarcasm: write to sp from a register without an `;! alloca` annotation is not supported: " .. raw)
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
			error("sarcasm: frame-relative memory access with base writeback is not supported "
				.. "(only callee-saved stp/ldp prologue/epilogue forms can be modeled): " .. raw)
		end

		-- Only plain base+immediate slots are virtualizable (no index register).
		if mem.index and mem.index.kind ~= "imm" then
			error("sarcasm: indexed stack-relative access is not supported on arm64: " .. raw)
		end
		local base = (mem.base.name == "sp") and 0 or ctx.fpOffset
		local off = base + immVal(mem.index)

		-- FP/SIMD data registers cannot be virtualized as GPR slot webs.
		for _, o in ops do
			if o.kind == "reg" and o.rc == "neon" then
				error("sarcasm: FP/SIMD register in a frame-relative memory access is not supported "
					.. "(NEON stack spills cannot be made memory-safe yet): " .. raw)
			end
		end

		local function dataReg(op, what)
			if op and op.kind == "reg" and (op.rc == "gpr" or op.rc == "zr") then return op end
			error("sarcasm: cannot virtualize frame-relative " .. what
				.. " (unsupported data register): " .. raw)
		end
		local function mov(dst, src)
			emit({ kind = "insn", mnemonic = "mov", operands = { dst, src }, raw = st.raw })
		end

		if FULL_STORE[m] then
			local src = dataReg(ops[1], "store")
			ctx.checkBounds(off, (src.width == 32) and 4 or 8, true, raw)
			mov(ctx.slotReg(off, src.width), src)
		elseif SUB_STORE[m] then
			local src = dataReg(ops[1], "store")
			local sz = SUB_STORE[m]
			ctx.checkBounds(off, sz, true, raw)
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
			ctx.checkBounds(off, (dst.width == 32) and 4 or 8, false, raw)
			mov(dst, ctx.slotReg(off, dst.width))
		elseif SUB_LOAD[m] then
			local dst = dataReg(ops[1], "load")
			local t = SUB_LOAD[m]
			ctx.checkBounds(off, t.size, false, raw)
			emit({ kind = "insn", mnemonic = t.ext, operands = { dst, ctx.slotReg(off, 32) }, raw = st.raw })
		elseif PAIR_STORE[m] then
			local s1, s2 = dataReg(ops[1], "pair store"), dataReg(ops[2], "pair store")
			local e = (s1.width == 32) and 4 or 8  -- element stride: w-pairs pack at 4 bytes
			ctx.checkBounds(off, e * 2, true, raw)
			mov(ctx.slotReg(off, s1.width), s1)
			mov(ctx.slotReg(off + e, s2.width), s2)
		elseif PAIR_LOAD[m] then
			local d1, d2 = dataReg(ops[1], "pair load"), dataReg(ops[2], "pair load")
			local e = (d1.width == 32) and 4 or 8
			ctx.checkBounds(off, e * 2, false, raw)
			mov(d1, ctx.slotReg(off, d1.width))
			mov(d2, ctx.slotReg(off + e, d2.width))
		elseif m == "ldpsw" then
			local d1, d2 = dataReg(ops[1], "pair load"), dataReg(ops[2], "pair load")
			ctx.checkBounds(off, 8, false, raw)
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
				ctx.rejectEscape(raw)
			end
		end
	end

	emit(st)
end

return arm64_frame
