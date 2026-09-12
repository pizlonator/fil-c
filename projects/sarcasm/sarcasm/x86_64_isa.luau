--!nonstrict
-- x86_64 instruction semantics: classify a parsed (dest-first, syntax-normalized)
-- instruction into register defs/uses and control flow. Operands come from x86parse
-- (already normalized to destination-first, so ops[1] is the destination).

local fp = require("./x86_64_fp")
local sig = require("./sig")

local x86 = {}

-- Normalize a mnemonic to its base operation (strip AT&T size suffix; unify movz*/movs*).
local BASE = {}
for _, m in { "mov","add","sub","and","or","xor","sal","shl","sar","shr","cmp","test",
	"lea","imul","inc","dec","neg","not","push","pop","adc","sbb","mul","idiv","div","ror","rol" } do
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
	-- the rest of the sign-extension family (single-reg forms behave like cltq;
	-- the dx-writing forms are pinned implicit-GPR instructions, see classify)
	cbw = "cbw", cbtw = "cbw", cwde = "cwde", cwtl = "cwde",
	cwd = "cwd", cwtd = "cwd", cdq = "cdq", cltd = "cdq",
	-- implicit-GPR families modeled by classify's pinned branches
	mulx = "mulx", mulxl = "mulx", mulxq = "mulx",
	cmpxchg = "cmpxchg", cmpxchgb = "cmpxchg", cmpxchgw = "cmpxchg",
	cmpxchgl = "cmpxchg", cmpxchgq = "cmpxchg",
	cmpxchg8b = "cmpxchg8b", cmpxchg16b = "cmpxchg16b",
	-- bit-string tests (base read [+ base write for s/r/c], index use, CF out —
	-- modeled by classify's bt branch below)
	bt = "bt", btb = "bt", btw = "bt", btl = "bt", btq = "bt",
	bts = "bts", btsb = "bts", btsw = "bts", btsl = "bts", btsq = "bts",
	btr = "btr", btrb = "btr", btrw = "btr", btrl = "btr", btrq = "btr",
	btc = "btc", btcb = "btc", btcw = "btc", btcl = "btc", btcq = "btc",
	endbr64 = "nop", nop = "nop", ud2 = "ud2", ret = "ret", retq = "ret", leave = "leave",
	-- multi-byte padding NOPs: same zero-effect class as endbr64/nop (their operand,
	-- when present, is a dummy encoding hint like 0x0(%rax,%rax,1), NOT a memory
	-- access — see the frame rewrite and lowerSpecial, which pass them through
	-- without an access check, and the renderer, which normalizes the bare forms).
	nopw = "nop", nopl = "nop",
	call = "call", callq = "call", callw = "call", jmp = "jmp", jmpq = "jmp", jmpw = "jmp",
	-- (callw/jmpw are the 16-bit operand-size forms, encodable in 64-bit mode
	-- with a 0x66 prefix: without these entries baseMnemonic leaves the raw
	-- mnemonic, so a `callw` would miss the frame pass's caller-saved carrier
	-- poisoning (x86_64_frame.transfer) and a `jmpw` would miss the CFG and
	-- tailcall classification (successors/jumpKind) — both fail-open-adjacent
	-- misclassifications of real control flow. calll/jmpl have no 64-bit
	-- encoding and stay on the unknown-mnemonic rejection.)
	-- the flag-byte transfers (zero explicit operands; implicit %ah <-> EFLAGS —
	-- modeled by classify's pinned branch below)
	lahf = "lahf", sahf = "sahf",
	-- movbe: the byte-swapping move to/from memory (modeled by classify's own
	-- branch below; movbeb is listed so it gets the clean unencodable-form
	-- rejection there instead of the unknown-mnemonic fallback)
	movbe = "movbe", movbew = "movbe", movbel = "movbe", movbeq = "movbe",
	movbeb = "movbe",
	-- double-precision shifts (modeled by classify's own branch below: dst RMW,
	-- src use, imm/%cl count use)
	shld = "shld", shldw = "shld", shldl = "shld", shldq = "shld",
	shrd = "shrd", shrdw = "shrd", shrdl = "shrd", shrdq = "shrd",
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
	sar=true, shr=true, imul=true, adc=true, sbb=true, ror=true, rol=true,
	rcl=true, rcr=true }
local FLAGS_ONLY = { cmp=true, test=true }
local DST_ONLY = { inc=true, dec=true, neg=true, ["not"]=true }
-- Shift/rotate mnemonics whose two-operand register-count form takes the count in
-- %cl (the ONLY register-count encoding in the ISA): classify pins the count web
-- to physical rcx for these (see the ALU2 branch).
local SHIFT_COUNT = { sal=true, shl=true, sar=true, shr=true, ror=true, rol=true,
	rcl=true, rcr=true }

-- The SDM locked-instruction set restricted to the ALU family (shifts/rotates and
-- imul are NOT lockable in hardware): add/adc/and/or/sbb/sub/xor.
local LOCKABLE_ALU = { add=true, adc=true, ["and"]=true, ["or"]=true, sbb=true, sub=true, xor=true }

-- May this instruction carry a `lock` prefix? The prefix is only sound to pass
-- through on instructions whose memory effects sarcasm models EXACTLY — a
-- memory-destination read-modify-write in the hardware locked-instruction set:
-- the lockable ALU mem-RMW family, inc/dec/neg/not, xadd, cmpxchg, and
-- cmpxchg8b/cmpxchg16b. Everything else (reg-reg forms, mov, FP/SIMD, xchg,
-- unmodeled mnemonics) keeps the safety proof intact by being rejected.
function x86.lockAllows(stmt)
	local ops = stmt.operands or {}
	if not (ops[1] and ops[1].kind == "mem") then return false end
	local m = x86.baseMnemonic(stmt.mnemonic)
	if LOCKABLE_ALU[m] or DST_ONLY[m] or m == "cmpxchg"
		or m == "cmpxchg8b" or m == "cmpxchg16b" then
		return true
	end
	return string.match(string.lower(stmt.mnemonic), "^xadd[bwlq]?$") ~= nil
end

-- Is `stmt` a memory-destination read-modify-write instruction (its memory
-- operand is both read and written)? Used by ptr-annotation validation
-- (transform): an invisicap `;! load ptr` / `;! store ptr` cannot replace an
-- RMW (that would silently drop the operation).
function x86.isMemRmw(stmt)
	local ops = stmt.operands or {}
	if not (ops[1] and ops[1].kind == "mem") then return false end
	local m = x86.baseMnemonic(stmt.mnemonic)
	if ALU2[m] or DST_ONLY[m] or m == "cmpxchg"
		or m == "cmpxchg8b" or m == "cmpxchg16b"
		or m == "shld" or m == "shrd" then
		return true
	end
	local raw = string.lower(stmt.mnemonic)
	return string.match(raw, "^xadd[bwlq]?$") ~= nil
		or string.match(raw, "^adcx[lq]?$") ~= nil
		or string.match(raw, "^adox[lq]?$") ~= nil
		or string.match(raw, "^crc32[bwlq]?$") ~= nil
end

-- EFLAGS classification, used for flag liveness at instrumentation points
-- (transform.luau brackets its check-emitting sequences with a save/restore of the
-- flags when a flag consumer can reach them — on x86_64 the round-trip is
-- pushfq/popq <temp> ... pushq <temp>/popfq through a virtual temp, see
-- x86_64_codegen.saveFlags). A "use" READS the flags (the incoming value matters);
-- a "def" writes ALL SIX flags (each of them either architecturally defined or
-- documented-undefined — either way no later read can depend on the pre-instruction
-- value, so the incoming flags are dead). Instructions that read AND write (adc/
-- sbb/adcx/adox/rcl/rcr) count as uses (the scan checks use before def).
--
-- PARTIAL flag writes must be in NEITHER table — the scan keeps scanning forward
-- past them, so a later consumer still triggers the save/restore (conservative and
-- correct):
--   inc/dec preserve CF; rol/ror preserve SF/ZF/AF/PF (and write only CF/OF);
--   shld/shrd are conservatively modeled as CF-only (SDM Vol.2: a nonzero count
--   additionally writes SF/ZF/PF and OF (defined for a 1-bit shift, undefined
--   otherwise) plus AF (undefined) — i.e. all six on a nonzero count — but
--   claiming only CF keeps the scan propagating the rest, which costs at most
--   an extra save/restore and never omits a required one); cmpxchg8b/16b write
--   only ZF; bt/bts/btr/btc write only CF; sahf leaves OF; div/idiv leave the
--   flags unmodified in practice (documented-undefined); stc/clc/cmc write only
--   CF; cld/std write only DF (see FLAG_DF below).
-- (The bit-scan/count group — bsf/bsr/tzcnt/lzcnt/popcnt — IS fully modeled
-- below despite leaving some flags undefined: an undefined flag is still a
-- WRITE — garbage, not the incoming value — so no later read can depend on
-- the pre-instruction value. See the FLAG_DEF audit there.)
-- A shift/rotate with a provable $0 count writes NO flags at all (SDM: "if the
-- count is 0, the flags are not affected") — see isShiftCountZero; those forms
-- are in NEITHER table either (the scan propagates through them, and DCE keeps
-- them while a flag consumer is reachable, exactly like every other no-def).
-- Unknown mnemonics are also neither (the scan continues — conservative).
local FLAG_USE = {}

-- String moves/stores (rep movs*/stos* and their bare single-step forms) read
-- the direction flag: a set DF copies/fills backwards, which the checked
-- lowering does not model (it traps) — so DF is live into every stringop.
-- Matched after baseMnemonic plus the one-letter suffix strip in flagUse
-- ("movsq"->"movs", "movsb"->"movs", ...); the dword Intel spellings
-- ("movsd"/"stosd", which keep their `d`) are matched exactly but only in
-- their zero-operand string form, so the FP scalar-double `movsd` (which has
-- operands and touches no flags) never counts as a use.
local STRINGOP_BASE = { movs = true, stos = true }

-- x87 conditional moves read the flags left by the PRECEDING instruction
-- (fcmovb/c/... test CF/ZF/PF exactly like the GPR cmov family). They are
-- modeled (fp EXACT, register-only) rather than rejected, so they must be
-- flag uses: the save/restore bracket passes the pending condition through
-- unchanged, and DCE then keeps every flag write feeding one.
local FCMOV = { fcmovb = true, fcmove = true, fcmovbe = true, fcmovu = true,

	fcmovnb = true, fcmovne = true, fcmovnbe = true, fcmovnu = true }

for _, m in {

	"adc","sbb",            -- carry-in / borrow-in reads

	"adcx","adox",          -- carry-in (adcx) / overflow-in (adox) reads

	"rcl","rcr",            -- rotate through carry: read CF

	"cmc",                  -- complement carry: reads CF (and writes CF).
	                        -- SUPPORTED (not rejected): the coarse use here, the
	                        -- CF-only flagUseMask/flagDefMask below, and the CF
	                        -- flagWriteNoDef entry agree exactly (use=true with a
	                        -- nonzero mask; def=false with a non-ALL mask).

	"loope","loopz",        -- ZF-consuming loop variants: read ZF (rejected by
	"loopne","loopnz",      -- fp.unsafeReject, but the coarse table must still
	                        -- name the read so coarse flagUse and flagUseMask
	                        -- agree on every path.
	                        -- ORDERING PROOF (reject precedes every flag
	                        -- consumer): fp.unsafeReject fires inside
	                        -- x86.classify, which lift.liftFunction invokes for
	                        -- every instruction strictly before
	                        -- transform.transformFunction's DCE rounds and
	                        -- before any flagsLiveFrom save-scan use (and
	                        -- x86_64_frame.analyzeFrame's own classify pass
	                        -- runs earlier still) — a rejected loope/loopz/
	                        -- loopne/loopnz aborts compilation before DCE or
	                        -- the save-scan ever sees it, so these entries are
	                        -- unreachable belt-and-braces. Executable proof:
	                        -- filc/tests/sarcasm-reject-loopz (compileFailure).

	"lahf",                 -- reads the flags into %ah

} do FLAG_USE[m] = true end
for m in STRINGOP_BASE do FLAG_USE[m] = true end
for m in FCMOV do FLAG_USE[m] = true end

local FLAG_DEF = {}
-- SDM Vol.2 audit (2026-09, felixcloutier/Intel SDM text):
--   crc32 is DELIBERATELY ABSENT: "Flags Affected: None" — it writes NO flags,
--   so listing it as a def would kill genuinely live flags and omit a required
--   save/restore (unsound). It stays in NEITHER table (propagate).
--   bsf/bsr: "The ZF flag is set to 1 if the source operand is 0; otherwise
--   ... cleared. The CF, OF, SF, AF, and PF flags are undefined." ZF is always
--   defined, the other five always garbage — all six killed on every path.
--   tzcnt/lzcnt: ZF and CF always defined (CF set iff the input was zero, ZF
--   iff the output is zero), OF/SF/PF/AF always undefined — all six killed on
--   every path (and on pre-BMI hosts these decode as bsf, whose mask is also
--   ALL, so the claim holds on every host).
--   popcnt: CF/OF/SF/AF/PF cleared (defined), ZF set iff the result is zero
--   (defined) — all six defined on every path.
-- An undefined flag is still a WRITE — garbage, not the incoming value — so no
-- later read can depend on the pre-instruction value: ALL-def is sound and
-- precise for the whole group.
for _, m in {
	"add","sub","and","or","xor","cmp","test",  -- the ALU family (all six written)
	"neg",                                      -- neg writes all six
	"shl","sal","shr","sar",                    -- shifts: AF undefined, the rest written
	"mul","imul",                               -- CF/OF defined, the rest undefined
	"xadd","cmpxchg",                           -- full flag writes
	"bsf","bsr","tzcnt","lzcnt","popcnt",       -- all six written (see audit above)
} do FLAG_DEF[m] = true end

-- Is `stmt` a shift/rotate whose count is provably the immediate $0 — in which
-- case the hardware writes NO flags (SDM: "if the count is 0, the flags are
-- not affected")? Covers shl/sal/shr/sar, rol/ror, rcl/rcr (two-operand,
-- destination-first: the count is ops[2]) and shld/shrd (three-operand: the
-- count is ops[3]). Only a literal $0 immediate counts: any other count (a
-- nonzero immediate, the %cl register form, or a missing count) keeps the
-- conservative kill — a dynamic count that happens to mask to zero still
-- preserves the flags at runtime, so killing is sound (an extra save, never
-- a missing one), while claiming no-def there would be unsound. (A nonzero
-- count masked to zero by the 5/6-bit count mask, e.g. $32 for a 32-bit
-- operand, likewise stays a kill: conservative and sound.)
local SHIFT_FAMILY = { shl = true, sal = true, shr = true, sar = true,
	rol = true, ror = true, rcl = true, rcr = true }
local function isShiftCountZero(stmt)
	if not stmt then return false end
	local raw = string.lower(stmt.mnemonic)
	local b = x86.baseMnemonic(raw)
	local s = string.gsub(b, "[bwlq]$", "")
	local count = nil
	if SHIFT_FAMILY[b] or SHIFT_FAMILY[s] then
		count = (stmt.operands or {})[2]
	elseif b == "shld" or b == "shrd" or s == "shld" or s == "shrd" then
		count = (stmt.operands or {})[3]
	else
		return false
	end
	if not count or count.kind ~= "imm" then return false end
	if count.value ~= nil then return count.value == 0 end
	if count.text ~= nil then
		local t = string.gsub(count.text, "^%$", "")
		local v = tonumber(t)
		return v == 0
	end
	return false
end

-- Does `stmt` READ the flags? Conditional consumers (jcc/setcc/cmov) and the
-- carry-in/rotate-through-carry families, plus lahf, the stringops (DF), and
-- the x87 conditional moves. Used by transform.luau's
-- flag-liveness scan (flagsLiveFrom).
function x86.flagUse(stmt)
	if not stmt then return false end
	local m = x86.baseMnemonic(string.lower(stmt.mnemonic))
	if m == "jcc" or m == "setcc" or m == "cmov" then return true end
	if FLAG_USE[m] then return true end
	-- baseMnemonic only normalizes mnemonics in its BASE/FIXED tables; the rest
	-- keep their AT&T size suffix (adcxq, crc32l, rclq, popcntq, shldq, btq, ...).
	m = string.gsub(m, "[bwlq]$", "")
	if FLAG_USE[m] then return true end
	-- The dword string moves keep their `d` ("movsd"/"stosd" — the Intel
	-- spellings; AT&T uses movsl/stosl), so the strip above never normalizes
	-- them: match exactly, but only in the zero-operand string form, so the
	-- FP scalar-double `movsd %xmm0, %xmm1` (which touches no flags) never
	-- counts as a use.
	local raw = string.lower(stmt.mnemonic)
	if (raw == "movsd" or raw == "stosd") and #(stmt.operands or {}) == 0 then
		return true
	end
	return false
end

-- Does `stmt` write ALL SIX flags (defined or documented-undefined — no later read
-- can depend on the pre-instruction values)? Used by transform.luau's flag-liveness
-- scan (flagsLiveFrom). Partial writes are deliberately NOT here (see above).
function x86.flagDef(stmt)
	if not stmt then return false end
	-- A $0-count shift/rotate writes nothing (see isShiftCountZero).
	if isShiftCountZero(stmt) then return false end
	local m = x86.baseMnemonic(string.lower(stmt.mnemonic))
	if FLAG_DEF[m] then return true end
	-- baseMnemonic only normalizes mnemonics in its BASE/FIXED tables; the rest
	-- keep their AT&T size suffix (tzcntq, popcntl, ...). Strip it ONLY as a
	-- fallback after the exact-base check above: several bases themselves end in
	-- b/w/l/q ("sub", "shl", "sal", "mul", "imul") and stripping them first would
	-- mangle the base ("sub" -> "su"), wrongly reporting no flag write. (DCE then
	-- deletes a flag-setting sub/shl/sal/mul/imul whose register result is dead
	-- but whose flags are live — e.g. the x86_64-mont5 reduction's
	-- `subq %r13,%rbp` feeding `adcq`.)
	m = string.gsub(m, "[bwlq]$", "")
	return FLAG_DEF[m] == true
end

-- Does `stmt` WRITE flags that the model does not track as a full def? This is the
-- dead-code-elimination complement of flagUse/flagDef (same normalization): flagDef
-- insns def the flags pseudo-temp, so liveness already protects their writes; the
-- read-and-write family counted as USES by the scan (adc/sbb write all six, adcx/
-- adox/rcl/rcr write CF/OF) and every PARTIAL write (inc/dec preserve CF; rol/ror
-- write CF/OF (SDM: SF/ZF/AF/PF always unaffected); shld/shrd conservatively
-- modeled as CF-only (SDM writes more on nonzero counts — see the note above —
-- the under-claim only propagates extra liveness); bt/bts/btr/btc, stc/clc/cmc
-- write only CF; cld/std write only DF; sahf writes all but OF)
-- also CHANGE the flags without def-ing them. Deleting such an instruction while a
-- flag consumer is forward-reachable would change the flags that consumer reads,
-- so DCE keeps it. (The flag-SAVE scan deliberately ignores these instructions —
-- a save/restore bracket passes the flags through unchanged, so for bracketing a
-- partial write is correctly invisible; removal is the one decision that must
-- respect them.) Pure flag CONSUMERS that write nothing (jcc/setcc/cmov/lahf,
-- the stringops' DF read, fcmov) are
-- NOT here: deleting them cannot change any flags.
-- crc32 is DELIBERATELY ABSENT here too (verified 2026-09): SDM "Flags
-- Affected: None" — it writes NO flags, so a dead crc32 whose register result
-- is dead may be DCE'd even while flags are live (deleting it cannot change
-- any flags), and the save-scan correctly propagates through it (NEITHER
-- coarse table, zero masks). Listing it here would only pin dead code; listing
-- it in FLAG_DEF would be UNSOUND (it would kill genuinely live flags — see
-- the FLAG_DEF audit above).
local FLAG_WRITE_NO_DEF = {}
for _, m in {
	"adc","sbb",              -- full six-flag writes (counted as uses for the scan)
	"adcx","adox",            -- carry / overflow out
	"rcl","rcr",              -- CF/OF out
	"inc","dec",              -- all but CF
	"rol","ror",              -- OF/CF
	"shld","shrd",            -- CF
	"bt","bts","btr","btc",   -- CF
	"stc","clc","cmc",        -- CF
	"cld","std",              -- DF (the direction flag; read by rep movs*/stos*)
	"sahf",                   -- all but OF (also pinned; unreachable in DCE)
} do FLAG_WRITE_NO_DEF[m] = true end

function x86.flagWriteNoDef(stmt)
	if not stmt then return false end
	-- A $0-count shift/rotate writes nothing (see isShiftCountZero).
	if isShiftCountZero(stmt) then return false end
	local m = x86.baseMnemonic(string.lower(stmt.mnemonic))
	if FLAG_WRITE_NO_DEF[m] then return true end
	-- same suffix fallback as flagUse: baseMnemonic only normalizes mnemonics in
	-- its BASE/FIXED tables; the rest keep their AT&T size suffix (adcxq, rclq,
	-- shldq, btq, incq, ...).
	m = string.gsub(m, "[bwlq]$", "")
	return FLAG_WRITE_NO_DEF[m] == true
end

-- Per-flag EFLAGS masks for the flag-liveness fixpoint (transform.luau's
-- flagsLiveFrom): which architectural flags an instruction READS (use) and
-- which it fully WRITES (def — the incoming value is dead, whether defined
-- or documented-undefined garbage). Partial writes appear in NEITHER mask
-- (the fixpoint propagates through them — sound and conservative), exactly
-- like the coarse flagUse/flagDef tables above, but per-flag granularity
-- kills liveness precisely: an `inc` (all but CF) no longer keeps CF alive
-- for a later `adc`, and a `cmc`-free `clc` no longer keeps anything alive.
-- Bit positions are the EFLAGS bits (CF=1, PF=4, AF=16, ZF=64, SF=128,
-- OF=2048); FLAG_ALL is all six. FLAG_DF is the direction flag (EFLAGS bit
-- 10): written by cld/std, read by the string moves/stores (rep movs*/stos*
-- and their bare forms — a set DF traps in the checked lowering). DF rides
-- the same fixpoint as the other six: cld/std fully write it (a kill), the
-- stringops read it (a use), and every other modeled instruction leaves it
-- alone (propagate — sound, since only cld/std modify DF).
-- 32-bit-universe note: all seven modeled bits (CF=1, PF=4, AF=16, ZF=64,
-- SF=128, DF=1024, OF=2048) fit in the bit32 32-bit universe, so the
-- fixpoints' bit32.bnot(def) is exact on the modeled bits — the flipped upper
-- 24 bits never meet a live bit (every use/def mask in this file is a sum of
-- the constants above), and bit32.band(w, bnot(def)) then clears exactly the
-- killed flags. No mask in this file exceeds 2^12, far below the 2^32 wrap
-- point, so no truncation or sign issue can arise.
local FLAG_CF, FLAG_PF, FLAG_AF = 1, 4, 16
local FLAG_ZF, FLAG_SF, FLAG_OF = 64, 128, 2048
local FLAG_DF = 1024
local FLAG_ALL = FLAG_CF + FLAG_PF + FLAG_AF + FLAG_ZF + FLAG_SF + FLAG_OF
x86.FLAG_ALL = FLAG_ALL
x86.FLAG_DF = FLAG_DF

-- Condition-code suffix (je/jne/jb/.../setb/cmovl/...) -> read mask. Unsigned,
-- signed, and flag-direct conditions all reduce to EFLAGS subsets.
local COND_MASK = {
	o = FLAG_OF, no = FLAG_OF,
	b = FLAG_CF, c = FLAG_CF, nae = FLAG_CF,
	ae = FLAG_CF, nb = FLAG_CF, nc = FLAG_CF,
	e = FLAG_ZF, z = FLAG_ZF,
	ne = FLAG_ZF, nz = FLAG_ZF,
	be = FLAG_CF + FLAG_ZF, na = FLAG_CF + FLAG_ZF,
	a = FLAG_CF + FLAG_ZF, nbe = FLAG_CF + FLAG_ZF,
	s = FLAG_SF, ns = FLAG_SF,
	p = FLAG_PF, pe = FLAG_PF,
	np = FLAG_PF, po = FLAG_PF,
	l = FLAG_SF + FLAG_OF, nge = FLAG_SF + FLAG_OF,
	ge = FLAG_SF + FLAG_OF, nl = FLAG_SF + FLAG_OF,
	le = FLAG_SF + FLAG_ZF + FLAG_OF, ng = FLAG_SF + FLAG_ZF + FLAG_OF,
	g = FLAG_SF + FLAG_ZF + FLAG_OF, nle = FLAG_SF + FLAG_ZF + FLAG_OF,
}

-- Condition suffix of a jcc/setcc/cmov mnemonic (nil when unparseable — the
-- masks then conservatively read everything).
local function condOf(m)
	local s = nil
	if string.sub(m, 1, 1) == "j" then s = string.sub(m, 2)
	elseif string.sub(m, 1, 3) == "set" then s = string.sub(m, 4)
	elseif string.sub(m, 1, 4) == "cmov" then s = string.sub(m, 5)
	else return nil end
	if COND_MASK[s] then return s end
	-- AT&T operand-size suffix (cmovbw/cmovll/...): strip one trailing w/l/q
	-- when the remainder is a condition.
	local t = string.sub(s, 1, -2)
	if string.match(s, "[wlq]$") and COND_MASK[t] then return t end
	return nil
end

-- EFLAGS bits READ by `stmt` (0 when it reads none). Agrees with flagUse in
-- the coarse sense (nonzero iff flagUse), except instructions flagUse counts
-- as uses that write nothing it tracks... (none: every coarse use reads at
-- least one flag here too).
function x86.flagUseMask(stmt)
	if not stmt then return 0 end
	local raw = string.lower(stmt.mnemonic)
	local m = x86.baseMnemonic(raw)
	if m == "jcc" or m == "setcc" or m == "cmov" then
		local c = condOf(raw)
		if c then return COND_MASK[c] end
		return FLAG_ALL
	end
	if m == "adc" or m == "sbb" then return FLAG_CF end
	if m == "adcx" then return FLAG_CF end
	if m == "adox" then return FLAG_OF end
	if m == "rcl" or m == "rcr" then return FLAG_CF end
	if m == "cmc" then return FLAG_CF end
	if m == "lahf" then return FLAG_SF + FLAG_ZF + FLAG_AF + FLAG_PF + FLAG_CF end
	-- x87 conditional moves read the pending condition (CF/ZF/PF by opcode;
	-- conservatively all six — an extra save, never a missing one). The exact
	-- opcode names never take a size suffix, so they match here; the strip
	-- below would mangle them ("fcmovb" -> "fcmov"), hence no fallback entry.
	if FCMOV[m] then return FLAG_ALL end
	-- String moves/stores read the direction flag (see FLAG_DF above). The
	-- suffixed forms ("movsq", "movsb", ...) match after the strip below;
	-- only the bare forms ("movs"/"stos") match here.
	if STRINGOP_BASE[m] then return FLAG_DF end
	if (raw == "movsd" or raw == "stosd") and #(stmt.operands or {}) == 0 then
		return FLAG_DF
	end
	-- ZF-consuming loop variants: rejected by fp.unsafeReject inside
	-- x86.classify — which lift.liftFunction runs strictly before DCE and
	-- any flagsLiveFrom use — so no such instruction can reach liveness
	-- (see the ORDERING PROOF at FLAG_USE). The mask still names the ZF
	-- read so the entry stays sound under any future relaxation.
	if m == "loope" or m == "loopz" or m == "loopne" or m == "loopnz" then
		return FLAG_ZF
	end
	m = string.gsub(m, "[bwlq]$", "")
	if m == "adc" or m == "sbb" then return FLAG_CF end
	if m == "adcx" then return FLAG_CF end
	if m == "adox" then return FLAG_OF end
	if m == "rcl" or m == "rcr" then return FLAG_CF end
	if m == "cmc" then return FLAG_CF end
	if STRINGOP_BASE[m] then return FLAG_DF end
	if m == "loope" or m == "loopz" or m == "loopne" or m == "loopnz" then
		return FLAG_ZF
	end
	return 0
end

-- EFLAGS bits fully WRITTEN by `stmt` (incoming value dead). Agrees with
-- flagDef (ALL iff flagDef) except adc/sbb, which the coarse table counts as
-- uses (their carry-in read matters to DCE) but which write all six flags —
-- nothing past them can observe the incoming flags, so the liveness fixpoint
-- kills precisely.
function x86.flagDefMask(stmt)
	if not stmt then return 0 end
	-- A $0-count shift/rotate writes nothing (see isShiftCountZero): no kill.
	if isShiftCountZero(stmt) then return 0 end
	local raw = string.lower(stmt.mnemonic)
	local m = x86.baseMnemonic(raw)
	if m == "adc" or m == "sbb" then return FLAG_ALL end
	if m == "adcx" then return FLAG_CF end
	if m == "adox" then return FLAG_OF end
	if m == "rcl" or m == "rcr" then return FLAG_CF + FLAG_OF end
	if m == "inc" or m == "dec" then return FLAG_ALL - FLAG_CF end
	if m == "rol" or m == "ror" then return FLAG_CF + FLAG_OF end
	if m == "shld" or m == "shrd" then return FLAG_CF end
	if m == "bt" or m == "bts" or m == "btr" or m == "btc" then return FLAG_CF end
	if m == "stc" or m == "clc" then return FLAG_CF end
	if m == "cmc" then return FLAG_CF end
	-- cld/std fully write the direction flag (see FLAG_DF above).
	if m == "cld" or m == "std" then return FLAG_DF end
	if m == "sahf" then return FLAG_SF + FLAG_ZF + FLAG_AF + FLAG_PF + FLAG_CF end
	if FLAG_DEF[m] then return FLAG_ALL end
	m = string.gsub(m, "[bwlq]$", "")
	if m == "adc" or m == "sbb" then return FLAG_ALL end
	if m == "adcx" then return FLAG_CF end
	if m == "adox" then return FLAG_OF end
	if m == "rcl" or m == "rcr" then return FLAG_CF + FLAG_OF end
	if m == "inc" or m == "dec" then return FLAG_ALL - FLAG_CF end
	if m == "rol" or m == "ror" then return FLAG_CF + FLAG_OF end
	if m == "shld" or m == "shrd" then return FLAG_CF end
	if m == "bt" or m == "bts" or m == "btc" or m == "btr" then return FLAG_CF end
	if m == "stc" or m == "clc" then return FLAG_CF end
	if m == "cmc" then return FLAG_CF end
	if m == "cld" or m == "std" then return FLAG_DF end
	if FLAG_DEF[m] then return FLAG_ALL end
	return 0
end

-- condition -> whether it's an unsigned/eq branch we generate; classify only needs control.
local COND_JMP = { jcc = true }

-- Label-target control mnemonics baseMnemonic does NOT map to jcc (jcxz/jecxz/
-- jrcxz already match ^j%a+$): their bare symbol operand is a code target, never
-- an absolute memory operand, so the moffs rejection below exempts them. (The
-- ZF-consuming loop variants loope/loopz/loopne/loopnz are rejected outright by
-- fp.unsafeReject before this ever matters, and xbegin by its own TSX rejection;
-- they stay listed so the exemption documents every label-target spelling.)
local LABEL_TARGET_MNEM = { loop = true, loopq = true, loopl = true, loopb = true,
	loopw = true, loope = true, loopz = true, loopne = true, loopnz = true,
	xbegin = true }

local function isGpr(op) return op and op.kind == "reg" and op.rc == "gpr" end

-- Is a write of register operand `o` a PARTIAL write — one whose emission
-- preserves the bits of the enclosing web above the written width, so the
-- instruction read-modifies the web rather than replacing it?
--   * a real GPR: an 8/16-bit write (%al/%ax & co) preserves bits 8/16-63 of
--     the enclosing 64-bit register; a 32-bit write zero-extends (a full def).
--   * a frame-slot pseudo-register (isSlot — the frame pass's virtualization
--     of an 8-byte stack slot, keyed by byte offset so accesses of every width
--     share one web): the slot web lives in a colored register, so an 8/16-bit
--     store to it (movb/movw) preserves the register's upper bits exactly like
--     a narrow memory store preserves the neighboring bytes. (A 32-bit store
--     to a slot zero-extends the colored register — a full def of the web,
--     like arm64's `str w` virtualization; a wider reload of that slot then
--     reads zeroes where memory would have preserved bytes — a pre-existing,
--     documented slot-virtualization gap, NOT expressible as register RMW.)
-- A partial write must be modeled as def+use+rmw of the web — modeling it as a
-- pure def lets reaching-definitions kill the previous web, so DCE drops the
-- instruction that produced the preserved bits while the emitted instruction
-- still preserves them (the ghash `xorq %rax,%rax` / `movb %sil,%al`
-- miscompile: the zeroing vanished and the movb left garbage in bits 8-63 of
-- an index register).
local function partialRegWrite(o)
	return (o.width or 64) < 32
end

local function memRegs(op, uses)
	if op.base and isGpr(op.base) then table.insert(uses, op.base) end
	if op.index and isGpr(op.index) then table.insert(uses, op.index) end
end

-- High-byte registers (%ah/%ch/%dh/%bh) -----------------------------------------
--
-- The parser maps a high-byte name to its register's web (nums 0-3, width 8) —
-- the SAME web as the low-byte al/cl/dl/bl, because the web model has no
-- subregister view. Every modeled instruction names the LOW byte of the colored
-- register (the renderer always emits regName(num, 8)), so an unhandled
-- high-byte operand would silently touch the low byte (`xchgb %ah, %bl` would
-- swap %al).
--
-- The modeled forms below handle the high byte EXACTLY with the implicit-GPR
-- pin machinery (codegen's emitPinned — the same machinery as lahf/sahf and
-- cmpxchg): every GPR operand of the instruction is pinned to its SPELLED
-- physical register (pin-in moves copy the reaching webs in, pin-out copies the
-- results back out), and the emitted instruction keeps the spelled registers,
-- so the hardware operates on the architected byte wherever the webs were
-- colored:
--   * a high-byte SOURCE reads bits 8-15 of the enclosing register's physical
--     register (the full 64-bit web is pinned in);
--   * a high-byte DESTINATION merges into bits 8-15 and preserves the rest —
--     modeled as a read-modify-write of the enclosing register's web;
--   * a low-byte partner destination (movb %ah, %cl) is likewise an RMW (its
--     bits 8-63 flow through the pin); a movzx/movsx destination is a PURE def
--     (the instruction zero-extends, defining the full register).
--
-- Encodability (gas enforces the same rules): an instruction with a high-byte
-- operand cannot have a REX prefix, so a byte register partner must be one of
-- %al/%cl/%dl/%bl (nums 0-3 — %spl/%bpl/%sil/%dil and %r8b+ need REX), a
-- 16/32-bit register partner must be num < 8, and NO 64-bit register operand
-- (REX.W) may appear. A high-byte operand with a MEMORY operand is rejected:
-- the bounds-checking rewrite addresses memory through an allocated temp that
-- could leave the REX-free registers.
local HIGH_BYTE_NUM = { ah = 0, ch = 1, dh = 2, bh = 3 }
local HB_ENCLOSING_NAME = { [0] = "rax", [1] = "rcx", [2] = "rdx", [3] = "rbx" }

local function isHighByteOp(o)
	return o and o.kind == "reg" and o.rc == "gpr" and o.width == 8
		and HIGH_BYTE_NUM[o.name or ""] ~= nil
end

-- Classify an instruction that has at least one high-byte operand (xchg is
-- handled by its own branch, which already pins both operands). Returns the
-- classify result or raises a clean rejection.
local function classifyHighByte(insn, m, ops)
	local rawMnem = string.lower(insn.mnemonic)
	local raw = insn.raw or rawMnem
	local function hbSpelling(o)
		return (insn.syntax == "intel") and o.name or ("%" .. o.name)
	end
	local hbOp
	for _, o in ops do
		if isHighByteOp(o) then hbOp = hbOp or o end
	end
	local generic = function()
		error("sarcasm: high-byte register " .. hbSpelling(hbOp) .. " is not supported in '"
			.. rawMnem .. "' (supported: movb, movzbl/movzbw/movsbl/movsbw reads, byte"
			.. " ALU/cmp/test/inc/dec/neg/not forms, setcc, xchgb; use a low-byte"
			.. " sequence for anything else): " .. raw)
	end
	-- Encodability of the spelled form: every operand must be a GPR register or an
	-- immediate, and register partners must not require a REX prefix.
	for _, o in ops do
		if o.kind == "mem" then
			error("sarcasm: high-byte register " .. hbSpelling(hbOp) .. " with a memory operand"
				.. " is not supported (the bounds-check rewrite addresses memory through an"
				.. " allocated register that may require a REX prefix, which a high-byte"
				.. " register cannot combine with; rewrite with a low-byte sequence): " .. raw)
		end
		if o.kind == "sym" or (o.kind == "imm" and o.absAddr) then
			generic()
		end
		if isGpr(o) and not isHighByteOp(o) then
			if o.width == 8 then
				if o.num > 3 then
					error("sarcasm: high-byte register " .. hbSpelling(hbOp)
						.. " cannot be combined with " .. ((insn.syntax == "intel") and o.name or ("%" .. o.name))
						.. " (a byte register other than %al/%cl/%dl/%bl requires a REX"
						.. " prefix, which a high-byte register cannot combine with): " .. raw)
				end
			elseif o.width == 16 or o.width == 32 then
				if o.num > 7 then
					error("sarcasm: high-byte register " .. hbSpelling(hbOp)
						.. " cannot be combined with " .. ((insn.syntax == "intel") and o.name or ("%" .. o.name))
						.. " (a register numbered 8 or above requires a REX prefix, which a"
						.. " high-byte register cannot combine with): " .. raw)
				end
			else
				error("sarcasm: high-byte register " .. hbSpelling(hbOp)
					.. " cannot be combined with a 64-bit register operand (a REX.W prefix"
					.. " is required, which a high-byte register cannot combine with): " .. raw)
			end
		end
	end

	local defs, uses, rmw = {}, {}, {}
	local pinUses, pinDefs = {}, {}
	-- the enclosing register's full web for a high-byte operand (a FRESH table per
	-- occurrence — the lift keys node.opTemp by table identity)
	local function hbUseOp(o)
		local u = { kind = "reg", rc = "gpr", num = o.num, width = 64,
			name = HB_ENCLOSING_NAME[o.num], implicit = true }
		table.insert(uses, u); table.insert(pinUses, u)
	end
	local function hbDefOp(o)
		local d = { kind = "reg", rc = "gpr", num = o.num, width = 64,
			name = HB_ENCLOSING_NAME[o.num], implicit = true }
		table.insert(defs, d); table.insert(pinDefs, d)
	end
	-- a high-byte operand in a WRITTEN position: an RMW of the enclosing web (the
	-- pin preserves every bit the instruction does not write).
	local function hbRmw(o)
		hbUseOp(o); hbDefOp(o); table.insert(rmw, o.num)
	end
	local function pinUse(o) table.insert(uses, o); table.insert(pinUses, o) end
	local function pinDef(o) table.insert(defs, o); table.insert(pinDefs, o) end
	-- a written low-byte partner: def + use (RMW — the pin preserves bits 8-63)
	local function pinRmw(o)
		table.insert(defs, o); table.insert(pinDefs, o)
		table.insert(uses, o); table.insert(pinUses, o)
		table.insert(rmw, o.num)
	end
	-- any written operand (high-byte -> enclosing RMW; low-byte partner -> RMW)
	local function writeOp(o)
		if isHighByteOp(o) then hbRmw(o) else pinRmw(o) end
	end
	local function readOp(o)
		if isHighByteOp(o) then hbUseOp(o)
		elseif isGpr(o) then pinUse(o) end
	end

	if m == "mov" then
		-- movb dst, src: dst a width-8 GPR, src a width-8 GPR or an immediate.
		if #ops ~= 2 or not isGpr(ops[1]) or ops[1].width ~= 8
			or (isGpr(ops[2]) and ops[2].width ~= 8) then
			generic()
		end
		writeOp(ops[1])
		readOp(ops[2])
	elseif m == "movzx" or m == "movsx" then
		-- movzbl/movzbw/movsbl/movsbw dst, src: the high-byte operand can only be
		-- the SOURCE (a high-byte movzx destination is not an encoding), and only
		-- the byte-source, sub-64-bit-destination forms are REX-free.
		if isHighByteOp(ops[1]) then
			error("sarcasm: a high-byte register cannot be the destination of movzx/movsx"
				.. " (the destination is a 16/32-bit register; that encoding does not"
				.. " exist): " .. raw)
		end
		if not isHighByteOp(ops[2]) then generic() end
		local srcLetter = string.match(rawMnem, "^mov[zs]([bw])")
		if insn.syntax ~= "intel" and srcLetter ~= "b" then
			error("sarcasm: " .. rawMnem .. " with a high-byte source is not encodable"
				.. " (only the byte-source, 16/32-bit-destination forms movzbl/movzbw/"
				.. "movsbl/movsbw are; " .. rawMnem .. " requires a REX prefix): " .. raw)
		end
		if not isGpr(ops[1]) or (ops[1].width ~= 32 and ops[1].width ~= 16) then
			error("sarcasm: " .. rawMnem .. " with a high-byte source requires a 16-bit"
				.. " or 32-bit register destination (a 64-bit destination requires a"
				.. " REX.W prefix): " .. raw)
		end
		-- the destination is a PURE def: movzx/movsx define the full register
		-- (zero/sign extension to 32 bits zeroes bits 32-63 in hardware).
		pinDef(ops[1])
		readOp(ops[2])
	elseif FLAGS_ONLY[m] or DST_ONLY[m] or ALU2[m] then
		-- cmpb/testb: all register operands are uses. inc/dec/neg/not: the lone
		-- register operand is an RMW. ALU forms: ops[1] RMW, ops[2] use.
		if SHIFT_COUNT[m] and isGpr(ops[2]) then
			error("sarcasm: a shift/rotate count can only come from %cl (the only"
				.. " register-count encoding); a high-byte register cannot encode it: "
				.. raw)
		end
		if FLAGS_ONLY[m] then
			for _, o in ops do readOp(o) end
		elseif DST_ONLY[m] then
			if not isGpr(ops[1]) then generic() end
			writeOp(ops[1])
		else
			if not isGpr(ops[1]) then generic() end
			writeOp(ops[1])
			for i = 2, #ops do readOp(ops[i]) end
		end
	elseif m == "setcc" then
		-- setcc %ah writes ONLY bits 8-15 of the enclosing register: an RMW of its
		-- web. The pin makes the widening rewrite unnecessary (and inapplicable).
		if not isHighByteOp(ops[1]) then generic() end
		writeOp(ops[1])
	else
		generic()
	end
	return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
		pin = { uses = pinUses, defs = pinDefs } }
end

-- String ops with implicit rsi/rdi memory (rep movs*, rep stos*, and their
-- bare single-step forms): element byte width by mnemonic (`movs`/`stos`
-- without a size letter are ambiguous and rejected in classifyStringOp).
local STRING_ELEM = {
	movsb = 1, movsw = 2, movsl = 4, movsd = 4, movsq = 8,
	stosb = 1, stosw = 2, stosl = 4, stosd = 4, stosq = 8,
}

-- Classify a stringop- or rep-prefixed instruction. Returns a classify result
-- for the supported shapes (rep movs*, rep stos*, bare movs*/stos*), nil for
-- anything this does not own (including `rep ret` / `rep nop`, which keep
-- their hint and classify normally downstream), and raises a clean rejection
-- otherwise. Runs BEFORE fp.unsafeReject in x86.classify so the supported
-- shapes never reach the string-op rejection (and so the zero-operand
-- `movsd` string form is claimed before the FP table could see it — `movsd`
-- WITH operands is an FP instruction and stays untouched here).
function x86.classifyStringOp(insn)
	local rawMnem = string.lower(insn.mnemonic)
	local raw = insn.raw or rawMnem
	local rep = insn.repPrefix  -- nil | "rep"/"repe"/"repne"/"repz"/"repnz"
	local ops = insn.operands or {}
	local kind, elem = nil, nil
	if rawMnem == "movsd" then
		-- ambiguous mnemonic: with operands it is the FP scalar-double move;
		-- with none it is the dword string move.
		if #ops == 0 then kind, elem = "movs", 4 end
	elseif STRING_ELEM[rawMnem] then
		elem = STRING_ELEM[rawMnem]
		kind = (string.sub(rawMnem, 1, 4) == "movs") and "movs" or "stos"
	elseif rawMnem == "movs" or rawMnem == "stos" then
		kind = rawMnem  -- elem stays nil: ambiguous width, rejected below
	end
	if kind then
		if #ops > 0 then
			local regs = (insn.syntax == "intel") and "rsi/rdi" or "%rsi/%rdi"
			error("sarcasm: '" .. rawMnem .. "' takes no explicit operands (its"
				.. " source and destination are the implicit " .. regs .. " registers;"
				.. " write the bare form): "
				.. raw)
		end
		-- The dword string move is spelled `movsl`/`stosl` in AT&T but
		-- `movsd`/`stosd` in Intel (each assembler rejects the other's
		-- spelling, so the check must run here — the re-emitted mnemonic
		-- would otherwise die in `as` with a cryptic message).
		if insn.syntax == "intel" then
			if rawMnem == "movsl" or rawMnem == "stosl" then
				error("sarcasm: '" .. rawMnem .. "' is not encodable in Intel syntax"
					.. " (use " .. string.gsub(rawMnem, "l$", "d") .. " for the dword"
					.. " string move): " .. raw)
			end
		elseif rawMnem == "stosd" then
			error("sarcasm: 'stosd' is not encodable in AT&T syntax"
				.. " (use stosl for the dword string store): " .. raw)
		end
		if not elem then
			error("sarcasm: cannot determine the element width of '" .. rawMnem
				.. "' (use an explicit size form: movsb/movsw/movsl/movsq or"
				.. " stosb/stosw/stosl/stosq): " .. raw)
		end
		if rep == "repne" or rep == "repnz" then
			error("sarcasm: `" .. rep .. "` prefix is not supported on '" .. rawMnem
				.. "' (only `rep`/`repe`/`repz`, which repeat while %rcx != 0): " .. raw)
		end
		if insn.annotation then
			error("sarcasm: annotation `" .. insn.annotation .. "` is not supported on '"
				.. rawMnem .. "' (string instructions need no annotation — their"
				.. " accesses are capability-checked against %rsi/%rdi): " .. raw)
		end
		if insn.lockPrefix then
			error("sarcasm: `lock` prefix is not supported on '" .. rawMnem
				.. "' (lock requires a memory-destination read-modify-write"
				.. " instruction sarcasm models exactly): " .. raw)
		end
		-- The hardware reads and advances the implicit registers. The rsi/rdi
		-- occurrences are SHARED between uses and defs (one table, RMW) so the
		-- pointer webs — and their capabilities — flow through the copy/fill
		-- unchanged (no ptrflow rule needed). The rcx def is a FRESH web (the
		-- hardware zeroes %rcx): a scalar 0. The transform routes the webs
		-- through the physical registers with explicit pin moves around the
		-- instruction (no fixedReg precoloring: the fail-stub marshalling also
		-- needs rdi/rsi, so pinned webs would be uncolorable).
		local defs, uses, rmw = {}, {}, {}
		local repInfo = { kind = kind, elem = elem, repPrefix = rep }
		local function imp(num, nm)
			return { kind = "reg", rc = "gpr", num = num, width = 64,
				name = nm, implicit = true }
		end
		local function rmwReg(num, nm)
			local r = imp(num, nm)
			table.insert(uses, r); table.insert(defs, r); table.insert(rmw, num)
			return r
		end
		if kind == "movs" then
			repInfo.rsi = rmwReg(6, "rsi")
			repInfo.rdi = rmwReg(7, "rdi")
		else
			repInfo.rdi = rmwReg(7, "rdi")
			local rax = imp(0, "rax")
			table.insert(uses, rax); repInfo.rax = rax
		end
		local rcxUse = imp(1, "rcx")
		table.insert(uses, rcxUse); repInfo.rcxUse = rcxUse
		local rcxDef = imp(1, "rcx")
		table.insert(defs, rcxDef); repInfo.rcxDef = rcxDef
		return { defs = defs, uses = uses, rmw = rmw, control = "none",
			fallsThrough = true, rep = repInfo }
	end
	-- Not a movs/stos shape: validate the rep prefix. `rep ret` (the AMD
	-- branch-prediction hint) and `rep nop` (pause) keep their prefix and
	-- classify normally; the renderer re-emits it. Anything else cannot be
	-- modeled — rejecting here keeps a meaningless prefix from being silently
	-- dropped (e.g. `rep addq` is NOT an add).
	if rep then
		if rawMnem == "ret" or rawMnem == "nop" then return nil end
		error("sarcasm: `" .. rep .. "` prefix is not supported on '" .. rawMnem
			.. "' (only rep movs* / rep stos* — including movsb/movsw/movsl/movsq"
			.. " and stosb/stosw/stosl/stosq — plus `rep ret`): " .. raw)
	end
	return nil
end

-- Classify. Returns { defs, uses, rmw, control, targets, fallsThrough }.
function x86.classify(insn)
	-- Unsafe instructions (syscall/privileged/port-I/O/unmodeled-string-ops/
	-- stray-prefixes/cr-dr moves) are rejected here, BEFORE baseMnemonic
	-- mapping, and FP/SIMD/x87 instructions get their def/use
	-- modeling from the fp knowledge module (GPR operands only; xmm-class
	-- registers are deliberately never modeled).
	local rawMnem = string.lower(insn.mnemonic)

	-- String ops with implicit rsi/rdi memory (rep movs*, rep stos*, and
	-- their bare single-step forms): modeled with implicit register effects
	-- for the transform's checked lowering (see classifyStringOp). Every
	-- other rep-prefixed instruction is validated here too (`rep ret` keeps
	-- its hint; anything else is rejected). Runs before unsafeReject so the
	-- supported shapes (including the zero-operand `movsd` string form) never
	-- reach the string-op rejection.
	do
		local rep = x86.classifyStringOp(insn)
		if rep then return rep end
	end
	local why = fp.unsafeReject(rawMnem, insn)
	if why then error("sarcasm: " .. why .. ": " .. (insn.raw or rawMnem)) end

	-- `lock` prefix (parsed as a prefix tag by x86_64_parse): allowed only on the
	-- exactly-modeled memory-destination RMWs (lockAllows). The locked instruction
	-- then rides the normal checked path (a single write-classified access check,
	-- including the not-readonly CanWrite test) with the prefix re-emitted by the
	-- renderer. Anything else is rejected so the safety proof stays intact.
	if insn.lockPrefix and not x86.lockAllows(insn) then
		error("sarcasm: lock prefix not allowed on '" .. rawMnem .. "' (lock requires a"
			.. " memory-destination read-modify-write instruction sarcasm models exactly:"
			.. " add/adc/and/or/sbb/sub/xor, inc/dec/neg/not, xadd, cmpxchg,"
			.. " cmpxchg8b/cmpxchg16b): " .. (insn.raw or rawMnem))
	end

	-- Local-subroutine marks (localcall.luau): a marked call is an unconditional
	-- branch to the per-caller clone's entry (no fall-through — the continuation
	-- is reached only through the clone's ret dispatch), and a marked ret is a
	-- multi-way branch to the clone's continuations. Modeling both as branches
	-- makes every CFG consumer (validateBody's reachability, the lift's
	-- reaching-definitions, DCE, regalloc) see the true control flow with zero
	-- special cases; the transform emits the actual lea+jmp / compare-chain.
	if insn.localCall then
		return { defs = {}, uses = {}, rmw = {}, control = "branch",
			targets = { insn.localCall.entry }, fallsThrough = false }
	end
	if insn.localRet then
		return { defs = {}, uses = {}, rmw = {}, control = "branch",
			targets = insn.localRet.conts, fallsThrough = false }
	end

	local m = x86.baseMnemonic(insn.mnemonic)
	local ops = insn.operands or {}

	-- Absolute-address (moffs) operands: a bare (non-$-prefixed) symbol or a bare
	-- numeric literal operand is, in gas's dialects, an ABSOLUTE MEMORY operand,
	-- not a value: `movq %rsi, myglobal` stores 8 bytes to the global, `addq
	-- myglobal, %rax` reads it, and `movq 0x600000, %rax` LOADS from that address
	-- (the AT&T parser tags such numeric literals with absAddr; the $-spelling is
	-- the immediate). Gas's Intel syntax gives a bare symbol the same moffs
	-- reading (`mov rsi, myglobal` loads — verified against `as`: it emits an
	-- R_X86_64_32S absolute relocation); only a bare NUMBER is an immediate
	-- there, and the Intel parser never tags it. No capability covers such an
	-- access, so it cannot be bounds-checked — the same class as a symbolic
	-- displacement (rejected by codegen.computeAddr) — but the parser does not
	-- surface it as a mem operand, so reject it here rather than passing it
	-- through unchecked (or worse, rendering the numeric form as an immediate
	-- move — a silent load-to-imm rewrite). This fires for every operand
	-- position (load, store, ALU source, push/pop) — and, via the `*` indirect
	-- marker overriding the codeTarget exemption below, for indirect-branch
	-- memory operands (`jmp *myglobal` & co) — and runs before fp.classify
	-- so FP/vector forms (`movss myglobal, %xmm0`) get the same rejection.
	-- Exempt mnemonics whose bare operand is a CODE target or not memory at all:
	-- call/jmp/jcc (incl. numeric local labels, which numlabel already rewrote
	-- to plain syms), the loop*/xbegin label forms, `int` (its operand is the
	-- immediate vector, not an address), and lea (address arithmetic — no
	-- dereference; `leaq myglobal, %rax` materializes the address as a value,
	-- like the supported `leaq myglobal(%rip), %rax`).
	local codeTarget = m == "call" or m == "jmp" or m == "jcc" or m == "lea"
		or rawMnem == "int" or LABEL_TARGET_MNEM[rawMnem]
	-- High-byte register operands (%ah/%ch/%dh/%bh, AT&T or Intel spelling) are
	-- modeled EXACTLY by classifyHighByte (the pin machinery — see its comment):
	-- reads extract bits 8-15 of the enclosing register's web, writes merge into
	-- bits 8-15. xchg pins both operands already, so its own branch handles the
	-- validation (and the renderer keeps the spelled high-byte name). Anything
	-- with an unsupported shape gets a clean rejection there, before it can be
	-- silently mis-modeled at the low byte.
	local hasHighByte = false
	for _, o in ops do
		if isHighByteOp(o) then hasHighByte = true break end
	end
	if hasHighByte and not string.match(m, "^xchg[bwlq]?$") then
		return classifyHighByte(insn, m, ops)
	end
	local function rejectHighByteAddr(o)
		if isHighByteOp(o) then
			error("sarcasm: high-byte register " .. ((insn.syntax == "intel") and o.name or ("%" .. o.name))
				.. " cannot be a memory operand base/index (only 64-bit registers may"
				.. " address memory): " .. (insn.raw or rawMnem))
		end
	end
	for _, o in ops do
		if o.kind == "mem" then
			rejectHighByteAddr(o.base)
			rejectHighByteAddr(o.index)
			-- A SEGMENT register as the memory base (%fs/%gs/...): segment-relative
			-- addressing has no capability to bounds-check against (gas itself rejects
			-- the spelling — `(%fs)' is not a valid base/index expression — and the
			-- fs/gs BASES belong to the Fil-C runtime, see the wrfsbase rejection).
			if o.base and o.base.rc == "seg" then
				error("sarcasm: segment register cannot be a memory operand base"
					.. " (fs/gs-relative addressing cannot be bounds-checked): "
					.. (insn.raw or rawMnem))
			end
			-- A memory operand with NO base register and NO index is a DISPLACEMENT-
			-- only (absolute) address: gas assembles `movq (4096), %rax` as
			-- `mov 0x1000,%rax` — the same un-checkable absolute-access class as the
			-- moffs forms rejected below. (An index-only form — `(,%rax,8)` — is a
			-- checked access through the index register and stays supported.)
			if not o.base and not o.index then
				error("sarcasm: memory access with an absolute address cannot be"
					.. " bounds-checked (a memory operand with no base register is a"
					.. " displacement-only absolute address; global data access is not"
					.. " yet supported): " .. (insn.raw or rawMnem))
			end
		end
	end
	for _, o in ops do
		-- The exemption is about operands that NAME a code target; an
		-- operand-level indirect marker (`*`) on a sym/absolute operand is an
		-- indirect branch THROUGH an absolute memory operand (`jmp *myglobal`
		-- jumps to the qword stored AT myglobal): the same un-checkable
		-- absolute-read class as every other moffs form — and for jcc there is
		-- no indirect encoding at all (rendering would silently emit the DIRECT
		-- branch). So the flag overrides the exemption. Register- and
		-- register-based-memory indirect branches (`jmp *%rdi`, `call
		-- *(%rax,%rcx,8)`) are reg/mem operands, untouched here.
		if not codeTarget or o.indirect then
			if o.kind == "sym" then
				-- Segment-register operands (%gs / gs) parse as bare symbols (the
				-- parser does not classify segment registers); a move to/from one
				-- (`movw %gs, %cx`) is a register move, never an absolute memory
				-- operand. The pattern follows the input syntax, so a bare AT&T
				-- symbol merely NAMED gs/fs/... is still treated as an address.
				-- (cr/dr register moves were already rejected by unsafeReject.)
				-- The `*` marker on a segment-register spelling is never valid,
				-- so this exemption does not apply to indirect-marked operands.
				local t = string.lower(o.text)
				local seg
				if not o.indirect then
					if insn.syntax == "intel" then seg = string.match(t, "^[cdefgs]s$")
					else seg = string.match(t, "^%%[cdefgs]s$") end
				end
				if not seg then
					error("sarcasm: memory access with a symbolic address cannot be"
						.. " bounds-checked (global data access is not yet supported): "
						.. (insn.raw or rawMnem))
				end
			end
			if o.kind == "imm" and o.absAddr then
				error("sarcasm: memory access with an absolute address cannot be"
					.. " bounds-checked (global data access is not yet supported): "
					.. (insn.raw or rawMnem))
			end
		end
	end

	local fpc = fp.classify(insn)
	if fpc then return fpc end
	local defs, uses, rmw = {}, {}, {}
	local control, targets, fallsThrough = "none", nil, true

	local function useOperand(o)
		if isGpr(o) then table.insert(uses, o) elseif o and o.kind == "mem" then memRegs(o, uses) end
	end

	-- `.alloca size, alignment, result` (the `.alloca` directive): operands are in
	-- WRITTEN order (no AT&T reversal). size/alignment: an immediate, a 32/64-bit
	-- GPR (including `%fil_` pseudos and frame-slot webs), or a frame-relative
	-- spill-slot memory reference (converted to a slot web by the frame rewrite —
	-- provisionally unmodeled here so pre-rewrite validation sees no phantom use).
	-- result: a 64-bit GPR, a pseudo, or a spill slot. Anything else (FP/vector
	-- registers, immediates/symbols as result, heap memory, indirect markers,
	-- non-64-bit result widths) is a clean rejection.
	if m == "alloca" then  -- the `.alloca` directive only (dotAlloca marker from the parser)
		if not insn.dotAlloca then
			error("sarcasm: `alloca` is only supported as the `.alloca size, alignment, result` directive: " .. (insn.raw or rawMnem))
		end
		if #ops ~= 3 then
			error("sarcasm: `.alloca` takes exactly 3 operands (size, alignment, result): " .. (insn.raw or rawMnem))
		end
		local function allocaUse(o, what)
			if o.indirect then
				error("sarcasm: `.alloca` " .. what .. " has an indirect (`*`) marker, which is not supported: " .. (insn.raw or rawMnem))
			end
			if o.kind == "imm" then return end
			if isGpr(o) then
				if o.width ~= 64 and o.width ~= 32 then
					error("sarcasm: `.alloca` " .. what .. " must be a 32/64-bit register (narrow writes do not define the full value): " .. (insn.raw or rawMnem))
				end
				table.insert(uses, o)
				return
			end
			if o.kind == "mem" and o.base and (o.base.rc == "sp" or o.base.name == "rbp") then return end
			error("sarcasm: `.alloca` " .. what .. " must be an immediate, a GPR (including a pseudoregister), or a frame spill slot: " .. (insn.raw or rawMnem))
		end
		allocaUse(ops[1], "size")
		allocaUse(ops[2], "alignment")
		do
			local o = ops[3]
			if o.indirect then
				error("sarcasm: `.alloca` result has an indirect (`*`) marker, which is not supported: " .. (insn.raw or rawMnem))
			end
			if isGpr(o) then
				if o.width ~= 64 then
					error("sarcasm: `.alloca` result must be a 64-bit register (a narrower destination would truncate the pointer): " .. (insn.raw or rawMnem))
				end
				table.insert(defs, o)
			elseif o.kind == "mem" and o.base and (o.base.rc == "sp" or o.base.name == "rbp") then
			-- spill-slot result: converted by the frame rewrite; provisionally unmodeled.
			else
				error("sarcasm: `.alloca` result must be a 64-bit register, a pseudoregister, or a frame spill slot: " .. (insn.raw or rawMnem))
			end
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true }
	end

	if m == "jmp" then
		control, fallsThrough = "branch", false
		targets = {}
		for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		if ops[1] and (isGpr(ops[1]) or (ops[1].kind == "mem")) then control = "indirect"; useOperand(ops[1]) end
	elseif m == "jcc" then
		control = "condbranch"; targets = {}
		for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
		-- jrcxz/jecxz branch on their IMPLICIT counter register (rcx, or ecx for the
		-- 0x67-address-size jecxz; the 16-bit jcxz is not encodable in 64-bit mode and
		-- is rejected by fp.unsafeReject). The counter read must be modeled or the
		-- allocator could color the counter's web into some other register while the
		-- instruction silently tests the (stale) physical rcx. The fixedReg mark makes
		-- the transform precolor the counter's web to physical rcx, exactly like loop.
		if rawMnem == "jrcxz" or rawMnem == "jecxz" then
			local w = (rawMnem == "jecxz") and 32 or 64
			table.insert(uses, { kind = "reg", rc = "gpr", num = 1, width = w,
				name = (w == 32) and "ecx" or "rcx", implicit = true, fixedReg = "rcx" })
		end
	elseif m == "call" then
		control = "call"
		if ops[1] and (isGpr(ops[1]) or ops[1].kind == "mem") then control = "indcall"; useOperand(ops[1]) end
	elseif m == "ret" then
		control, fallsThrough = "return", false
	elseif m == "nop" or m == "ud2" or m == "leave" then
		-- no gpr effects we model (leave restores rsp/rbp; frame handled elsewhere)
	elseif string.match(m, "^loop[bwlq]?$") then
		-- LOOP/LOOPQ/LOOPL: a label-target CONDITIONAL branch whose counter is the
		-- implicit rcx (ecx for the 0x67-address-size loopl spelling). Hardware
		-- DECREMENTS the counter and branches on the decremented value. This used to
		-- classify as control="none" with no register effects, which (a) defeated the
		-- fall-off reachability check — a `loop` to a label near the end of the body
		-- compiled and executed into sarcasm's own next emission at runtime — (b) left
		-- the physical rcx decrement unmodeled, silently desyncing any live rcx web
		-- (hardware 0x1233 after the loop, sarcasm still 0x1234), and (c) let a `loop`
		-- to a non-local label bypass the branch-target (tail-call) rejection.
		-- Model it as a conditional branch with USE+DEF+RMW of the implicit counter
		-- (the decrement always happens; the branch is on the decremented value). The
		-- use and def deliberately SHARE one operand table: the lift keys opTemp by
		-- table identity, and the RMW unification is exactly what makes the counter
		-- flow through the instruction as one web. fixedReg makes the transform
		-- precolor that web to physical rcx — the modeled decrement must land in the
		-- register the instruction actually decrements. (The ZF-consuming variants
		-- loope/loopz/loopne/loopnz are rejected by fp.unsafeReject: injected
		-- memory-access checks clobber flags, so their pending condition could not be
		-- honored. See also the jrcxz/jecxz use modeling in the jcc branch above.)
		if m == "loopb" or m == "loopw" then
			error("sarcasm: " .. rawMnem .. " is a 16-bit-counter form that is not"
				.. " encodable in 64-bit mode (use loop, loopq or loopl): "
				.. (insn.raw or rawMnem))
		end
		local w = (m == "loopl") and 32 or 64
		local cnt = { kind = "reg", rc = "gpr", num = 1, width = w,
			name = (w == 32) and "ecx" or "rcx", implicit = true, fixedReg = "rcx" }
		table.insert(uses, cnt)
		table.insert(defs, cnt)
		table.insert(rmw, 1)
		control = "condbranch"; targets = {}
		for _, o in ops do if o.kind == "sym" then table.insert(targets, o.text) end end
	elseif m == "cltq" or m == "cbw" or m == "cwde" then
		-- single-register sign extension (al->ax / ax->eax / eax->rax): an RMW of
		-- the rax web; codegen's lowerSpecial rewrites it to a renameable movsxx.
		table.insert(defs, { kind="reg", rc="gpr", num=0, width=64, name="rax" })
		table.insert(uses, { kind="reg", rc="gpr", num=0, width=64, name="rax" })
		table.insert(rmw, 0)
	elseif m == "cqo" or m == "cdq" or m == "cwd" then
		-- sign-extend the accumulator into the paired dx register (rax->rdx:rax,
		-- eax->edx:eax, ax->dx:ax): implicit USE of rax, implicit DEF of rdx,
		-- pinned to the physical registers by codegen's emitPinned (regalloc must
		-- not rename either side, or the extension silently reads/writes the
		-- wrong webs — this is the div/idiv setup idiom).
		-- NB: every implicit operand OCCURRENCE is a fresh table — the lift keys
		-- node.opTemp by table identity, so a def table and a use table for the
		-- same register must never be shared (the use's temp would clobber the
		-- def's in opTemp and the pin-out copy would write the wrong web).
		local axU = { kind="reg", rc="gpr", num=0, width=64, name="rax", implicit=true }
		local dxD = { kind="reg", rc="gpr", num=2, width=64, name="rdx", implicit=true }
		table.insert(uses, axU)
		table.insert(defs, dxD)
		if m == "cwd" then
			-- cwd writes only %dx: bits 16-63 of the rdx web are PRESERVED — a
			-- partial write. Model rdx as an implicit RMW pinned in exactly like
			-- the accumulator: emitPinned copies the reaching rdx web into
			-- physical rdx before the instruction, so the bits cwd preserves are
			-- the web's own (a pure def would let the previous web be killed
			-- while the hardware keeps its bits — the partial-write miscompile).
			-- (cdq writes %edx — zero-extending, a full def; cqo writes %rdx.)
			local dxU = { kind="reg", rc="gpr", num=2, width=64, name="rdx", implicit=true }
			table.insert(uses, dxU)
			table.insert(rmw, 2)
			return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
				pin = { uses = { axU, dxU }, defs = { dxD } } }
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = { axU }, defs = { dxD } } }
	elseif m == "div" or m == "idiv" or m == "mul" or (m == "imul" and #ops == 1) then
		-- One-operand multiply/divide: the explicit operand is the divisor /
		-- multiplicand (a use); the accumulator pair rdx:rax is an implicit
		-- operand the def/use model must name, or regalloc will color those webs
		-- elsewhere and the dividend/quotient silently corrupts. The implicit
		-- registers are pinned to the physical registers by codegen's emitPinned:
		-- the reaching webs are copied into physical rax/rdx before the
		-- instruction and the result webs are copied back out after it
		-- (coalescing makes the copies free when the user already had the values
		-- in rax/rdx — the common case).
		--   div/idiv: dividend rdx:rax (uses both), quotient rax + remainder rdx
		--     (defs both). The 8-bit form uses/defs AX alone — modeled as rax.
		--   mul/imul: multiplicand rax (use), product rdx:rax (defs both). The
		--     8-bit form uses AL and defs AX — both modeled as rax alone.
		-- (2/3-operand imul has NO implicit effects and keeps the ALU2 model.)
		local w = insn.opsize
		if not w then
			for _, o in ops do
				if isGpr(o) then w = math.max(1, math.floor((o.width or 64) / 8)) break end
			end
		end
		if not w then
			for _, o in ops do
				if o and o.kind == "mem" and o.size then w = o.size break end
			end
		end
		if not w then
			error("sarcasm: cannot determine the memory access width of '" .. rawMnem
				.. "' (unsized memory operand is ambiguous; use a size suffix or PTR annotation): "
				.. (insn.raw or rawMnem))
		end
		for _, o in ops do useOperand(o) end
		-- fresh table per implicit occurrence (see the cqo comment above)
		local function imp(num)
			return { kind="reg", rc="gpr", num=num, width=w * 8,
				name = (num == 0) and "rax" or "rdx", implicit=true }
		end
		local pin
		if w == 1 then
			local axU, axD = imp(0), imp(0)
			table.insert(uses, axU); table.insert(defs, axD)
			pin = { uses = { axU }, defs = { axD } }
		elseif m == "mul" or m == "imul" then
			local axU, axD, dxD = imp(0), imp(0), imp(2)
			table.insert(uses, axU)
			table.insert(defs, axD); table.insert(defs, dxD)
			if w == 2 then
				-- mulw/imulw write only %dx: bits 16-63 of the rdx web are
				-- PRESERVED — a partial write. Pin the reaching rdx web in
				-- (exactly like cwd) so the preserved bits are the web's own.
				-- (mull writes %edx — zero-extending; mulq writes %rdx; div/
				-- idiv already read rdx as the dividend's high half.)
				local dxU = imp(2)
				table.insert(uses, dxU)
				table.insert(rmw, 2)
				pin = { uses = { axU, dxU }, defs = { axD, dxD } }
			else
				pin = { uses = { axU }, defs = { axD, dxD } }
			end
		else
			local axU, dxU, axD, dxD = imp(0), imp(2), imp(0), imp(2)
			table.insert(uses, axU); table.insert(uses, dxU)
			table.insert(defs, axD); table.insert(defs, dxD)
			pin = { uses = { axU, dxU }, defs = { axD, dxD } }
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true, pin = pin }
	elseif m == "mulx" then
		-- BMI2 MULX dst1, dst2, src2: dst1:dst2 = rdx * src2 — rdx is an implicit
		-- SOURCE (pinned in by emitPinned, like the div family) but is NOT
		-- written; both explicit destinations are ordinary renameable defs. (The
		-- conservative first-reg-def model was wrong twice over: the second
		-- destination was unmodeled and the implicit rdx use invisible.)
		local dx = { kind="reg", rc="gpr", num=2, width=64, name="rdx", implicit=true }
		if isGpr(ops[1]) then table.insert(defs, ops[1]) end
		if isGpr(ops[2]) then table.insert(defs, ops[2]) end
		for i = 3, #ops do useOperand(ops[i]) end
		table.insert(uses, dx)
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = { dx }, defs = {} } }
	elseif m == "cmpxchg" then
		-- CMPXCHG dest, src: the accumulator (al/ax/eax/rax by size) is an
		-- implicit read-modify-write operand (compared with dest; overwritten
		-- with dest on mismatch), the explicit dest is an RMW, src a use. The
		-- accumulator is pinned to physical rax by emitPinned. (The paired
		-- double-width forms cmpxchg8b/cmpxchg16b are modeled by the next
		-- branch.)
		local w = insn.opsize
		if not w then
			for _, o in ops do
				if isGpr(o) then w = math.max(1, math.floor((o.width or 64) / 8)) break end
			end
		end
		-- fresh table per implicit accumulator occurrence (see the cqo comment)
		local accU = { kind="reg", rc="gpr", num=0, width=(w or 64) * 8, name="rax", implicit=true }
		local accD = { kind="reg", rc="gpr", num=0, width=(w or 64) * 8, name="rax", implicit=true }
		if isGpr(ops[1]) then
			table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		elseif ops[1] and ops[1].kind == "mem" then
			memRegs(ops[1], uses)
		end
		for i = 2, #ops do useOperand(ops[i]) end
		table.insert(uses, accU); table.insert(defs, accD)
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = { accU }, defs = { accD } } }
	elseif m == "cmpxchg8b" or m == "cmpxchg16b" then
		-- CMPXCHG8B m64 / CMPXCHG16B m128: the ONLY encodable operand is memory
		-- (there is no register form). Implicit operands: the expected value
		-- edx:eax (resp. rdx:rax) is an implicit read-modify-write PAIR
		-- (compared with dest; overwritten with the old dest on mismatch) and
		-- ecx:ebx (resp. rcx:rbx) is the implicit new-value source. All four
		-- are pinned to their physical registers by codegen's emitPinned,
		-- exactly like the div/mul/cmpxchg family: the reaching webs of the
		-- implicit-use registers are copied into the physical registers before
		-- the instruction and the mismatch-writeback results copied back out
		-- into fresh webs after it; regalloc colors everything else AROUND the
		-- fixed rax/rdx/rbx/rcx effects via the precolored temps (rbx needs no
		-- special treatment — it is an ordinary allocatable callee-saved GPR,
		-- and the pinned instruction's def/use temps keep any other web from
		-- being colored into it across the pin).
		if not (ops[1] and ops[1].kind == "mem") then
			error("sarcasm: " .. rawMnem .. " requires a memory operand (it has no"
				.. " register form): " .. (insn.raw or rawMnem))
		end
		memRegs(ops[1], uses)
		-- fresh table per implicit occurrence (see the cqo comment above)
		local function imp(num, nm)
			return { kind="reg", rc="gpr", num=num, width=64, name=nm, implicit=true }
		end
		local axU, dxU, bxU, cxU = imp(0, "rax"), imp(2, "rdx"), imp(3, "rbx"), imp(1, "rcx")
		local axD, dxD = imp(0, "rax"), imp(2, "rdx")
		table.insert(uses, axU); table.insert(uses, dxU)
		table.insert(uses, bxU); table.insert(uses, cxU)
		table.insert(defs, axD); table.insert(defs, dxD)
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = { axU, dxU, bxU, cxU }, defs = { axD, dxD } } }
	elseif m == "bt" or m == "bts" or m == "btr" or m == "btc" then
		-- bt/bts/btr/btc base, index (dest-first operands): the base is a bit
		-- string READ (all four) and — for bts/btr/btc — WRITTEN (the indexed bit
		-- is set/reset/complemented), the index is a use, CF takes the tested bit
		-- (a partial flag write, FLAG_WRITE_NO_DEF). The conservative
		-- unknown-mnemonic model was wrong twice over: it def'd the base (so the
		-- instruction read a stale web — sha256_block_data_order's `btq $41,%r9`
		-- dispatch test read the wrong register) and missed the index use. A
		-- MEMORY base is a bit-string access of unbounded extent (the index
		-- selects the addressed qword AROUND the operand), which cannot be
		-- bounds-checked — reject it.
		if ops[1] and ops[1].kind == "mem" then
			error("sarcasm: " .. rawMnem .. " with a memory base is not supported"
				.. " (a bit-string access of unbounded extent cannot be bounds-checked;"
				.. " load the value into a register first): " .. (insn.raw or rawMnem))
		end
		if isGpr(ops[1]) then
			table.insert(uses, ops[1])
			if m ~= "bt" then
				table.insert(defs, ops[1])
				table.insert(rmw, ops[1].num)
			end
		end
		for i = 2, #ops do useOperand(ops[i]) end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true }
	elseif m == "cpuid" or m == "rdtsc" or m == "rdtscp" or m == "xgetbv"
		or m == "rdpkru" or m == "wrpkru" or m == "monitor" or m == "mwait" then
		-- Zero-explicit-operand instructions whose ONLY register effects are
		-- implicit GPRs, modeled with the same pin mechanism as the
		-- div/mul/cmpxchg family (fresh table per implicit occurrence — see the
		-- cqo comment above): codegen's emitPinned copies the reaching webs of
		-- the implicit uses into the physical registers before the instruction
		-- and the physical results back out into fresh webs after it, and
		-- regalloc colors everything else AROUND the fixed registers (rbx is an
		-- ordinary allocatable callee-saved GPR — cpuid's implicit rbx def
		-- needs no special treatment). The instruction renders as the bare
		-- mnemonic. Effects modeled (SDM Vol. 2B):
		--   cpuid:   uses eax (leaf) + ecx (sub-leaf); defs eax/ebx/ecx/edx.
		--   rdtsc:   defs eax/edx (edx:eax = TSC). No uses.
		--   rdtscp:  defs eax/edx/ecx (edx:eax = TSC, ecx = TSC_AUX). No uses.
		--   xgetbv:  uses ecx (XCR index); defs eax/edx.
		--   rdpkru:  uses ecx (must be 0); defs eax (PKRU) + edx (zeroed).
		--   wrpkru:  uses eax (PKRU value) + ecx (=0) + edx (=0). No defs.
		--   monitor: uses rax (64-bit address) + ecx (extensions) + edx (hints).
		--            No defs, and NO memory operand: MONITOR does not access
		--            memory, so there is nothing to bounds-check (a wild
		--            address can only fault — an uncatchable signal and a safe
		--            halt, which the safety model accepts).
		--   mwait:   uses eax (hints) + ecx (extensions). No defs.
		-- (umwait/tpause stay rejected: their implicit EDX:EAX TSC-deadline use
		-- pairs an explicit operand with implicit ones in a way the signature
		-- marshalling does not yet model.)
		if #ops ~= 0 then
			error("sarcasm: " .. rawMnem .. " takes no explicit operands (gas's"
				.. " operand decorations like `monitor %rax, %ecx, %edx` are not"
				.. " supported; write the bare mnemonic): " .. (insn.raw or rawMnem))
		end
		-- fresh table per implicit occurrence (see the cqo comment above)
		local function imp(num, nm, w)
			return { kind="reg", rc="gpr", num=num, width=w, name=nm, implicit=true }
		end
		local pinUses, pinDefs = {}, {}
		local function u(num, nm, w)
			local r = imp(num, nm, w)
			table.insert(uses, r); table.insert(pinUses, r)
		end
		local function d(num, nm, w)
			local r = imp(num, nm, w)
			table.insert(defs, r); table.insert(pinDefs, r)
		end
		if m == "cpuid" then
			u(0, "rax", 32); u(1, "rcx", 32)
			d(0, "rax", 32); d(3, "rbx", 32); d(1, "rcx", 32); d(2, "rdx", 32)
		elseif m == "rdtsc" then
			d(0, "rax", 32); d(2, "rdx", 32)
		elseif m == "rdtscp" then
			d(0, "rax", 32); d(2, "rdx", 32); d(1, "rcx", 32)
		elseif m == "xgetbv" or m == "rdpkru" then
			u(1, "rcx", 32); d(0, "rax", 32); d(2, "rdx", 32)
		elseif m == "wrpkru" then
			u(0, "rax", 32); u(1, "rcx", 32); u(2, "rdx", 32)
		elseif m == "monitor" then
			u(0, "rax", 64); u(1, "rcx", 32); u(2, "rdx", 32)
		else  -- mwait
			u(0, "rax", 32); u(1, "rcx", 32)
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = pinUses, defs = pinDefs } }
	elseif m == "lahf" or m == "sahf" then
		-- LAHF / SAHF: the flag byte moves between EFLAGS and %ah (bits 8-15 of the
		-- rax web; the web model has no subregister view — the SAME web as %al — so
		-- the whole 64-bit rax web is the modeled operand). No explicit operands.
		--   lahf: reads the flags, writes bits 8-15 of rax — an implicit
		--     read-modify-write of the rax web, PINNED to physical rax exactly like
		--     the 8-bit div form: codegen's emitPinned copies the web's 64-bit value
		--     into physical rax, emits the bare `lahf`, and copies rax back out —
		--     wherever the web is colored, AH lands at bits 8-15 of the colored
		--     register (and when it IS rax the copies coalesce away as self-moves).
		--     (Reading the byte back: a literal `%ah` operand is now modeled
		--     exactly by the high-byte gate — the pin lands the web in physical rax
		--     and the read extracts bits 8-15, so `movzbl %ah, %edx` sees exactly
		--     the flags lahf wrote.)
		--   sahf: reads bits 8-15 of rax, writes SF/ZF/AF/PF/CF (OF UNCHANGED —
		--     hence in NEITHER flag-liveness table, like every partial write) —
		--     modeled as a uses-only pin (like mulx): the reaching web is copied
		--     into physical rax before the instruction; nothing is written back.
		-- Without the pin the %ah write would be invisible to the web model (the
		-- old rax web neither killed nor updated) and regalloc could rename rax
		-- across the instruction.
		if #ops ~= 0 then
			error("sarcasm: " .. rawMnem .. " takes no explicit operands (its only"
				.. " operand is the implicit %ah / flag byte): " .. (insn.raw or rawMnem))
		end
		-- fresh table per implicit occurrence (see the cqo comment above)
		local axU = { kind="reg", rc="gpr", num=0, width=64, name="rax", implicit=true }
		table.insert(uses, axU)
		if m == "lahf" then
			local axD = { kind="reg", rc="gpr", num=0, width=64, name="rax", implicit=true }
			table.insert(defs, axD)
			table.insert(rmw, 0)
			return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
				pin = { uses = { axU }, defs = { axD } } }
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = { axU }, defs = {} } }
	elseif FLAGS_ONLY[m] then
		for _, o in ops do useOperand(o) end
	elseif ALU2[m] then
		-- dst = dst OP src : ops[1] is def+use, rest are uses
		if isGpr(ops[1]) then table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		elseif ops[1] and ops[1].kind == "mem" then memRegs(ops[1], uses) end
		for i = 2, #ops do useOperand(ops[i]) end
		-- Register-count shifts/rotates (`shlq %cl, %rdx` & co): the count register
		-- is ALWAYS %cl — the only register-count encoding in the ISA. The count's
		-- web must therefore LIVE in physical rcx: left renameable, regalloc colors
		-- it anywhere and the emitted `shlq %sil, %rdx` either fails to assemble or
		-- silently shifts by the wrong register. Pin it with the emitPinned
		-- machinery (the count's web is copied into physical rcx before the
		-- instruction; the rendered count operand is the fixed %cl).
		if SHIFT_COUNT[m] and isGpr(ops[2]) then
			if ops[2].width ~= 8 or ops[2].num ~= 1 then
				error("sarcasm: a shift/rotate count register must be %cl (the only"
					.. " register-count encoding in the ISA): " .. (insn.raw or rawMnem))
			end
			return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
				pin = { uses = { ops[2] }, defs = {} } }
		end
	elseif DST_ONLY[m] then
		if isGpr(ops[1]) then table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		elseif ops[1] and ops[1].kind == "mem" then memRegs(ops[1], uses) end
	elseif string.match(m, "^bswap[bwlq]?$") then
		-- BSWAP r32/r64: single-operand byte swap — an RMW of that ONE web, passed
		-- through raw. (The conservative first-reg-def fallback modeled the operand
		-- as a pure DEF with no use: the def web collided onto a fresh temp the
		-- allocator colored freely, so the emitted bswap read an UNINITIALIZED
		-- register and the operand's real value was silently dropped.) The RMW model
		-- keeps the web flowing through the instruction, exactly like inc/dec/neg/not.
		-- A 32-bit bswap zero-extends into the full web, which the identity-RMW model
		-- expresses truthfully (the web IS the post-instruction register content).
		-- More than one operand is not an encoding (gas: "number of operands
		-- mismatch") — reject it here with a message that names the actual problem
		-- instead of letting the extra operand ride into a downstream assembler
		-- error.
		if #ops ~= 1 then
			error("sarcasm: bswap takes exactly one register operand (it has no"
				.. " two-operand form): " .. (insn.raw or rawMnem))
		end
		if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr") then
			error("sarcasm: bswap takes exactly one register operand (it has no memory"
				.. " or immediate form): " .. (insn.raw or rawMnem))
		end
		-- bswap is r32/r64-ONLY: the 16-bit and 8-bit forms do not exist (the byte
		-- swap of an 8-bit register is a no-op; gas rejects `bswapw %ax`/`bswapb %al`
		-- with "invalid instruction suffix for `bswap'"). Reject at classify with a
		-- message naming the actual problem instead of letting the unencodable
		-- spelling ride into that downstream assembler error from the temp file.
		if ops[1].width ~= 32 and ops[1].width ~= 64 then
			error("sarcasm: bswap requires a 32-bit or 64-bit register operand"
				.. " (bswapw/bswapb are not encodable): " .. (insn.raw or rawMnem))
		end
		table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
	elseif m == "movbe" then
		-- MOVBE: the byte-swapping move to/from MEMORY (m16/m32/m64 <-> r16/r32/r64):
		-- a load form (`movbeq (%rsi), %rax` — load, then byte-swap into the
		-- destination) and a store form (`movbeq %rax, (%rsi)` — byte-swap the
		-- source, then store). Modeled exactly like the ordinary mov load/store:
		-- the memory operand rides the normal bounds-checked path, the register
		-- operand is an ordinary renameable def (load) or use (store), and the
		-- spelled instruction passes through (the swap is a register-value
		-- transformation the web model need not see — like bswap). The
		-- register-to-register spelling is not a baseline x86-64 encoding (gas
		-- only accepts it as an APX instruction), and the 8-bit form does not
		-- exist (a byte swap of one byte is a plain move; gas rejects movbeb with
		-- "invalid instruction suffix"): reject both cleanly.
		if #ops ~= 2 then
			error("sarcasm: movbe takes exactly two operands (a byte-swapping move"
				.. " between a register and memory): " .. (insn.raw or rawMnem))
		end
		local dst, src = ops[1], ops[2]
		if dst.kind == "mem" and isGpr(src) then
			-- store: byte-swap src, store to mem
			if src.width == 8 then
				error("sarcasm: movbeb is not encodable (movbe exists only in 16/32/64-bit"
					.. " forms; an 8-bit byte swap is a plain movb): " .. (insn.raw or rawMnem))
			end
			memRegs(dst, uses)
			table.insert(uses, src)
		elseif isGpr(dst) and src.kind == "mem" then
			-- load: load from mem, byte-swap into dst
			if dst.width == 8 then
				error("sarcasm: movbeb is not encodable (movbe exists only in 16/32/64-bit"
					.. " forms; an 8-bit byte swap is a plain movb): " .. (insn.raw or rawMnem))
			end
			table.insert(defs, dst)
			-- movbew writes only 16 bits of its destination (a partial write).
			if partialRegWrite(dst) then
				table.insert(uses, dst); table.insert(rmw, dst.num)
			end
			memRegs(src, uses)
		else
			error("sarcasm: movbe requires a memory operand (the register-to-register"
				.. " spelling is not a baseline x86-64 encoding; use a mov plus bswap"
				.. " sequence instead): " .. (insn.raw or rawMnem))
		end
	elseif m == "shld" or m == "shrd" then
		-- SHLD/SHRD dst, src, count (dest-first): double-precision shift —
		-- dst = (dst:src) shifted left (shld) or right (shrd) by the count. The
		-- destination is read AND written (an RMW — the unknown-mnemonic fallback
		-- used to model it as a pure def, silently dropping the destination's
		-- contribution to the result), the source is a plain use, and the count is
		-- an immediate or %cl — the only register-count encoding, pinned to
		-- physical rcx exactly like the plain shift/rotate count (ALU2 branch).
		if #ops ~= 3 then
			error("sarcasm: " .. rawMnem .. " takes exactly three operands (destination,"
				.. " source, count): " .. (insn.raw or rawMnem))
		end
		if isGpr(ops[1]) then
			table.insert(defs, ops[1]); table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		elseif ops[1] and ops[1].kind == "mem" then
			memRegs(ops[1], uses)
		end
		useOperand(ops[2])
		if isGpr(ops[3]) then
			if ops[3].width ~= 8 or ops[3].num ~= 1 then
				error("sarcasm: a shift/rotate count register must be %cl (the only"
					.. " register-count encoding in the ISA): " .. (insn.raw or rawMnem))
			end
			table.insert(uses, ops[3])
			return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
				pin = { uses = { ops[3] }, defs = {} } }
		end
	elseif string.match(m, "^xchg[bwlq]?$") then
		-- XCHG: BOTH operands are read AND written (each receives the other's value).
		-- A register-to-register xchg is modeled with the implicit-GPR pin machinery
		-- (exactly like cmpxchg): emitPinned copies both webs into their physical
		-- registers, the passthrough xchg swaps the physical registers, and each
		-- physical register is copied back out into the FRESH web its register's def
		-- site created — so the def of the first register's web receives the second
		-- operand's value, which is the hardware dataflow. (The conservative
		-- first-reg-def fallback defined only the first operand and dropped the swap
		-- entirely; a plain def+use pair cannot express the crossing without colliding
		-- both webs into one temp.)
		--
		-- A memory-operand xchg is an implicitly LOCKED read-modify-write in hardware
		-- (the lock happens with or without a `lock` prefix) and is not one of the
		-- exactly-modeled memory RMWs: reject.
		if (ops[1] and ops[1].kind == "mem") or (ops[2] and ops[2].kind == "mem") then
			error("sarcasm: xchg with a memory operand is an implicitly locked read-modify-write"
				.. " (the hardware locks it even without a `lock` prefix; it is not one of the"
				.. " exactly-modeled memory RMWs; cannot prove memory safety): " .. (insn.raw or rawMnem))
		end
		if not (isGpr(ops[1]) and isGpr(ops[2])) then
			error("sarcasm: xchg needs two general registers (stack-pointer and vector-register"
				.. " exchanges are not supported): " .. (insn.raw or rawMnem))
		end
		-- A high-byte operand (%ah/%ch/%dh/%bh) is modeled exactly by the pin
		-- (the enclosing web lands in the spelled physical register and the
		-- renderer keeps the high-byte name) — but only when the OTHER operand
		-- keeps the instruction REX-free: a byte partner must be one of
		-- %al/%cl/%dl/%bl (nums 0-3), a 16/32-bit partner must be num < 8, and
		-- no 64-bit partner (REX.W) may appear. (The high-byte gate above
		-- exempts xchg because this branch already pins both operands.)
		for _, o in ops do
			if isHighByteOp(o) then
				local partner = (o == ops[1]) and ops[2] or ops[1]
				local bad = (partner.width == 8 and partner.num > 3)
					or (partner.width == 16 and partner.num > 7)
					or (partner.width == 32 and partner.num > 7)
					or (partner.width ~= 8 and partner.width ~= 16 and partner.width ~= 32)
				if bad then
					error("sarcasm: high-byte register " .. ((insn.syntax == "intel") and o.name or ("%" .. o.name))
						.. " cannot be combined with " .. ((insn.syntax == "intel") and partner.name or ("%" .. partner.name))
						.. " (the partner requires a REX prefix, which a high-byte register"
						.. " cannot combine with): " .. (insn.raw or rawMnem))
				end
			end
		end
		-- fresh table per def occurrence (see the cqo comment above): the def sites are
		-- new webs, distinct from the use occurrences of the same registers.
		local function imp(o)
			return { kind="reg", rc="gpr", num=o.num, width=64, name=o.name, implicit=true }
		end
		local d1, d2 = imp(ops[1]), imp(ops[2])
		table.insert(defs, d1); table.insert(defs, d2)
		table.insert(uses, ops[1]); table.insert(uses, ops[2])
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true,
			pin = { uses = { ops[1], ops[2] }, defs = { d1, d2 } } }
	elseif m == "mov" or m == "movzx" or m == "movsx" or m == "lea" or m == "cmov" or m == "setcc" then
		-- ops[1] def, ops[2..] use (mem regs used); a store (mem dst) uses the value instead.
		--
		-- setcc models as a FULL-WIDTH def of its destination web (like mov): the web
		-- model has no subregister view (webs are keyed by register number), but the
		-- hardware writes only the low byte. The model is made truthful by codegen's
		-- lowerSpecial, which rewrites a register-destination setcc into setcc +
		-- zero-extension (a 32-bit write zeroes bits 32-63, so the whole web reads
		-- 0/1 after the pair; the extension writes no flags). A memory destination
		-- is a complete 8-bit store and needs none of that.
		-- (A high-byte destination (%ah) never reaches here: the high-byte gate at
		-- the top of classify models it exactly as a pinned RMW of the enclosing
		-- register's web — no widening is needed, and lowerSpecial skips pinned
		-- nodes.)
		if isGpr(ops[1]) then table.insert(defs, ops[1])
		elseif ops[1] and ops[1].kind == "mem" then memRegs(ops[1], uses) end
		if (ops[1] and ops[1].kind == "mem") and isGpr(ops[2]) then table.insert(uses, ops[2]) end
		if m == "cmov" and isGpr(ops[1]) then table.insert(uses, ops[1]); table.insert(rmw, ops[1].num) end
		-- A PARTIAL-WIDTH destination (an 8/16-bit low register, or a narrow
		-- frame-slot pseudo-register) read-modifies the enclosing web: the
		-- hardware preserves every bit above the written width (see
		-- partialRegWrite). Model the def as an RMW or the previous web is
		-- silently killed and the preserved bits become whatever regalloc left
		-- in the colored register. movzbl/movzwl & co keep their pure defs (a
		-- zero/sign-extending write to a 32/64-bit register IS a full def).
		-- setcc with a REAL register destination also stays a full def:
		-- codegen's lowerSpecial appends the movzx that makes the full-width
		-- model true (a slot destination gets no widening — its byte store is
		-- a plain partial write, modeled here). cmov is already an RMW above.
		if m ~= "cmov" and isGpr(ops[1]) and partialRegWrite(ops[1])
			and not (m == "setcc" and not ops[1].isSlot) then
			table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
		end
		for i = 2, #ops do
			local o = ops[i]
			if o and o.kind == "mem" then memRegs(o, uses)
			elseif isGpr(o) and not (ops[1] and ops[1].kind == "mem") then table.insert(uses, o) end
		end
	elseif m == "push" then
		useOperand(ops[1])
	elseif m == "pop" then
		-- popw writes only 16 bits of its destination (a partial write of the
		-- enclosing web); popq is a full def.
		if isGpr(ops[1]) then
			table.insert(defs, ops[1])
			if partialRegWrite(ops[1]) then
				table.insert(uses, ops[1]); table.insert(rmw, ops[1].num)
			end
		end
	elseif m == "cld" or m == "std" then
		-- CLD/STD write only the direction flag (DF=0 / DF=1): no GPR effects,
		-- no memory effects. Modeled explicitly (NOT unknown) so the flag
		-- tables name the DF write precisely (flagDefMask) and the rep
		-- movs*/stos* lowering's DF check sees a live DF use feeding it.
		-- (As unknown register-only forms they used to pass through with an
		-- all-live conservative use; the precise model keeps that soundness
		-- with fewer spurious saves.)
		if #ops ~= 0 then
			error("sarcasm: " .. rawMnem .. " takes no operands (it operates on the"
				.. " implicit direction flag): " .. (insn.raw or rawMnem))
		end
	else
		-- Unknown mnemonic. With a memory operand there is no way to determine the
		-- access width, so any default-width bounds check is unsound (enqcmd's m512
		-- source, an unknown vector load, a gather/scatter spelling the fp table
		-- missed, ...): reject. Register-only unknown forms still pass through with
		-- the conservative first-reg-def model below.
		for _, o in ops do
			if o.kind == "mem" then
				error("sarcasm: cannot determine the memory access width of '" .. rawMnem
					.. "' (cannot prove memory safety): " .. (insn.raw or rawMnem))
			end
		end
		-- unknown: conservative (first reg def, rest use). A partial-width first
		-- register def additionally READS its web (an 8/16-bit write preserves
		-- the upper bits — see partialRegWrite).
		local first = true
		for _, o in ops do
			if isGpr(o) then
				if first then
					table.insert(defs, o)
					if partialRegWrite(o) then
						table.insert(uses, o); table.insert(rmw, o.num)
					end
					first = false
				else table.insert(uses, o) end
			elseif o and o.kind == "mem" then memRegs(o, uses) end
		end
		return { defs = defs, uses = uses, rmw = rmw, control = "none", fallsThrough = true, unknown = true }
	end

	return { defs = defs, uses = uses, rmw = rmw, control = control, targets = targets, fallsThrough = fallsThrough }
end

x86.isGpr = isGpr

-- The register-defined memory access width (bytes) of a GPR instruction, or nil when
-- the instruction's memory width is NOT register-defined: the movzx/movsx narrowing
-- family and lea are size-driven/memory-neutral, and no-GPR-operand forms
-- (push/pop/call/jmp mem forms, inc/dec/neg/not mem-only forms) legitimately take
-- their width from the PTR annotation or AT&T suffix. Used by the heap path
-- (x86_64_codegen.accessSizeAlign) and the frame rewrite (x86_64_frame) to reject an
-- Intel PTR size annotation that CONTRADICTS the true access width — the emitted
-- instruction encodes the register width regardless of the annotation
-- (`mov WORD PTR [rdi], rax` stores 8 bytes, not 2).
function x86.gprAccessRegWidth(stmt)
	local b = x86.baseMnemonic(string.lower(stmt.mnemonic))
	if b == "lea" or b == "movzx" or b == "movsx" then return nil end
	if stmt.opsize then return stmt.opsize end
	for _, o in (stmt.operands or {}) do
		if isGpr(o) then return math.max(1, math.floor((o.width or 64) / 8)) end
	end
	return nil
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
-- GPR-class args are packed DENSELY (a pointer-class arg occupies two consecutive
-- slots -- intval, lower; a scalar-class arg occupies one slot, intval only),
-- matching pizlonated clang output. FLOAT/DOUBLE args do NOT consume GPR slots:
-- they pass in the vector registers xmm0..xmm7 in declaration order among the FP
-- args alone (a float in the low 32 bits, a double in the full 64 — two floats
-- never pack one register), exactly like pizlonated clang (verified:
-- long(double,int,float,long) places the double in xmm0, the int in rdx, the
-- float in xmm1, the long in rcx). Clang's x86_64 fast CC has only FOUR dense
-- argument GPRs (rdx, rcx, r8, r9); GPR argument WORDS beyond the fourth are
-- passed on the stack as densely packed 8-byte slots in declaration word order,
-- the caller storing word w (0-based, w >= 4) at 8*(w-4)(%rsp) before the call
-- (the callee reads it at 8+8*(w-4)(%rsp), above its return address) — verified
-- against pizlonated clang callers and callees for 5..14-word signatures, incl.
-- a pointer straddling the r9/stack boundary (void(int,int,int,ptr): p.iv in r9,
-- p.lo on the stack). sarcasm marshals those stack words in both directions
-- (see transform.luau's entry unpack and callsite marshalling). FP args beyond
-- the eighth (xmm0..xmm7) are still rejected.
local DENSE_ARG_REGS = { 2, 1, 8, 9 }        -- rdx, rcx, r8, r9
local DENSE_ARG_REG_COUNT = 4                -- words beyond go on the stack
local function denseArgSlot(argTypes, k)
	local slot = 1
	for i = 1, k - 1 do
		if sig.isFpClass(argTypes[i]) then
			-- FP arg (float/double): passes in an xmm register, consumes no GPR slot
		elseif argTypes[i] == 7 then
			slot += 2                           -- 7 = TYPE_PTR (see sig.luau)
		else
			slot += 1
		end
	end
	return slot
end
-- xmm-register index (0-based) of the FP arg at declaration position k: the count
-- of FP args before it (FP args allocate xmm0..xmm7 in declaration order among FP
-- args). Also the xmm index of the FP RETURN (ordinal 0: xmm0).
local function fpArgOrdinal(argTypes, k)
	local m = 0
	for i = 1, k - 1 do
		if sig.isFpClass(argTypes[i]) then m += 1 end
	end
	return m
end
-- SysV C-call argument registers: rdi, rsi, rdx, rcx, r8, r9. Doubles as the
-- AUTHOR-VISIBLE yolo input-asm register sequence for GPR arguments (the entry
-- unpack copies the internal dense fast-CC regs rdx,rcx,r8,r9 into them).
x86.SYSV = { 7, 6, 2, 1, 8, 9 }

x86.CC = {
	myth = 7,       -- rdi
	fo = 6,         -- rsi
	argIv = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k)] end,
	argLo = function(argTypes, k) return DENSE_ARG_REGS[denseArgSlot(argTypes, k) + 1] end,  -- ptr args only
	-- number of dense argument words that fit in registers; word w (0-based) with
	-- w >= argRegCount travels on the stack (see the DENSE_ARG_REGS comment).
	argRegCount = DENSE_ARG_REG_COUNT,
	-- 0-based dense word index of argument k's intval word (a pointer's lower word
	-- is at denseSlot+1). FP args consume no dense words.
	denseSlot = function(argTypes, k) return denseArgSlot(argTypes, k) - 1 end,
	-- yolo (SysV) integer-argument register count: args 1..6 ride rdi..r9; the 7th
	-- and later integer-class arguments live on the stack at entry (8+8i(%rsp)).
	yoloRegCount = 6,
	retFlag = 0,    -- al (rax)
	retIv = 2,      -- rdx
	retLo = 1,      -- rcx
	-- yolo input-asm register for argument k. FP args pass in the xmm registers
	-- (their xmm index is fpArgOrdinal) and consume no yolo GPR: arg k's yolo GPR
	-- register is its 1-based ordinal among the NON-FP args (nil for an FP arg).
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
		return x86.SYSV[n + 1]
	end,
	fpArgOrdinal = fpArgOrdinal,   -- xmm index (0-based) of FP arg k
}

return x86
