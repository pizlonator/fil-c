--!nonstrict

-- x86_64 floating-point / SIMD knowledge module: SSE/AVX/AVX512 scalar+packed FP,
-- vector moves, MMX, x87, AES/SHA crypto, mask (k) registers, and the unsafe-instruction
-- reject list. Consulted by x86_64_isa.classify (def/use modeling + rejections),
-- x86_64_codegen.accessSizeAlign (checked-access width/alignment), and x86_64_frame
-- (stack-slot widths).
--
-- FP/SIMD registers are NEVER modeled in defs/uses: xmm-class register numbers (0-31)
-- would collide with GPR numbers in the lift's web analysis. Only GPR operands (and
-- GPR memory base/index registers) are modeled; vector registers pass through verbatim.
--
-- Entry shape:
--   {
--     w      = <bytes> | "vec" | "pspd" | fn(ops, mem) -> bytes | nil
--              ("vec": widest vector operand bytes; "pspd": max(16, widest vector bytes))
--     align  = <bytes> | "vec" | nil     -- nil = 1 (unaligned-permissive)
--     gpr    = "dst" | "src" | fn(ops, defs, uses)   -- model the GPR operand
--     memOk  = false | nil               -- false: reject if a memory operand is present
--     reject = "<reason>"                -- always reject with a clean error
--     noCheck= true                      -- memory-neutral (prefetch): pass through, no check
--     claim  = false | nil               -- false: only engage when an xmm-class register
--                                        -- is present (for mnemonics shared with GPR code)
--   }
--
-- The resolved width is authoritative over any Intel PTR size annotation:
-- fp.accessWidth cleanly rejects a PTR size contradicting an ISA-fixed width,
-- except for the size-driven width functions (SIZE_DRIVEN: the x87 PTR fallback
-- and cvtsi2ss/sd's integer source) which take their width from the annotation.

local fp = {}

-- ---------------------------------------------------------------------------
-- operand helpers

local function isGprOp(o) return o and o.kind == "reg" and o.rc == "gpr" end
local function isXmmOp(o) return o and o.kind == "reg" and o.rc == "xmm" end

local function memOperandOf(ops)
	for _, o in ops do if o.kind == "mem" then return o end end
	return nil
end
fp.memOperand = memOperandOf

local function opsHaveFp(ops)
	for _, o in ops do
		if isXmmOp(o) then return true end
		if o.kind == "mem" and (isXmmOp(o.base) or isXmmOp(o.index)) then return true end
	end
	return false
end

function fp.hasFpReg(insn) return opsHaveFp(insn.operands or {}) end

-- Vector byte width implied by an xmm-class register operand. x87 st(N) and mask
-- k registers do not define a vector width (return nil).
local function vecBytesOf(o)
	if not isXmmOp(o) then return nil end
	local n = o.name or ""
	local p3 = string.sub(n, 1, 3)
	if p3 == "xmm" then return 16 end
	if p3 == "ymm" then return 32 end
	if p3 == "zmm" then return 64 end
	if string.sub(n, 1, 2) == "mm" then return 8 end
	return nil
end
fp.vecBytesOf = vecBytesOf

local function widestVecBytes(ops)
	local w = nil
	for _, o in ops do
		local b = vecBytesOf(o)
		if b and (not w or b > w) then w = b end
	end
	return w
end
fp.widestVecBytes = widestVecBytes

-- ---------------------------------------------------------------------------
-- width / gpr-role helper functions used by table entries

-- cvtsi2ss/cvtsi2sd (unsuffixed or VEX): the memory source is an integer of GPR width.
local function cvtIntW(ops, mem)
	if mem and mem.size then return mem.size end
	for _, o in ops do
		if isGprOp(o) then return math.max(1, math.floor((o.width or 32) / 8)) end
	end
	return 4
end

-- movnti: GPR store whose width is REGISTER-determined (%esi -> 4, %rsi -> 8). An
-- Intel mem.size (PTR) annotation never wins over the register (a `movnti DWORD PTR
-- [rdi], rsi` still stores 8 bytes); accessWidth rejects a contradicting annotation.
local function movntiW(ops, mem)
	for _, o in ops do
		if isGprOp(o) then return math.max(4, math.floor((o.width or 64) / 8)) end
	end
	return 4
end

-- Width functions whose result is DRIVEN BY the Intel mem.size (PTR) annotation:
-- the x87 memory-form fallback (fld DWORD PTR -> 4, fld TBYTE PTR -> 10 — the
-- unsuffixed x87 mnemonics encode no width of their own) and the cvtsi2ss/cvtsi2sd
-- integer memory source (m32/m64 is genuinely operand-size determined, and the PTR
-- annotation is what conveys it). accessWidth's PTR-contradiction check exempts
-- these; every other entry's width is ISA-fixed, and a contradicting PTR size is a
-- lie that would under-size the bounds check / materialized stack slot, so
-- accessWidth rejects it.
local SIZE_DRIVEN = { [cvtIntW] = true }

-- remaining-x87 memory-form width: from the Intel PTR size only (size-driven).
local function x87MemW(ops, mem) return mem and mem.size or nil end
SIZE_DRIVEN[x87MemW] = true

-- VEX/EVEX vmovddup: xmm mem form reads 8; ymm/zmm forms read the full vector.
local function vmovddupW(ops)
	local b = widestVecBytes(ops) or 16
	return (b >= 32) and b or 8
end

-- VEX/EVEX widening convert (vcvtps2pd, vcvtdq2pd, vcvtph2ps): mem source is half the
-- destination vector width.
local function vCvtHalfW(ops)
	local d = vecBytesOf(ops and ops[1]) or 16
	return math.max(1, math.floor(d / 2))
end

-- VEX/EVEX narrowing converts (vcvtpd2ps & siblings): the memory SOURCE width is
-- SIZE-DRIVEN, because the destination register class does not determine it —
-- e.g. vcvtpd2ps reads m128 into an xmm (.128), m256 into an xmm (.256), or m512
-- into a YMM (.512), and the ph family (vcvtpd2ph, ...) reads m128/m256/m512
-- into an xmm every time. The width comes from the Intel PTR size annotation or
-- the AT&T source-width suffix (vcvtpd2psx -> 16, vcvtpd2psy -> 32; the ph-quad
-- family also has vcvtpd2phz -> 64 — its .512 form shares the xmm destination,
-- so the z suffix is its only gas-encodable sized spelling). A bare unsized
-- memory operand is accepted only when the destination class is unambiguous
-- (ymm destination => .512 => m512 for the ps/dq/bf16 family); everything else
-- is ambiguous (gas rejects it too, but we must reject it BEFORE rendering
-- anything gas would fail — and before ever under-checking the access).
--
-- Families (gas 2.42 form matrix, verified against `as`):
--   "dbl"   ps/dq/bf16 converts: source 16/32/64, dest xmm/xmm/ymm. No z suffix.
--   "quadz" 8-byte->ph converts: source 16/32/64, dest xmm always. x/y/z suffixes.
--   "quad"  4-byte->ph converts: source 16/32/64, dest xmm/xmm/ymm. x/y suffixes
--           only — the .512 form has NO z-suffixed spelling (`vcvtdq2phz` does
--           not assemble); like the dbl family it is written bare with a ymm
--           destination, so a bare ymm-dest form means an m512 source.
local NARROW_DBL = { vcvtpd2ps = true, vcvtpd2dq = true, vcvttpd2dq = true,
	vcvtpd2udq = true, vcvttpd2udq = true, vcvtuqq2ps = true, vcvtqq2ps = true,
	vcvtneps2bf16 = true, vcvtps2phx = true }
local NARROW_QUADZ = { vcvtpd2ph = true, vcvtqq2ph = true, vcvtuqq2ph = true }
local NARROW_QUAD = { vcvtdq2ph = true, vcvtudq2ph = true }

-- Resolve a narrowing-convert mnemonic to (family, x/y/z suffix, base mnemonic),
-- or nil when it is not one. The x/y/z suffix is part of the SPELLING, not the
-- AT&T [bwlq] size suffix (vcvtps2phx is itself a base mnemonic whose suffixed
-- forms are vcvtps2phxx/vcvtps2phxy).
local function narrowFamily(mnem)
	if NARROW_DBL[mnem] then return "dbl", nil, mnem end
	if NARROW_QUADZ[mnem] then return "quadz", nil, mnem end
	if NARROW_QUAD[mnem] then return "quad", nil, mnem end
	local base, suf = string.match(mnem, "^(.-)([xyz])$")
	if base then
		if NARROW_DBL[base] and suf ~= "z" then return "dbl", suf, base end
		if NARROW_QUADZ[base] then return "quadz", suf, base end
		if NARROW_QUAD[base] and suf ~= "z" then return "quad", suf, base end
	end
	return nil
end
fp.narrowFamily = narrowFamily

-- The AT&T rendering of a BARE narrowing mnemonic with a sized memory operand
-- needs the source-width suffix (bare vcvtpd2ps with an xmm destination is
-- ambiguous to gas): 16 -> x, 32 -> y, 64 -> z for the ph-quad family (the
-- ps/dq family's .512 form stays bare — its ymm destination disambiguates it).
function fp.narrowSuffix(mnem, memSize)
	local fam, suf = narrowFamily(mnem)
	if not fam or suf then return nil end
	if memSize == 16 then return "x" end
	if memSize == 32 then return "y" end
	if memSize == 64 and fam == "quadz" then return "z" end
	return nil
end

local function vCvtNarrowW(ops, mem, mnem, raw)
	local fam, suf, base = narrowFamily(mnem)
	base = base or mnem
	local legal = { [16] = true, [32] = true, [64] = true }
	local w
	if suf then
		w = (suf == "x") and 16 or (suf == "y") and 32 or 64
	elseif mem and mem.size then
		w = mem.size
		if not legal[w] then
			error("sarcasm: PTR size annotation (" .. tostring(w) .. " bytes) is not a legal"
				.. " memory source width for '" .. base .. "' (one of 16, 32, 64): " .. (raw or mnem))
		end
	elseif (fam == "dbl" or fam == "quad") and vecBytesOf(ops and ops[1]) == 32 then
		-- bare unsized, unambiguous ymm destination => .512 (m512 source). The
		-- quad family's .512 form has no z-suffixed spelling, so the bare
		-- ymm-dest form is its ONLY gas-encodable sized spelling.
		w = 64
	else
		error("sarcasm: cannot determine the memory access width of '" .. mnem
			.. "' (unsized memory operand is ambiguous; use a size suffix or PTR annotation): "
			.. (raw or mnem))
	end
	-- the destination vector class must match the resolved form (gas rejects
	-- e.g. vcvtpd2psy with a ymm destination — reject it cleanly ourselves)
	local dw = vecBytesOf(ops and ops[1])
	local need = ((fam == "dbl" or fam == "quad") and w == 64) and 32 or 16
	if dw and dw ~= need then
		error("sarcasm: '" .. mnem .. "' has no memory form with a " .. w
			.. "-byte source and this destination register class: " .. (raw or mnem))
	end
	return w
end
SIZE_DRIVEN[vCvtNarrowW] = true

-- EVEX widening convert from ph (2-byte) elements to 8-byte destinations
-- (vcvtph2pd, vcvtph2qq, vcvtph2uqq): the memory source is a QUARTER of the
-- destination vector width (zmm dest -> m128).
local function vCvtQuarterW(ops)
	local d = vecBytesOf(ops and ops[1]) or 16
	return math.max(1, math.floor(d / 4))
end

-- lzcnt/tzcnt/popcnt: the memory source width is the DESTINATION GPR's byte width
-- (lzcnt (%rdi), %rax -> 8, %eax -> 4, %ax -> 2).
local function dstGprW(ops, mem)
	local o = ops and ops[1]
	if isGprOp(o) then return math.max(1, math.floor((o.width or 64) / 8)) end
	if mem and mem.size then return mem.size end
	return 8
end

-- vcvtps2ph with a memory DESTINATION (its only memory form: the source is always a
-- register): the ph side is HALF the ps side, so the store is half the SOURCE vector
-- width (ymm -> 16, zmm -> 32). (vCvtDoubleW is WRONG here: it models a double-width
-- memory source, which is the vcvtpd2ps shape, not the vcvtps2ph shape.)
local function vcvtps2phW(ops)
	local b = widestVecBytes(ops) or 16
	return math.max(1, math.floor(b / 2))
end

-- lss/lfs/lgs: far-pointer load — memory source width = destination GPR bytes plus
-- the 2-byte segment selector (16-bit dest -> 4, 32-bit -> 6, 64-bit -> 10).
local function farPtrW(ops)
	local o = ops and ops[1]
	if isGprOp(o) then return math.max(1, math.floor((o.width or 32) / 8)) + 2 end
	return 6
end

-- movd/movq/kmov family: the lone GPR operand is a def when it is the destination
-- (ops[1], dest-first), otherwise a use.
local function gprByPosition(ops, defs, uses)
	for _, o in ops do
		if isGprOp(o) then
			if o == ops[1] then table.insert(defs, o) else table.insert(uses, o) end
		end
	end
end

-- xadd: BOTH explicit operands are read-modify-write (the source receives the old
-- destination); a memory destination is an RMW store. (The conservative
-- first-reg-def model is wrong here: it would drop the source's def and the
-- destination's use.)
local function gprXadd(ops, defs, uses)
	for _, o in ops do
		if isGprOp(o) then table.insert(defs, o); table.insert(uses, o) end
	end
end

-- adcx/adox/crc32: the destination is read-modify-write (dst += src + CF, resp.
-- dst = crc32(dst, src)); the source is a plain use. (adcx/adox DO have r/m
-- source forms — opcode 0f38f6 takes r/m32/64 — so the memory forms are modeled
-- like the register forms, not rejected.)
local function gprRmwDst(ops, defs, uses)
	if isGprOp(ops[1]) then table.insert(defs, ops[1]); table.insert(uses, ops[1]) end
	for i = 2, #ops do
		if isGprOp(ops[i]) then table.insert(uses, ops[i]) end
	end
end

-- xadd/adcx/adox: the access width is the shared operand width — the register
-- operand that is always present determines it (xaddb %sil, (%rdi) -> 1).
local function firstGprW(ops, mem)
	for _, o in ops do
		if isGprOp(o) then return math.max(1, math.floor((o.width or 64) / 8)) end
	end
	if mem and mem.size then return mem.size end
	return 8
end

-- crc32: the memory source width is the crc SOURCE width (r/m8 -> 1, r/m16 -> 2,
-- r/m32 -> 4, r/m64 -> 8), from the AT&T suffix (crc32b/w/l/q), the source
-- register width, or the Intel PTR size; an unsized memory source defaults to
-- the DESTINATION register width, mirroring gas (`crc32 (%rdi), %eax` assembles
-- as crc32l). Not every (dest, source) pair is encodable — an r32 destination
-- takes m8/m16/m32, an r64 destination takes m8/m64 — illegal combinations are
-- rejected cleanly rather than emitted for gas to fail.
local CRC32_SUFFIX = { b = 1, w = 2, l = 4, q = 8 }
local function crc32W(ops, mem, mnem, raw)
	local w = CRC32_SUFFIX[string.match(mnem, "^crc32([bwlq])$") or ""]
	if not w then
		local src = ops and ops[2]
		if isGprOp(src) then w = math.max(1, math.floor((src.width or 32) / 8)) end
	end
	if not w and mem and mem.size then w = mem.size end
	local dstW
	local dst = ops and ops[1]
	if isGprOp(dst) then dstW = math.max(1, math.floor((dst.width or 32) / 8)) end
	if not w and dstW then w = dstW end
	w = w or 4
	if dstW and not (w == 1 or (dstW == 4 and (w == 2 or w == 4)) or (dstW == 8 and w == 8)) then
		error("sarcasm: illegal crc32 destination/source width combination (" .. dstW
			.. "-byte destination, " .. w .. "-byte source) for '" .. mnem .. "': "
			.. (raw or mnem))
	end
	return w
end

-- ---------------------------------------------------------------------------
-- reject reasons

local REASON_X87STATE = "x87 environment/state-save instructions are not supported (cannot prove memory safety)"
local REASON_XSAVE = "xsave/xrstor state save/restore size is CPU-dependent (cannot prove memory safety)"
local REASON_GATHER = "gather/scatter instructions touch multiple discrete addresses (cannot prove memory safety)"
local REASON_MASKMOVDQU = "maskmovdqu/maskmovq store to an implicit memory operand (the DS:rdi destination) that cannot be bounds-checked (cannot prove memory safety)"
local REASON_ESTRI = "pcmpestri/pcmpistri-family instructions have implicit GPR operands (rax/rdx/rcx) that are not modeled (cannot prove memory safety)"
local REASON_AMX = "AMX tile instructions have strided, configuration-dependent memory footprints that cannot be bounds-checked (cannot prove memory safety)"

local REJECT_GATHER = { reject = REASON_GATHER, claim = true }

-- ---------------------------------------------------------------------------
-- the EXACT table

local EXACT = {
	-- SSE scalar FP ---------------------------------------------------------
	movss = { w = 4, claim = true }, movsd = { w = 8, claim = true },
	addss = { w = 4, claim = true }, addsd = { w = 8, claim = true },
	subss = { w = 4, claim = true }, subsd = { w = 8, claim = true },
	mulss = { w = 4, claim = true }, mulsd = { w = 8, claim = true },
	divss = { w = 4, claim = true }, divsd = { w = 8, claim = true },
	minss = { w = 4, claim = true }, minsd = { w = 8, claim = true },
	maxss = { w = 4, claim = true }, maxsd = { w = 8, claim = true },
	sqrtss = { w = 4, claim = true }, sqrtsd = { w = 8, claim = true },
	rcpss = { w = 4, claim = true }, rsqrtss = { w = 4, claim = true },
	cmpss = { w = 4, claim = true }, cmpsd = { w = 8, claim = true },
	comiss = { w = 4, claim = true }, comisd = { w = 8, claim = true },
	ucomiss = { w = 4, claim = true }, ucomisd = { w = 8, claim = true },
	roundss = { w = 4, claim = true }, roundsd = { w = 8, claim = true },
	cvtss2sd = { w = 4, claim = true }, cvtsd2ss = { w = 8, claim = true },

	-- SSE packed FP ---------------------------------------------------------
	movups = { w = 16, claim = true }, movupd = { w = 16, claim = true },
	movaps = { w = 16, align = 16, claim = true }, movapd = { w = 16, align = 16, claim = true },
	movntps = { w = 16, claim = true }, movntpd = { w = 16, claim = true },
	movhps = { w = 8, claim = true }, movhpd = { w = 8, claim = true },
	movlps = { w = 8, claim = true }, movlpd = { w = 8, claim = true },
	movddup = { w = 8, claim = true },  -- NB: SSE mem form reads 8, not 16
	movsldup = { w = 16, claim = true }, movshdup = { w = 16, claim = true },
	addps = { w = 16, claim = true }, addpd = { w = 16, claim = true },
	subps = { w = 16, claim = true }, subpd = { w = 16, claim = true },
	mulps = { w = 16, claim = true }, mulpd = { w = 16, claim = true },
	divps = { w = 16, claim = true }, divpd = { w = 16, claim = true },
	minps = { w = 16, claim = true }, minpd = { w = 16, claim = true },
	maxps = { w = 16, claim = true }, maxpd = { w = 16, claim = true },
	sqrtps = { w = 16, claim = true }, sqrtpd = { w = 16, claim = true },
	rcpps = { w = 16, claim = true }, rsqrtps = { w = 16, claim = true },
	cmpps = { w = 16, claim = true }, cmppd = { w = 16, claim = true },
	andps = { w = 16, claim = true }, andpd = { w = 16, claim = true },
	andnps = { w = 16, claim = true }, andnpd = { w = 16, claim = true },
	orps = { w = 16, claim = true }, orpd = { w = 16, claim = true },
	xorps = { w = 16, claim = true }, xorpd = { w = 16, claim = true },
	shufps = { w = 16, claim = true }, shufpd = { w = 16, claim = true },
	unpckhps = { w = 16, claim = true }, unpckhpd = { w = 16, claim = true },
	unpcklps = { w = 16, claim = true }, unpcklpd = { w = 16, claim = true },
	haddps = { w = 16, claim = true }, haddpd = { w = 16, claim = true },
	hsubps = { w = 16, claim = true }, hsubpd = { w = 16, claim = true },
	addsubps = { w = 16, claim = true }, addsubpd = { w = 16, claim = true },
	dpps = { w = 16, claim = true }, dppd = { w = 16, claim = true },
	roundps = { w = 16, claim = true }, roundpd = { w = 16, claim = true },
	blendps = { w = 16, claim = true }, blendpd = { w = 16, claim = true },
	blendvps = { w = 16, claim = true }, blendvpd = { w = 16, claim = true },
	insertps = { w = 4, claim = true },  -- mem form reads m32

	-- SSE/AVX converts (asymmetric shapes) ------------------------------------
	cvtps2pd = { w = 8, claim = true }, cvtpd2ps = { w = 16, claim = true },
	cvtdq2ps = { w = 16, claim = true }, cvtps2dq = { w = 16, claim = true },
	cvttps2dq = { w = 16, claim = true }, cvtdq2pd = { w = 8, claim = true },
	cvtpd2dq = { w = 8, claim = true }, cvttpd2dq = { w = 8, claim = true },
	cvtpi2ps = { w = 8, claim = true }, cvtpi2pd = { w = 8, claim = true },
	cvtps2pi = { w = 8, claim = true }, cvttps2pi = { w = 8, claim = true },
	cvtpd2pi = { w = 16, claim = true }, cvttpd2pi = { w = 16, claim = true },
	vcvtps2pd = { w = vCvtHalfW, claim = true }, vcvtdq2pd = { w = vCvtHalfW, claim = true },
	vcvtph2ps = { w = vCvtHalfW, claim = true },
	vcvtps2ph = { w = vcvtps2phW, claim = true },
	-- NB: the narrowing converts (vcvtpd2ps & siblings, incl. the AT&T x/y/z
	-- suffix spellings) are NOT here — they resolve through narrowFamily in
	-- fp.lookup with the size-driven vCvtNarrowW width.

	-- remaining AVX512 packed converts (mem bytes = source lanes x source element
	-- bytes; element sizes from the SDM operand forms) ---------------------------
	-- widening (mem = dest vector bytes / 2)
	vcvtudq2pd = { w = vCvtHalfW, claim = true },
	vcvtps2qq = { w = vCvtHalfW, claim = true }, vcvttps2qq = { w = vCvtHalfW, claim = true },
	vcvtps2uqq = { w = vCvtHalfW, claim = true }, vcvttps2uqq = { w = vCvtHalfW, claim = true },
	vcvtph2dq = { w = vCvtHalfW, claim = true }, vcvttph2dq = { w = vCvtHalfW, claim = true },
	vcvtph2udq = { w = vCvtHalfW, claim = true }, vcvttph2udq = { w = vCvtHalfW, claim = true },
	vcvtph2psx = { w = vCvtHalfW, claim = true },
	-- widening ph (2-byte) -> 8-byte destinations (mem = dest vector bytes / 4)
	vcvtph2pd = { w = vCvtQuarterW, claim = true },
	vcvtph2qq = { w = vCvtQuarterW, claim = true }, vcvttph2qq = { w = vCvtQuarterW, claim = true },
	vcvtph2uqq = { w = vCvtQuarterW, claim = true }, vcvttph2uqq = { w = vCvtQuarterW, claim = true },
	-- same-width element re-interpretations (mem = dest vector bytes)
	vcvtps2udq = { w = "vec", claim = true }, vcvttps2udq = { w = "vec", claim = true },
	vcvtpd2qq = { w = "vec", claim = true }, vcvttpd2qq = { w = "vec", claim = true },
	-- full-width UNSIGNED converts (8-byte elements either side, like pd2qq)
	vcvtpd2uqq = { w = "vec", claim = true }, vcvttpd2uqq = { w = "vec", claim = true },
	vcvtuqq2pd = { w = "vec", claim = true }, vcvtqq2pd = { w = "vec", claim = true },
	vcvtudq2ps = { w = "vec", claim = true },
	vcvtne2ps2bf16 = { w = "vec", claim = true },
	vcvtph2w = { w = "vec", claim = true }, vcvttph2w = { w = "vec", claim = true },
	vcvtw2ph = { w = "vec", claim = true },

	-- AVX512-VNNI / AVX-VNNI(-INT8) dot products: FULL-vector memory source. The
	-- ss/sd suffix rule would otherwise pin e.g. vpdpwssd to 8 bytes for a 64-byte
	-- read. (Must be EXACT: the p-family pattern would catch the vp* spellings but
	-- never vp4dpwssd, whose post-v character is a digit.)
	vpdpbusd = { w = "vec", claim = true }, vpdpbusds = { w = "vec", claim = true },
	vpdpwssd = { w = "vec", claim = true }, vpdpwssds = { w = "vec", claim = true },
	vpdpbssd = { w = "vec", claim = true }, vpdpbssds = { w = "vec", claim = true },
	-- 4VNNIW: the memory operand is a single m128 (16 bytes) regardless of the zmm
	-- width (EVEX.512 ... m128 per the SDM) — NOT the vector width.
	vp4dpwssd = { w = 16, claim = true }, vp4dpwssds = { w = 16, claim = true },
	-- AVX512-IFMA 52-bit multiply-add: full-vector memory source. (Must be EXACT:
	-- the digits keep the p-family pattern from matching — "vpmadd52luq" is not
	-- ^v?p[%a]+$.)
	vpmadd52huq = { w = "vec", claim = true }, vpmadd52luq = { w = "vec", claim = true },
	-- AVX512BW sum-of-absolute-differences: full-width memory source.
	vdbpsadbw = { w = "vec", claim = true },

	-- vector moves ------------------------------------------------------------
	movd = { w = 4, gpr = gprByPosition, claim = true },
	movq = { w = 8, gpr = gprByPosition, claim = false },  -- shared with GPR `movq`
	movdqa = { w = 16, align = 16, claim = true }, movdqu = { w = 16, claim = true },
	lddqu = { w = 16, claim = true }, movntdq = { w = 16, claim = true },
	movntdqa = { w = 16, claim = true },
	movnti = { w = movntiW, gpr = "src", claim = true },
	-- AVX512 byte/word/dword/qword vector moves (width = vector width)
	vmovdqu8 = { w = "vec", claim = true }, vmovdqu16 = { w = "vec", claim = true },
	vmovdqu32 = { w = "vec", claim = true }, vmovdqu64 = { w = "vec", claim = true },
	vmovdqa32 = { w = "vec", align = "vec", claim = true },
	vmovdqa64 = { w = "vec", align = "vec", claim = true },
	vmovddup = { w = vmovddupW, claim = true },
	-- broadcasts (non-decorator forms)
	vbroadcastss = { w = 4, claim = true }, vbroadcastsd = { w = 8, claim = true },
	vbroadcastf128 = { w = 16, claim = true }, vbroadcasti128 = { w = 16, claim = true },
	vbroadcastf32x2 = { w = 8, claim = true }, vbroadcastf32x4 = { w = 16, claim = true },
	vbroadcastf32x8 = { w = 32, claim = true },
	vbroadcastf64x2 = { w = 16, claim = true }, vbroadcastf64x4 = { w = 32, claim = true },
	vbroadcasti32x2 = { w = 8, claim = true }, vbroadcasti32x4 = { w = 16, claim = true },
	vbroadcasti32x8 = { w = 32, claim = true },
	vbroadcasti64x2 = { w = 16, claim = true }, vbroadcasti64x4 = { w = 32, claim = true },
	-- AVX512 single-element broadcasts: the memory form reads ONE element (1/2/4/8
	-- bytes) regardless of the destination vector width, and the AVX512 GPR-source
	-- form reads a GPR that MUST be modeled (otherwise regalloc can rename the
	-- producing web and the broadcast reads a stale register). gpr="src" is a no-op
	-- for the xmm/memory-source forms (no GPR operand exists).
	vpbroadcastb = { w = 1, gpr = "src", claim = true },
	vpbroadcastw = { w = 2, gpr = "src", claim = true },
	vpbroadcastd = { w = 4, gpr = "src", claim = true },
	vpbroadcastq = { w = 8, gpr = "src", claim = true },
	-- extract/insert 128-bit lanes (mem width = lane width)
	vperm2f128 = { w = "vec", claim = true }, vperm2i128 = { w = "vec", claim = true },
	vextractf128 = { w = 16, claim = true }, vextracti128 = { w = 16, claim = true },
	vinsertf128 = { w = 16, claim = true }, vinserti128 = { w = 16, claim = true },
	vextractf32x4 = { w = 16, claim = true }, vextractf32x8 = { w = 32, claim = true },
	vextractf64x2 = { w = 16, claim = true }, vextractf64x4 = { w = 32, claim = true },
	vinsertf32x4 = { w = 16, claim = true }, vinsertf32x8 = { w = 32, claim = true },
	vinsertf64x2 = { w = 16, claim = true }, vinsertf64x4 = { w = 32, claim = true },
	vextracti32x4 = { w = 16, claim = true }, vextracti32x8 = { w = 32, claim = true },
	vextracti64x2 = { w = 16, claim = true }, vextracti64x4 = { w = 32, claim = true },
	vinserti32x4 = { w = 16, claim = true }, vinserti32x8 = { w = 32, claim = true },
	vinserti64x2 = { w = 16, claim = true }, vinserti64x4 = { w = 32, claim = true },
	vshuff32x4 = { w = "vec", claim = true }, vshuff64x2 = { w = "vec", claim = true },
	vshufi32x4 = { w = "vec", claim = true }, vshufi64x2 = { w = "vec", claim = true },
	valignd = { w = "vec", claim = true }, valignq = { w = "vec", claim = true },

	-- GPR-mixing conversions (the miscompile fixes: the GPR must be modeled right) --
	cvtsi2ss = { w = cvtIntW, gpr = "src", claim = true },
	cvtsi2sd = { w = cvtIntW, gpr = "src", claim = true },
	cvtsi2ssl = { w = 4, gpr = "src", claim = true }, cvtsi2ssq = { w = 8, gpr = "src", claim = true },
	cvtsi2sdl = { w = 4, gpr = "src", claim = true }, cvtsi2sdq = { w = 8, gpr = "src", claim = true },
	cvttss2si = { w = 4, gpr = "dst", claim = true }, cvttsd2si = { w = 8, gpr = "dst", claim = true },
	cvtss2si = { w = 4, gpr = "dst", claim = true }, cvtsd2si = { w = 8, gpr = "dst", claim = true },
	cvttss2sil = { w = 4, gpr = "dst", claim = true }, cvttss2siq = { w = 4, gpr = "dst", claim = true },
	cvttsd2sil = { w = 8, gpr = "dst", claim = true }, cvttsd2siq = { w = 8, gpr = "dst", claim = true },
	cvtss2sil = { w = 4, gpr = "dst", claim = true }, cvtss2siq = { w = 4, gpr = "dst", claim = true },
	cvtsd2sil = { w = 8, gpr = "dst", claim = true }, cvtsd2siq = { w = 8, gpr = "dst", claim = true },
	-- EVEX unsigned-integer scalar converts (vcvtusi2ss/sd mirror cvtsi2ss/sd: the
	-- GPR/mem integer source is size-driven and MUST be modeled as a use — an
	-- unmodeled source reads a stale register after regalloc. The reverse usi
	-- family mirrors cvttss2si & co: mem forms read m32/m32/m64/m64.)
	vcvtusi2ss = { w = cvtIntW, gpr = "src", claim = true },
	vcvtusi2sd = { w = cvtIntW, gpr = "src", claim = true },
	vcvtusi2ssl = { w = 4, gpr = "src", claim = true }, vcvtusi2ssq = { w = 8, gpr = "src", claim = true },
	vcvtusi2sdl = { w = 4, gpr = "src", claim = true }, vcvtusi2sdq = { w = 8, gpr = "src", claim = true },
	vcvttss2usi = { w = 4, gpr = "dst", claim = true }, vcvtss2usi = { w = 4, gpr = "dst", claim = true },
	vcvttsd2usi = { w = 8, gpr = "dst", claim = true }, vcvtsd2usi = { w = 8, gpr = "dst", claim = true },
	-- move-mask extracts: GPR destination, register-only
	movmskps = { gpr = "dst", memOk = false, claim = true },
	movmskpd = { gpr = "dst", memOk = false, claim = true },
	pmovmskb = { gpr = "dst", memOk = false, claim = true },
	vpmovmskb = { gpr = "dst", memOk = false, claim = true },
	vpmovmskw = { gpr = "dst", memOk = false, claim = true },
	vpmovmskd = { gpr = "dst", memOk = false, claim = true },
	vpmovmskq = { gpr = "dst", memOk = false, claim = true },
	pextrb = { w = 1, gpr = "dst", claim = true }, pextrw = { w = 2, gpr = "dst", claim = true },
	pextrd = { w = 4, gpr = "dst", claim = true }, pextrq = { w = 8, gpr = "dst", claim = true },
	extractps = { w = 4, gpr = "dst", claim = true },
	pinsrb = { w = 1, gpr = "src", claim = true }, pinsrw = { w = 2, gpr = "src", claim = true },
	pinsrd = { w = 4, gpr = "src", claim = true }, pinsrq = { w = 8, gpr = "src", claim = true },
	-- opmask moves (k<->gpr/mem)
	kmovb = { w = 1, gpr = gprByPosition, claim = true },
	kmovw = { w = 2, gpr = gprByPosition, claim = true },
	kmovd = { w = 4, gpr = gprByPosition, claim = true },
	kmovq = { w = 8, gpr = gprByPosition, claim = true },

	-- crypto --------------------------------------------------------------------
	aesenc = { w = 16, claim = true }, aesdec = { w = 16, claim = true },
	aesenclast = { w = 16, claim = true }, aesdeclast = { w = 16, claim = true },
	aesimc = { memOk = false, claim = true },
	aeskeygenassist = { w = 16, claim = true },
	pclmulqdq = { w = 16, claim = true }, vpclmulqdq = { w = "vec", claim = true },
	sha1rnds4 = { w = 16, claim = true }, sha1nexte = { w = 16, claim = true },
	sha1msg1 = { w = 16, claim = true }, sha1msg2 = { w = 16, claim = true },
	sha256rnds2 = { w = 16, claim = true }, sha256msg1 = { w = 16, claim = true },
	sha256msg2 = { w = 16, claim = true },
	-- SHA512-NI (VEX.256.F2.0F38): vsha512rnds2 (ymm,ymm,xmm/m128 — reads xmm0
	-- implicitly, see IMPLICIT_XMM0_USE) and vsha512msg1 (ymm,xmm/m128) read
	-- m128 memory sources; vsha512msg2 (ymm,ymm/m256) reads a full m256. gas
	-- before 2.42 does not know these, so the renderer emits them as .byte
	-- (x86_64_render).
	vsha512rnds2 = { w = 16, claim = true },
	vsha512msg1 = { w = 16, claim = true }, vsha512msg2 = { w = 32, claim = true },
	-- GFNI: width = vector width (xmm=16; the VEX/EVEX vgf2p8* forms ride the
	-- v-alias of these entries, ymm=32/zmm=64). gf2p8affine* carry a trailing
	-- immediate. NB: LLVM and GNU binutils spell Intel's GF2P8AFFINEB (opcode
	-- 0F3A CE) as "gf2p8affineqb" and GF2P8AFFINEQB (0F3A CF) as
	-- "gf2p8affineinvqb" — all three spellings are covered.
	gf2p8mulb = { w = "vec", claim = true },
	gf2p8affineb = { w = "vec", claim = true }, gf2p8affineqb = { w = "vec", claim = true },
	gf2p8affineinvqb = { w = "vec", claim = true },
	-- SM3/SM4: xmm-only families, one 16-byte mem operand (clang/binutils spell
	-- them vsm3*/vsm4* — VEX.128 encodings — which ride the v-alias of these).
	sm3rnds2 = { w = 16, claim = true }, sm3msg1 = { w = 16, claim = true },
	sm3msg2 = { w = 16, claim = true },
	sm4key4 = { w = 16, claim = true }, sm4rnds4 = { w = 16, claim = true },

	-- mxcsr ---------------------------------------------------------------------
	ldmxcsr = { w = 4, claim = true }, stmxcsr = { w = 4, claim = true },

	-- descriptor-table/system-register memory forms (unprivileged) ----------------
	-- sgdt/sidt store 2-byte limit + 8-byte base = 10 bytes; sldt/str/smsw store
	-- 2 bytes (their register forms take a GPR destination). The store-vs-load
	-- labeling for these lives in x86_64_codegen.isStoreInsn.
	sgdt = { w = 10, claim = true }, sidt = { w = 10, claim = true },
	sldt = { w = 2, gpr = "dst", claim = true }, str = { w = 2, gpr = "dst", claim = true },
	smsw = { w = 2, gpr = "dst", claim = true },
	-- far-pointer loads: dest GPR bytes + 2-byte selector (load semantics).
	lss = { w = farPtrW, gpr = "dst", claim = true },
	lfs = { w = farPtrW, gpr = "dst", claim = true },
	lgs = { w = farPtrW, gpr = "dst", claim = true },

	-- x87 memory forms (AT&T suffixes: s/l/t for float, s/l/q(ll) for integer) -----
	flds = { w = 4, claim = true }, fldl = { w = 8, claim = true }, fldt = { w = 10, claim = true },
	fsts = { w = 4, claim = true }, fstl = { w = 8, claim = true }, fstt = { w = 10, claim = true },
	fstps = { w = 4, claim = true }, fstpl = { w = 8, claim = true }, fstpt = { w = 10, claim = true },
	filds = { w = 2, claim = true }, fildl = { w = 4, claim = true },
	fildq = { w = 8, claim = true }, fildll = { w = 8, claim = true },
	fists = { w = 2, claim = true }, fistl = { w = 4, claim = true },
	fistq = { w = 8, claim = true },
	fistps = { w = 2, claim = true }, fistpl = { w = 4, claim = true },
	fistpq = { w = 8, claim = true }, fistpll = { w = 8, claim = true },
	fisttps = { w = 2, claim = true }, fisttpl = { w = 4, claim = true }, fisttpq = { w = 8, claim = true },
	fiadds = { w = 2, claim = true }, fiaddl = { w = 4, claim = true },
	fisubs = { w = 2, claim = true }, fisubl = { w = 4, claim = true },
	fisubrs = { w = 2, claim = true }, fisubrl = { w = 4, claim = true },
	fimuls = { w = 2, claim = true }, fimull = { w = 4, claim = true },
	fidivs = { w = 2, claim = true }, fidivl = { w = 4, claim = true },
	fidivrs = { w = 2, claim = true }, fidivrl = { w = 4, claim = true },
	ficoms = { w = 2, claim = true }, ficoml = { w = 4, claim = true },
	ficomps = { w = 2, claim = true }, ficompl = { w = 4, claim = true },
	fcoms = { w = 4, claim = true }, fcoml = { w = 8, claim = true },
	fcomps = { w = 4, claim = true }, fcompl = { w = 8, claim = true },
	fadds = { w = 4, claim = true }, faddl = { w = 8, claim = true },
	fsubs = { w = 4, claim = true }, fsubl = { w = 8, claim = true },
	fmuls = { w = 4, claim = true }, fmull = { w = 8, claim = true },
	fdivs = { w = 4, claim = true }, fdivl = { w = 8, claim = true },
	fsubrs = { w = 4, claim = true }, fsubrl = { w = 8, claim = true },
	fdivrs = { w = 4, claim = true }, fdivrl = { w = 8, claim = true },
	fbld = { w = 10, claim = true }, fbstp = { w = 10, claim = true },
	-- fnstsw/fstsw %ax write ONLY %ax: bits 16-63 of the enclosing register are
	-- preserved — a partial write, so the GPR destination is an RMW of its web
	-- (gprRmwDst + the auto-rmw below; codegen's lowerSpecial pins the reaching
	-- web into physical rax so the preserved bits are the web's own).
	fnstsw = { w = 2, gpr = gprRmwDst, claim = true }, fstsw = { w = 2, gpr = gprRmwDst, claim = true },
	fnstcw = { w = 2, claim = true }, fstcw = { w = 2, claim = true }, fldcw = { w = 2, claim = true },

	-- fxsave/fxrstor: fixed 512-byte state image, 16-byte aligned -----------------
	fxsave = { w = 512, align = 16, claim = true }, fxsave64 = { w = 512, align = 16, claim = true },
	fxrstor = { w = 512, align = 16, claim = true }, fxrstor64 = { w = 512, align = 16, claim = true },

	-- MMX/misc reg-only ----------------------------------------------------------
	emms = { memOk = false, claim = true }, femms = { memOk = false, claim = true },
	vzeroupper = { memOk = false, claim = true }, vzeroall = { memOk = false, claim = true },

	-- memory-neutral hints --------------------------------------------------------
	prefetch = { noCheck = true, claim = true },
	prefetcht0 = { noCheck = true, claim = true }, prefetcht1 = { noCheck = true, claim = true },
	prefetcht2 = { noCheck = true, claim = true }, prefetchnta = { noCheck = true, claim = true },
	prefetchw = { noCheck = true, claim = true }, prefetchwt1 = { noCheck = true, claim = true },
	-- cache-line management: clflush/clflushopt/clwb move no data to or from the
	-- processor — they invalidate/write-back the addressed line in the cache
	-- hierarchy, which changes nothing the checker models (the memory contents a
	-- program can observe are unaffected). A wild address can only fault, an
	-- uncatchable-signal safe halt the safety model accepts (the same reasoning as
	-- monitor; unlike clzero, which WRITES zeros to memory at rax and is rejected).
	clflush = { noCheck = true, claim = true },
	clflushopt = { noCheck = true, claim = true }, clwb = { noCheck = true, claim = true },

	-- hard rejects -----------------------------------------------------------------
	fsave = { reject = REASON_X87STATE, claim = true }, fnsave = { reject = REASON_X87STATE, claim = true },
	frstor = { reject = REASON_X87STATE, claim = true },
	fstenv = { reject = REASON_X87STATE, claim = true }, fnstenv = { reject = REASON_X87STATE, claim = true },
	fldenv = { reject = REASON_X87STATE, claim = true },
	xsave = { reject = REASON_XSAVE, claim = true }, xsave64 = { reject = REASON_XSAVE, claim = true },
	xrstor = { reject = REASON_XSAVE, claim = true }, xrstor64 = { reject = REASON_XSAVE, claim = true },
	xsaveopt = { reject = REASON_XSAVE, claim = true }, xsaveopt64 = { reject = REASON_XSAVE, claim = true },
	xsaves = { reject = REASON_XSAVE, claim = true }, xsaves64 = { reject = REASON_XSAVE, claim = true },
	xrstors = { reject = REASON_XSAVE, claim = true }, xrstors64 = { reject = REASON_XSAVE, claim = true },
	xsavec = { reject = REASON_XSAVE, claim = true }, xsavec64 = { reject = REASON_XSAVE, claim = true },
	-- AVX2 vector-masked loads/stores (vmaskmovps/pd, vpmaskmovd/q): the
	-- explicit mask VECTOR operand's per-lane sign bits are the lane enables.
	-- They get the same mask-aware bounds check as the AVX512 {k} forms (the
	-- fixed-position algorithm; the mask is extracted with vmovmskps/pd) —
	-- exactly what the Fil-C compiler emits for the x86_avx_maskload/store
	-- intrinsics. Width = the vector width (the full footprint).
	vmaskmovps = { w = "vec", claim = true }, vmaskmovpd = { w = "vec", claim = true },
	vpmaskmovd = { w = "vec", claim = true }, vpmaskmovq = { w = "vec", claim = true },
	maskmovdqu = { reject = REASON_MASKMOVDQU, claim = true },  -- implicit rdi addressing
	maskmovq = { reject = REASON_MASKMOVDQU, claim = true },
	-- NB: cmpxchg8b/cmpxchg16b are NOT here — they are genuinely MODELED (implicit
	-- edx:eax/ecx:ebx resp. rdx:rax/rcx:rbx operand pairs pinned through
	-- emitPinned) in x86_64_isa.classify.
}
fp.EXACT = EXACT

-- reg-only x87 (no memory form): reject only if a memory operand sneaks in.
for _, m in { "fldz", "fld1", "fldpi", "fldl2t", "fldl2e", "fldlg2", "fldln2",
	"faddp", "fsubp", "fsubrp", "fmulp", "fdivp", "fdivrp", "fcompp", "fucompp",
	"fxch", "fxtract", "fyl2x", "fyl2xp1", "f2xm1", "fabs", "fchs", "fcos", "fsin",
	"fsincos", "fptan", "fpatan", "fprem", "fprem1", "frndint", "fscale", "fsqrt",
	"ftst", "fxam", "fwait", "fnop", "ffree", "ffreep", "fincstp", "fdecstp",
	"fnclex", "fclex", "fninit", "finit",
	"fcomi", "fcomip", "fucomi", "fucomip",
	"fcmovb", "fcmove", "fcmovbe", "fcmovu", "fcmovnb", "fcmovne", "fcmovnbe", "fcmovnu" } do
	EXACT[m] = { memOk = false, claim = true }
end

-- BMI1/BMI2/ADX/bsf/bsr/xadd/crc32 GPR instructions (common in clang
-- -march=native output): exact entries, like lzcnt/tzcnt/popcnt above. Only
-- gas-encodable spellings are listed (e.g. there are no 16-bit BMI forms —
-- shlxw/bextrw/pdepw do not exist — so those stay unknown and reject on any
-- memory operand). The trivial entries model first-reg-def/rest-use with the
-- access width from the destination GPR; xadd is RMW on BOTH explicit operands;
-- adcx/adox/crc32 are RMW-destination. mulx and cmpxchg are deliberately NOT
-- here: they have implicit rax/rdx effects and are pinned in x86_64_isa.classify.
for _, m in { "shlx", "sarx", "shrx", "andn", "bextr", "blsi", "blsr", "blsmsk",
	"bzhi", "pdep", "pext", "rorx" } do
	for _, s in { "", "l", "q" } do
		EXACT[m .. s] = { w = dstGprW, gpr = gprByPosition, claim = true }
	end
end
for _, m in { "bsf", "bsr" } do
	for _, s in { "", "w", "l", "q" } do
		EXACT[m .. s] = { w = dstGprW, gpr = gprByPosition, claim = true }
	end
end
for _, s in { "", "b", "w", "l", "q" } do
	EXACT["xadd" .. s] = { w = firstGprW, gpr = gprXadd, claim = true }
	EXACT["crc32" .. s] = { w = crc32W, gpr = gprRmwDst, claim = true }
end
for _, m in { "adcx", "adox" } do
	for _, s in { "", "l", "q" } do
		EXACT[m .. s] = { w = firstGprW, gpr = gprRmwDst, claim = true }
	end
end

-- pmovzx/pmovsx: memory source width = destination vector bytes / divisor.
local PMOV_DIV = { bw = 2, bd = 4, bq = 8, wd = 2, wq = 4, dq = 2 }

-- vpmovqb/qw/qd/db/dw/wb truncating stores (and the saturating vpmovs*/vpmovus*
-- families): memory width = source vector bytes / divisor (qb: 8 qwords -> 8 bytes
-- from a zmm, ...). Same divisors for all three families.
local PMOVTR_DIV = { qb = 8, qw = 4, qd = 2, db = 4, dw = 2, wb = 2 }

-- ---------------------------------------------------------------------------
-- AVX512 {k}-masked / AVX2 vector-masked memory access metadata
--
-- fp.maskedAccessInfo(mnem, ops) -> { elem=E, lanes=N, vecsize=V, contiguous,
-- extract } | nil describes the lane structure of a SUPPORTED masked memory
-- access (nil = masked forms of this mnemonic stay rejected):
--   E          = per-lane memory element bytes (the mask granularity)
--   N          = lane count of the ACCESS = number of mask bits used
--   V          = full memory footprint in bytes (= N*E; the fast-path width)
--   contiguous = expand/compress form: the accessed range is
--                [eff, eff + popcount(mask)*E) instead of the lane positions
--   extract    = "k" (an AVX512 {%kN} writemask, read with kmovw/kmovd/kmovq)
--                or "movmsk" (an AVX2 vmaskmov/vpmaskmov mask vector operand,
--                whose per-lane sign bits are read with vmovmskps/vmovmskpd)
--
-- The check algorithm (transform.luau's emitMaskedAccessCheck) is exactly what
-- the Fil-C compiler emits for the masked intrinsics (FilPizlonator's
-- lowerIntrinsicAccess): ValidObject (even for a zero mask), CanWrite for
-- stores, then the full-width fast path [lower, upper - V] refined by the mask
-- in the below/above slow paths (fixed-position: cttz/highest-set-bit lane
-- extents; contiguous: popcount extent).

-- {k}-masked vector moves: element size by mnemonic (the aligned 32/64 and ps/pd
-- forms keep their usual alignment check, clamped to 8 like the plain forms).
local MASKED_MOVE_ELEM = {
	vmovdqu8 = 1, vmovdqu16 = 2, vmovdqu32 = 4, vmovdqu64 = 8,
	vmovups = 4, vmovupd = 8,
	vmovdqa32 = 4, vmovdqa64 = 8, vmovaps = 4, vmovapd = 8,
}

-- vpmov truncating/saturating stores: memory element size by pair (the lane
-- count of the access is the memory footprint / element size).
local PMOVTR_ELEM = { qb = 1, qw = 2, qd = 4, db = 1, dw = 2, wb = 1 }

-- vexpand*/vpexpand* loads and vcompress*/vpcompress* stores (contiguous):
-- element size by suffix.
local EXPAND_ELEM = { b = 1, w = 2, d = 4, q = 8, ps = 4, pd = 8 }

-- Broadcast memory forms ({1toN} embedded broadcast is handled by the caller via
-- mem.bcastN): a broadcast load reads its one memory element/block
-- UNCONDITIONALLY — a {k} writemask gates only the register destination lanes
-- (verified against the SDM and the Fil-C compiler, which checks masked
-- broadcast sources as ordinary element-width accesses; on this dev hardware a
-- zero mask also suppresses the read, which only makes the unconditional check
-- conservative). These ride the plain access check even with a {k} decorator.
local BROADCAST_LOAD = {
	vpbroadcastb = true, vpbroadcastw = true, vpbroadcastd = true, vpbroadcastq = true,
	vbroadcastss = true, vbroadcastsd = true, vbroadcastf128 = true, vbroadcasti128 = true,
	vbroadcastf32x2 = true, vbroadcastf32x4 = true, vbroadcastf32x8 = true,
	vbroadcastf64x2 = true, vbroadcastf64x4 = true,
	vbroadcasti32x2 = true, vbroadcasti32x4 = true, vbroadcasti32x8 = true,
	vbroadcasti64x2 = true, vbroadcasti64x4 = true,
}
fp.BROADCAST_LOAD = BROADCAST_LOAD

function fp.maskedAccessInfo(mnem, ops)
	local V = widestVecBytes(ops)
	if not V then return nil end
	local E = MASKED_MOVE_ELEM[mnem]
	if E then
		return { elem = E, lanes = V // E, vecsize = V, contiguous = false, extract = "k" }
	end
	-- AVX2 vector-masked forms (inherently masked by the explicit mask vector
	-- operand): vmaskmovps/vpmaskmovd = dword lanes (vmovmskps),
	-- vmaskmovpd/vpmaskmovq = qword lanes (vmovmskpd).
	if mnem == "vmaskmovps" or mnem == "vpmaskmovd" then
		return { elem = 4, lanes = V // 4, vecsize = V, contiguous = false, extract = "movmsk" }
	end
	if mnem == "vmaskmovpd" or mnem == "vpmaskmovq" then
		return { elem = 8, lanes = V // 8, vecsize = V, contiguous = false,
			extract = "movmsk", movmskPd = true }
	end
	-- {k}-masked truncating/saturating vpmov stores.
	local tsat, tpair = string.match(mnem, "^v?pmov([su]*)([a-z][a-z])$")
	if tpair and (tsat == "" or tsat == "s" or tsat == "us") then
		local e = PMOVTR_ELEM[tpair]
		local div = PMOVTR_DIV[tpair]
		if e then
			local v = math.max(1, math.floor(V / div))
			return { elem = e, lanes = v // e, vecsize = v, contiguous = false, extract = "k" }
		end
	end
	-- {k}-masked expand loads / compress stores (contiguous).
	local suf = string.match(mnem, "^vexpand([bdwq])$") or string.match(mnem, "^vpexpand([bdwq])$")
		or string.match(mnem, "^vcompress([bdwq])$") or string.match(mnem, "^vpcompress([bdwq])$")
		or string.match(mnem, "^vexpand(p[sd])$") or string.match(mnem, "^vcompress(p[sd])$")
	if suf then
		local e = EXPAND_ELEM[suf]
		return { elem = e, lanes = V // e, vecsize = V, contiguous = true, extract = "k" }
	end
	-- {k}-masked ALU memory SOURCE (rsaz-avx512's `vpsubq .Lmask52x4(%rip),%ymm3,
	-- %ymm3{%k1}`): AVX-512 fault suppression reads the memory operand only for
	-- lanes whose writemask bit is set, at the instruction's natural element
	-- granularity — the same lane structure as a masked vector load, so the
	-- mask-aware check applies unchanged. Supported shapes: an EVEX integer
	-- (p-family) or packed-FP mnemonic with a trailing element letter
	-- (b/w/d/q/ps/pd — the granularity), the memory operand NOT in the
	-- destination position (EVEX ALU memory operands are always the last
	-- source), and no embedded broadcast (which reads one element
	-- unconditionally and rides the plain check). Anything else keeps the
	-- generic masked rejection.
	local mem = memOperandOf(ops)
	if mem and ops[1] ~= mem and not mem.bcastN then
		local elem
		if string.match(mnem, "^vp[%a]+$") or string.match(mnem, "^vperm[i]?[a-z0-9]*$") then
			local l = string.sub(mnem, -1)
			elem = ({ b = 1, w = 2, d = 4, q = 8 })[l]
		else
			local stem, psuf = string.match(mnem, "^v([%a][%w]-)(p[sd])$")
			if psuf and string.sub(stem, 1, 1) ~= "p" then elem = (psuf == "ps") and 4 or 8 end
		end
		if elem then
			return { elem = elem, lanes = V // elem, vecsize = V, contiguous = false, extract = "k" }
		end
	end
	return nil
end

-- ---------------------------------------------------------------------------
-- lookup: EXACT table + pattern rules

-- shared entry for the narrowing converts (see vCvtNarrowW): size-driven width.
local NARROW_ENTRY = { w = vCvtNarrowW, claim = true }

function fp.lookup(mnem)
	mnem = string.lower(mnem)
	local e = EXACT[mnem]
	if e then return e end

	-- narrowing converts (vcvtpd2ps & siblings + their AT&T x/y/z suffix
	-- spellings). Must precede the ss/sd/ps/pd suffix rule below ("vcvtpd2ps"
	-- would otherwise parse a bogus "ps" suffix) and the p-family pattern.
	if narrowFamily(mnem) then return NARROW_ENTRY end

	-- hard-reject families (patterned; also caught by the vector-index frame check)
	if string.sub(mnem, 1, 7) == "vgather" or string.sub(mnem, 1, 8) == "vpgather"
		or string.sub(mnem, 1, 8) == "vscatter" or string.sub(mnem, 1, 9) == "vpscatter" then
		return REJECT_GATHER
	end

	-- AMX tiles (tileloadd/tileloadd64/tileloaddt1/tilestored/tilestored64/
	-- tilestoredt1/tilezero/tilerelease + ldtilecfg): a tile load/store touches a
	-- strided, palette-configuration-dependent footprint (up to 16 rows x 64
	-- bytes) that cannot be bounds-checked as a single access.
	if string.sub(mnem, 1, 4) == "tile" or mnem == "ldtilecfg" then
		return { reject = REASON_AMX, claim = true }
	end

	-- lzcnt/tzcnt/popcnt (+ AT&T w/l/q size suffixes): GPR bit-scan/count ops
	-- whose memory source width is the destination GPR's byte width; the GPR
	-- operand is a def. (Must precede the p-family pattern: "popcnt" would
	-- otherwise parse as an MMX-class mnemonic.)
	if string.match(mnem, "^lzcnt[wlq]?$") or string.match(mnem, "^tzcnt[wlq]?$")
		or string.match(mnem, "^popcnt[wlq]?$") then
		return { w = dstGprW, gpr = gprByPosition, claim = true }
	end

	-- vexpand*/vcompress* (AVX512 expand/compress; the vpexpand*/vpcompress*
	-- spellings ride the p-family pattern below): with no {k} writemask these touch
	-- ALL addressed lanes — a contiguous full-width access — so the width is the
	-- vector width. {k}-masked forms are supported too: the transform emits the
	-- contiguous (popcount) mask-aware check (fp.maskedAccessInfo). (Must precede
	-- the ss/sd/ps/pd suffix rule: "vcompressd" would otherwise parse a bogus "sd"
	-- suffix at 8 bytes.)
	if string.match(mnem, "^vexpand[bdwq]$") or string.match(mnem, "^vcompress[bdwq]$") then
		return { w = "vec", claim = true }
	end

	-- vpermi2*/vpermt2* two-source permutes (vpermi2b/w/d/q/ps/pd, vpermt2b/w/d/q/
	-- ps/pd): the digits defeat the p-family and ss/sd/ps/pd suffix patterns; a
	-- full-vector-width access like the other full-vector permutes. (Must precede
	-- the ss/sd/ps/pd suffix rule so the digit forms get one consistent entry.)
	if string.match(mnem, "^vperm[it]2") then return { w = "vec", claim = true } end

	-- pcmpestri/pcmpistri/pcmpestrm/pcmpistrm: accepted by the p-family pattern below,
	-- but they have UNMODELED IMPLICIT GPR effects — the estri forms read physical
	-- rax/rdx (explicit string lengths) and pcmpestri/pcmpistri clobber rcx (the
	-- index result). The def/use model cannot express that, so regalloc could rename
	-- those registers and silently miscompile: reject. (No VEX forms exist, but
	-- strip a hypothetical v-prefix so such spellings get the same clean error.)
	do
		local e = mnem
		if string.sub(e, 1, 1) == "v" then e = string.sub(e, 2) end
		if e == "pcmpestri" or e == "pcmpistri" or e == "pcmpestrm" or e == "pcmpistrm" then
			return { reject = REASON_ESTRI, claim = true }
		end
	end

	-- pmovzx/pmovsx (also VEX): ^v?pmov[sz]x(bw|bd|bq|wd|wq|dq)$
	local px, pair = string.match(mnem, "^v?pmov([sz])x([a-z]+)$")
	if px then
		local div = PMOV_DIV[pair]
		if div then
			return { w = function(ops)
				local d = vecBytesOf(ops and ops[1]) or 16
				return math.max(1, math.floor(d / div))
			end, claim = true }
		end
		return nil
	end

	-- vpmovqb/qw/qd/db/dw/wb and the saturating vpmovs*/vpmovus* forms (EVEX-only):
	-- TRUNCATING stores — the memory destination is the source vector bytes divided
	-- by the element-ratio (qb: zmm -> 8 bytes, not the full 64 the p-family
	-- pattern would check). Takes precedence over that pattern.
	local tsat, tpair = string.match(mnem, "^v?pmov([su]*)([a-z][a-z])$")
	if tpair and (tsat == "" or tsat == "s" or tsat == "us") then
		local div = PMOVTR_DIV[tpair]
		if div then
			return { w = function(ops)
				local b = widestVecBytes(ops) or 16
				return math.max(1, math.floor(b / div))
			end, claim = true }
		end
	end

	-- VEX/EVEX alias of an EXACT entry (vaddps -> addps, ...): packed 16-byte forms
	-- widen to the operand vector width; aligned forms keep their alignment.
	if string.sub(mnem, 1, 1) == "v" and #mnem > 1 then
		local be = EXACT[string.sub(mnem, 2)]
		if be then
			local c = {}
			for k, v in be do c[k] = v end
			if c.w == 16 then c.w = "vec" end
			if c.align == 16 then c.align = "vec" end
			return c
		end
	end

	-- scalar/packed suffix rules: a (possibly v-prefixed) FP mnemonic ending in
	-- ss/sd/ps/pd (or the AVX512-FP16 packed suffix ph) whose stem starts with a
	-- letter. (v4fmaddps fails the stem rule: after stripping 'v' it starts with
	-- a digit, so it stays unknown -> rejected.)
	--
	-- p-STEM EXCLUSION: after the optional 'v', a stem starting with 'p' is NEVER
	-- a scalar/packed-FP mnemonic — the p-family is the integer vector family
	-- (pabsd, pmaxsd, pminsd, vpdpwusd[s], vpdpbsud[s], vpdpwsud[s], vpopcntd,
	-- ...), where a trailing "sd"/"ss"/"ps"/"pd" is part of the NAME, not a
	-- scalar/packed suffix. The scalar rule pinned those FULL-VECTOR accesses to
	-- 8 bytes (vpabsd (%rdi), %zmm0 reads 64 bytes but was checked at 8);
	-- excluding p-stems lets them fall through to the p-family pattern below
	-- (width = widest vector operand). Scalar FMA keeps its scalar width:
	-- vfmadd231sd's stem is "fmadd231" -> 8 bytes.
	--
	-- The AVX512-FP16 packed suffix ph resolves to "pspd" (= the full vector
	-- width, like ps/pd): vaddph/vmulph/vgetexpph/... are full-vector forms. The
	-- SCALAR FP16 suffix sh is deliberately NOT matched here: vaddsh & co read
	-- m16 (2 bytes), but a width from this rule would be wrong for the
	-- GPR-mixing scalar-half converts (vcvtusi2sh's integer source is m32/m64)
	-- and would leave vcvtsh2si's GPR destination unmodeled — so every sh form
	-- stays unknown and rejects cleanly on a memory operand instead.
	do
		local rest = mnem
		if string.sub(rest, 1, 1) == "v" then rest = string.sub(rest, 2) end
		if string.sub(rest, 1, 1) ~= "p" then
			local stem, suf = string.match(rest, "^([%a][%w]-)([ps][sdh])$")
			if stem and suf ~= "sh" then
				if suf == "ss" then return { w = 4, claim = false } end
				if suf == "sd" then return { w = 8, claim = false } end
				return { w = "pspd", claim = false }
			end
		end
	end

	-- MMX/SSE/AVX integer p-family (padd*, psub*, pand*, pcmpeq*, pshuf*, ...):
	-- width = widest vector operand (mm=8 covers all MMX forms).
	if string.match(mnem, "^v?p[%a]+$") then return { w = "vec", claim = false } end

	-- opmask register ops (kandw/korw/kxnorq/...): register-only (kmov is EXACT).
	if string.match(mnem, "^k[%a%d]+$") then return { memOk = false, claim = true } end

	-- remaining x87-shaped mnemonics: reg-only forms pass through; an Intel-sized
	-- memory form (fadd DWORD PTR [rdi]) gets its width from mem.size.
	if string.match(mnem, "^f[%a%d]*$") then
		return { w = x87MemW, claim = true }
	end

	return nil
end

-- ---------------------------------------------------------------------------
-- def/use modeling. Returns the same shape as x86.classify, or nil when the
-- mnemonic is not an FP/SIMD instruction. Raises clean sarcasm: errors for reject
-- entries, memOk=false with a memory operand, and UNSUPPORTED masked-{k} memory
-- accesses (the supported ones — vector moves, truncating stores,
-- expand/compress — are accepted here and get their mask-aware bounds check
-- from the transform; see fp.maskedAccessInfo).

function fp.classify(insn)
	local mnem = string.lower(insn.mnemonic)
	local ops = insn.operands or {}
	local raw = insn.raw or mnem
	local entry = fp.lookup(mnem)
	local mem = memOperandOf(ops)
	if entry then
		if entry.reject then
			error("sarcasm: " .. entry.reject .. ": " .. raw)
		end
		if not opsHaveFp(ops) and not entry.claim then
			return nil  -- not an FP use of a shared mnemonic (e.g. GPR `movq`)
		end
		if mem then
			if entry.memOk == false then
				error("sarcasm: `" .. mnem .. "` does not have a memory form"
					.. " (cannot prove memory safety): " .. raw)
			end
			local knum = nil
			for _, o in ops do
				if o.deco then
					local kn = string.match(o.deco, "{%%?k([0-7])}")
					if kn then knum = kn break end
				end
			end
			if knum then
				-- AVX512 {k}-masked memory access. A broadcast memory operand
				-- ({1toN} embedded broadcast or a vpbroadcast*/vbroadcast* form)
				-- reads its one element/block UNCONDITIONALLY — the writemask
				-- gates only the register destination lanes — so it rides the
				-- plain element-width access check. The supported masked
				-- moves/truncating-stores/expand/compress forms get the
				-- mask-aware bounds check from the transform. Everything else
				-- rejects (incl. masked ALU memory sources, whose lane/mask
				-- structure sarcasm does not model).
				if knum == "0" then
					error("sarcasm: {%k0} is not a valid writemask on a memory access"
						.. " (k0 reads as all-ones; gas rejects it too): " .. raw)
				end
				if not (mem.bcastN or BROADCAST_LOAD[mnem]) then
					local mi = fp.maskedAccessInfo(mnem, ops)
					if not mi or mi.extract ~= "k" then
						error("sarcasm: AVX512 mask-register ({k}) memory access on '" .. mnem
							.. "' is not supported (only the masked vector moves, truncating"
							.. " stores, and expand/compress forms are; masked-off lanes may"
							.. " not touch memory; cannot prove memory safety): " .. raw)
					end
				end
			end
			-- AVX2 vmaskmov/vpmaskmov shape validation: exactly three operands
			-- with the mask VECTOR at ops[2] (dest-first: dest, mask, mem /
			-- mem, mask, src). A malformed form would make the mask-aware
			-- check read a bogus mask operand (gas rejects it too, but reject
			-- it cleanly first).
			if mnem == "vmaskmovps" or mnem == "vmaskmovpd"
				or mnem == "vpmaskmovd" or mnem == "vpmaskmovq" then
				if #ops ~= 3 or not isXmmOp(ops[2]) then
					error("sarcasm: `" .. mnem .. "` requires the three-operand form"
						.. " (load: dest, mask, mem; store: mem, mask, src): " .. raw)
				end
			end
		end
		local defs, uses = {}, {}
		local g = entry.gpr
		if g == "dst" or g == "src" then
			for _, o in ops do
				if isGprOp(o) then table.insert(g == "dst" and defs or uses, o) end
			end
		elseif type(g) == "function" then
			g(ops, defs, uses)
		end
		if mem then
			if isGprOp(mem.base) then table.insert(uses, mem.base) end
			if isGprOp(mem.index) then table.insert(uses, mem.index) end
		end
		-- RMW registers: a GPR operand that is both a def and a use (xadd's two
		-- operands, the adcx/adox/crc32 destination) must have its use and def
		-- webs unioned — otherwise the operand renders as the use web's register
		-- while subsequent reads reach the def web's (a silent desync). (Also
		-- covers a memory base/index register aliasing the destination.)
		local rmw = {}
		local defNums = {}
		for _, o in defs do defNums[o.num] = true end
		for _, o in uses do
			if defNums[o.num] then table.insert(rmw, o.num) end
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true }
	end
	-- unknown mnemonic: an FP/vector/x87/VEX-shaped instruction with a memory
	-- operand whose width we cannot determine is unsound to check at any default
	-- width, so reject it. Register-only unknown forms pass through.
	if mem and (opsHaveFp(ops) or string.match(mnem, "^[fv]")) then
		error("sarcasm: cannot determine the memory access width of '" .. mnem
			.. "' (cannot prove memory safety): " .. raw)
	end
	return nil
end

-- ---------------------------------------------------------------------------
-- memory access width/alignment. Returns (w, align) or nil when there is no
-- memory operand or the width is unknowable. (noCheck entries return nil; the
-- codegen lowerSpecial hook passes them through unchecked.)
--
-- The width resolved here is AUTHORITATIVE: an Intel PTR size annotation
-- (mem.size) must never override it — the emitted instruction encodes the
-- ISA-fixed width regardless of the annotation (a `vmovdqu64 DWORD PTR [mem],
-- zmm0` still stores 64 bytes), so letting a lying annotation under-size the
-- result would under-check a heap access or under-size a materialized stack slot
-- (a stack-overflow-into-the-caller hole). Instead, a PTR size that CONTRADICTS
-- the resolved ISA-fixed width is rejected cleanly. Size-driven width functions
-- (SIZE_DRIVEN — the x87 PTR fallback and the cvtsi2ss/sd integer source) take
-- their width FROM the annotation by design and are exempt. `raw` (the source
-- text of the instruction) is used only for the rejection message.

function fp.accessWidth(mnem, ops, raw)
	ops = ops or {}
	local mem = memOperandOf(ops)
	if not mem then return nil end
	local function checkSize(w)
		if w and mem.size and mem.size ~= w then
			error("sarcasm: PTR size annotation (" .. tostring(mem.size)
				.. " bytes) contradicts the memory access width (" .. tostring(w)
				.. " bytes) of '" .. mnem .. "': " .. (raw or mnem))
		end
		return w
	end
	-- embedded broadcast {1toN}: the access is one element (all lanes share it).
	if mem.bcastN then
		local b = widestVecBytes(ops) or 16
		return checkSize(math.max(1, math.floor(b / mem.bcastN))), 1
	end
	local entry = fp.lookup(mnem)
	if not entry or entry.reject or entry.noCheck then return nil end
	if not opsHaveFp(ops) and not entry.claim then return nil end
	local w = entry.w
	local sizeDriven = false
	if type(w) == "function" then
		sizeDriven = SIZE_DRIVEN[w] or false
		w = w(ops, mem, mnem, raw)
	end
	if w == "vec" then w = widestVecBytes(ops) end
	if w == "pspd" then w = math.max(16, widestVecBytes(ops) or 16) end
	if not w then return nil end
	if not sizeDriven then checkSize(w) end
	local a = entry.align
	if a == "vec" then a = widestVecBytes(ops) end
	return w, (a or 1)
end

-- ---------------------------------------------------------------------------
-- Vector-register effects for the width-aware FP save/restore expansion
-- (x86_64_codegen.expandFpSaves). fp.vecEffects(stmt) returns two maps keyed by
-- vector register NUMBER (xmm/ymm/zmm alias by number, 0-31):
--
--   uses: num -> read width in bytes (4/8/16/32/64): the instruction reads the
--         register's value over [0, w).
--   defs: num -> "provided width" in bytes: the instruction PROVIDES values
--         (written, or architecturally zeroed) over [0, w); bits ABOVE w are
--         preserved (flow through from the previous value).
--
-- Correctness contract (the save/restore can only be wrong by UNDER-saving):
-- it is ALWAYS safe to over-approximate a use width, and always safe to report
-- providedWidth 0 (everything flows through); it is NEVER safe to
-- under-approximate a read width or to overstate a provided width. The default
-- for anything unrecognized is exactly that safe direction: no defs (all
-- liveness flows through the destination), every vector operand used at its
-- full register-class width (xmm 16 / ymm 32 / zmm 64 — a read can never
-- exceed the class width).
--
-- k0-7 (opmask), mmN (MMX) and st(N) (x87) operands are rc="xmm" too, but they
-- are not vector-save state (an SSE2-only runtime can clobber none of them):
-- they are IGNORED here (never crash, never recorded).

-- xmm/ymm/zmm operand -> (register number, register-class width in bytes);
-- nil for k/mm/st and non-register operands.
local function vecRegOf(o)
	if not isXmmOp(o) then return nil end
	local n = o.name or ""
	local p3 = string.sub(n, 1, 3)
	if p3 == "xmm" then return o.num, 16 end
	if p3 == "ymm" then return o.num, 32 end
	if p3 == "zmm" then return o.num, 64 end
	return nil  -- mmN / kN / st(N)
end

-- FP scalar/packed suffix, mirroring fp.lookup's rule: after an optional 'v',
-- the stem must start with a non-'p' letter (vpabsd/vpdpwssd & co are
-- packed-INTEGER names whose trailing sd/ss is part of the NAME, not a scalar
-- suffix). Returns ("scalar", 4|8) for ss/sd, ("packed", nil) for ps/pd/ph,
-- or nil (incl. FP16 'sh', whose forms stay on the safe default).
local function fpSuffixKind(mnem)
	local rest = mnem
	if string.sub(rest, 1, 1) == "v" then rest = string.sub(rest, 2) end
	if string.sub(rest, 1, 1) == "p" then return nil end
	local stem, suf = string.match(rest, "^([%a][%w]-)([ps][sdh])$")
	if not stem or suf == "sh" then return nil end
	if suf == "ss" then return "scalar", 4 end
	if suf == "sd" then return "scalar", 8 end
	return "packed", nil
end

-- Read-only instructions whose FIRST operand is a vector register: no
-- mnemonic-family rule below may mistake ops[1] for a def (that would kill
-- liveness flowing THROUGH the register — an under-save). Read-only forms
-- with a GPR/k/memory first operand need no entry: no vector-def candidate
-- exists for them anyway.
local READONLY_VEC_FIRST = {
	comiss = true, comisd = true, ucomiss = true, ucomisd = true,
	testps = true, testpd = true, ptest = true,
	vcomiss = true, vcomisd = true, vucomiss = true, vucomisd = true,
	vtestps = true, vtestpd = true, vptest = true,
}

-- Legacy full-vector PURE moves: dest[0,16) = f(src), dest[16,64) preserved,
-- NO read of the old destination. Every other legacy packed form is modeled
-- as an RMW (dest use + providedWidth 16) — the sound direction.
local LEGACY_PURE_MOVE = {
	movups = true, movupd = true, movaps = true, movapd = true,
	movdqa = true, movdqu = true, movntdq = true, movntdqa = true,
	movntps = true, movntpd = true, lddqu = true,
	movsldup = true, movshdup = true, movddup = true,
}

-- Legacy packed instructions that provide only PART of [0,16) (they preserve
-- bytes BELOW their written lanes): claiming a 16-byte providedWidth would
-- under-save the preserved bytes, so they ride the safe default instead.
local LEGACY_PARTIAL = {
	movhps = true, movhpd = true, movlps = true, movlpd = true,  -- write [8,16) or [0,8), preserve the rest
	insertps = true, blendps = true, blendpd = true,             -- per-lane merge with old dest
	pinsrb = true, pinsrw = true, pinsrd = true, pinsrq = true,  -- single-element merge
}

-- Instructions with an implicit %xmm0 operand the operand list never names
-- (the blend mask; the SHA256/SHA512 state input). Missing it would under-save
-- xmm0 around an injected runtime call. (vblendv* name their mask explicitly.)
local IMPLICIT_XMM0_USE = { blendvps = true, blendvpd = true, sha256rnds2 = true,
	vsha512rnds2 = true }

-- Scalar->GPR converts read only the low 4/8 bytes of their vector source
-- (without this the default full-class use would widen every upstream save).
local CVTSI_READ = {}
for _, m in { "cvttss2si", "cvtss2si", "vcvttss2si", "vcvtss2si",
	"vcvttss2usi", "vcvtss2usi" } do
	CVTSI_READ[m] = 4; CVTSI_READ[m .. "l"] = 4; CVTSI_READ[m .. "q"] = 4
end
for _, m in { "cvttsd2si", "cvtsd2si", "vcvttsd2si", "vcvtsd2si",
	"vcvttsd2usi", "vcvtsd2usi" } do
	CVTSI_READ[m] = 8; CVTSI_READ[m .. "l"] = 8; CVTSI_READ[m .. "q"] = 8
end

-- Legacy full-vector converts whose mnemonic escapes the ps/pd suffix rule
-- (the dq families): dest is a full 16 bytes of converted/zeroed lanes.
local LEGACY_CVT_FULL = {
	cvtdq2ps = true, cvtps2dq = true, cvttps2dq = true,
	cvtpd2dq = true, cvttpd2dq = true,
}

-- Self-zeroing idioms (pxor/xorps/xorpd/psub* with both operands the SAME
-- register): a pure def — the RMW model below would otherwise keep the
-- register spuriously live upward of the zeroing.
local SELF_ZERO = { pxor = true, xorps = true, xorpd = true,
	psubb = true, psubw = true, psubd = true, psubq = true }

function fp.vecEffects(stmt)
	local mnem = string.lower(stmt.mnemonic)
	local ops = stmt.operands or {}
	local uses, defs = {}, {}

	local function addUse(num, w)
		if num and (uses[num] or 0) < w then uses[num] = w end
	end
	local function addDef(num, w)
		if num and (defs[num] or 0) < w then defs[num] = w end
	end

	-- Specials first: these may have NO vector register operand at all, so
	-- they must precede the no-vector fast path.
	if mnem == "vzeroupper" then
		-- Zeroes [16,64) of zmm0-15 (only [16,32) on non-AVX512 hardware, but
		-- no 64-byte read can happen there); the low 128 bits of every
		-- register are UNCHANGED. Model the preserved low half as a 16-byte
		-- use so that liveness flows through — a bare providedWidth=64 "kill"
		-- would under-save it (e.g. an xmm value intentionally carried past a
		-- pre-call vzeroupper).
		for i = 0, 15 do addUse(i, 16); addDef(i, 64) end
		return uses, defs
	end
	if mnem == "vzeroall" then
		for i = 0, 15 do addDef(i, 64) end  -- zeroes each register in full: kill
		return uses, defs
	end
	if mnem == "fxsave" or mnem == "fxsave64" then
		for i = 0, 15 do addUse(i, 16) end   -- reads all of xmm0-15 (mm/st ignored)
		return uses, defs
	end
	if mnem == "fxrstor" or mnem == "fxrstor64" then
		for i = 0, 15 do addDef(i, 16) end   -- restores all of xmm0-15
		return uses, defs
	end

	-- Collect the vector (xmm/ymm/zmm) register operands with roles: ops[1]
	-- is the destination position (parse normalizes dest-first), ops[2..]
	-- the sources.
	local dNum, dClass = vecRegOf(ops[1])
	local srcs = {}
	for i = 2, #ops do
		local n, c = vecRegOf(ops[i])
		if n then table.insert(srcs, { num = n, class = c }) end
	end
	if not dNum and #srcs == 0 then
		-- no vector state touched (GPR-only, k/mm/st-only, or memory-only)
		return uses, defs
	end

	-- The safe default: no defs (ALL liveness flows through the destination),
	-- every vector operand used at its full class width. Never under-saves.
	local function defaultEffects()
		addUse(dNum, dClass)
		for _, s in srcs do addUse(s.num, s.class) end
		return uses, defs
	end

	-- Implicit %xmm0 uses (blend mask / SHA state) — recorded regardless of
	-- the path taken below.
	if IMPLICIT_XMM0_USE[mnem] then addUse(0, 16) end

	local sfx, sBytes = fpSuffixKind(mnem)   -- "scalar" (4/8) | "packed" | nil

	-- Scalar->GPR converts read only the low element of the vector source.
	local siRead = CVTSI_READ[mnem]
	if siRead then
		for _, s in srcs do addUse(s.num, siRead) end
		return uses, defs
	end

	-- Memory-destination (store) and read-only-first-operand forms have no
	-- vector def. Stores read the source at the access width (scalar stores:
	-- the low 4/8 bytes); read-only register forms use both operands.
	local mem = memOperandOf(ops)
	local isStore = (mem ~= nil) and (ops[1] == mem)
	if isStore or READONLY_VEC_FIRST[mnem] then
		if sfx == "scalar" then
			addUse(dNum, sBytes)   -- nil-safe: a store has no vector first operand
			for _, s in srcs do addUse(s.num, sBytes) end
			return uses, defs
		end
		return defaultEffects()
	end

	-- VEX/EVEX forms zero all bits above the written state up to MAXVL, so a
	-- vector destination provides the full 64 bytes.
	if string.sub(mnem, 1, 1) == "v" then
		-- vmovd/vmovq: xmm<->GPR moves. GPR/mem -> xmm zeroes [w,64); the
		-- xmm->GPR form reads the low 4/8 bytes (vmovq xmm,xmm reads 8).
		if mnem == "vmovd" or mnem == "vmovq" then
			local w = (mnem == "vmovd") and 4 or 8
			if dNum then addDef(dNum, 64) end
			for _, s in srcs do addUse(s.num, w) end
			return uses, defs
		end
		if not dNum then
			-- k-register destination (vpcmp*, ...) or GPR destination.
			return defaultEffects()
		end
		addDef(dNum, 64)
		-- Accumulate-style forms READ the destination (a bare 64-byte def
		-- would kill that liveness — an under-save): FMA (vfmadd*/vfmsub*/
		-- vfnmadd*/vfnmsub*, incl. the FP16 complex families), VNNI
		-- dot-product accumulate (vpdp* and the 4-iteration vp4dp*), 4VNNIW
		-- v4fmadd*/v4fnmadd*, IFMA 52-bit multiply-accumulate (vpmadd52*),
		-- ternary logic (vpternlog*), VBMI2 funnel shifts (vpshld*/vpshrd*),
		-- two-source permutes whose destination supplies indices or a table
		-- half (vpermi2*/vpermt2*), vfixupimm*, and XOP multiply-accumulate
		-- (vpmacs*).
		local readsDest = string.match(mnem, "^vpdp") or string.match(mnem, "^vpternlog")
			or string.match(mnem, "^vpsh[lr]d") or string.match(mnem, "^vfixupimm")
			or string.match(mnem, "^vperm[it]2") or string.match(mnem, "^vpmadd52")
			or string.match(mnem, "^vp4dp") or string.match(mnem, "^v4fmadd")
			or string.match(mnem, "^v4fnmadd") or string.match(mnem, "^vpmac")
			or (string.match(mnem, "^vf")
				and (string.find(mnem, "madd", 1, true) or string.find(mnem, "msub", 1, true)))
		if readsDest then addUse(dNum, dClass) end
		-- EVEX merge-masking ({k} WITHOUT {z}): masked lanes keep the OLD
		-- destination value — a full-class destination use (the merge).
		local deco = ops[1].deco
		if deco and string.match(deco, "{%%k%d") and not string.find(deco, "{z}", 1, true) then
			addUse(dNum, dClass)
		end
		if sfx == "scalar" then
			-- 3-operand scalar (dest, src1, src2): dest[0,s) = f(src1, src2),
			-- dest[s,16) = src1[s,16), dest[16,64) = 0. src1 merges at 16;
			-- src2 is read at the SOURCE element width (the cvt pair swaps).
			local src2W = sBytes
			local m2 = string.sub(mnem, 2)
			if m2 == "cvtsd2ss" then src2W = 8 elseif m2 == "cvtss2sd" then src2W = 4 end
			if srcs[1] then addUse(srcs[1].num, 16) end
			if srcs[2] then addUse(srcs[2].num, src2W) end
		else
			for _, s in srcs do addUse(s.num, s.class) end
		end
		return uses, defs
	end

	-- Legacy (non-VEX) forms never touch bits [16,64).
	-- movd/movq: GPR/mem -> xmm writes 4/8 bytes AND zeroes the rest of the
	-- 16-byte register (movq xmm,xmm also zeroes [8,16)); xmm -> GPR reads
	-- the low 4/8 bytes.
	if mnem == "movd" or mnem == "movq" then
		local w = (mnem == "movd") and 4 or 8
		if dNum then addDef(dNum, 16) end
		for _, s in srcs do addUse(s.num, w) end
		return uses, defs
	end

	-- cvtsi2ss/cvtsi2sd (legacy): dest[0,s) = cvt(int source); dest[s,16) is
	-- the OLD destination (flows through — not a merge READ of the result
	-- bytes); [16,64) preserved. (The GPR/mem integer source is not a vector
	-- use.)
	local cvtIntW2 = ({ cvtsi2ss = 4, cvtsi2ssl = 4, cvtsi2ssq = 4,
		cvtsi2sd = 8, cvtsi2sdl = 8, cvtsi2sdq = 8 })[mnem]
	if cvtIntW2 then
		addDef(dNum, cvtIntW2)
		return uses, defs
	end

	if sfx == "scalar" then
		if mnem == "movss" or mnem == "movsd" then
			if mem then
				-- scalar LOAD: writes [0,s) AND zeroes [s,16).
				addDef(dNum, 16)
			else
				-- reg-reg move: dest[0,s) = src[0,s); dest[s,16) preserved
				-- (it flows through the s-byte providedWidth).
				addDef(dNum, sBytes)
				for _, s in srcs do addUse(s.num, sBytes) end
			end
			return uses, defs
		end
		if mnem == "cvtss2sd" or mnem == "cvtsd2ss" then
			-- dest[0,s2) = cvt(src); dest[s2,16) = OLD dest (flow-through).
			local sDst = (mnem == "cvtss2sd") and 8 or 4
			local sSrc = (mnem == "cvtss2sd") and 4 or 8
			addDef(dNum, sDst)
			for _, s in srcs do addUse(s.num, sSrc) end
			return uses, defs
		end
		-- Generic legacy scalar ALU (addss/addsd/...; also the unary
		-- rcpss/sqrtss-class forms): dest[0,s) = f(dest[0,s), src[0,s));
		-- dest[s,16) preserved. Model as an RMW: dest use @s + providedWidth
		-- s. (Unary forms do not truly read dest[0,s); the extra use is the
		-- safe direction.)
		addDef(dNum, sBytes)
		addUse(dNum, sBytes)
		for _, s in srcs do addUse(s.num, sBytes) end
		return uses, defs
	end

	-- Legacy packed / full-vector forms.
	if LEGACY_PARTIAL[mnem] then
		return defaultEffects()
	end
	if LEGACY_PURE_MOVE[mnem] then
		addDef(dNum, 16)
		for _, s in srcs do addUse(s.num, s.class) end
		return uses, defs
	end
	-- Self-zeroing idiom (pxor %xmmN,%xmmN & co): a pure def.
	if SELF_ZERO[mnem] and dNum and srcs[1] and srcs[1].num == dNum and #srcs == 1 then
		addDef(dNum, 16)
		return uses, defs
	end
	if sfx == "packed" or LEGACY_CVT_FULL[mnem] or string.match(mnem, "^p[%a]+$") then
		-- Full-vector RMW (the conservative shape for every legacy packed form
		-- that reaches here): the destination reads+writes [0,16); sources are
		-- read at their class width.
		addDef(dNum, 16)
		addUse(dNum, 16)
		for _, s in srcs do addUse(s.num, s.class) end
		return uses, defs
	end

	-- Anything unrecognized: the safe default.
	return defaultEffects()
end

-- ---------------------------------------------------------------------------
-- unsafe-instruction rejection (consulted for EVERY mnemonic, FP or not, at the
-- top of x86.classify before baseMnemonic mapping, so e.g. `lodsq` is seen
-- before anything could normalize it). Returns a reason string or nil.

local REASON_SYSCALL = "system-call instructions cannot be bounds-checked (their effect on memory safety is unknowable)"
local REASON_PORTIO = "port-I/O instructions cannot be bounds-checked (they transfer data the checker cannot see)"
local REASON_PRIV = "privileged instructions are not supported (cannot prove memory safety)"
local REASON_STRING = "string instructions use implicit rsi/rdi memory operands that cannot be bounds-checked (cannot prove memory safety)"
local REASON_PREFIX = "instruction prefixes (rep/lock/xacquire/xrelease) are not supported (the prefixed instruction's memory effects cannot be modeled; cannot prove memory safety)"
local REASON_INT = "software interrupt `int N` is not supported for N != 3 (only int3/`int $3`, which raises SIGTRAP, is allowed)"
local REASON_CRDR = "control/debug register moves are not supported (privileged processor state; cannot prove memory safety)"
local REASON_CLZERO = "clzero stores to an implicit memory operand (the 64-byte cache line addressed by rax) that cannot be bounds-checked (cannot prove memory safety)"
local REASON_XLAT = "xlatb loads from an implicit memory operand (the byte at rbx + al) that cannot be bounds-checked (cannot prove memory safety)"
local REASON_FARCTL = "far control-transfer instructions (lcall/ljmp) have implicit stack memory effects (a far return-frame push) that cannot be modeled (cannot prove memory safety)"
local REASON_FSGSBASE = "FSGSBASE instructions (rdfsbase/rdgsbase/wrfsbase/wrgsbase) read or write the fs/gs thread-pointer base registers, which the Fil-C runtime owns (silently corrupting fs.base/gs.base breaks thread-local storage for the whole thread; cannot prove memory safety)"
local REASON_SEGWRITE = "writes to a segment register (es/cs/ss/ds/fs/gs) change processor state the checker cannot model (a bad selector raises an unconverted hardware fault; cannot prove memory safety)"
local REASON_SEGXFER = "push/pop of a segment register is not supported (it transfers the segment selector, which the checker does not model)"
local REASON_SWAPGS = "swapgs swaps the gs.base thread-pointer base, which the Fil-C runtime owns (silently corrupting it breaks thread-local state; cannot prove memory safety)"
local REASON_TSX = "TSX transactional-memory instructions (xbegin/xend/xabort) have transactional control flow and memory semantics the checker cannot model (an xabort/xbegin fallback path is unmodeled control flow; cannot prove memory safety)"
local REASON_SETSSBSY = "setssbsy operates on the CET shadow stack through the implicit shadow-stack pointer (memory the checker cannot see or bounds-check; cannot prove memory safety)"
local REASON_SGX = "SGX enclave instructions (encls/enclu/enclv) have implicit eax/rbx/rcx operands and enclave-controlled memory effects the checker cannot model (cannot prove memory safety)"
local REASON_NOTRACK = "the CET `notrack` prefix is not supported (it changes indirect-branch tracking for the instruction that follows, which the checker does not model; cannot prove memory safety)"
local REASON_LOOPZF = "loop's ZF-consuming form branches on the flags left by the PRECEDING instruction, and sarcasm's injected memory-access checks clobber EFLAGS (the documented known issue), so the pending condition could be silently polluted (modeling it cannot be proven sound; use plain `loop`, which branches on the decremented counter instead)"
local REASON_ADDRSIZE = "an address-size prefix changes the addressing mode of the instruction that follows, which sarcasm does not model (the size-suffixed spelling is supported where one exists, e.g. loopl for the 0x67-address-size `loop`)"

local REASON_FLAGREAD = "flag-reading/writing instructions with unmodeled EFLAGS effects cannot be proven sound against sarcasm's injected flag-clobbering checks (the flag-liveness model has no entry for this mnemonic, so a default-dead assumption would silently omit a required save/restore)"
local REASON_BCD = "BCD arithmetic instructions (daa/das/aaa/aas/aam/aad) read and write AF/CF with unmodeled EFLAGS effects that sarcasm's injected flag-clobbering checks could silently pollute (cannot prove memory safety)"

-- Instructions with implicit GPR operands the def/use model cannot express (same
-- class as pcmpestri/pcmpistri, which the fp table rejects): regalloc could rename
-- the registers they secretly read/write and silently miscompile. (Exact-key table:
-- `rdtsc` matches only "rdtsc", never a legit look-alike mnemonic.)
local function implicitGprReason(mnem, list)
	return mnem .. " has implicit GPR operands (" .. list
		.. ") that are not modeled (cannot prove memory safety)"
end

-- push/pop of a segment register (exact mnemonic set — `popcnt` must NOT match a
-- "^pop" prefix here): a selector TRANSFER, not a write the write-check could name.
local SEG_PUSHPOP = {
	push = true, pushw = true, pushl = true, pushq = true,
	pop = true, popw = true, popl = true, popq = true,
}

local UNSAFE = {
	syscall = REASON_SYSCALL, sysenter = REASON_SYSCALL,
	sysexit = REASON_SYSCALL, sysret = REASON_SYSCALL,
	["in"] = REASON_PORTIO, ins = REASON_PORTIO,
	inb = REASON_PORTIO, inw = REASON_PORTIO, inl = REASON_PORTIO,
	insb = REASON_PORTIO, insw = REASON_PORTIO, insl = REASON_PORTIO, insd = REASON_PORTIO,
	out = REASON_PORTIO, outs = REASON_PORTIO,
	outb = REASON_PORTIO, outw = REASON_PORTIO, outl = REASON_PORTIO,
	outsb = REASON_PORTIO, outsw = REASON_PORTIO, outsl = REASON_PORTIO, outsd = REASON_PORTIO,
	hlt = REASON_PRIV, wrmsr = REASON_PRIV, rdmsr = REASON_PRIV,
	-- privileged descriptor-table/machine-status-word loads (#GP at CPL > 0);
	-- sgdt/sidt/sldt/str/smsw are unprivileged and supported (EXACT widths above).
	lgdt = REASON_PRIV, lidt = REASON_PRIV, lldt = REASON_PRIV,
	ltr = REASON_PRIV, lmsw = REASON_PRIV,
	into = REASON_INT, int1 = REASON_INT,
	-- implicit memory operands the checker cannot see (same class as the string
	-- ops): clzero writes the 64-byte line at rax, xlatb reads rbx + al.
	clzero = REASON_CLZERO,
	xlat = REASON_XLAT, xlatb = REASON_XLAT,
	-- far call/jump: implicit far return-frame stack push (and a code-segment
	-- change) sarcasm cannot model — even the immediate form touches the stack.
	lcall = REASON_FARCTL, ljmp = REASON_FARCTL,
	-- implicit GPR reads/writes the def/use model cannot express: umwait/tpause
	-- read the implicit EDX:EAX TSC-deadline pair (the explicit r32 is only a
	-- control hint; SDM Vol. 2B — "UMWAIT r32, <edx>, <eax>", "TPAUSE r32,
	-- <edx>, <eax>"). (cpuid/rdtsc/rdtscp/xgetbv/rdpkru/wrpkru/monitor/mwait
	-- were once in this class too; their implicit GPR operands are now modeled
	-- exactly — see x86_64_isa.classify's implicit-only pinned branch.)
	umwait = implicitGprReason("umwait", "rax/rdx"),
	tpause = implicitGprReason("tpause", "rax/rdx"),
	-- FSGSBASE (the thread-pointer BASE registers, distinct from the selectors):
	-- wrfsbase with a canonical value silently replaces fs.base (no fault, TLS
	-- just breaks), wrgsbase does the same to gs.base, and the rd forms leak the
	-- thread-pointer bases the runtime owns.
	wrfsbase = REASON_FSGSBASE, wrgsbase = REASON_FSGSBASE,
	rdfsbase = REASON_FSGSBASE, rdgsbase = REASON_FSGSBASE,
	-- gs.base swap: the runtime owns gs.base (the myth/thread register).
	swapgs = REASON_SWAPGS,
	-- TSX: xbegin (both the label form and the no-operand form — the label is a
	-- fallback CODE target, never an absolute memory operand, so it must be
	-- rejected here before the operand classification), xend, xabort.
	xbegin = REASON_TSX, xend = REASON_TSX, xabort = REASON_TSX,
	-- CET shadow-stack busy-setter: implicit SSP memory effect (see REASON_SETSSBSY).
	setssbsy = REASON_SETSSBSY,
	-- SGX: implicit eax/rbx/rcx leaf/pointer operands plus enclave memory effects.
	encls = REASON_SGX, enclu = REASON_SGX, enclv = REASON_SGX,
	-- AMD SVM launch: privileged (like hlt/wrmsr) and rewrites EFER/CR0 state.
	skinit = REASON_PRIV,
	-- CET `notrack` prefix (parsed as a mnemonic with the real instruction as its
	-- operand): reject with its own message rather than the misleading
	-- "symbolic address" error the bare-symbol operand used to produce.
	notrack = REASON_NOTRACK,
	-- umonitor has no implicit GPR (its address is the explicit operand), but it
	-- arms address-monitoring hardware on a cache-line-sized memory range the
	-- checker cannot see or bounds-check — and unlike monitor (whose address is
	-- the implicit rax operand, modeled in classify) a faulting umonitor range
	-- cannot be attributed to a checked access.
	umonitor = "umonitor arms address-monitoring hardware on a memory range whose extent cannot be bounds-checked (cannot prove memory safety)",
	-- loop's ZF-consuming variants: see REASON_LOOPZF (plain `loop` branches on the
	-- counter register and IS supported — classified as a conditional branch with an
	-- implicit rcx use/def in x86_64_isa.classify).
	loope = REASON_LOOPZF, loopz = REASON_LOOPZF,
	loopne = REASON_LOOPZF, loopnz = REASON_LOOPZF,
	-- The 16-bit-counter jcxz is not encodable in 64-bit mode (gas: "`jcxz' is not
	-- supported in 64-bit mode"); its siblings jrcxz/jecxz are supported and modeled.
	jcxz = "jcxz is not supported in 64-bit mode (the 16-bit-counter form is not encodable; use jrcxz, or jecxz for the 32-bit counter)",
	-- Address-size prefixes (the explicit-prefix spelling of the 0x67 forms): the
	-- prefixed instruction itself is not modeled. The size-suffixed spellings
	-- (loopl/loopq, jecxz/jrcxz) are the supported way to write those forms.
	addr32 = REASON_ADDRSIZE, addr16 = REASON_ADDRSIZE,
	-- Flag-stack transfers: pushf/pushfq/pushfd READ EFLAGS (a pending
	-- condition from a preceding flag-setter would be silently polluted by an
	-- injected check) and popf/popfq/popfd WRITE EFLAGS with values the model
	-- does not track. Neither is in the flag-liveness tables, so both stay
	-- rejected rather than silently passing with a dead default.
	pushf = REASON_FLAGREAD, pushfq = REASON_FLAGREAD, pushfd = REASON_FLAGREAD,
	pushfw = REASON_FLAGREAD,
	popf = REASON_FLAGREAD, popfq = REASON_FLAGREAD, popfd = REASON_FLAGREAD,
	popfw = REASON_FLAGREAD,
	-- salc (set AL from carry) reads CF with no entry in the flag-use tables.
	salc = REASON_FLAGREAD,
	-- BCD arithmetic: AF/CF readers+writers with unmodeled flag effects.
	daa = REASON_BCD, das = REASON_BCD, aaa = REASON_BCD, aas = REASON_BCD,
	aam = REASON_BCD, aad = REASON_BCD,
	-- arpl reads/writes the RPL bits with ZF effects the model does not track.
	arpl = REASON_FLAGREAD,
}

-- string ops: implicit rsi/rdi memory the checker cannot see. Only the
-- read/write-compare shapes stay rejected here (lods/scas/cmps in every
-- size, plus the zero-operand movsd/cmpsd ambiguity — movsd/cmpsd are FP
-- instructions when they have operands, so those reject only in their
-- zero-operand string form). The movs/stos family (rep-prefixed or bare
-- single-step) is modeled by x86_64_isa.classify's stringop branch and
-- lowered with capability range checks by the transform.
local STRINGOPS = {
	lods = true, lodsb = true, lodsw = true, lodsl = true, lodsq = true, lodsd = true,
	scas = true, scasb = true, scasw = true, scasl = true, scasq = true, scasd = true,
	cmps = true, cmpsb = true, cmpsw = true, cmpsl = true, cmpsq = true,
}
local STRINGOPS_ZERO_OPERAND = { movsd = true, cmpsd = true }

-- prefixes that still reach here as mnemonics. `lock` is parsed as a prefix
-- TAG by the parser (x86_64_parse) and validated in x86_64_isa.classify — a
-- "lock" mnemonic only survives here when the input stacks prefixes
-- (`lock lock xadd`), which stays rejected. `rep`/`repe`/`repne`/`repz`/
-- `repnz` are likewise parsed as a repPrefix tag (x86_64_parse) and validated
-- in x86_64_isa.classify — rep movs*/stos* lower to checked copies/fills,
-- `rep ret` keeps its branch-prediction hint.
local PREFIXES = {
	lock = true, xacquire = true, xrelease = true,
}

function fp.unsafeReject(mnem, insn)
	mnem = string.lower(mnem)
	local reason = UNSAFE[mnem]
	if reason then return reason end
	if STRINGOPS[mnem] then return REASON_STRING end
	if PREFIXES[mnem] then return REASON_PREFIX end
	if STRINGOPS_ZERO_OPERAND[mnem] and #(insn.operands or {}) == 0 then return REASON_STRING end
	-- Writes INTO a segment register. The parser now classifies %es/%cs/%ss/%ds/%fs/%gs
	-- as segment-register operands (rc="seg"), so the destination of `movw %ax, %fs`,
	-- `mov %ax, %fs`, `movq %rax, %fs` (and every other segment register) is visible
	-- here instead of parsing as a bare symbol that passed through raw. The
	-- destination-first operand model puts the destination in position 1 for every
	-- instruction that can write a segment register (mov/pop), so this is precise:
	-- reads (`movl %fs, %eax`) keep operands[2] and pass through — the selector read
	-- defines the destination web with the selector's value, which is sound (the
	-- emitted instruction writes the web's allocated register). Segment-QUALIFIED
	-- memory operands (`movq %fs:0x28, %rax`) never parse as a segment register (the
	-- token is "fs:0x28") and keep their existing symbolic-displacement rejection.
	--
	-- push/pop of a segment register gets its OWN message: a push only READS the
	-- selector (the old routing misreported it as a write), and a pop's selector load
	-- rides the stack push/pop machinery. Modeling either through the frame machinery
	-- is not worth it — the selector transfer is what is unmodeled — so both keep this
	-- rejection while genuine selector WRITES (mov to a segment register, the only
	-- other instruction that can write one) keep the write-specific message above.
	local dest = (insn.operands or {})[1]
	if dest and dest.kind == "reg" and dest.rc == "seg" then
		if SEG_PUSHPOP[mnem] then return REASON_SEGXFER end
		return REASON_SEGWRITE
	end
	if mnem == "int" then
		local ops = insn.operands or {}
		local v = ops[1] and ops[1].kind == "imm" and (ops[1].value or tonumber(ops[1].text)) or nil
		if v == 3 then return nil end  -- int $3 == int3: SIGTRAP is fine
		return REASON_INT
	end
	-- mov to/from %crN/%drN: the parser leaves cr3/dr7 etc. as bare symbols (keeping
	-- a leading `%` in AT&T syntax).
	if mnem == "mov" or mnem == "movq" or mnem == "movl" then
		for _, o in (insn.operands or {}) do
			if o.kind == "sym" and string.match(string.lower(o.text), "^%%?[cd]r[0-9]+$") then
				return REASON_CRDR
			end
		end
	end
	return nil
end

return fp
