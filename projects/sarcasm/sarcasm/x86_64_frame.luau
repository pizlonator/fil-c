--!nonstrict
-- x86_64 frame policy for the shared frame preprocessor (frame.luau). Provides frame-size
-- analysis and the per-statement rewrite: dropping SysV frame setup/teardown (push/pop of
-- rbp + callee-saved, `mov %rsp,%rbp`, `sub/add %rsp`, `leave`, `endbr64`), virtualizing
-- rbp/rsp-relative slots as pseudo-registers, and rejecting stack-address escapes.
--
-- Frame geometry is derived from the PROLOGUE PREFIX ONLY (see analyzeFrame): the leading
-- run of frame-setup instructions (callee-saved pushes/pops, `movq %rsp,%rbp`, constant
-- `subq $imm,%rsp` allocations and net-balanced `addq $imm,%rsp` undos within the prefix)
-- interleaved with prologue-transparent instructions (ones that cannot observe or modify
-- the stack state). The frame size is the NET stack depth at the end of that prefix (D0) —
-- relative to the frame pointer when one is established — so a transient mid-body
-- `subq $8,%rsp; addq $8,%rsp` pad or `pushq %rbx; ...; popq %rbx` pair can no longer
-- inflate frameSize and desync the rbp<->rsp slot normalization (a former silent
-- miscompile).
--
-- After the prologue, rsp must provably stay at D0 except inside balanced callee-saved
-- push/pop save regions (those are simply dropped — sarcasm synthesizes its own frame —
-- which is sound only while no stack access observes the shifted rsp). analyzeFrame runs a
-- control-flow rsp-depth analysis that proves at which abstract depth every instruction
-- executes, and the rewrite REJECTS whatever cannot be proven safe rather than silently
-- miscompiling:
--   * pops with no provably matching push (same register, same slot depth) on every path —
--     dropping such a pop would silently lose its load, and the slot it actually reads is
--     not that register's save (e.g. an unpaired `popq %rbx` after `pushq %rbp` reads the
--     saved frame pointer — a live stack address — into rbx). Balanced pairs, epilogue
--     restores of prologue saves, and verified-teardown pops all pair exactly and keep
--     being dropped. Inside a VERIFIED epilogue teardown (a straight path to `ret` through
--     callee-saved pops and frame-touch-free computation) a pop at an UNKNOWN depth pairs
--     off the outstanding save stack by register alone: the slot position is unknowable
--     there, but every statement in the span is dropped and the synthesized epilogue owns
--     the real %rsp, so only the register identity matters;
--   * reads of the frame pointer as a VALUE (`movq %rbp, %rax`, an rbp memory index) while
--     rbp is the frame pointer — the same stack-address escape class as reading rsp, so the
--     fall-through escape check treats rbp as a stack register whenever usesRbp holds;
--   * mid-function NON-constant rsp adjustments (`subq`/`addq`/`mov %reg,%rsp` from an
--     unvalidated source) that are neither prologue/epilogue frame setup,
--     a recovery of a known-good saved stack pointer, nor a PROVEN mid-function
--     CONSTANT adjustment (dynamic stack allocation is spelled with the `.alloca`
--     directive, which never touches %rsp; a register-source rsp adjustment stays
--     unanalyzable). A constant `subq $imm, %rsp` /
--     `addq $imm, %rsp` / `leaq K(%rsp), %rsp` executed at a statically known
--     depth with a non-negative resulting depth is an ordinary depth change,
--     NOT a rejection: the depth lattice tracks it exactly like a push (merges
--     still require agreement — two paths reaching a join at different depths
--     degrade to the unprovable "dyn"), every later slot access keys off the
--     depth exactly (the perturbed-depth normalization, disp + D0 - d), pops
--     stay save-pairing-gated, and every return stays depth-0-gated. This is
--     the fixed-size inner-frame shape: balanced `leaq -168(%rsp),%rsp` …
--     `leaq 168(%rsp),%rsp` scratch pairs (rsaz-*k-avxifma), mid-function frees
--     ahead of a re-allocation (poly1305's `leaq 48(%rsp),%rsp` before jumping
--     into the alloca path — the alloca is then re-entered at depth 0 from
--     every path), and the ecp_nistz256 `addq $416,%rsp` shared-tail
--     frame-reshape. The sub/add forms clobber the flags, so dropping them
--     additionally requires the flags to be provably dead (nothing reads them
--     before a full flag write on any path); `leaq` is flags-neutral. An
--     adjustment at an UNKNOWN depth, or one whose resulting depth would go
--     negative (above the entry rsp), stays rejected.
--     Recovering %rsp from a known-good value — `movq %rbp,%rsp`, `leaq N(%rbp),%rsp`,
--     or a register the prologue saved %rsp into — is accepted and dropped anywhere,
--     mid-function frees included: rbp-relative virtualization is unaffected,
--     and rsp-relative accesses, pops, and returns stay depth-gated. The prologue save
--     itself (`movq %rsp,%rbx`) is prologue-transparent and DROPPED: the parked value
--     has no meaning in the output (sarcasm synthesizes its own frame), so only the
--     recovery is consumed — the save register may not be read while it holds the save,
--     and only callee-saved registers may hold one (a call would clobber the rest).
--     Mid-function constant adjustments compose with the phantom carriers: the
--     parked value is absolute and every recovery revives an exact depth, so an
--     adjustment inside a carrier's lifetime is sound and accepted (poly1305's
--     `leaq 48(%rsp),%rax` save immediately ahead of the free); interactions
--     that break the carrier discipline (redefinition, non-carrier reads) stay
--     rejected by the existing poison rules. An unanalyzable rsp adjustment
--     makes the depth unknown but pops nothing, so the abstract
--     save stack SURVIVES across it (and across the inert computation in its scope)
--     until one of those recoveries restores a known depth (`movq %rbp,%rsp; popq %rbp;
--     ret` pairs its pops exactly like `leave`);
--   * a MID-FUNCTION `leave`: hardware `leave` also restores the CALLER's frame pointer
--     into %rbp, which would desync every later rbp-relative virtualization — `leave` is
--     accepted only as a proven epilogue teardown (leading straight to `ret` through
--     callee-saved pops and ordinary computation), exactly like the `addq $imm,%rsp`
--     starter. A teardown-verified `addq $imm,%rsp` (and the `ret` that terminates its
--     span) is accepted even at an unknown depth: everything in the span is dropped
--     and the synthesized epilogue owns the real %rsp, so the output is correct
--     either way.
--     Per-point frame-pointer establishment (x86_fp) backs this up: rbp-relative
--     accesses are accepted only where %rbp provably still holds the function's frame
--     pointer on every path (an early-exit path that skipped the prologue, and any point
--     after a paired `popq %rbp`, are "not established");
--   * stack accesses and returns executed while rsp is of UNKNOWN depth
--     (e.g. after an unanalyzable rsp adjustment with no rbp restore), or unbalanced
--     (returns); accesses at a STATICALLY KNOWN perturbed depth key exactly in
--     normalized coordinates (disp + D0 - d) and virtualize like any other
--     slot (save-slot aliases ride the save model);
--   * mid-function pushes/pops of non-callee-saved operands other than the SPILL
--     forms — a `pushq %r9` parking a caller-saved register on the stack at a
--     statically known depth is an ordinary frame-slot STORE (the rsaz-avxifma
--     spill pairs), and a pop whose slot provably holds such a spill (top of
--     the abstract save stack, spill-marked, at the exact pop depth) is the
--     matching ordinary slot LOAD — web-exact, capabilities included. The
--     spill store additionally writes the real materialized bytes when the
--     slot falls inside an FP-tainted cluster (analyzeFrame rejects any
--     cluster traffic overlapping the slot while its entry is live, so the
--     two homes can never desync); pops of the frame pointer
--     that do not pair with its save (a paired `popq %rbp` — e.g. the no-frame-cleanup
--     leaf epilogue `popq %rbp; ret` — IS a sound restore), `leave` without an
--     established frame pointer, and mid-function `movq %rsp,%rbp`;
--   * `enter`/`enterq` (packed push-rbp/mov/sub frame setup sarcasm does not model —
--     passed through it would shift rsp and clobber rbp inside the synthesized frame);
--   * vector-indexed memory operands (gather/scatter: base or index of xmm-class) —
--     they touch multiple discrete addresses that cannot be bounds-checked;
--   * a `lock` prefix on any stack-frame access — the slot is virtualized into a
--     pseudo-register or materialized into the thread-confined synthesized frame
--     before classify's lockAllows validation runs, so accepting it would silently
--     elide the prefix (a locked memory RMW degenerating into an unlocked register
--     operation); `lock` stays supported on the modeled heap mem-RMWs only.
--
-- FP/SIMD registers (xmm-class: xmm/ymm/zmm/mm/st/k) are IN SCOPE: instructions naming
-- them pass through the frame policy (heap accesses are bounds-checked later by the
-- transform using the x86_64_fp knowledge module's widths). Stack-frame accesses by
-- FP instructions cannot ride the GPR slot virtualization (a slot pseudo-register is
-- not a vector register), so analyzeFrame runs an FP TAINT SCAN over the body's stack
-- accesses: every stack range an FP/vector instruction touches — plus any GPR range
-- overlapping it (aliasing) — is MATERIALIZED into a reserved region of sarcasm's
-- synthesized frame, i.e. the instruction is kept and its memory operand rewritten to
-- a real disp(%rsp) whose offset preserves the input's guaranteed alignment. All other
-- stack slots virtualize into pseudo-registers as before.

local x86 = require("./x86_64_isa")
local x86parse = require("./x86_64_parse")
local fp = require("./x86_64_fp")
local sig = require("./sig")
local frameShared = require("./frame")

local x86_64_frame = {}

local WIDTH_LETTER = { b = 1, w = 2, l = 4, q = 8 }  -- AT&T size suffix -> bytes

-- rsp is always the stack pointer; rbp is a stack (frame) register only when the function
-- actually establishes it as a frame pointer (`mov %rsp,%rbp`). With frame-pointer omission
-- (the -O2 default), rbp is just an ordinary callee-saved GPR holding values.
local function isStackReg(op, usesRbp) return op and op.kind == "reg" and (op.rc == "sp" or (usesRbp and op.name == "rbp")) end

-- Is this instruction an FP/SIMD/x87 use of memory (vs an ordinary GPR access)? The
-- knowledge module "claims" its own mnemonics even without an xmm-class operand
-- (ldmxcsr, movnti, cvtsi2sd, ...); claim=false entries (movq, the ss/sd/ps/pd and
-- p-family patterns, ...) only engage when an xmm-class register is actually present.
-- Memory-neutral hints (prefetch*) are not FP uses: they need no slot at all.
local function isFpInsn(fpe, st)
	if fpe and fpe.noCheck then return false end
	return fp.hasFpReg(st) or (fpe ~= nil and fpe.claim ~= false)
end

-- GPR stack-access width in bytes: an Intel PTR size when present, the movz*/movs*
-- source letter, a trailing AT&T [bwlq] suffix, default 8. (FP accesses are sized by
-- the fp knowledge module instead.) setcc carries NO size suffix anywhere but writes
-- exactly one byte — defaulting it to 8 would virtualize `sete 8(%rsp)` as a qword
-- slot the emitted `sete %rX` cannot encode (and model an 8-byte store the
-- instruction never performs).
local function gprStackWidth(m, mem)
	if mem.size then return mem.size end
	if x86.baseMnemonic(m) == "setcc" then return 1 end
	local zs = string.match(m, "^mov[zs]([bwlq])")   -- movzwl -> source letter 'w'
	if zs then return WIDTH_LETTER[zs] end
	local suf = string.match(m, "([bwlq])$")
	return (suf and WIDTH_LETTER[suf]) or 8
end
local function immVal(op) if op and op.kind == "imm" then return op.value or tonumber(op.text) or 0 end return 0 end

-- Is n a power of two >= 16 (an `and $-n, %rsp` alignment note)?
local function isStackAlign(n)
	if n < 16 then return false end
	while n % 2 == 0 do n = n / 2 end
	return n == 1
end

-- NOTE: the historical isAlloca annotation sniff is gone with the alloca
-- annotations (validateBody rejects them before frame analysis runs).

-- The abstract rsp depth: a number (bytes below the entry rsp) or "dyn" (unknown —
-- an unanalyzable rsp adjustment's aftermath). The historical TABLE depths
-- ({anchored}/{dynreg} region scopes) are unreachable since the alloca
-- annotations went away; isDyn covers every non-numeric form.
local function isDynDepth(d) return d == "dyn" or type(d) == "table" end
x86_64_frame.isDynDepth = isDynDepth

-- Does the instruction read or write rsp (as an explicit operand or memory base/index), or
-- access memory through rbp while rbp is the frame pointer? push/pop/call/ret/leave also
-- touch rsp implicitly; callers handle those separately.
local function insnTouchesFrame(st, usesRbp)
	for _, o in (st.operands or {}) do
		if o.kind == "reg" then
			if o.rc == "sp" then return true end
		elseif o.kind == "mem" then
			if (o.base and o.base.rc == "sp") or (o.index and o.index.rc == "sp") then return true end
			if usesRbp and o.base and o.base.name == "rbp" then return true end
		end
	end
	return false
end

-- Does the instruction write rsp (destination operand, or the implicit push/pop
-- write — including the RFLAGS stack transfers, which move the same 8-byte word)?
local function insnWritesSp(st)
	local ops = st.operands
	if ops[1] and ops[1].kind == "reg" and ops[1].rc == "sp" then return true end
	local m = string.lower(st.mnemonic)
	local b = x86.baseMnemonic(m)
	return b == "push" or b == "pop" or x86.isFlagStackXfer(m)
end

-- A full-width (64-bit) `movq %rsp, %reg` SAVE form: the register parks the stack
-- pointer. The frame-pointer establishment `movq %rsp, %rbp` is NOT a save — every
-- consumer checks the rbp case first. A sub-register form (`movl %esp, %ebx`) truncates
-- the parked value and is not a save.
local function isSpSaveForm(st)
	local ops = st.operands
	return x86.baseMnemonic(string.lower(st.mnemonic)) == "mov"
		and ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr" and ops[1].name ~= "rbp"
		and (ops[1].width == 64 or ops[1].width == nil)
		and ops[2] and ops[2].rc == "sp" and (ops[2].width == 64 or ops[2].width == nil)
end

-- The matching `movq %reg, %rsp` RECOVERY form (writing %rsp from a register). The
-- frame-pointer source `movq %rbp, %rsp` is NOT this form when rbp is the frame
-- pointer — every consumer checks that first. A sub-register form truncates and is
-- not a recovery. When rbp is NOT established as a frame pointer (usesRbp false) it
-- is an ordinary callee-saved GPR: `movq %rbp, %rsp` from an rbp carrier is a plain
-- recovery (rsaz-avx2 parks the saved stack pointer in rbp).
local function isSpRecoverForm(st, usesRbp)
	local ops = st.operands
	return x86.baseMnemonic(string.lower(st.mnemonic)) == "mov"
		and ops[1] and ops[1].rc == "sp" and (ops[1].width == 64 or ops[1].width == nil)
		and ops[2] and ops[2].kind == "reg" and ops[2].rc == "gpr"
		and (ops[2].name ~= "rbp" or not usesRbp)
		and (ops[2].width == 64 or ops[2].width == nil)
end

-- Result of analyzeFrame that the per-statement rewrite needs but the shared frame.luau
-- ctx does not carry (it only forwards frameSize/fpOffset/usesFp). analyzeFrame always
-- runs (via frame.preprocess) before any rewrite call for the same body, single-threaded.
local D0 = 0  -- net rsp depth at the end of the prologue prefix (the frame base)
local rbpNorm = 0  -- rbp-relative normalization base (D0 - fpDepth; 0 without an fp)
-- Highest-numbered vector register (0-31) the most recently analyzed body never
-- names, or nil when all are used: the rewrite's {%kN}-masked stack lowering
-- rides it as scratch (see the vecUsed scan in analyzeFrame). Single-threaded
-- discipline, exactly like D0/rbpNorm.
local maskScratchNum = nil

-- The {%kN}-masked vector-move mnemonics the stack rewrite can lower exactly
-- (masked-off lanes preserved by the load / register-masked-move / store
-- sequence): the mnemonic and its unaligned-load/store twin (the unmasked slot
-- traffic never carries the aligned form's alignment promise — the FULL width
-- is bounds-proven and materialized instead).
local MASKED_STACK_MOVE = {
	vmovdqu8 = "vmovdqu8", vmovdqu16 = "vmovdqu16",
	vmovdqu32 = "vmovdqu32", vmovdqu64 = "vmovdqu64",
	vmovdqa32 = "vmovdqu32", vmovdqa64 = "vmovdqu64",
	vmovups = "vmovups", vmovupd = "vmovupd",
	vmovaps = "vmovups", vmovapd = "vmovupd",
}

-- Desired stack alignment (max over the prologue/mid-function `and $-N, %rsp`
-- notes, 16 when none was seen) and the rsp depth at the strongest note.
-- `and $-N, %rsp` only rounds rsp DOWN by at most N-1, so dropping it keeps
-- every normalized slot key consistent within a run (all post-and traffic
-- keys off the same post-and numbering, and the prefix holds no slot traffic
-- by construction); the synthesized frame is then emitted N-aligned so
-- aligned (vmovdqa-class) accesses keep their input alignment. Set by
-- analyzeFrame (single-threaded discipline, like D0).
local frameAlignReq = 16
local frameAndDepth = 0
local frameAndIdx = nil  -- the prologue `and $-N, %rsp` statement's index, if any

-- Containing-granule map for PARTIAL (narrow) slot accesses (set by analyzeFrame;
-- single-threaded discipline, like D0): maps a normalized frame offset with 8-byte
-- GPR slot traffic (an 8-byte access the rewrite virtualizes into a slot web) to
-- true. A narrow (1/2/4-byte) access whose address falls inside such a granule
-- routes to the CONTAINING web with a partial-access marker (x86_slotPartial)
-- instead of creating a fresh web at its own offset — mirroring real memory, where
-- a narrow store writes bytes of the enclosing qword and a narrow load reads them.
-- (Without this, `mov 4(%rsp),%ebx` after `movq %rax,0(%rsp)` reads a web nothing
-- ever defined — the "sarcasm miscompiles 4-byte loads from the high half of an
-- 8-byte slot" gap, and a 32-bit store to offset 0 zero-extending-kills the 8-byte
-- web's high bytes.) Reset per analyzeFrame run, exactly like frameAlignReq.
local slotGranules = {}

-- Whether the latest analyzeFrame run executed the cloneRet balance proof
-- (the run's cloneRet check is gated on `regions ~= nil`). The rewrite's
-- cloneRet exemption (the depth-0-at-ret gate) asserts this: the exemption may
-- only be consumed on marks from a run whose proof backed them. Every caller
-- today passes the same (always-empty) regions list, so the proof runs on every
-- analyzeFrame run, the flag is always true, and the assert is a vestigial
-- machine check — kept so a future caller passing a nil regions list fails
-- loudly instead of silently. Same single-threaded discipline as
-- D0/frameAlignReq below: frame.preprocess runs analyzeFrame immediately
-- before the rewrite loop for the same body, so the flag is true whenever the
-- rewrite can see a cloneRet.
local cloneRetProofRan = false

-- Incoming yolo stack-argument state for the CURRENT function (set by analyzeFrame,
-- consumed by the rewrite — same single-threaded discipline as D0):
--   yoloCount — number of SysV stack (7th+ integer-class) arguments in the entry
--     signature; they arrive at 8+8i(%rsp) at entry, ABOVE the return address —
--     outside the analyzed frame — so their entry-time reads get redirected to
--     virtual slot webs (GPR) or the materialized area (FP/vector).
--   yoloBase — base-16-relative offset of that materialized area (nil when no FP
--     instruction reads an incoming stack argument); the transform fills it from
--     the incoming fast-CC words and the layout reserves yoloBytes for it.
local yoloCount = 0
local yoloBase = nil

-- ---------------------------------------------------------------------------
-- `#! stack buffer (...)` — annotated STACK BUFFERS (x86_64).
--
-- A long-form declaration `#! stack buffer (id, %reg + lo, %reg + hi)` on a stack
-- access (or a rep movs*/stos*) declares the normalized frame byte range
-- [K + lo, K + hi) — K is the base register's normalized stack offset at the
-- declaring statement (%rsp at depth d: D0 - d; %rbp with an established frame
-- pointer: rbpNorm; any other register: a live saved-rsp carrier from the smap,
-- K = D0 - carrier depth) — as a RAW BYTE BUFFER. Every stack access whose
-- normalized byte range falls fully inside a declared buffer is lowered into a
-- dedicated region of the synthesized output frame (real memory, no capability
-- webs); indexed accesses get runtime bounds checks; nothing in the region can
-- hold a pointer capability. Short forms `#! stack buffer (id)` reference a
-- buffer declared by a long form ANYWHERE in the file (the driver pre-pass
-- collects the long forms and hands us the canonical ranges via
-- beginStackBufResolution/endStackBufResolution below).
--
-- All long-form declarations of one id across the file must resolve to the SAME
-- normalized range (the driver pre-pass enforces it; per-function re-resolution
-- re-checks, so a pre-pass/rewrite disagreement — e.g. through an alloca region
-- the pre-pass could not see — is a compile error, never a miscompile).
--
-- Module state, same single-threaded discipline as D0 above:
local sbCanon = nil       -- id -> { lo = loN, hi = hiN, fn = <declaring function> }
local sbResolving = false -- true during the driver pre-pass (long forms only)
local sbLayout = nil      -- { groups = { {lo, hi, areq, base} }, bytes = total } (this function)

-- Driver pre-pass entry: reset the canonical-range table and switch to
-- resolution-only mode (analyzeFrame runs triggered by the pre-pass skip the
-- buffer group scan, because the canonical ranges are not complete yet).
function x86_64_frame.beginStackBufResolution()
	sbCanon = {}
	sbResolving = true
	sbLayout = nil
end

-- Driver pre-pass exit: keep the canonical table, re-enable the full scan.
function x86_64_frame.endStackBufResolution()
	sbResolving = false
end

-- The canonical (normalized) range declared for `id`, or nil. Used by the driver
-- pre-pass after resolving a declaration, and by the per-function scan for short
-- forms.
function x86_64_frame.stackBufCanonOf(id)
	if sbCanon then return sbCanon[id] end
	return nil
end

function x86_64_frame.recordStackBufCanon(id, lo, hi, fnName, isSub)
	sbCanon[id] = { lo = lo, hi = hi, fn = fnName, sub = isSub and true or nil }
end

-- The buffer layout of the most recently analyzed function: groups (merged,
-- sorted by low offset) with region-relative placeholder bases, and the total
-- region size. Consumed by the transform (tr.bufBytes and the buffer-base shift).
function x86_64_frame.stackBufLayout()
	return sbLayout
end

-- The placeholder base (within the buffer region) of the group containing the
-- normalized range [lo, hi). Used by the transform's rep lowering.
function x86_64_frame.stackBufGroupBaseFor(lo, hi)
	if not sbLayout then
		error("sarcasm: internal error: no stack buffer layout for the rep alias")
	end
	for _, g in sbLayout.groups do
		if lo >= g.lo and hi <= g.hi then return g.base end
	end
	error("sarcasm: internal error: stack buffer range [" .. tostring(lo) .. ", "
		.. tostring(hi) .. ") is not covered by any lowered buffer group")
end

-- Resolve a statement's `#! stack buffer (...)` annotation to the normalized
-- frame byte range it declares: returns (loN, hiN), or nil when the annotation
-- is a short form whose canonical range is not available yet (the driver
-- pre-pass runs before the canonical table is complete — the caller skips the
-- statement then; the real per-function runs always have the table).
--   d        — the provable rsp depth at the statement (transfer arg or mark)
--   smap     — the saved-rsp carrier map at the statement
--   fpState  — the frame-pointer state ("est" = established)
--   inPrologue — whether the statement sits in the prologue prefix
-- A long form names its base register directly: %rsp resolves against the
-- statement's depth, %rbp against the frame pointer (or an rbp carrier), any
-- other register must be a live saved-rsp carrier in `smap`. The resolved range
-- is cross-checked against the file-wide canonical entry for its id (recorded by
-- the driver pre-pass; two declarations resolving differently are a compile
-- error — the "same id declared twice with different offsets" rejection).
--
-- B2 shared-tail clones (tailcall.luau's st.b2clone) are the one exception to
-- the canonical rule. A clone's statements execute at the JUMP SITE's rsp with
-- the clone's own `sub`/`and` setup as ordinary code, so the clone's depth marks
-- are the natural continuation of the jumper's depth tracking — the same
-- address-identical normalization every other clone access uses
-- (disp + D0 - d). A declaration inside a clone therefore resolves IN THE
-- CLONE'S OWN CONTEXT and must NOT participate in the file-wide canonical
-- table: the same buffer id declared in the source function's own body
-- resolves to [0, N) there and to a negative (below-the-jumper-frame) range in
-- each jumper's clone — both are correct, because they describe the same real
-- bytes in two coordinate systems (the clone runs at the jumper's rsp). Clone
-- declarations also cannot satisfy short forms (a short form names a
-- canonical file-wide range, which is a property of the declaring function's
-- own frame, not of a clone's private window).
local function resolveBufRangeAt(st, d, smap, fpState, inPrologue)
	local sb = st.stackBufAnno
	if not sb then return nil end
	local loN, hiN
	if sb.short then
		if st.b2clone then
			error("sarcasm: `stack buffer (" .. sb.id .. ")` short form on a"
				.. " shared-tail clone statement is not supported (a clone's"
				.. " buffer window is private to the clone's own frame context;"
				.. " declare it with the long form `#! stack buffer (" .. sb.id
				.. ", %rsp, %rsp + N)` on an access inside the cloned region): "
				.. (st.raw or st.mnemonic))
		end
		local canon = sbCanon and sbCanon[sb.id] or nil
		if canon == nil then
			if sbResolving then return nil end
			error("sarcasm: `stack buffer (" .. sb.id .. ")` short form has no long form"
				.. " anywhere in this file (declare the buffer with e.g. `#! stack buffer ("
				.. sb.id .. ", %rsp, %rsp + N)` on a buffer access in some function): "
				.. (st.raw or st.mnemonic))
		end
		if canon.sub and not st.localClone then
			error("sarcasm: `stack buffer (" .. sb.id .. ")` is declared inside local"
				.. " subroutine '" .. tostring(canon.fn) .. "' (its bytes live in that"
				.. " subroutine's own frame); only that subroutine's cloned code"
				.. " addresses it - declare a buffer in this function with the"
				.. " long form instead: " .. (st.raw or st.mnemonic))
		end
		loN, hiN = canon.lo, canon.hi
	else
		if d == nil then
			error("sarcasm: `stack buffer` annotation on an unreachable (dead-code)"
				.. " statement cannot be resolved: " .. (st.raw or st.mnemonic))
		end
		if st.localClone and sb.base ~= "rsp" then
			error("sarcasm: `stack buffer` annotation on a local subroutine clone"
				.. " statement must use %rsp as its base (%" .. sb.base
				.. " names a register whose clone-context binding the analysis"
				.. " cannot attribute to the clone's own frame): "
				.. (st.raw or st.mnemonic))
		end
		local k
		if sb.base == "rsp" then
			if isDynDepth(d) then
				error("sarcasm: `stack buffer` annotation at an unprovable (dynamic or"
					.. " unknown) stack depth cannot be resolved: " .. (st.raw or st.mnemonic))
			end
			if st.b2clone then
				-- A clone statement is never in the prologue prefix (clones are
				-- appended after the jumper body), and its depth is the natural
				-- continuation of the jumper's tracking — resolve exactly like
				-- any other clone access: k = D0 - d.
				k = D0 - d
			elseif st.localClone then
				-- Feature (stack buffers in localcall clones): the clone's own
				-- `sub` frame keys with the +8 return-address compensation
				-- (the hardware `call` the subroutine was written against
				-- pushes a return address the clone model does not store), so
				-- the declaration's %rsp bounds resolve in the clone's own
				-- keyed coordinates exactly like every other clone access:
				-- k = D0 - d - 8. The buffer's bytes live in the clone's own
				-- sub-frame area (checkRangeFits proves the extent against the
				-- clone's own allocated stack). A clone declaration resolves IN
				-- THE CLONE'S OWN CONTEXT and does not participate in the
				-- file-wide canonical table (each caller's clone re-resolves it;
				-- the subroutine's own text registered the canonical SUB-FRAME
				-- range in the pre-pass).
				k = D0 - d - 8
			else
				if inPrologue and d ~= D0 then
					-- A statement in the prologue prefix at a depth other than the
					-- frame base (mid-setup: between the pushes and the sub) — the
					-- prefix's rsp-relative addressing keys differently there. A
					-- prefix statement AT the frame-base depth (the leaf-function
					-- shape: a straight-line body the prologue scan never exits) is
					-- fine.
					error("sarcasm: `stack buffer` annotation inside the prologue prefix"
						.. " at a perturbed stack depth is not supported: "
						.. (st.raw or st.mnemonic))
				end
				k = D0 - d
			end
		elseif st.b2clone then
			error("sarcasm: `stack buffer` annotation on a shared-tail clone"
				.. " statement must use %rsp as its base (%" .. sb.base
				.. " names a register whose clone-context binding the analysis"
				.. " cannot attribute to the clone's own frame): " .. (st.raw or st.mnemonic))
		elseif sb.base == "rbp" then
			if fpState == "est" then
				k = rbpNorm
			else
				local e = smap and smap["rbp"] or nil
				if not (e and not e.redefined and not e.slot and not e.roff and not e.conflict) then
					error("sarcasm: `stack buffer` annotation base %rbp is neither an"
						.. " established frame pointer nor a live saved-rsp carrier here: "
						.. (st.raw or st.mnemonic))
				end
				k = D0 - e.depth
			end
		else
			local e = smap and smap[sb.base] or nil
			if not (e and not e.redefined and not e.slot and not e.roff and not e.conflict) then
				error("sarcasm: `stack buffer` annotation base %" .. sb.base .. " is not a"
					.. " live saved-rsp stack alias at this statement (create one with"
					.. " `mov %" .. "rsp, %" .. sb.base .. "` or `lea K(%rsp), %" .. sb.base
					.. "`): " .. (st.raw or st.mnemonic))
			end
			k = D0 - e.depth
		end
		loN, hiN = k + sb.lo, k + sb.hi
		if not st.b2clone and not st.localClone then
			local canon = sbCanon and sbCanon[sb.id] or nil
			if canon then
				if canon.lo ~= loN or canon.hi ~= hiN then
					error("sarcasm: `stack buffer (" .. sb.id .. ")` long forms disagree: the"
						.. " declaration in function '" .. tostring(canon.fn) .. "' resolves"
						.. " to [" .. tostring(canon.lo) .. ", " .. tostring(canon.hi)
						.. ") but this one resolves to [" .. tostring(loN) .. ", "
						.. tostring(hiN) .. ") (the offsets must match): "
						.. (st.raw or st.mnemonic))
				end
			elseif not sbResolving then
				-- The driver pre-pass records every long form it can see (function
				-- bodies). A miss here means this declaration was invisible to it.
				error("sarcasm: `stack buffer (" .. sb.id .. ")` long form was not seen by"
					.. " the file-wide declaration scan (long forms inside local subroutine"
					.. " bodies are not supported): " .. (st.raw or st.mnemonic))
			end
		end
	end
	if hiN <= loN then
		error("sarcasm: `stack buffer (" .. sb.id .. ")` declares an empty range: "
			.. (st.raw or st.mnemonic))
	end
	return loN, hiN
end

-- Driver pre-pass: resolve the long-form declaration on `st` (the caller has run
-- analyzeFrame on the statement's body, so the statement marks, D0 and the smap
-- discipline are current) and return the normalized range. Short forms return
-- nil (they contribute no declaration).
function x86_64_frame.resolveStackBufDeclAt(st)
	if not (st.stackBufAnno and not st.stackBufAnno.short) then return nil end
	return resolveBufRangeAt(st, st.x86_depth, st.x86_smap, st.x86_fp,
		st.x86_prologue == true)
end

-- Map a memory read to its incoming yolo stack-argument slot: return the 0-based
-- slot index i when `st` is a LOAD-LIKE instruction (the memory operand is a pure
-- source — not a store/RMW destination, not an address-taking lea) whose memory
-- operand reads exactly entry_rsp + 8 + 8*i with 0 <= i < yoloCount. `baseName` is
-- the expected base register ("rsp" for a direct read at depth 0, the alias
-- register otherwise) and `saveDepth` the depth at which that register's value
-- equals entry_rsp (0 for %rsp itself at depth 0; the alias save's depth).
local function yoloArgSlotOf(st, baseName, saveDepth)
	if yoloCount == 0 then return nil end
	local ops = st.operands
	local mem
	for _, o in ops do if o.kind == "mem" then mem = o end end
	if not mem then return nil end
	if not (mem.base and mem.base.name == baseName and mem.base.kind == "reg") then return nil end
	if mem.index or mem.dispSym then return nil end
	if ops[1] == mem then return nil end              -- store / RMW destination
	if x86.isMemRmw(st) then return nil end
	local b = x86.baseMnemonic(string.lower(st.mnemonic))
	if b == "lea" then return nil end                 -- address escape, not a read
	-- A clone statement's rsp-relative displacement keys 8 lower (the +8 rule):
	-- an incoming stack argument sits a further return-address word above the
	-- clone's own rsp. An ALIAS-based read keys off the alias's parked value
	-- (the entry rsp itself) — absolute, and unaffected by the clone's rsp
	-- shift — so the compensation applies only to the rsp spelling. (This is
	-- what lets a localcall clone read the incoming stack arguments through
	-- the entry-rsp alias its caller materialized — mont5's mul4x_internal
	-- `movd 8(%rax),%xmm5`.)
	local i = (mem.dispVal or 0) - saveDepth - 8
		- ((st.localClone and baseName == "rsp") and 8 or 0)
	if i < 0 or i % 8 ~= 0 then return nil end
	i = i / 8
	if i >= yoloCount then return nil end
	return i
end

-- Is `st` an entry-rsp materialization that may serve as an alias for reading the
-- incoming yolo stack arguments (`leaq 0(%rsp), %reg` — the aesni_ocb prologue
-- shape — at a provable rsp depth)? (The `movq %rsp, %reg` form is recognized by
-- isSpSaveForm instead.)
local function isYoloLeaSave(st, d)
	if yoloCount == 0 then return false end
	if d == nil or isDynDepth(d) then return false end
	local ops = st.operands
	if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "lea" then return false end
	if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr" and ops[1].name ~= "rbp") then return false end
	local mem = ops[2]
	-- The entry stack pointer relative to the access's rsp: d above it for a plain
	-- statement, d + 8 for a clone (the return-address word — the +8 rule).
	local want = st.localClone and (d + 8) or 0
	return mem and mem.kind == "mem" and mem.base and mem.base.rc == "sp"
		and not mem.index and not mem.dispSym and (mem.dispVal or 0) == want
end

-- A virtual frame slot is keyed SLOT_BASE + offset; the yolo incoming
-- stack-argument slots live in their own band below SLOT_BASE. A mid-function
-- inner frame reaches NEGATIVE normalized offsets (the scratch area below the
-- frame base), so a deep enough one could numerically collide with the argument
-- band and silently alias two different addresses to one slot web. Reject such
-- a collision cleanly instead of desyncing. (Only reachable with >6 integer
-- arguments AND a deep inner frame; materialized slots use real memory and
-- never collide.)
local function slotOffSafe(off)
	local num = frameShared.SLOT_BASE + off
	return num < frameShared.YOLO_SLOT_BASE
		or num >= frameShared.YOLO_SLOT_BASE + 8 * yoloCount
end

-- Pre-scan the frame: derive its geometry from the PROLOGUE PREFIX (net depth at the first
-- instruction that is not frame setup and cannot be prologue-transparent), determine
-- whether rbp is established as the frame pointer there, and annotate every statement with
--   * st.x86_prologue      — inside the prologue prefix;
--   * st.x86_depth         — abstract rsp depth (bytes below the entry rsp) at which the
--                            instruction executes, or "dyn" when unknown, or nil when the
--                            instruction is unreachable (dead code gets no depth checks);
--   * st.x86_teardownStart — a post-prologue `addq $imm,%rsp` / `movq %rbp,%rsp` verified
--                            as epilogue teardown (a straight path to `ret` involving only
--                            callee-saved pops and ordinary, non-stack computation);
--   * st.x86_teardown      — inside such a verified teardown region (the terminating
--                            `ret` is part of the span);
-- The frame size for rbp-based normalization is D0 - fpDepth (so rbp = rsp + frameSize at
-- the post-prologue rsp); without a frame pointer it is the net constant allocation.
function x86_64_frame.analyzeFrame(body, sigDesc, regions)
	-- analyzeFrame may run more than once on the same body (the transform calls it for
	-- frame geometry before frame.preprocess runs it again for the rewrite), and it may
	-- see a DIFFERENT body state each time. (No pass marks statements anymore; the
	-- historical allocaPlace/allocaOwned marks are gone with the alloca annotations.)
	-- are this analysis's OUTPUT: clear stale ones from a previous run so a conditional
	-- mark (an unpaired pop, a teardown region) never survives from an analysis that
	-- saw different marks.
	for _, st in body do
		st.x86_prologue = nil
		st.x86_depth = nil
		st.x86_saves = nil
		st.x86_unpairedPop = nil
		st.x86_spillPop = nil
		st.x86_flagsPop = nil
		st.x86_teardownStart = nil
		st.x86_teardown = nil
		st.x86_fp = nil
		st.x86_spRecover = nil
		st.x86_saveStore = nil
		st.x86_carrierLoad = nil
		st.x86_leaSave = nil
		st.x86_spSave = nil
		st.x86_dropRestore = nil
		st.x86_midSp = nil
		st.x86_regionCarrier = nil
		st.x86_aliasOff = nil
		st.x86_aliasSaveIdx = nil
		st.x86_midAnd = nil
		st.x86_preAnd = nil
		st.x86_postAnd = nil
		st.x86_smap = nil
		st.x86_b2band = nil
		st.x86StackBufAccess = nil
		st.x86StackBufIdx = nil
		st.x86StackBufRep = nil
		st.x86StackBufLea = nil
		st.x86StackBufAddr = nil
		st.x86StackBufLeaValue = nil
		st.x86StackBufLeaValueComputed = nil
	end
	frameAlignReq = 16
	frameAndDepth = 0
	frameAndIdx = nil
	slotGranules = {}
	-- Record whether this run executes the cloneRet balance proof below
	-- (gated on `regions ~= nil`; every caller passes the always-empty regions
	-- list, so this is true on every run). The rewrite asserts the flag before
	-- consuming any cloneRet exemption — a vestigial machine check that stays
	-- loud if a future caller ever passes nil.
	cloneRetProofRan = (regions ~= nil)

	-- Feature A (by-address `#! stack buffer` accesses), pre-fixpoint probe: the
	-- canonical ranges of every buffer id LONG-FORM-declared in this body. The
	-- fixpoint's carrier recognition consults them to keep a buffer-interior
	-- `leaq K(%rsp), %reg` from parking a PHANTOM carrier when its destination is
	-- used as a VALUE (the web must hold a real — lowered — buffer address). The
	-- ranges are the canonical ones the driver pre-pass resolved; for this
	-- function's own declarations they are exact (the canonical rule pins every
	-- non-clone long form of an id to one file-wide normalized range, and the
	-- per-function scan re-checks it). For a subroutine-declared id the canonical
	-- range is in the SUBROUTINE's own frame coordinates, which a clone's leas
	-- only reach through a per-caller shift this early pass cannot know — such a
	-- lea keeps today's path (a false negative, never unsound). A FALSE POSITIVE
	-- (a lea whose offset coincides with a canonical range it does not belong to)
	-- is equally safe: the lea only stops parking a carrier, and the buffer scan
	-- — which uses the exact post-fixpoint ranges — or the rewrite's escape
	-- rejection owns it then.
	local bufProbeRanges = nil
	local bufProbeSuppress = false
	-- Per-B2-clone raw declaration ranges (see the collection above): clone idx
	-- -> array of `stackBufAnno` long forms declared on the clone's statements.
	local bufB2DeclRaw = nil
	-- Registers whose value an ANNOTATED rep consumes as its stack-alias side:
	-- the carrier path (not the by-address value path) materializes those, so
	-- the value-lea exemption below must leave them alone.
	local bufRepRegs = {}
	do
		for _, st in body do
			if st.kind == "insn" and st.stackBufAnno and st.repPrefix ~= nil then
				-- The implicit %rsi (6) and %rdi (7) of a rep movs*/stos*: the
				-- rep's stack-alias sides ride the carrier machinery, so a lea
				-- parking one of them must stay a carrier even when its offset
				-- lands inside a declared buffer.
				bufRepRegs[6] = true
				bufRepRegs[7] = true
			end
		end
	end
	do
		local hasDecl = false
		for _, st in body do
			if st.kind == "insn" and st.stackBufAnno and not st.stackBufAnno.short then
				hasDecl = true break
			end
		end
		if hasDecl then
			-- Resolution-only runs (the driver pre-pass) have an incomplete
			-- canonical table — the pre-pass records each declaration only AFTER
			-- this analysis. There the fixpoint merely suppresses the carrier
			-- value-use rejection for lea-form carriers (the analysis output the
			-- pre-pass consumes — depths, carriers, aliases — is re-derived by
			-- every later run; only the resolution reads it), so a value-used
			-- buffer lea cannot abort the pre-pass before the canonical table
			-- exists.
			bufProbeSuppress = sbResolving
			for _, st in body do
				if st.kind == "insn" and st.stackBufAnno and not st.stackBufAnno.short then
					local canon = sbCanon and sbCanon[st.stackBufAnno.id] or nil
					if canon then
						if not bufProbeRanges then bufProbeRanges = {} end
						table.insert(bufProbeRanges, { lo = canon.lo, hi = canon.hi,
							id = st.stackBufAnno.id })
					end
				end
			end
			-- B2 shared-tail clone declarations, keyed per clone index: the raw
			-- (%rsp + lo, %rsp + hi) annotation ranges the clone's own statements
			-- declare. The fixpoint's clone-lea probe (below) tests a clone lea's
			-- RAW displacement against them — in the clone's own frame coordinates
			-- the declaration and the lea key the same way (both %rsp-relative at
			-- the statement's depth), so a `leaq K(%rsp)` whose K lands inside a
			-- raw declared range is a buffer-interior lea even though its
			-- normalized offset lives in the clone's band. A clone's declarations
			-- and its buffer leas sit at the same post-prologue depth in every
			-- shape this models (the OpenSSL mb bodies); if a future shape
			-- separates them by pushes, the probe's raw test and the buffer
			-- scan's exact banded test disagree and the lea falls back to
			-- today's carrier path (whose value-use rejection fails the compile
			-- closed — never unsound).
			for _, st in body do
				if st.kind == "insn" and st.stackBufAnno and not st.stackBufAnno.short
					and type(st.b2clone) == "number" then
					if not bufB2DeclRaw then bufB2DeclRaw = {} end
					local lst = bufB2DeclRaw[st.b2clone]
					if not lst then lst = {} bufB2DeclRaw[st.b2clone] = lst end
					table.insert(lst, st.stackBufAnno)
				end
			end
		end
	end

	-- Fixed-alloca region lookup mirroring frame.preprocess's: `dyn` selects the
	-- alignment-anchored regions ("dyn"-depth rsp coordinates), static the
	-- lea/mov-anchored ones. Every analyzeFrame run receives the same (always
	-- empty) regions list — there are no region sources left — so this lookup
	-- always misses and the `regions ~= nil` gates below always hold; those
	-- gates are vestigial, kept for the region plumbing's shape. (The
	-- historical region-sensitive behaviors they fed — the caller-saved-save
	-- region corner and the in-region lea exclusion — are gone with the region
	-- sources.)
	local function regionOf(off, dyn)
		for _, r in (regions or {}) do
			if (r.anchored and true or false) == (dyn and true or false)
				and off >= r.base and off < r.base + r.size then return r end
		end
		return nil
	end

	-- (sigDesc is the function's entry signature. A float/double argument or FP
	-- return rides in the xmm registers EVEN when the body never names a vector
	-- register — FP args are typically consumed by a call, an FP return produced,
	-- without any SSE instruction in the body — so the vector-save reservation must
	-- be forced from the SIGNATURE alone, exactly like arm64's usesNeon forcing.
	-- Otherwise an injected runtime call (pollcheck slow path, allocation, ptr-store
	-- barrier) could silently clobber an FP argument. The transform's callsite-side
	-- forcing (transform.luau) covers FP CALLSITE signatures; this covers the entry.)
	local sigHasFp = false
	if sigDesc then
		if sigDesc.retTypes and sigDesc.retTypes[1] and sig.isFpClass(sigDesc.retTypes[1]) then
			sigHasFp = true
		else
			for _, t in (sigDesc.argTypes or {}) do
				if sig.isFpClass(t) then sigHasFp = true break end
			end
		end
	end

	-- Incoming yolo stack arguments (see the yoloCount/yoloBase comment above): the
	-- SysV 7th+ integer-class arguments live at 8+8i(%rsp) at entry. Count them
	-- from the signature (FP args pass in xmm0..xmm7 and never reach the stack —
	-- signatures with >8 FP args are rejected before the frame pass runs).
	yoloCount = 0
	yoloBase = nil
	if sigDesc then
		local nf = 0
		for _, t in (sigDesc.argTypes or {}) do
			if not sig.isFpClass(t) then
				nf += 1
				if nf > 6 then yoloCount += 1 end
			end
		end
	end

	-- jump-target label names (backward edges must end the prologue: instructions behind
	-- such a label can re-execute, so their stack effect cannot be counted once)
	local targeted = {}
	for _, st in body do
		if st.kind == "insn" then
			local b = x86.baseMnemonic(string.lower(st.mnemonic))
			if b == "jmp" or b == "jcc" then
				for _, o in st.operands do
					if o.kind == "sym" then targeted[o.text] = true end
				end
			end
		end
	end

	-- label name -> index of the insn it precedes; nextInsn[i] -> next insn index after i.
	-- Both skip every non-insn statement (labels, directives, blanks): labelAt
	-- maps each label to the next INSN (pending labels accumulate across
	-- directives/blanks, so a clone entry buried under extra labels still
	-- resolves to its first real instruction), and nextInsn threads each index
	-- to the next INSN (only insns update the cursor, so a `jmp` whose target
	-- label is followed by labels/nops/directives and then an `and` still
	-- reaches that `and` — the jumper header's x86_preAnd mark flows across
	-- the jump edge: see sarcasm-reject-b2-preand-gap-att — and straight-line
	-- gaps never break fallthrough: see sarcasm-tail-b2-gap-att).
	local labelAt, nextInsn = {}, {}
	do
		local pending = {}
		for i, st in body do
			if st.kind == "insn" then
				for _, n in pending do labelAt[n] = i end
				pending = {}
			elseif st.kind == "label" then
				table.insert(pending, st.name)
			end
		end
		for _, n in pending do labelAt[n] = #body + 1 end
		local nxt = #body + 1
		for i = #body, 1, -1 do
			nextInsn[i] = nxt
			if body[i].kind == "insn" then nxt = i end
		end
		-- MACHINE CHECK of the skip invariant above (the x86_preAnd proof and
		-- every CFG consumer ride on it): every label resolves to an insn index
		-- (or just past the end for trailing labels), and every nextInsn step
		-- lands on an insn (or just past the end) — no label, directive, or
		-- blank in either map. sarcasm-tail-b2-gap-att proves it end to end
		-- (a clone entry buried under labels/nops still resolves, and
		-- straight-line gaps never break fallthrough).
		for _, t in pairs(labelAt) do
			assert(t == #body + 1 or (body[t] and body[t].kind == "insn"),
				"sarcasm: internal error: label mapped to a non-insn statement")
		end
		for i = 1, #body do
			local n = nextInsn[i]
			assert(n == #body + 1 or (body[n] and body[n].kind == "insn"),
				"sarcasm: internal error: nextInsn mapped to a non-insn statement")
		end
	end

	-- A prologue-prefix push of a NON-callee-saved (caller-saved) full-width
	-- register: not a save/restore pair (sarcasm drops those) but a SPILL —
	-- the pushed value is genuinely stored and reloaded later by an ordinary
	-- load (x25519's `pushq %rdi` / aes's `pushq %rdx` parking an argument
	-- across a call, reloaded as `movq (%rsp),%r8`). The push virtualizes as
	-- an ordinary slot STORE (its slot packs into the frame — see
	-- spillBytes), never into the dropped-save model: the register's web
	-- keeps its (possibly redefined) value, and reloads read the slot web.
	-- (A matching POP into the register is NOT a load: popping a spill slot
	-- is rejected like any unmodelable pop — no corpus shape needs it.)
	local function isSpillPushForm(st)
		local ops = st.operands
		return x86.baseMnemonic(string.lower(st.mnemonic)) == "push"
			and ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
			and (ops[1].width == 64 or ops[1].width == nil)
			and not x86.CALLEE_SAVED[ops[1].name]
	end

	-- Prologue prefix scan.
	local depth, subBytes = 0, 0
	local spillBytes = 0  -- frame bytes reserved for spill pushes (above)
	local pstack = {}  -- outstanding saves: { name = reg, depth = slot depth }, push order
	local fpDepth, usesRbp = nil, false
	local prologueEnd = #body + 1
	do
		local i = 1
		while i <= #body do
			local st = body[i]
			if st.kind ~= "insn" then
				if st.kind == "label" and targeted[st.name] then prologueEnd = i break end
			else
				local m = string.lower(st.mnemonic)
				local b = x86.baseMnemonic(m)
				local ops = st.operands
				if b == "push" and ops[1] and ops[1].kind == "reg" and x86.CALLEE_SAVED[ops[1].name] then
					depth += 8
					table.insert(pstack, { name = ops[1].name, depth = depth })
				elseif b == "pop" and ops[1] and ops[1].kind == "reg" and x86.CALLEE_SAVED[ops[1].name]
					and #pstack > 0 and pstack[#pstack].name == ops[1].name
					and pstack[#pstack].depth == depth
					and not (usesRbp and ops[1].name == "rbp") then
					-- net-balanced transient save pair inside the prefix: the pop must
					-- restore the LAST outstanding push of the SAME register at the SAME
					-- slot depth — anything else reads a slot that is not that register's
					-- save (e.g. the saved frame pointer), so it is not a save/restore
					-- round-trip and cannot be netted out. (Popping the frame pointer
					-- itself is an rbp clobber, not frame setup: it ends the prefix.)
					depth -= 8
					table.remove(pstack)
				elseif isSpillPushForm(st) then
					-- Spill push (see above): frame setup, not a save — the store
					-- reserves its slot in the frame.
					depth += 8
					spillBytes += 8
				elseif b == "push" or b == "pop" or x86.isFlagStackXfer(m) then
					-- any other push/pop (or an RFLAGS stack transfer — the word it
					-- moves is modeled only at a provable post-prologue depth) is
					-- never prologue-transparent (implicit rsp write)
					prologueEnd = i break
				elseif b == "mov" and ops[1] and ops[1].name == "rbp" and ops[2] and ops[2].rc == "sp"
					and not usesRbp then
					usesRbp = true; fpDepth = depth
				elseif b == "lea" and ops[1] and ops[1].kind == "reg" and ops[1].name == "rbp"
					and ops[2] and ops[2].kind == "mem" and ops[2].base and ops[2].base.rc == "sp"
					and not ops[2].index and not ops[2].dispSym and (ops[2].dispVal or 0) == 0 then
					-- `leaq 0(%rsp), %rbp` (and the displacement-less `leaq (%rsp), %rbp`):
					-- the lea spelling of the frame-pointer establishment `movq %rsp, %rbp`
					-- (OpenSSL's aes-gcm-avx512 prologue). Identical semantics — rbp parks
					-- the current rsp — so it establishes the frame pointer exactly like the
					-- mov form: usesRbp/fpDepth drive rbpNorm, the fp state, and every
					-- rbp-relative key downstream. A SECOND establishment inside the prefix
					-- ends it (the statement is then rejected mid-body by the rewrite, the
					-- same as a second `movq %rsp, %rbp`, which the prefix ends via
					-- insnTouchesFrame — this branch must run before the transparent
					-- lea-reads-sp case below, which would otherwise swallow it).
					if not usesRbp then
						usesRbp = true; fpDepth = depth
					else
						prologueEnd = i break
					end
				elseif b == "mov" and ops[2] and ops[2].rc == "sp"
					and ops[1] and ops[1].rc == "gpr" and ops[1].name ~= "rbp" then
					-- `movq %rsp, %rbx`: the prologue parked the (known) stack pointer in
					-- a register. Transparent to the frame geometry: the save is DROPPED
					-- by the rewrite (sarcasm synthesizes its own frame, so the parked
					-- value has no output meaning), the rsp-depth fixpoint tracks the
					-- save (a later `movq %rbx, %rsp` recovery revives the saved depth),
					-- and the rewrite rejects saves into anything but a callee-saved
					-- register (a call would clobber the rest).
				elseif b == "sub" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "imm" then
					depth += immVal(ops[2]); subBytes += immVal(ops[2])
				elseif b == "add" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "imm"
					and immVal(ops[2]) <= subBytes then
					-- undoes part of the prologue allocation: net it out (an epilogue add
					-- is never reached here — the scan has long stopped at the body).
					-- EXCEPTION: a degenerate body whose traffic never syntactically
					-- touches rsp/rbp (alias-register-only traffic, string ops aside)
					-- lets the scan run all the way into the epilogue: an `add` whose
					-- NEXT instruction is a `ret` IS the epilogue teardown, so end the
					-- prefix there instead of netting (netting would make D0 the
					-- epilogue depth and degenerate every frame coordinate).
					do
						local ni = nextInsn[i]
						if ni and body[ni] and body[ni].kind == "insn"
							and x86.baseMnemonic(string.lower(body[ni].mnemonic)) == "ret" then
							prologueEnd = i break
						end
					end
					depth -= immVal(ops[2]); subBytes -= immVal(ops[2])
				elseif b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2]
					and ops[2].kind == "mem" and ops[2].base and ops[2].base.rc == "sp"
					and not ops[2].index and not ops[2].dispSym then
					-- `leaq -N(%rsp), %rsp` is the lea spelling of `subq $N, %rsp`
					-- (x25519/gf2m/bsaes prologue shape); `leaq +N(%rsp), %rsp`
					-- undoes it like the constant add.
					local v = ops[2].dispVal or 0
					if v < 0 then
						depth += -v; subBytes += -v
					elseif v > 0 and v <= subBytes then
						depth -= v; subBytes -= v
					elseif v ~= 0 then
						prologueEnd = i break
					end
				elseif b == "and" and ops[1] and ops[1].rc == "sp"
					and ops[2] and ops[2].kind == "imm" then
					-- `and $-N, %rsp` (N a power of two >= 16 — the rsaz-avx2
					-- `and $-1024/-64/-32, %rsp` shape): rounds the prologue rsp
					-- DOWN by at most N-1 bytes for vector alignment. Transparent
					-- to the frame geometry (the depth stays put — the prefix
					-- holds no slot traffic by construction, so every later slot
					-- keys off the same post-and numbering) and dropped by the
					-- rewrite; the alignment is recorded so the synthesized frame
					-- is emitted N-aligned and aligned accesses keep proving.
					local n = -immVal(ops[2])
					if isStackAlign(n) then
						if n > frameAlignReq then
							frameAlignReq = n; frameAndDepth = depth
						end
						-- The and's own position: the carrier/alias keying below
						-- decides "parked after the and" by prefix order when the
						-- alignment and is a PROLOGUE one (the mid-function walk
						-- owns the mid-function case).
						frameAndIdx = i
					else
						prologueEnd = i break
					end
				elseif b == "lea" and ops[1] and ops[1].kind == "reg" and ops[1].rc ~= "sp"
					and ops[2] and ops[2].kind == "mem"
					and ((ops[2].base and (ops[2].base.rc == "sp" or (usesRbp and ops[2].base.name == "rbp")))
						or (ops[2].index and ops[2].index.rc == "sp")) then
					-- A lea that merely READS the stack pointer (or frame pointer) as
					-- an address base — the `leaq (%rsp),%rax` saved-rsp
					-- materialization (rsaz-avx2/gather5) — does not alter the
					-- stack state, so it is transparent for the geometry scan
					-- (exactly like the `movq %rsp,%rbx` save form): the carrier /
					-- lea-save / escape machinery downstream owns its validity.
					-- (Without this the prefix would stop here and a following
					-- constant frame allocation would be orphaned into a
					-- "mid-function" adjustment.)
				elseif b == "lea" and ops[1] and ops[1].kind == "reg" and ops[1].rc == "sp" then
					prologueEnd = i break
				elseif insnWritesSp(st) then
					-- any other rsp write (`leaq -N(%rbp), %rsp` teardown, register-source
					-- `subq %reg, %rsp`, `movq %rbp, %rsp`) is not prologue frame setup
					prologueEnd = i break
				elseif b == "nop" then
					-- endbr64 / padding: transparent
				elseif b == "jcc" then
					-- A leading conditional branch is part of the prologue when it guards
					-- the frame setup (an early-exit path that runs with the caller's rsp
					-- and returns: `test %edi,%edi; je .Learly` before `pushq %rbp`).
					-- Hand-written code and optimized builds both spell it this way. The
					-- scan stays sound without inspecting the target path: a targeted
					-- label anywhere ahead still ends the prefix, and the CFG depth/save
					-- analysis merges the early path's depth 0 with the frame path's —
					-- any shape where the guard can jump into or out of the middle of the
					-- setup disagrees and is rejected downstream. Frame-pointer-relative
					-- accesses are additionally gated on per-point frame-pointer
					-- establishment (x86_fp), so a path that skipped the setup can never
					-- virtualize against a frame pointer it does not have.
				elseif b == "jmp" or b == "call" or b == "ret" or b == "leave"
					or string.match(m, "^loop") then
					prologueEnd = i break
				elseif st.repPrefix ~= nil
					or (#ops == 0 and (string.match(m, "^movs[bwlqd]?$")
						or string.match(m, "^stos[bwlqd]?$"))) then
					-- A string op (rep movs*/stos* or the bare single-step form):
					-- implicit %rsi/%rdi/%rcx memory traffic — never prologue-
					-- transparent even though it names no operands. (Without this,
					-- a body whose only frame traffic is through a stack-alias
					-- register and a rep would swallow its own epilogue into the
					-- prefix: the teardown `add` nets out, D0 becomes the epilogue
					-- depth, and every frame coordinate degenerates.)
					prologueEnd = i break
				elseif insnTouchesFrame(st, usesRbp) then
					prologueEnd = i break
				elseif usesRbp and ops[1] and ops[1].kind == "reg" and ops[1].name == "rbp" then
					prologueEnd = i break  -- rbp clobbered after being established as fp
				end
			end
			i += 1
		end
	end
	D0 = depth
	-- The rbp NORMALIZATION base: rbp-relative displacement d normalizes to
	-- frame offset rbpNorm + d (rbp = rsp + rbpNorm at the post-prologue rsp).
	-- This must be exactly depth - fpDepth for the rsp/rbp normalizations to
	-- agree on one slot per address — including the rbp-at-the-bottom layout
	-- (`leaq -N(%rsp),%rsp; movq %rsp,%rbp` — bsaes/aesni-xts), where it is 0.
	rbpNorm = usesRbp and (depth - fpDepth) or 0
	-- Frame extent for BOUNDS checks: the constant allocation plus the
	-- spill-push reservation, but never below the normalization base (the
	-- save pushes are covered by the save-slot model, which bypasses the
	-- bounds check). At the rbp-at-the-bottom layout the ordinary frame sits
	-- ABOVE rbp (rbp-relative [0, N)), so the extent is the allocation, not
	-- the (zero) normalization base.
	local frameSize = usesRbp and math.max(rbpNorm, subBytes + spillBytes)
		or (subBytes + spillBytes)

	-- Control-flow rsp-depth + save-pairing analysis. The abstract state is (depth, saves):
	-- depth is bytes below the entry rsp ("dyn" when unknown), and saves is the abstract
	-- stack of outstanding pushes — { name = reg, num = regnum, depth = slot depth,
	-- redefined = bool } in push order — used to prove that every dropped pop restores
	-- what a matching dropped push saved. `redefined` records whether the pushed
	-- register's web was REDEFINED between the push and here (any modeled definition:
	-- an explicit move, an implicit clobber like cpuid's ebx, ...): the drop-the-pop
	-- model is only sound while the web still holds the pushed value, so a redefined
	-- save rejects aliasing slot accesses (the slot holds the value pushed BEFORE the
	-- redefinition) and its pop's reload is checked for observed uses (see the
	-- checkLostReload scan in the annotation pass below). transfer maps the state
	-- before an instruction to the state after it; merges require agreement on both
	-- components (a disagreement, or any unanalyzable-rsp influence, yields "dyn" —
	-- unprovable).
	local classCache = {}
	-- Classify via x86.classify, which rejects unsafe instructions
	-- (fp.unsafeReject) by raising. analyzeFrame runs strictly before the
	-- lift/DCE/save-scan in transform.transformFunction, so a rejection here
	-- (or in the lift's own classify pass) aborts compilation before DCE or
	-- the flag save-scan ever sees the instruction — see x86_64_isa.luau's
	-- ORDERING PROOF.
	local function classOf(st)
		local c = classCache[st]
		if c == nil then c = x86.classify(st); classCache[st] = c end
		return c
	end
	local function truncateSaves(sv, nd)
		-- Drop save entries whose slots are no longer above rsp (deeper than the new
		-- depth): after rsp is reset to a known position (`leave` / `movq %rbp, %rsp`) the
		-- saves below it are abandoned.
		if sv == "dyn" then return "dyn" end
		local c = {}
		for _, e in sv do
			if e.depth > nd then break end
			table.insert(c, e)
		end
		return c
	end
	-- The core rsp-depth/save-stack/frame-pointer transition. `recoverDepth` is the depth
	-- a `movq %reg, %rsp` recovery revives (non-nil only when the incoming smap holds an
	-- outstanding, unredefined prologue rsp save for the source register — see transfer).
	local function transferCore(st, d, sv, fp, recoverDepth)
		local m = string.lower(st.mnemonic)
		local b = x86.baseMnemonic(m)
		local ops = st.operands
		if b == "leave" then
			-- `leave` = `movq %rbp, %rsp` + `popq %rbp`: recovers a known depth (and the
			-- matching save shape) from the frame pointer; without one — or when the
			-- frame pointer is not provably established on every path to this point — it
			-- is unprovable. Hardware `leave` also RESTORES the caller's frame pointer
			-- into %rbp, so the frame-pointer state ends here either way.
			if not usesRbp or fp ~= "est" then return "dyn", "dyn", fp end
			local nd = fpDepth - 8
			return nd, truncateSaves(sv, nd), "no"
		end
		if b == "mov" and ops[1] and ops[1].rc == "sp" then
			-- A `movq %rA, %rsp` rsp update from an unvalidated source (not a recovery):
			-- outstanding saves survive at their slots.
			-- `movq %rbp, %rsp` restores rsp from the frame pointer — an epilogue
			-- teardown, or a mid-function free. Like `leave`, it recovers a
			-- known depth (and the surviving save shape) from the frame pointer even
			-- when an unanalyzable adjustment made the depth unknown, so the teardown `popq %rbp`
			-- after it still pairs with the prologue save. Unlike `leave` it does not
			-- touch %rbp, so the frame-pointer state is unchanged. Any other source
			-- makes the depth unknown — except a recovery from a prologue rsp save
			-- (validated by the smap in transfer below), which is just as known-good.
			if usesRbp and fp == "est" and ops[2] and ops[2].name == "rbp" then
				return fpDepth, truncateSaves(sv, fpDepth), fp
			end
			if recoverDepth then
				return recoverDepth, truncateSaves(sv, recoverDepth), fp
			end
			return "dyn", "dyn", fp
		end
		if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
			and usesRbp and fp == "est" and ops[2].base and ops[2].base.name == "rbp"
			and not ops[2].index and not ops[2].dispSym then
			-- `leaq -N(%rbp), %rsp` restores rsp from the frame pointer: the lea form of
			-- `movq %rbp, %rsp`, used when callee-saved registers were pushed after the
			-- frame pointer was established — as an epilogue teardown, or as a
			-- mid-function free. Like `leave`/`movq %rbp, %rsp` it recovers a
			-- known depth (fpDepth + N) and the surviving save shape from the frame
			-- pointer even when an unanalyzable adjustment made the depth unknown; the pops that
			-- follow must still pair with the prologue saves — a mismatching depth is
			-- caught by the save-pairing analysis at the pops themselves.
			local nd = fpDepth - (ops[2].dispVal or 0)
			return nd, truncateSaves(sv, nd), fp
		end
		if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
			and ops[2].base and ops[2].base.kind == "reg"
			and (ops[2].base.name ~= "rbp" or not usesRbp)
			and not ops[2].index and not ops[2].dispSym and recoverDepth then
			-- `leaq K(%reg), %rsp` recovers rsp from a saved-rsp CARRIER (the lea form of
			-- `movq %reg, %rsp` — the perlasm `leaq (%rsi), %rsp`): the incoming smap
			-- proved the base register holds the save value (recoverDepth = the parked
			-- depth minus K, computed in transfer), so exactly like the mov form the
			-- recovery revives that known depth and the surviving save shape.
			return recoverDepth, truncateSaves(sv, recoverDepth), fp
		end
		if b == "mov" and ops[1] and ops[1].name == "rbp" and ops[2] and ops[2].rc == "sp" then
			-- Establishing the frame pointer (`movq %rsp, %rbp`): from here %rbp holds a
			-- fixed frame-relative value on this path. Re-establishing it mid-function
			-- is rejected by the rewrite, so this is the one transition to "est".
			return d, sv, "est"
		end
		if b == "lea" and ops[1] and ops[1].kind == "reg" and ops[1].name == "rbp"
			and ops[2] and ops[2].kind == "mem" and ops[2].base and ops[2].base.rc == "sp"
			and not ops[2].index and not ops[2].dispSym and (ops[2].dispVal or 0) == 0 then
			-- `leaq 0(%rsp), %rbp`: the lea spelling of the frame-pointer establishment
			-- (see the prologue prefix scan) — the same single transition to "est".
			return d, sv, "est"
		end
		if isDynDepth(d) or sv == "dyn" then
			-- At an unknown depth the DEPTH is unprovable, but the outstanding
			-- saves still sit at their slots: an instruction that cannot touch rsp
			-- propagates the save stack unchanged so a later `leave`/`movq %rbp, %rsp`
			-- can revive it (the rewrite rejects every stack access, push/pop, or
			-- return that would execute at an unknown depth, so only inert computation
			-- may carry it). A push at an unknown depth — or any state whose save shape
			-- is already unknown — is unprovable. A pop's slot position is unknowable
			-- here, but it still consumes the most recent outstanding save: whether the
			-- pop may be DROPPED as that save's restore is decided by the annotation
			-- pass, which pairs it only inside a verified epilogue teardown and rejects
			-- it everywhere else. A constant rsp adjustment pops nothing, so the save
			-- stack survives it too (the annotation pass decides whether the adjustment
			-- is a verified teardown `addq $imm, %rsp`). The frame-pointer state is
			-- INDEPENDENT of the rsp poison: an unanalyzable adjustment perturbs rsp, never rbp, so
			-- rbp-relative spill traffic stays provable through the poisoned region and
			-- fp is propagated unchanged.
			-- The {anchored} region scope propagates through inert computation exactly
			-- like the plain "dyn": the andq only lowered rsp, and nothing may move it
			-- again before the recovery — every rsp write below degrades to plain "dyn".
			if sv == "dyn" then return "dyn", "dyn", fp end
			if b == "push" or x86.isFlagStackPush(m) then return "dyn", "dyn", fp end
			if b == "pop" or x86.isFlagStackPop(m) then
				local c = { table.unpack(sv) }
				if #c > 0 then
					table.remove(c)
					return "dyn", c, fp
				end
				return "dyn", "dyn", fp
			end
			if (b == "sub" or b == "add") and ops[1] and ops[1].rc == "sp" then
				return "dyn", sv, fp
			end
			-- Any modeled definition of a pushed register breaks the link between its
			-- outstanding save and the register's CURRENT web exactly as at a known
			-- depth (the def-marking below is reached only on the known-depth path,
			-- which this branch returns before): mark the save redefined so a pop
			-- paired after the depth is revived cannot silently drop a reload of a
			-- stale value. (Never mutate the incoming list — it is shared across the
			-- fixpoint.)
			local c = classOf(st)
			if #c.defs > 0 and #sv > 0 then
				local hit = false
				for _, e in sv do
					if not e.redefined then
						for _, dd in c.defs do
							if dd.kind == "reg" and dd.rc == "gpr" and dd.num == e.num then
								hit = true break
							end
						end
					end
					if hit then break end
				end
				if hit then
					local out = {}
					for _, e in sv do
						local redefined = e.redefined
						if not redefined then
							for _, dd in c.defs do
								if dd.kind == "reg" and dd.rc == "gpr" and dd.num == e.num then
									redefined = true break
								end
							end
						end
						table.insert(out, { name = e.name, num = e.num, depth = e.depth,
							spill = e.spill, redefined = redefined })
					end
					sv = out
				end
			end
			-- Inert computation: propagate the depth unchanged.
			return d, sv, fp
		end
		if x86.isFlagStackPush(m) then
			-- PUSHF/PUSHFQ: the RFLAGS word is a SPILL stack word — exactly like a
			-- caller-saved register's spill push, but its value is the materialized
			-- flags (the transform lowers the statement into pushfq; pop <flags web>,
			-- and the rewrite files that web into the word's slot right after). The
			-- spill mark keeps the save-slot alias model and the dropped-save model
			-- away from the word: the slot is ordinary, virtualized frame storage.
			local c = { table.unpack(sv) }
			table.insert(c, { name = x86.FLAGS_REG_NAME, num = x86.FLAGS_REG_NUM,
				depth = d + 8, spill = true, redefined = false })
			return d + 8, c, fp
		end
		if x86.isFlagStackPop(m) then
			-- POPF/POPFQ: consumes the word at the current depth — the same shape
			-- maintenance as a pop. Whether the word is provable (an outstanding
			-- pushfq word or a materialized spill push, never a dropped save) is
			-- verified from the INCOMING state at the popfq itself (the
			-- x86_flagsPop annotation); an unprovable word poisons the save stack
			-- exactly like an unpaired pop.
			local c = { table.unpack(sv) }
			if #c > 0 and c[#c].depth == d then
				table.remove(c)
				return d - 8, c, fp
			end
			return d - 8, "dyn", fp
		end
		if b == "push" then
			local c = { table.unpack(sv) }
			table.insert(c, {
				name = (ops[1] and ops[1].kind == "reg") and ops[1].name or "?",
				num = (ops[1] and ops[1].kind == "reg") and ops[1].num or nil,
				depth = d + 8,
				-- A spill push (the prologue form above, or the same shape at a
				-- known mid-function depth): the slot is an ordinary frame slot,
				-- not a dropped save — the restore-load pass and the save-slot
				-- alias model skip spill entries, and a pop of one is unpaired.
				spill = (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
					and not x86.CALLEE_SAVED[ops[1].name]) or nil,
				redefined = false })
			return d + 8, c, fp
		end
		if b == "pop" then
			-- The pop consumes the slot at the current depth. Whether that slot holds a
			-- matching save is verified from the INCOMING state at the pop (see the
			-- x86_unpairedPop annotation); here only the stack shape is maintained.
			local c = { table.unpack(sv) }
			if #c > 0 and c[#c].depth == d then
				-- A paired `popq %rbp` restores the SAVED frame pointer — the caller's —
				-- so %rbp stops being this function's frame pointer from here on: any
				-- later rbp-relative access would address the caller's frame. The
				-- per-point frame-pointer state records exactly that. A SPILL pop (a
				-- caller-saved register's parked slot) is an ordinary slot load, not a
				-- save restore — but one whose destination is %rbp still clobbers the
				-- frame pointer, so poison the state exactly the same way.
				local poppedFp = usesRbp and ops[1] and ops[1].kind == "reg"
					and (c[#c].name == "rbp" or (c[#c].spill and ops[1].name == "rbp"))
				table.remove(c)
				return d - 8, c, (poppedFp and "no") or fp
			end
			return d - 8, "dyn", fp
		end
		local nd = d
		if (b == "sub" or b == "add") and ops[1] and ops[1].rc == "sp" then
			if ops[2] and ops[2].kind == "imm" then
				nd = (b == "sub") and (d + immVal(ops[2])) or (d - immVal(ops[2]))
			else
				return "dyn", "dyn", fp  -- unanalyzable rsp write
			end
		end
		if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
			and ops[2].base and ops[2].base.rc == "sp" and not ops[2].index and not ops[2].dispSym then
			-- `leaq K(%rsp), %rsp`: the lea spelling of a constant rsp adjustment
			-- (prologue `leaq -N(%rsp), %rsp` allocation, epilogue `leaq +N(%rsp), %rsp`
			-- teardown) — same depth math as the sub/add forms.
			nd = d - (ops[2].dispVal or 0)
		end
		if nd < d then sv = truncateSaves(sv, nd) end
		-- A full-width (8-byte) plain store to a save slot RE-SYNCS the slot with the
		-- pushed register's web: the hardware slot receives the stored value and the
		-- save-slot model defines the register's web with exactly that value, so the
		-- two agree again from here on (a redefinition before the store is harmless —
		-- the store overwrites both). Clear the save's redefined state for such a
		-- store; every other access keeps the state from the def-marking below.
		if b == "mov" and sv ~= "dyn" and #sv > 0 and d == D0 then
			local mem = nil
			for _, o in ops do if o.kind == "mem" then mem = o end end
			if mem and not mem.index and not mem.dispSym and mem.base
				and (mem.base.rc == "sp" or (usesRbp and mem.base.name == "rbp")) then
				local w0 = gprStackWidth(m, mem)
				local off = (mem.dispVal or 0)
				if mem.base.rc ~= "sp" then off = rbpNorm + off end
				if w0 == 8 then
					local changed = false
					for _, e in sv do
						if e.redefined and off == d - e.depth then changed = true break end
					end
					if changed then
						local out = {}
						for _, e in sv do
							if e.redefined and off == d - e.depth then
								table.insert(out, { name = e.name, num = e.num,
									depth = e.depth, spill = e.spill, redefined = false })
							else
								table.insert(out, e)
							end
						end
						sv = out
					end
				end
			end
		end
		-- Any modeled definition of a pushed register breaks the link between its
		-- outstanding save and the register's CURRENT web: the save's slot still holds
		-- the value pushed before the redefinition, but the dropped pop would restore
		-- the save into a register whose web now holds something else. (Never mutate
		-- the incoming list — it is shared across the fixpoint.)
		local c = classOf(st)
		if #c.defs > 0 and sv ~= "dyn" and #sv > 0 then
			local hit = false
			for _, e in sv do
				if not e.redefined then
					for _, dd in c.defs do
						if dd.kind == "reg" and dd.rc == "gpr" and dd.num == e.num then
							hit = true break
						end
					end
				end
				if hit then break end
			end
			if hit then
				local out = {}
				for _, e in sv do
					local redefined = e.redefined
					if not redefined then
						for _, dd in c.defs do
							if dd.kind == "reg" and dd.rc == "gpr" and dd.num == e.num then
								redefined = true break
							end
						end
					end
					table.insert(out, { name = e.name, num = e.num, depth = e.depth,
						spill = e.spill, redefined = redefined })
				end
				return nd, out, fp
			end
		end
		return nd, sv, fp
	end

	-- The full transition, adding the SAVED-RSP CARRIER MAP (smap) to the core state.
	-- The prologue `movq %rsp, %reg` save (and its `leaq K(%rsp), %reg` form) parks a
	-- value sarcasm's synthesized frame makes PHANTOM: the save, every carrier flow of
	-- it, and the recovery are ALL dropped, so the value has no output existence and
	-- injected runtime calls / SysV callee clobbers can never corrupt it. The smap
	-- tracks, per program point, which CARRIERS provably hold the save value:
	--   * register carriers — keyed by register name; a save may be copied reg→reg
	--     (`movq %carrier, %reg`), keeping both;
	--   * frame-slot carriers — keyed by slot offset (`movq %carrier, off(%rsp)` /
	--     `off(%rbp)`); a full-width load from the slot (`movq off(%rsp), %reg`)
	--     re-materializes a register carrier. Slot keys live in the access's own
	--     coordinates: "S"..<normalized frame offset> at provable depth (the same
	--     key the slot virtualization uses), "D"..<disp> at "dyn" depth (the
	--     anchored-region coordinates — cleared on any later rsp write, which would
	--     shift those coordinates).
	-- A recovery (`movq %reg, %rsp`, or `leaq K(%reg), %rsp`) may read any carrier
	-- that provably holds the save value and revives the parked depth (minus K).
	-- Any OTHER read of a register carrier, and any redefinition of one, poisons it
	-- (the `redefined` merge rule); a store overlapping a slot carrier poisons the
	-- slot, and a non-carrier-form LOAD from it is rejected outright (it would
	-- observe the phantom value). CFG joins require per-key agreement.
	-- Register class is unrestricted: the carrier discipline makes every use either
	-- dropped or rejected, so a caller-saved register (or a caller-saved save riding
	-- through a region slot — the sha256/sha512/wp idiom) is exactly as sound as a
	-- callee-saved one. EXCEPTIONS: (a) with incoming yolo STACK ARGUMENTS
	-- (yoloCount > 0) a save additionally serves as an entry-rsp ALIAS whose
	-- entry-time argument reads the rewrite redirects to the arguments' slots
	-- (those reads remove the register use entirely); (b) the historical region
	-- corner — a caller-saved save with a static frame-escape region based at
	-- rsp+0, which the region redirect kept alive as a REAL value — is GONE with
	-- the region sources, so every recorded save is a phantom carrier. Every run
	-- carries the (always empty) regions list, so all saves (callee- AND
	-- caller-saved, prologue and post-prologue) are recorded on every run.
	-- The FP/SIMD knowledge module, aliased for scopes where the `fp` frame-pointer
	-- state parameter shadows the file-scope `fp` module.
	local fpKnowledge = fp
	local function slotKeyOf(mem, d, st)
		-- Carrier-slot key for a plain base+disp stack memory operand (see the smap
		-- comment), or nil for a non-stack/unkeyable operand. A clone statement's
		-- displacement keys 8 lower (the +8 rule — localcall.luau).
		if not (mem and mem.kind == "mem" and mem.base and mem.base.kind == "reg"
			and not mem.index and not mem.dispSym) then return nil end
		local disp = mem.dispVal or 0
		local bias = (st and st.localClone) and 8 or 0
		if mem.base.rc == "sp" then
			if isDynDepth(d) then
				-- "dyn"-space coordinates. A clone statement's displacement keys
				-- 8 lower at ANY dynamic depth (the +8 rule — the return-address
				-- word sits between the clone's rsp and the caller's frame, so
				-- the same physical slot is spelled 8 higher inside a clone);
				-- in a {dynreg} scope the region coordinate additionally adds
				-- the region's bias.
				local dd = disp - ((st and st.localClone) and 8 or 0)
				if type(d) == "table" and d.dynreg then dd = dd + d.dynreg.bias end
				return "D" .. dd, dd, "D"
			end
			if d == nil then return nil end
			return "S" .. (disp + D0 - d - bias), disp + D0 - d - bias, "S"
		end
		if usesRbp and mem.base.name == "rbp" then
			return "S" .. (rbpNorm + disp), rbpNorm + disp, "S"
		end
		return nil
	end
	-- A region-pointer value: a register provably holding <region base + K>, from a
	-- region-redirect lea (`leaq K(%rsp), %reg` landing in a fixed region — the wp
	-- `leaq 128(%rsp), %r10`). Recorded in the smap as { roff = K, rbase, rspace }:
	-- the register is a REAL pointer (reads are legal — unlike carriers), and a
	-- carrier save-store/load THROUGH it keys the slot in the region's coordinate
	-- space (`movq %rax, 32(%r10)` parks the save in region slot 128+32).
	-- Is `st` such a region-pointer lea? Returns (roff, rbase, rspace).
	local function regionLeaOf(st, d)
		local ops = st.operands
		if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "lea" then return nil end
		if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr" and ops[1].name ~= "rbp") then return nil end
		local mem = ops[2]
		if not (mem and mem.kind == "mem" and mem.base and mem.base.kind == "reg"
			and not mem.index and not mem.dispSym) then return nil end
		local disp = mem.dispVal or 0
		if mem.base.rc == "sp" and type(d) == "table" then
			-- anchored scope: raw displacements are the region's coordinates —
			-- except that a clone statement's displacement keys 8 lower (the +8
			-- rule — the return-address word sits between the clone's rsp and the
			-- caller's frame, so the same physical slot is spelled 8 higher inside
			-- a clone). slotKeyOf applies the same compensation to the region's
			-- memory accesses; the address-computing lea must agree with them
			-- (rsaz_512_mul's `leaq 8(%rsp),%rdi` t[] base in the __rsaz_512_mul
			-- clone — it must land on region+0, not region+8).
			local r = d.anchored
			if r then
				local dd = disp - (st.localClone and 8 or 0)
				if dd >= r.base and dd < r.base + r.size then
					return dd - r.base, r.base, "D"
				end
				return nil
			end

			if d.dynreg then
				-- dynamic-region scope: every lea computes a region pointer
				-- (region coordinate = bias + disp; -8 for a clone — the +8
				-- rule), e.g. aes's `leaq 768(%rsp),%rbp` cache-alias trick.
				return disp + d.dynreg.bias - (st.localClone and 8 or 0), 0, "D"
			end
			return nil
		end
		if d == nil or isDynDepth(d) then return nil end
		local off = disp
		if mem.base.rc ~= "sp" then
			if usesRbp and mem.base.name == "rbp" then off = rbpNorm + disp
			else return nil end
		elseif st.localClone then
			-- the +8 rule (the return-address compensation on rsp-relative
			-- clone accesses — localcall.luau)
			off = off + D0 - d - 8
		end
		local r = regionOf(off, false)
		if r then return off - r.base, r.base, "S" end
		return nil
	end
	-- Is `st` a full-width `movq %rA, %rB` copy of a region-pointer value?
	local function roffCopyOf(st, smap)
		local ops = st.operands
		if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "mov" then return nil end
		if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr" and ops[1].name ~= "rbp"
			and (ops[1].width == 64 or ops[1].width == nil)) then return nil end
		if not (ops[2] and ops[2].kind == "reg" and ops[2].rc == "gpr" and ops[2].name ~= "rbp"
			and (ops[2].width == 64 or ops[2].width == nil)) then return nil end
		local e = smap[ops[2].name]
		if e and e.roff and not e.redefined then return e end
		return nil
	end
	-- Resolve a memory access through a saved-rsp carrier register to a normalized
	-- frame offset: `mov %rsp, %reg` / `lea K(%rsp), %reg` parks entry_rsp - P
	-- (P = e.depth -- an ABSOLUTE value, identical on every path that reaches
	-- here with the carrier live), so `disp(%reg)` addresses normalized offset
	-- o = disp + D0 - P (- 8 inside a clone, the +8 rule). Returns o, or nil
	-- when the access is not an alias-resolvable stack access (indexed or
	-- symbolic displacement, address-taking lea, or a base that is not this
	-- carrier). Yolo-argument reads, region membership, and frame bounds are
	-- the CALLERS' business (they run their own checks first/after).
	local function aliasSlotOf(st, mem, e)
		if not (mem and mem.base and mem.base.kind == "reg" and mem.base.name == e.name) then
			return nil
		end
		if mem.index or mem.dispSym then return nil end
		if x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then return nil end
		return (mem.dispVal or 0) + D0 - e.depth - ((st.localClone and 8) or 0)
	end
	-- Slot key for a plain base+disp memory operand: stack-based (slotKeyOf), or a
	-- region-pointer-derived base register (smap[base].roff) — the latter keys the
	-- slot in the region's own coordinate space, so the two spellings of one slot
	-- (rsp-relative direct, or through a region pointer) unify.
	local function memSlotKey(mem, d, smap, st)		if not (mem and mem.kind == "mem" and mem.base and mem.base.kind == "reg"
			and not mem.index and not mem.dispSym) then return nil end
		if mem.base.rc == "sp" or (usesRbp and mem.base.name == "rbp") then
			return slotKeyOf(mem, d, st)
		end
		local e = smap and smap[mem.base.name]
		if e and e.roff and not e.redefined then
			local off = e.rbase + e.roff + (mem.dispVal or 0)
			return e.rspace .. off, off, e.rspace
		end
		return nil
	end
	-- Is `st` a full-width `movq %carrier, %reg` carrier copy (reg→reg save flow)?
	-- rbp may hold a carrier only when it is not the frame pointer (usesRbp false —
	-- the rsaz-avx2 `movq %rax,%rbp` / `movq %rbp,%rax` shape).
	local function carrierCopyOf(st, smap)
		local ops = st.operands
		if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "mov" then return nil end
		if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
			and (ops[1].name ~= "rbp" or not usesRbp)
			and (ops[1].width == 64 or ops[1].width == nil)) then return nil end
		if not (ops[2] and ops[2].kind == "reg" and ops[2].rc == "gpr"
			and (ops[2].name ~= "rbp" or not usesRbp)
			and (ops[2].width == 64 or ops[2].width == nil)) then return nil end
		local e = smap[ops[2].name]
		if e and not e.redefined and not e.slot and not e.roff and not e.conflict then return e end
		return nil
	end
	-- Is `st` a full-width `movq %carrier, off(%rsp)/off(%rbp)` save-store (register
	-- carrier -> slot carrier)? Returns the carrier and the slot key.
	local function saveStoreOf(st, smap, d)
		local ops = st.operands
		if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "mov" then return nil end
		local mem = ops[1]
		if not (mem and mem.kind == "mem") then return nil end
		if not (ops[2] and ops[2].kind == "reg" and ops[2].rc == "gpr"
			and (ops[2].name ~= "rbp" or not usesRbp)
			and (ops[2].width == 64 or ops[2].width == nil)) then return nil end
		local e = smap[ops[2].name]
		if not (e and not e.redefined and not e.slot and not e.roff and not e.conflict) then return nil end
		local key, off, space = memSlotKey(mem, d, smap, st)
		if not key then return nil end
		return e, key, off, space
	end
	-- Is `st` a full-width `movq off(%rsp)/off(%rbp), %reg` carrier-load (slot carrier
	-- -> register carrier)? Returns the slot carrier.
	local function carrierLoadOf(st, smap, d)
		local ops = st.operands
		if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "mov" then return nil end
		if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
			and (ops[1].name ~= "rbp" or not usesRbp)
			and (ops[1].width == 64 or ops[1].width == nil)) then return nil end
		local key = memSlotKey(ops[2], d, smap, st)
		if not key then return nil end
		local e = smap[key]
		if e and not e.redefined and not e.conflict then return e end
		return nil
	end
	-- Is `st` a `movq K(%carrier), %dest` epilogue restore load through a saved-rsp
	-- carrier (the perlasm movq-restore riding the recovered pointer)? The carrier's
	-- parked depth minus K names the read save slot; it is a restore iff an
	-- outstanding pushed save sits at exactly that depth and the destination is that
	-- save's register. Returns the save-stack entry, or nil.
	local function restoreLoadOf(st, smap, sv)
		local ops = st.operands
		if x86.baseMnemonic(string.lower(st.mnemonic)) ~= "mov" then return nil end
		if not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
			and (ops[1].width == 64 or ops[1].width == nil)) then return nil end
		local mem = ops[2]
		if not (mem and mem.kind == "mem" and mem.base and mem.base.kind == "reg"
			and not mem.index and not mem.dispSym) then return nil end
		if not (sv and sv ~= "dyn") then return nil end
		local e = smap and smap[mem.base.name]
		if not (e and not e.redefined and not e.slot and not e.roff and not e.conflict) then return nil end
		local sdepth = e.depth - (mem.dispVal or 0)
		for _, s in sv do
			if s.depth == sdepth then
				if s.name == ops[1].name then return s end
				return nil
			end
		end
		return nil
	end
	-- Is `st` a memory access THROUGH the carrier register `e` that lands inside the
	-- {anchored} region scope the depth `d` names? A pre-alloca frame-anchor carrier
	-- (poly1305's `leaq -88(%rsp), %r11` ahead of a large `subq $0x178, %rsp` adjustment (historically `;! alloca
	-- result size=376`) parks the absolute value entry_rsp - e.depth; the region
	-- base is entry_rsp - scopeDepth EXACTLY (a subq-only anchor — the andq form
	-- loses scopeDepth and with it this identity), so the access addresses region
	-- offset scopeDepth - e.depth + disp and redirects to the region pointer (the
	-- transform's region redirect; the frame rewrite passes it through marked
	-- st.x86_regionCarrier). The carrier's use is the memory base ONLY (a second
	-- use of the register would observe the phantom value — rejected as always).
	local function regionCarrierAccess(st, d, e)
		if type(d) ~= "table" or not d.anchored or not d.scopeDepth then return false end
		-- Control-flow reads are not region traffic: `call *D(%carrier)` /
		-- `jmp *D(%carrier)` reads the carrier as a CODE address even when D
		-- lands inside the anchored scope — redirecting it would jump to the
		-- region pointer while the use-check exemption below silently consumed
		-- the phantom read (same class as the aliasOk call/jmp guard there).
		do
			local bb = x86.baseMnemonic(string.lower(st.mnemonic))
			if bb == "call" or bb == "jmp" then return false end
		end
		local mem
		for _, o in (st.operands or {}) do if o.kind == "mem" then mem = o end end
		if not (mem and mem.base and mem.base.kind == "reg" and mem.base.name == e.name
			and not mem.index and not mem.dispSym) then return false end
		local ro = d.scopeDepth - e.depth + (mem.dispVal or 0)
		return ro >= d.anchored.base and ro < d.anchored.base + d.anchored.size
	end
	-- Feature (D0 interior lea-save carriers): conservative reachability scan for
	-- the prologue-depth carrier relaxation (see the transfer's isLeaSave block).
	-- Assigned below, after `successors` (the fixpoint's own CFG) is defined —
	-- `transfer` above closes over the upvalue, and the fixpoint only calls it
	-- once that assignment has run. Results are cached per lea statement (the
	-- scan is deterministic within one analyzeFrame run; the cache table is a
	-- local of this run, so a later run with different regions recomputes).
	local leaSaveUsesMemoryOnly = nil
	local leaMemOnlyCache = {}
	local function transfer(st, d, sv, smap, fp, idx)
		local ops = st.operands
		-- A save form records its save wherever the depth is provable (a "dyn" or
		-- unreachable depth cannot name a parked value). Post-prologue saves are as
		-- sound as prologue ones — the value derives from the CURRENT rsp at a static
		-- depth, and the carrier discipline drops or rejects every use of it — which
		-- is exactly what the perlasm shortcut-dispatch preamble needs (the save sits
		-- behind the dispatch's merge label, after the prologue scan stopped). (The
		-- historical exclusion for a caller-saved save with a static frame-escape
		-- region based at rsp+0 is gone with the region sources.)
		-- Gate condition — callee-saved always; any register with yolo stack
		-- arguments; B2-clone statements always; caller-saved otherwise (the
		-- regions argument is always non-nil today — an empty list — so the
		-- recording gate holds on every run).
		local isSave = isSpSaveForm(st) and d ~= nil and not isDynDepth(d)
			and (x86.CALLEE_SAVED[ops[1].name] ~= nil or yoloCount > 0
				or st.b2clone ~= nil
				or (regions ~= nil and not regionOf(0, false)))
			and (idx < prologueEnd or not (st.allocaPlace or st.allocaOwned))
		-- A saved-rsp materialization via lea. The yolo form (`leaq 0(%rsp), %reg`,
		-- the aesni_ocb prologue shape) is an entry-rsp alias like the mov save. The
		-- general form — `leaq d(%rsp), %reg` at a provable depth d (the perlasm
		-- recovery-address computation, `leaq 280+48(%rsp), %rsi` at depth 328) —
		-- parks the ENTRY stack pointer (depth 0). The BELOW-rsp form —
		-- `leaq -K(%rsp), %reg` (K > 0), poly1305's frame anchor `leaq -88(%rsp),
		-- %r11` — parks entry_rsp - (d + K): a carrier at depth d + K whose only
		-- legal flows are the carrier discipline's (a later recovery revives the
		-- parked depth; an access through it inside an anchored region scope
		-- redirects to the region — regionCarrierAccess). A lea with 0 < K < d
		-- computes a frame-INTERIOR address: that is frame-address-taking (the
		-- rewrite rejects it unless a carrier/value-lea form legalizes it), not a
		-- saved stack pointer. (The historical exclusion when K landed in a static
		-- alloca region — the region redirect's REAL pointer arithmetic — is gone
		-- with the region sources.)
		local isLeaSave = isYoloLeaSave(st, d)
		local leaSaveForm = nil
		local leaSaveParked = nil
		if not isLeaSave and regions ~= nil
			and not (st.allocaPlace or st.allocaOwned)
			and d ~= nil and not isDynDepth(d)
			and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
			local mem = ops[2]
			-- Feature (lea-save carriers at provable depths): the destination may be
			-- %rbp too, when %rbp is NOT the frame pointer (usesRbp false — the same
			-- freedom carrierCopyOf grants the rsaz-avx2 `movq %rax,%rbp` shape); with
			-- a frame pointer established, a lea into %rbp would clobber the FP and
			-- stays unrecognized (the rewrite rejects it).
			if ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
				and (ops[1].name ~= "rbp" or not usesRbp)
				and mem and mem.kind == "mem" and mem.base and mem.base.rc == "sp"
				and not mem.index and not mem.dispSym then
				local k = mem.dispVal or 0
				-- A clone statement parks the ENTRY stack pointer with K = d + 8
				-- (the return-address word — the +8 rule); the static-region
				-- membership test keys its displacement the same way.
				local pk = st.localClone and (k - 8) or k
				local rkey = st.localClone and (k + D0 - d - 8) or k
				-- Feature (lea-save carriers at provable depths): a `leaq K(%rsp), %reg`
				-- at ANY provable depth parks a carrier whose value is
				-- entry_rsp - (d - K) — an ABSOLUTE entry-relative address exactly like
				-- the historical forms. An access `disp(%reg)` through it resolves to
				-- the normalized offset K + disp + D0 - d — the SAME slot an rsp-relative
				-- access `(disp + K)(%rsp)` at depth d keys — so the carrier spelling is
				-- pure sugar over the existing (fixpoint-proven) rsp-relative machinery:
				-- the alias window, bounds checks, region membership, and FP-cluster
				-- keying below are all shared. This is what real perlasm parks mid-body:
				-- chacha's `leaq 0x100(%rsp),%rcx` behind a dispatch merge label (the
				-- prologue scan stopped, so d=328 while D0=0) and poly1305's
				-- `leaq 0x90(%rsp),%rax` post-`and` frame anchor. Restricting the new
				-- form to NUMBER depths keeps the {anchored}/{dynreg} scopes (table d)
				-- on their own restore-base lea paths, and keeping it off localcall
				-- clone statements keeps their carrier spellings exactly as they
				-- were. B2 shared-tail clone statements ARE included: their carrier
				-- recognition is gated to the shapes the rewrite would otherwise
				-- REJECT as escapes (see the memory-only-use pass below), so the
				-- value-escaping clone leas keep today's path byte for byte.
				local leaAny = type(d) == "number" and (d ~= D0 or ops[1].name == "rbp")
					and not st.localClone
				-- Feature (D0 interior lea-save carriers): at the PROLOGUE depth
				-- (d == D0) the historical forms (K == d: the entry-rsp park;
				-- K <= 0: the below-rsp anchor) are joined by the frame-INTERIOR
				-- K when every reachable use of the destination register between
				-- this lea and its (re)definition is a single plain memory-base
				-- use — the exact shape the carrier discipline's aliasOk check
				-- legalizes (chacha's 4xop/8x bodies: `leaq 0x100(%rsp),%rcx`
				-- inside the prologue prefix, whose register the body only ever
				-- reads as an addressing base). Parking the carrier keeps the lea
				-- out of the rewrite's escape rejection (a frame-interior lea
				-- without a legalized shape fails the compile). The scan is
				-- CONSERVATIVE: any value use
				-- (an ALU operand, a push, an address-taking lea through the
				-- register, an indexed or symbolic memory form, a control-flow
				-- read, a second use in one statement, or anything classify
				-- cannot model exactly), any carrier use whose alias window or
				-- region membership the alias check would reject, and any
				-- control-flow ambiguity about whether the register still holds
				-- the carrier all leave the lea unmarked — the rewrite then
				-- rejects it as an escape, byte for byte today's behavior.
				-- B2-clone statements stay on their own banding pass, and the
				-- %rbp destination stays on the leaAny path (it is one when the
				-- frame pointer is not established).
				local leaD0 = nil
				-- B2 shared-tail clone statements are included: a clone entered at
				-- the jumper's entry depth re-runs its own prologue, so its
				-- frame-INTERIOR leas sit at dd == D0 exactly like the jumper's
				-- own (the OpenSSL sha1-mb shared 4x bodies). The carrier's parked
				-- value is entry-relative (absolute), and an access through it
				-- keys `disp + D0 - depth` — the SAME slot the clone's rsp-relative
				-- spelling keys — so the clone banding is a no-op here and the
				-- same conservative memory-only scan that legalizes the jumper's
				-- lea legalizes the clone's.
				if type(d) == "number" and d == D0 and not st.localClone
					and ops[1].name ~= "rbp"
					and k > 0 and k ~= d then
					leaD0 = leaSaveUsesMemoryOnly(st, idx, d - k)
				end
				-- Feature (by-address buffer value leas): a `leaq K(%rsp), %reg`
				-- whose computed normalized offset lands inside a DECLARED STACK
				-- BUFFER's range does not park a phantom carrier when the carrier
				-- scan would reject its uses — the destination web is an ordinary
				-- integer web holding the buffer byte address (the rewrite lowers
				-- the lea into the buffer region and the annotated accesses
				-- through it become runtime-checked by-address accesses). The
				-- memory-only-use shapes the carrier discipline legalizes keep
				-- today's static carrier path (leaSaveUsesMemoryOnly re-proves
				-- them here); only the value-use shapes (an ALU operand, a
				-- cmov source, a store to memory, ...) fall out to the ordinary
				-- def. The HISTORICAL forms (K == d: the entry-rsp park;
				-- K <= 0: the below-rsp anchor) never fall out — a buffer may
				-- overlap the below-rsp window, and their carrier + static
				-- classification already handles its accesses.
				-- B2 shared-tail clone statements are included (Feature A in
				-- clones, the aesni-mb dec8x shape): their probe tests the lea's
				-- RAW displacement against the clone's own raw declared ranges
				-- (bufB2DeclRaw above) — the clone's band is applied later, by
				-- the buffer scan's exact banded re-check in phase (a0), so a
				-- probe hit only decides carrier-vs-web here. A probe hit also
				-- suppresses the carrier value-use rejection for the toggle
				-- idiom (`xorq $0x80, %reg` — the aesni-mb dec8x base flip): the
				-- web is a real value, and computing on it is ordinary integer
				-- arithmetic.
				local inBufProbe = false
				if bufProbeRanges and type(d) == "number" and not isDynDepth(d) then
					if st.b2clone then
						local lst = bufB2DeclRaw and bufB2DeclRaw[st.b2clone] or nil
						if lst then
							for _, sba in lst do
								if k >= sba.lo and k < sba.hi then
									inBufProbe = true break
								end
							end
						end
					else
						local loff = k + D0 - d - (st.localClone and 8 or 0)
						for _, r in bufProbeRanges do
							if loff >= r.lo and loff < r.hi then inBufProbe = true break end
						end
					end
				end
				if (pk == d or pk <= 0 or leaAny or leaD0) and not regionOf(rkey, false) then
					if inBufProbe and not bufRepRegs[ops[1].num]
						and not leaSaveUsesMemoryOnly(st, idx, d - k) then
						-- a buffer-interior VALUE lea: an ordinary web definition.
						-- The web's value is COMPUTED ON (this is the fall-out
						-- reason: the lea's uses are not all memory-only — the
						-- aesni-mb dec8x `xorq $0x80, %rbp` base flip) — recorded
						-- for the buffer scan's group alignment identity (a
						-- computed base's arithmetic keys absolute address bits,
						-- which the lowered group must preserve).
						st.x86StackBufLeaValueComputed = true
					elseif inBufProbe and st.localClone then
						-- A buffer-interior lea inside a localcall clone: the
						-- carrier's parked value would key the clone's alias
						-- accesses through the return-address compensation
						-- twice (the transfer parks clone leas 8 deep AND the
						-- alias keying subtracts 8), so the carrier spelling
						-- cannot name the buffer bytes. The value-lea form is
						-- uniformly correct for clone buffer leas: the rewrite
						-- emits the lea into the lowered buffer region with the
						-- same keying the by-address checks use.
					else
						isLeaSave = true
						leaSaveForm = "leaq " .. k .. "(%rsp)"
					end
				end
			end
		end
		-- The perlasm epilogue's RESTORE-BASE lea (D7): after the body, the
		-- epilogue computes the register-restore base from the CURRENT rsp —
		-- `leaq 160+56(%rsp), %rsi` — but rsp was replaced by the alloca region,
		-- so the value is semantically meaningless under the region model. It
		-- provably only flows into the saved-register reloads (`movq -48(%rsi),
		-- %r15`) and the %rsp recovery (`leaq (%rsi), %rsp`), so treat the lea
		-- as a PHANTOM CARRIER definition (dropped), exactly like the lea-save
		-- above: in an {anchored} scope whose exact post-reservation depth is
		-- known (scopeDepth), `leaq K(%rsp), %reg` computes entry_rsp -
		-- (scopeDepth - K) — a carrier parked at depth scopeDepth - K. K inside
		-- the region is the region redirect's REAL pointer (regionLeaOf), never
		-- a carrier; K past it was previously the perturbed-rsp rejection, so no
		-- accepted shape changes. (A clone's +8 compensation: the scope's
		-- reservation ran in the CALLER, so the clone's rsp sits one
		-- return-address word lower — scopeDepth - 8 is the clone's own
		-- entry-depth spelling.)
		if not isLeaSave and regions ~= nil
			and not (st.allocaPlace or st.allocaOwned)
			and type(d) == "table" and d.anchored and d.scopeDepth
			and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
			local mem = ops[2]
			if ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr" and ops[1].name ~= "rbp"
				and mem and mem.kind == "mem" and mem.base and mem.base.rc == "sp"
				and not mem.index and not mem.dispSym then
				local k = mem.dispVal or 0
				local sd = d.scopeDepth + (st.localClone and 8 or 0)
				local parked = sd - k
				if parked >= 0 and k >= d.anchored.base + d.anchored.size then
					isLeaSave = true
					leaSaveForm = "leaq " .. k .. "(%rsp)"
					leaSaveParked = parked
				end
			end
		end
		-- The rbp spelling of the same restore base (bsaes's `leaq 120(%rbp),
		-- %rax`): with the frame pointer provably established, rbp = entry_rsp -
		-- fpDepth exactly — independent of any rsp perturbation — so `leaq
		-- K(%rbp), %reg` computes entry_rsp - (fpDepth - K), a carrier parked at
		-- depth fpDepth - K. A K landing in the fixed frame or a static region
		-- is frame-address-taking (the region redirect / the escape rejection),
		-- not a saved stack pointer.
		if not isLeaSave and regions ~= nil
			and not (st.allocaPlace or st.allocaOwned)
			and usesRbp and fp == "est"
			and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
			local mem = ops[2]
			if ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr" and ops[1].name ~= "rbp"
				and mem and mem.kind == "mem" and mem.base and mem.base.kind == "reg"
				and mem.base.name == "rbp" and not mem.index and not mem.dispSym then
				local k = mem.dispVal or 0
				local parked = fpDepth - k
				local rkey = rbpNorm + k
				if parked >= 0 and not regionOf(rkey, false)
					and (rkey < 0 or rkey >= frameSize) then
					isLeaSave = true
					leaSaveForm = "leaq " .. k .. "(%rbp)"
					leaSaveParked = parked
				end
			end
		end
		-- Feature (derived stack carriers): a `leaq K(%carrier), %reg` whose base
		-- register provably holds an UNREDEFINED saved-rsp carrier derives a NEW
		-- carrier parked at the base's depth minus K. The parked value is
		-- entry-relative and absolute exactly like the base's (the base holds
		-- entry_rsp - depth; adding K gives entry_rsp - (depth - K)), so the
		-- derived carrier rides the same discipline: dropped by the rewrite,
		-- every non-carrier use rejected, and accesses through it key slots via
		-- the same aliasSlotOf normalization (disp + D0 - depth) — with the
		-- clone-band shift applied by the existing aliasOff banding pass, since
		-- the derived carrier's saveIdx names the deriving lea (clone code when
		-- the base was parked there). This is the perlasm rolling-cursor shape
		-- (sha1-avx2's X[]+K[] window: `leaq 128(%rsp),%r13` then
		-- `leaq 256(%r13),%r13` per schedule phase). A K that would park above
		-- the entry rsp (depth - K < 0) stays unproven and falls through to the
		-- ordinary carrier-use rejection. Runs before the recovery/carrier-flow
		-- detection below: the derivation both READS the base carrier (legally)
		-- and defines the destination.
		local derivedDepth = nil
		if x86.baseMnemonic(string.lower(st.mnemonic)) == "lea"
			and ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
			and (ops[1].name ~= "rbp" or not usesRbp)
			and (ops[1].width == 64 or ops[1].width == nil)
			and ops[2] and ops[2].kind == "mem" and ops[2].base
			and ops[2].base.kind == "reg" and ops[2].base.rc == "gpr"
			and (ops[2].base.name ~= "rbp" or not usesRbp)
			and not ops[2].index and not ops[2].dispSym then
			local e = smap[ops[2].base.name]
			local k = ops[2].dispVal or 0
			if e and not e.redefined and not e.slot and not e.roff and not e.conflict
				and type(e.depth) == "number" and e.depth - k >= 0 then
				derivedDepth = e.depth - k
			end
		end
		if derivedDepth then
			local out = {}
			for k2, e2 in pairs(smap) do out[k2] = e2 end
			out[ops[1].name] = { name = ops[1].name, num = ops[1].num,
				depth = derivedDepth, saveIdx = idx,
				redefined = false }
			local nd0, nsv0, nfp0 = transferCore(st, d, sv, fp, nil)
			return nd0, nsv0, out, nfp0
		end
		local owned = nil  -- the historical allocaPlace/allocaOwned marks are never set
		-- (the `;! alloca` annotations are gone), so recovery/carrier detection always runs.
		-- The recovery forms: `movq %reg, %rsp` revives the parked depth; `leaq K(%reg),
		-- %rsp` (base register not rbp — that is the frame-pointer recovery above)
		-- revives the parked depth minus K. Either may read any live carrier. (No
		-- callee-saved requirement: the recovery is dropped, so no clobber is
		-- observable — the carrier discipline makes every other use a rejection.)
		local recoverDepth = nil
		if not owned and isSpRecoverForm(st, usesRbp) then
			local e = smap[ops[2].name]
			if e and not e.redefined and not e.slot and not e.roff and not e.conflict then recoverDepth = e.depth end
		end
		if not owned and recoverDepth == nil
			and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea"
			and ops[1] and ops[1].kind == "reg" and ops[1].rc == "sp"
			and (ops[1].width == 64 or ops[1].width == nil)
			and ops[2] and ops[2].kind == "mem" and ops[2].base and ops[2].base.kind == "reg"
			and (ops[2].base.name ~= "rbp" or not usesRbp)
			and not ops[2].index and not ops[2].dispSym then
			local e = smap[ops[2].base.name]
			if e and not e.redefined and not e.slot and not e.roff and not e.conflict then
				local rd = e.depth - (ops[2].dispVal or 0)
				if rd >= 0 then recoverDepth = rd end
			end
		end
		-- The remaining carrier flows (register copy, save-store, carrier-load), all
		-- dropped by the rewrite exactly like the save itself.
		local copySrc = (not owned) and carrierCopyOf(st, smap) or nil
		local storeSrc, storeKey, storeOff, storeSpace = nil, nil, nil, nil
		if not owned and not copySrc then storeSrc, storeKey, storeOff, storeSpace = saveStoreOf(st, smap, d) end
		local loadSrc = (not owned and not copySrc and not storeSrc) and carrierLoadOf(st, smap, d) or nil
		local restoreSave = (not owned) and restoreLoadOf(st, smap, sv) or nil
		-- Region-pointer values (the region-redirect lea and its copies): tracked so
		-- carrier save-stores/loads THROUGH them key the slot in region coordinates.
		local rOff, rBase, rSpace = nil, nil, nil
		if not owned and not copySrc and not storeSrc and not loadSrc then
			rOff, rBase, rSpace = regionLeaOf(st, d)
		end
		local roffCopySrc = (not owned and not rOff) and roffCopyOf(st, smap) or nil
		local nd, nsv, nfp = transferCore(st, d, sv, fp, recoverDepth)
		-- An rsp write (of any form — including a recovery or a push/pop) shifts the
		-- "dyn"-space coordinates: slot carriers keyed in it cannot survive.
		local function clearDynSlots(out)
			if not insnWritesSp(st) then return out end
			local cleaned = nil
			for k, e in pairs(out) do
				if e.slot and e.space == "D" then
					if not cleaned then cleaned = {} for k2, e2 in pairs(out) do cleaned[k2] = e2 end end
					cleaned[k] = nil
				end
			end
			return cleaned or out
		end
		if isSave or isLeaSave then
			-- A (re)definition of the save: the register freshly parks a known rsp
			-- depth. This must not run the def-marking below: the save re-registers
			-- itself. (A clone statement's parked depth carries the return-address
			-- word too — the +8 rule. A restore-base lea — the {anchored}-scope or
			-- rbp-derived form — carries its computed parked depth directly.)
			local out = {}
			for k, e in pairs(smap) do out[k] = e end
			local parkedDepth = d
			if leaSaveParked then
				parkedDepth = leaSaveParked
			elseif isLeaSave then
				parkedDepth = d + (st.localClone and 8 or 0) - (ops[2].dispVal or 0)
			end
			out[ops[1].name] = { name = ops[1].name, num = ops[1].num,
				depth = parkedDepth, saveIdx = idx,
				redefined = false, form = leaSaveForm
					or (isLeaSave and "leaq 0(%rsp)" or nil) }
			return nd, nsv, clearDynSlots(out), nfp
		end
		if recoverDepth then
			-- The recovery reads the save and writes %rsp; the save REGISTER is
			-- unchanged, so it still holds the save — a second recovery from it stays
			-- valid, and so does further traffic that does not read it.
			return nd, nsv, clearDynSlots(smap), nfp
		end
		if copySrc then
			local out = {}
			for k, e in pairs(smap) do out[k] = e end
			out[ops[1].name] = { name = ops[1].name, num = ops[1].num, depth = copySrc.depth,
				-- saveIdx points at THIS copy statement: the `#! stack buffer`
				-- machinery may materialize the carrier's defining instruction
				-- (a plain lea into the buffer region), and for a copy the
				-- defining statement is the copy itself, not the original save.
				saveIdx = idx,
				redefined = false, form = copySrc.form }
			return nd, nsv, clearDynSlots(out), nfp
		end
		if storeSrc then
			local out = {}
			for k, e in pairs(smap) do out[k] = e end
			out[storeKey] = { slot = true, name = "the frame slot", depth = storeSrc.depth,
				saveIdx = storeSrc.saveIdx,
				redefined = false, off = storeOff, space = storeSpace }
			return nd, nsv, clearDynSlots(out), nfp
		end
		if loadSrc then
			local out = {}
			for k, e in pairs(smap) do out[k] = e end
			out[ops[1].name] = { name = ops[1].name, num = ops[1].num, depth = loadSrc.depth,
				saveIdx = loadSrc.saveIdx,
				redefined = false }
			return nd, nsv, clearDynSlots(out), nfp
		end
		if rOff then
			-- The region-redirect lea defines a region-pointer value (a REAL pointer —
			-- reads are legal, unlike carriers): record its offset for carrier-slot
			-- keying through the register.
			local out = {}
			for k, e in pairs(smap) do out[k] = e end
			out[ops[1].name] = { name = ops[1].name, num = ops[1].num,
				roff = rOff, rbase = rBase, rspace = rSpace, redefined = false }
			return nd, nsv, clearDynSlots(out), nfp
		end
		if roffCopySrc then
			local out = {}
			for k, e in pairs(smap) do out[k] = e end
			out[ops[1].name] = { name = ops[1].name, num = ops[1].num,
				roff = roffCopySrc.roff, rbase = roffCopySrc.rbase,
				rspace = roffCopySrc.rspace, redefined = false }
			return nd, nsv, clearDynSlots(out), nfp
		end
		-- Any other instruction. A modeled def of a save register (an ordinary move, a
		-- pop INTO it, an implicit clobber like cpuid's ebx) breaks the link between the
		-- register and its saved value — mirroring the pushed-save redefinition logic
		-- above. And while a save is OUTSTANDING (unredefined) the register's web IS the
		-- dropped save: the rewrite removed the `movq %rsp, %reg` that defined it, so
		-- the web holds no value the output computes — any read of it (as an operand,
		-- through a memory base, or by pushing the register) would observe garbage.
		-- Reject it. (Never mutate the incoming map — it is shared across the fixpoint.)
		local c = classOf(st)
		local out = smap
		-- A real hardware call clobbers every caller-saved register (SysV AMD64):
		-- a live register carrier parked in one no longer holds the save when
		-- the call returns, so poison it here — any later recovery or carrier
		-- use through it fails closed at the use site (and a merge with a
		-- call-free path marks the register conflicted, which is equally
		-- fail-closed). Every hardware-call spelling is covered: baseMnemonic
		-- normalizes `call`/`callq`/`callw` to one "call" (x86_64_isa FIXED —
		-- `callw` is the encodable 16-bit form; `calll` has no 64-bit encoding
		-- and stays rejected). An indirect `call *%reg` THROUGH a carrier is
		-- first a use of the phantom value: the use-check loop below iterates
		-- the INCOMING map, so it stays a static error at the call itself, not
		-- a silent poison (see sarcasm-reject-b2-carrier-indcall-att); a
		-- `call *mem` through a carrier register or slot is rejected the same
		-- way (the aliasOk/region/slot guards above exclude call/jmp — see
		-- sarcasm-reject-b2-carrier-memcall-att and
		-- sarcasm-reject-b2-carrier-memslot-att). Callee-saved carriers
		-- survive, by a two-case trust argument (not by assumption): a
		-- same-file callee is ALSO compiled by sarcasm, whose emitted prologue
		-- saves and epilogue restores every callee-saved register it touches
		-- (verifiable in the callee's own output — see
		-- sarcasm-tail-b2-carrier-callee-att, which executes across one); an
		-- extern (or register-indirect, capability-checked) callee is outside
		-- sarcasm's proof, and its preservation is the SysV ABI contract —
		-- the same trust boundary every cross-module call in Fil-C already
		-- rides on. Slot carriers live in frame memory, which the call cannot
		-- touch, and are left alone. Local calls are branches to inlined
		-- clones, not hardware calls — they clobber nothing by themselves
		-- (the clone's own redefinitions poison through the ordinary
		-- def-marking below, so a caller-saved carrier live across a localcall
		-- whose clone clobbers the register is still rejected — at the
		-- clobber when it reads, at the recovery/ret when it only writes; see
		-- sarcasm-reject-b2-carrier-local-att), so they are excluded. In particular this proves the
		-- B2-clone carrier relaxation (a clone may park in any register)
		-- live-across-no-calls: a caller-saved clone carrier that reaches a
		-- nested B1/local/annotated call is poisoned on every path through
		-- the call.
		if x86.baseMnemonic(string.lower(st.mnemonic)) == "call" and st.localCall == nil then
			for pname, pe in pairs(smap) do
				if not pe.redefined and not pe.slot and not pe.roff and not pe.conflict
					and x86.CALLEE_SAVED[pe.name] == nil then
					if out == smap then
						out = {}
						for k, v in pairs(smap) do out[k] = v end
					end
					local ne = {}
					for k, v in pairs(pe) do ne[k] = v end
					ne.redefined = true
					out[pname] = ne
				end
			end
		end
		for name, e in pairs(smap) do
			if not e.redefined and not e.slot and not e.roff and not e.conflict then
				-- Count this instruction's modeled uses of the save register. Allowed
				-- survivors: the memory base of an entry-time read of an incoming yolo
				-- stack argument (the rewrite redirects it to the argument's slot,
				-- removing the register use entirely); an epilogue restore load through
				-- the carrier (its base is the only use, and the load is dropped);
				-- the self-xor/self-sub zeroing idiom (`xorl %eax, %eax` — the value
				-- written does not depend on the register's old value, so the modeled
				-- use observes nothing; the def below kills the alias, exactly as a
				-- plain redefinition). Anything else observes the dropped save.
				-- ORDERING (reject-vs-poison): this loop iterates the INCOMING
				-- map (`smap`, never the poison-mutated `out`), so a use of the
				-- phantom value is rejected HERE even when the same instruction
				-- also poisons the register below (a `call *%carrier-reg` is a
				-- static error at the call itself, never a silent poison — see
				-- sarcasm-reject-b2-carrier-indcall-att). A `leaq` escape through
				-- a carrier (`leaq 0(%carrier), %reg` — aliasSlotOf refuses leas)
				-- is likewise rejected here rather than recorded: the
				-- isLeaSave early-return above requires an %rsp base, so a
				-- carrier base can never be swallowed into a save (see
				-- sarcasm-reject-b2-carrier-lea-att).
				local bb = x86.baseMnemonic(string.lower(st.mnemonic))
				local zeroing = false
				do
					if (bb == "xor" or bb == "sub") and ops[1] and ops[2]
						and ops[1].kind == "reg" and ops[2].kind == "reg"
						and ops[1].num == e.num and ops[2].num == e.num then
						zeroing = true
					end
				end
				local nUses = 0
				for _, u in c.uses do
					if u.kind == "reg" and u.rc == "gpr" and u.num == e.num then nUses += 1 end
				end
				-- A stack access through a stack+offset alias carrier (mov %rsp,
				-- %reg / lea K(%rsp), %reg, then disp(%reg)): the register's
				-- value is absolute (entry_rsp - depth on every path), so the
				-- access keys a static frame slot exactly like an rsp-relative
				-- access (the rewrite virtualizes/materializes it). The use is
				-- legal only for a single memory-base use resolving inside the
				-- frame (there are no promoted regions anymore); anything else
				-- observes the phantom value. Control-flow reads are NOT data accesses:
				-- `call *D(%carrier)` / `jmp *D(%carrier)` reads the carrier as
				-- a CODE address, so even when D lands inside the frame the
				-- alias computation must not legalize it (that would jump to
				-- the phantom value — a callee-saved carrier would not even be
				-- poisoned afterwards, so nothing downstream would catch it;
				-- see sarcasm-reject-b2-carrier-memcall-att). The transform's
				-- annotation walk rejects every memory-operand indirect call
				-- anyway — this guard keeps the frame pass's own proof
				-- (reject at the use site) instead of relying on it.
				local aliasOk = false
				if nUses == 1 and not owned and bb ~= "call" and bb ~= "jmp" then
					local amem = nil
					for _, o in (st.operands or {}) do if o.kind == "mem" then amem = o end end
					local ao = amem and aliasSlotOf(st, amem, e)
					if ao and not yoloArgSlotOf(st, name, e.depth) then
						local afpe = fpKnowledge.lookup(string.lower(st.mnemonic))
						local aw = nil
						if isFpInsn(afpe, st) then
							aw = fpKnowledge.accessWidth(string.lower(st.mnemonic), ops, st.raw)
						else
							aw = gprStackWidth(string.lower(st.mnemonic), amem)
						end
						-- The alias window's LOWER bound follows the access point's own
						-- provable depth, exactly like a direct rsp-relative access at
						-- that depth (whose valid window extends down to the current rsp
						-- and its red zone when the depth is perturbed: the rewrite's
						-- `lowB = min(-128, D0 - d - 128)`). The carrier spelling names
						-- the same addresses as the rsp-relative spelling (the keys are
						-- address-canonical), so it must get the same window — this is
						-- what lets poly1305's `leaq -88(%rsp),%r11` anchor reach its
						-- frame at normalized offsets well below -128. A "dyn"/scope
						-- depth keeps the historical conservative [-128, frameSize)
						-- window. (The upper bound stays frameSize for both spellings.)
						local lowA = -128
						if type(d) == "number" then lowA = math.min(-128, D0 - d - 128) end
						-- The UPPER bound is deferred for B2 shared-tail clone statements:
						-- their (unbanded, absolute) carrier offsets feed the frame-size
						-- extension below, which runs after this fixpoint — checking them
						-- against the pre-extension extent would reject shapes the final
						-- frame provably covers. The rewrite's checkBounds (running with
						-- the FINAL extent) remains the binding gate.
						if aw and ao >= lowA
							and (st.b2clone or ao + aw <= frameSize)
							and not regionOf(ao, false) then
							aliasOk = true
						end
					end
				end
				-- An ANNOTATED REP (`rep movs*`/`rep stos*` carrying
				-- `#! stack buffer (id, ...)`): the implicit %rsi/%rdi use of a
				-- live register carrier is the STACK-ALIAS side of the copy —
				-- allowed (and recorded on the statement for the transform's rep
				-- lowering) when the buffer's declared range covers the alias's
				-- stack offset statically (the dynamic byte-count check is emitted
				-- at the rep itself). The carrier's value is phantom, so the pin
				-- moves around the rep materialize the LOWERED buffer address
				-- instead — the transform owns that; here we only prove the side
				-- is covered and record it.
				-- An ANNOTATED INDEXED access through the carrier (`#! stack buffer
				-- (...)` on an indexed stack-relative access whose base is this
				-- carrier): the frame pass's buffer scan resolves the annotation
				-- and the rewrite lowers the access into the buffer with a runtime
				-- bounds check — the base use vanishes like every other legal
				-- carrier use. (The buffer's base register may also name this
				-- carrier directly.)
				local idxBufOk = false
				if nUses == 1 and st.stackBufAnno and not owned
					and bb ~= "call" and bb ~= "jmp" then
					local amem = nil
					for _, o in (st.operands or {}) do if o.kind == "mem" then amem = o end end
					if amem and amem.index and amem.base and amem.base.kind == "reg"
						and amem.base.name == name then
						idxBufOk = true
					end
				end
				local repSide = false
				if nUses == 1 and st.stackBufAnno and c.rep and st.localClone then
					-- A `rep` string op carrying a stack-buffer annotation inside a
					-- localcall clone: the clone's +8 keying would need its own
					-- alias-side model (the carrier's parked depth and the
					-- rep's dynamic count check both shift), which is not built.
					error("sarcasm: `stack buffer` annotation on a rep string op inside"
						.. " a local subroutine clone is not supported (the clone's"
						.. " return-address compensation would need its own rep alias"
						.. " model): " .. (st.raw or st.mnemonic))
				end
				if nUses == 1 and st.stackBufAnno and c.rep and not owned then
					local sideNum = nil
					local isWrite = nil
					if c.rep.kind == "movs" then
						if e.num == 6 then sideNum, isWrite = 6, false
						elseif e.num == 7 then sideNum, isWrite = 7, true end
					else
						if e.num == 7 then sideNum, isWrite = 7, true end
					end
					if sideNum then
						local loN, hiN = resolveBufRangeAt(st, d, smap, fp, idx < prologueEnd)
						if loN == nil then
							-- A short form during the driver's resolution-only run
							-- (the canonical table is not complete yet): allow
							-- provisionally — the real per-function run re-resolves
							-- (and errors on a long-form-less id).
							repSide = sbResolving
						else
							local k = D0 - e.depth
							if k < loN then
								error("sarcasm: rep string's stack-alias side %" .. name
									.. " (stack offset " .. tostring(k) .. ") is below the"
									.. " stack buffer (" .. st.stackBufAnno.id .. ") range ["
									.. tostring(loN) .. ", " .. tostring(hiN) .. "): "
									.. (st.raw or st.mnemonic))
							end
							-- Rebuild the mark each iteration (idempotent per
							-- fixpoint round; the per-register side keys accumulate
							-- across this loop's iterations for both %rsi and %rdi).
							st.x86StackBufRep = { lo = loN, hi = hiN,
								id = st.stackBufAnno.id }
							st.x86StackBufRep[name] = { K = k, num = sideNum,
								isWrite = isWrite }
							repSide = true
						end
					end
				end
				if nUses > 0 and not zeroing and not repSide and not idxBufOk
					and not (nUses == 1 and yoloArgSlotOf(st, name, e.depth))
					and not (restoreSave and nUses == 1)
					-- A memory access through the carrier that lands inside the
					-- current anchored region scope (the poly1305 frame-anchor
					-- shape): redirected to the region pointer — the base use
					-- vanishes. Any OTHER use observes the phantom value.
					and not (nUses == 1 and regionCarrierAccess(st, d, e))
					and not aliasOk then
					-- Resolution-only runs (the driver pre-pass) suppress the
					-- rejection for LEA-form carriers: a buffer-interior value
					-- lea only legalizes once the canonical table exists (see
					-- bufProbeRanges above), and the pre-pass's analysis output
					-- is re-derived by every later run — only the declaration
					-- resolution reads it. The use falls through to the ordinary
					-- def-marking (a redefining use kills the carrier, as today).
					if not (bufProbeSuppress and e.form ~= nil) then
						error("sarcasm: reading %" .. name .. " while it holds the prologue"
						.. " `" .. (e.form or "movq %rsp") .. ", %" .. name .. "` save cannot be proven safe (the"
						.. " save is dropped by the rewrite — the register's stack value"
						.. " does not exist in the output; recover %rsp from it first or"
						.. " redefine it): " .. (st.raw or st.mnemonic))
					end
				end
			end
			if not e.redefined and not e.slot then
				for _, dd in c.defs do
					if dd.kind == "reg" and dd.rc == "gpr" and dd.num == e.num then
						if out == smap then
							out = {}
							for k, v in pairs(smap) do out[k] = v end
						end
						out[name] = { name = e.name, num = e.num, depth = e.depth,
							saveIdx = e.saveIdx,
							redefined = true }
						break
					end
				end
			end
		end
		-- Slot-carrier traffic: a stack WRITE overlapping a live slot carrier poisons
		-- the slot (the slot then holds an ordinary value; a later carrier-load or
		-- recovery from it is rejected); a stack LOAD from a live slot carrier that is
		-- not the carrier-load form would observe the phantom value — reject it.
		-- (Region-pointer-derived bases key the slot in the region's coordinates —
		-- the same space the direct rsp-relative spelling uses.)
		-- A `call *off(%rsp)` / `jmp *off(%rsp)` memory operand READS the slot
		-- (the indirect target) even though it sits in operand position 1: when
		-- the slot holds a carrier (%rsp-slot carrier — the value parked by
		-- `movq %carrier, off(%rsp)`), the read observes the phantom value and
		-- must be rejected, never misclassified as a write that silently drops
		-- the slot (see sarcasm-reject-b2-carrier-memslot-att). ORDERING: this
		-- rejection runs on the same incoming map the call-poison above spared
		-- (slot carriers are never poisoned — frame memory survives calls, and
		-- sarcasm-tail-b2-carrier-slot-att proves a slot carrier stays usable
		-- across one), so surviving the poison never legalizes the read.
		local slotBb = x86.baseMnemonic(string.lower(st.mnemonic))
		for _, o in ops do
			if o.kind == "mem" then
				local key, off, space = memSlotKey(o, d, smap, st)
				if key then
					local w = nil
					-- (fpKnowledge, not the `fp` frame-pointer state parameter.)
					local fpe = fpKnowledge.lookup(string.lower(st.mnemonic))
					if isFpInsn(fpe, st) then w = fpKnowledge.accessWidth(string.lower(st.mnemonic), ops, st.raw)
					else w = gprStackWidth(string.lower(st.mnemonic), o) end
					for k, e in pairs(smap) do
						if e.slot and not e.redefined and e.space == space and w
							and off < e.off + 8 and off + w > e.off then
							local isWrite = (ops[1] == o) and slotBb ~= "call" and slotBb ~= "jmp"
							if isWrite then
								-- a write (the mem operand is the destination): poison
								if out == smap then
									out = {}
									for k2, v in pairs(smap) do out[k2] = v end
								end
								out[k] = nil
							else
								error("sarcasm: stack access into a frame slot that holds the"
									.. " saved stack pointer cannot be proven safe (the save is"
									.. " phantom — the slot's value does not exist in the output;"
									.. " overwrite the slot first): " .. (st.raw or st.mnemonic))
							end
						end
					end
				end
			end
		end
		return nd, nsv, clearDynSlots(out), nfp
	end

	local function mergeSaves(a, b)
		if a == nil then return b end
		if a == "dyn" or b == "dyn" then return "dyn" end
		if #a ~= #b then return "dyn" end
		local grew = false
		for k = 1, #a do
			if a[k].name ~= b[k].name or a[k].depth ~= b[k].depth
				or (a[k].spill or false) ~= (b[k].spill or false) then return "dyn" end
			if b[k].redefined and not a[k].redefined then grew = true end
		end
		if not grew then return a end
		-- A redefinition on ONE incoming path poisons the save for both (the slot
		-- holds the pre-redefinition value regardless of which path was taken).
		local c = {}
		for k = 1, #a do
			c[k] = { name = a[k].name, num = a[k].num, depth = a[k].depth,
				spill = a[k].spill, redefined = a[k].redefined or b[k].redefined }
		end
		return c
	end
	local function mergeSmaps(a, b)
		if a == nil then return b end
		if b == nil then return a end
		if a == b then return a end
		-- Per-register agreement: a register's save must have the same depth and the
		-- same redefined-ness on every incoming path. Any disagreement — including one
		-- path carrying no save at all (it cannot have executed the save, or a path
		-- through a redefinition) — poisons that register: its recovery becomes
		-- unprovable and is rejected at the recovery site.
		-- A disagreement where EITHER side holds a live saved-rsp value (stack +
		-- offset) is a control-flow AMBIGUITY (one path stack+offset, another a
		-- heap/alloca/argument pointer or nothing): the merged state keeps an
		-- explicit conflict mark instead of dropping the entry. Ordinary
		-- (runtime-checked) uses of a conflicted register stay legal, but any
		-- stack-form access or %rsp recovery through it is a static error, and
		-- returning/storing the live value was already an error before the merge.
		local function isLiveCarrier(x)
			return x and not x.redefined and not x.slot and not x.roff and not x.conflict
		end
		local out = a
		for name, e in pairs(a) do
			local f = b[name]
			if not f or f.depth ~= e.depth or f.redefined ~= e.redefined
				or f.roff ~= e.roff or f.rbase ~= e.rbase or f.rspace ~= e.rspace
				or f.conflict ~= e.conflict then
				if out == a then
					out = {}
					for k, v in pairs(a) do out[k] = v end
				end
				if isLiveCarrier(e) or isLiveCarrier(f)
					or (e and e.conflict) or (f and f.conflict) then
					out[name] = { conflict = true, name = name,
						num = (e and e.num) or (f and f.num) }
				else
					out[name] = nil
				end
			else
				-- Agreement on depth: merge the save position to the earliest
				-- (min). A carrier saved before a mid-function `and` that
				-- reaches a use after it through a merge (saves on mixed
				-- sides) must still count as crossing, so the earliest save
				-- decides. Unknown (nil) counts as 0 (before everything) to
				-- fail closed. Slot carriers (save-stores) merge the same way.
				local es, fs = e.saveIdx, f.saveIdx
				if es ~= fs then
					local m = math.min(es or 0, fs or 0)
					if (es or 0) ~= m then
						if out == a then
							out = {}
							for k, v in pairs(a) do out[k] = v end
						end
						local ne = {}
						for k, v in pairs(e) do ne[k] = v end
						ne.saveIdx = m
						out[name] = ne
					end
				end
			end
		end
		-- (A register only b holds is already absent from a — poisoned — so nothing to
		-- do for it; returning a itself when nothing was removed keeps the fixpoint's
		-- identity-based change detection stable.)
		return out
	end
	-- CFG successors over the POST-tailcall-rewrite body. B2 audit: every
	-- shape tailcall.rewriteFunction emits is covered with no special cases —
	-- a retargeted jumper->clone jump is a direct `jmp` to a renamed label
	-- (the labelAt edge; the clone entry's crossJumpEntry mark is not a CFG
	-- fact), a conditional B2 join keeps its jcc fallthrough, nested-B1 via
	-- blocks are direct-jump chains (jcc->via, via call, via `jmp tailret`),
	-- the tailRet epilogue is a label plus a `ret` (no successors, like every
	-- ret), clone rets are plain `ret`s (no successors: they return from the
	-- jumper, never to the jump site), and nested-clone jumps are direct
	-- `jmp`s. Completeness here is load-bearing for x86_preAnd reachability:
	-- a missing edge (in either direction — the preAnd walk follows
	-- predecessors derived from these successors) would leave clone code
	-- depth-less (unreached) and skip its checks, reopening alias-across-and
	-- (see sarcasm-reject-b2-cross-and-att, which pins the jumper-header
	-- define / in-clone use-after-and rejection end to end). Fallthrough
	-- completeness matters in the other direction too: every non-branch
	-- insn (including `leave`, which execution continues past) falls through
	-- via nextInsn, while trapping insns (`ud2`/`int3`/`int`/`hlt`) terminate
	-- — see the trap guard below and sarcasm-tail-b2-gap-att (label/nop gaps
	-- must not break fallthrough) and sarcasm-tail-b2-ownand-att (a clone's
	-- own `and` must not over-poison the jumper).
	local function successors(i)
		local st = body[i]
		-- Local-subroutine marks (localcall.luau): a local call transfers to the
		-- per-caller clone's entry (no fall-through — the continuation is reached
		-- only through the clone's ret dispatch); a local ret branches to the
		-- clone's continuations. The clone executes at the CALLER's depth (the
		-- no-push clone model moves no stack pointer), so the state flows through
		-- both edges unchanged.
		if st.localCall then
			local t = labelAt[st.localCall.entry]
			if t then return { t } end
			return {}
		end
		if st.localRet then
			local out = {}
			for _, cont in st.localRet.conts do
				local t = labelAt[cont]
				if t then table.insert(out, t) end
			end
			return out
		end
		local m = string.lower(st.mnemonic)
		local b = x86.baseMnemonic(m)
		local nxt = nextInsn[i]
		local out = {}
		if b == "ret" then return out end
		-- Trapping instructions never fall through: `ud2` raises #UD, `int3` and
		-- `int` (only `int $3` passes fp.unsafeReject — SIGTRAP) trap, and `hlt`
		-- is privileged (rejected by fp.unsafeReject, listed so any future
		-- relaxation still gets the CFG right). A fallthrough edge here would be
		-- a spurious CFG edge: for x86_preAnd reachability it over-marks
		-- (fail-closed but imprecise — a trap before an `and` would poison
		-- innocent header traffic), and for flagsDeadAfter/checkLostReload it
		-- invents paths that can never execute. (`leave`, by contrast, IS
		-- fallthrough — an rsp/rbp update whose execution continues on the next
		-- instruction — so it keeps the fallthrough below deliberately.)
		-- NOTE: x86.classify mirrors this trap set (fallsThrough=false for the
		-- same mnemonics), so validateBody's reachability, the lift's
		-- reaching-definitions, regalloc and DCE all model these as terminal too;
		-- this function remains the frame pass's own precise CFG.
		if m == "ud2" or m == "int3" or m == "int" or m == "hlt" then return out end
		if b == "jmp" then
			local o = st.operands[1]
			if o and o.kind == "sym" and labelAt[o.text] then return { labelAt[o.text] } end
			return out  -- indirect jump: leaves the function (tail call)
		end
		if b == "jcc" or string.match(m, "^loop") then
			local o = st.operands[1]
			if o and o.kind == "sym" and labelAt[o.text] then table.insert(out, labelAt[o.text]) end
		end
		if nxt <= #body then table.insert(out, nxt) end
		return out
	end

	-- Feature (D0 interior lea-save carriers): the conservative scan behind the
	-- transfer's `leaD0` relaxation (see the long comment there). Question: may
	-- the `leaq K(%rsp), %reg` at statement `idx` (executing at the prologue
	-- depth, K inside the frame) park a carrier — i.e. is every use of `reg`
	-- on every path from this lea until the register's next definition a use
	-- the carrier discipline legalizes? It walks the SAME CFG the depth
	-- fixpoint walks (successors above), forward from the function entry, with
	-- a four-state abstract value for the register (merged on arrival):
	--   0 — the register holds no parked value (before the lea, or after a
	--       definition of it on every path reaching this point);
	--   1 — it provably holds THIS lea's carrier (parked depth parkedDepth);
	--   2 — it holds a carrier parked by a DIFFERENT lea (a foreign park): its
	--       uses are that lea's own scan's business, but a merge with state 1
	--       is a smap depth disagreement (CONFLICT);
	--   3 — CONFLICT: paths disagree about whether any parked value is live
	--       (the smap merge records an explicit conflict entry, and the
	--       rewrite statically rejects every memory access through a
	--       conflicted base register) — so a memory-base use here falls back.
	-- The walk also carries its own rsp-depth tracking (`dstate`, a number or
	-- "dyn", mirroring the depth fixpoint's transitions with conservative
	-- "dyn" fallbacks) — used ONLY to compute a foreign park's parked depth,
	-- so two leas parking the same value (chacha 8x's re-park: same K at the
	-- same depth) merge as state 1 instead of falsely conflicting.
	-- At state 1 the only legal use shape is the aliasOk one: exactly one
	-- modeled use of the register, as the plain (no index, no symbolic
	-- displacement) base of the statement's single memory operand, on an
	-- instruction that neither computes an address through the carrier (`lea`)
	-- nor reads it for control flow (`call *`/`jmp *`), with the self-xor/
	-- self-sub zeroing idiom exempt (the written value does not depend on the
	-- old one) and an incoming yolo stack-argument read redirected. The access
	-- must also resolve inside the alias window (normalized offset
	-- disp + D0 - parkedDepth in [-128, frameSize)) — the same proof the
	-- fixpoint's aliasOk re-runs at the use, with
	-- the lea's own depth as the window's depth (every later use point's window
	-- is at least as wide, so this is the tightest bound). A definition of the
	-- register (any width — the same modeled-def rule the smap's def-marking
	-- uses) drops the carrier on outgoing edges. A plain `ret` reached while
	-- the register holds the carrier (state 1, or state 3 for %rax in a
	-- function returning an integer class) is the one non-memory read classify
	-- does not model — the rewrite rejects a %rax carrier live at a returning
	-- ret — so it falls back too. A hardware call reads the SysV integer
	-- argument registers (classify models neither that nor the caller-saved
	-- clobber, so the scan owns both). Anything the scan cannot prove (a
	-- statement classify rejects, a shape it does not know) returns false,
	-- which keeps today's escape-rejection path byte for byte.
		leaSaveUsesMemoryOnly = function(st, idx, parkedDepth)
		local cached = leaMemOnlyCache[st]
		if cached ~= nil then return cached end
		local res = false
		local failWhy = nil
		while true do
		local dest = st.operands and st.operands[1]
		if not (dest and dest.kind == "reg" and dest.rc == "gpr") then break end
		local parsed = x86parse.classifyReg(dest.name)
		if not (parsed and parsed.rc == "gpr") then break end
		local regNum = parsed.num
		local regName = dest.name
		-- The rewrite's returning-ret guard keys on the integer-class return
		-- (the same expression the annotation pass uses below).
		local retIsGpr = sigDesc and not sigDesc.isVoid
			and not (sigDesc.retTypes[1] and sig.isFpClass(sigDesc.retTypes[1]))
		local firstInsn
		for i, s0 in body do
			if s0.kind == "insn" then firstInsn = i break end
		end
		if firstInsn == nil then break end
		-- Abstract state per statement index, merged on arrival:
		--   cstate 0 — the register does not hold any parked value here;
		--   cstate 1 — it holds THIS lea's carrier (parked depth parkedDepth);
		--   cstate 2 — it holds a carrier parked by a DIFFERENT lea whose parked
		--              depth provably differs from (or is unknown against) this
		--              one — uses are that lea's own business, but a merge with
		--              state 1 is a smap CONFLICT;
		--   cstate 3 — CONFLICT/ambiguous: paths disagree (the smap merge would
		--              record an explicit conflict entry — the rewrite statically
		--              rejects every memory access through a conflicted base).
		-- `dstate` is this walk's own rsp-depth tracking (number, or "dyn"), used
		-- ONLY to compute another same-destination lea's parked depth — it
		-- mirrors the depth fixpoint's transitions with conservative "dyn"
		-- fallbacks, so a provable depth here is the depth the fixpoint proves.
		local seen = {}
		local instateC, instateD = {}, {}
		local work = { { firstInsn, 0, 0 } }
		instateC[firstInsn] = 0
		instateD[firstInsn] = 0
		res = true
		while #work > 0 do
			local item = table.remove(work)
			local i, s, dv = item[1], item[2], item[3]
			local key = i .. ":" .. s .. ":" .. tostring(dv)
			if seen[key] then continue end
			seen[key] = true
			local st2 = body[i]
			-- (successors only ever yields insn indices, but stay defensive.)
			if not (st2 and st2.kind == "insn") then res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break end
			local okC, c = pcall(classOf, st2)
			if not (okC and c) then res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break end
			local bm = string.lower(st2.mnemonic)
			local bb = x86.baseMnemonic(bm)
			local ops2 = st2.operands or {}
			-- Outgoing carrier state.
			local out = s
			local isMyLea = (i == idx)
			if not isMyLea and bb == "lea" and ops2[1] and ops2[1].kind == "reg"
				and ops2[1].rc == "gpr" and ops2[1].num == regNum
				and ops2[2] and ops2[2].kind == "mem" and ops2[2].base
				and ops2[2].base.rc == "sp" and not ops2[2].index
				and not ops2[2].dispSym then
				-- Another lea parks the same register: the parked value is
				-- rsp + K at THIS statement's depth.

				if type(dv) == "number" then
					local pdf = dv - (ops2[2].dispVal or 0)
					out = (pdf == parkedDepth) and 1 or 2
				else
					out = 2
				end
			elseif isMyLea then
				out = 1
			else
				for _, dd in c.defs do
					if dd.kind == "reg" and dd.rc == "gpr" and dd.num == regNum then
						out = 0 break
					end
				end
			end
			-- A hardware call reads the SysV integer ARGUMENT registers (the
			-- callee receives them) and clobbers every caller-saved register.
			-- classify models neither, so the scan owns both: an argument
			-- register holding the carrier at a call is a VALUE use (fall back
			-- — the promotion path's region pointer is a real, defined value
			-- there), and any caller-saved carrier dies at the call exactly
			-- like the smap's call poison. A LOCAL call is a branch to an
			-- inlined clone: it reads the arguments too (the clone consumes
			-- them) but clobbers nothing by itself.
			if bb == "call" then
				if (s == 1 or s == 3) and x86.ARG_REGS[regNum] then
					res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break
				end
				-- Only a LIVE (unconflicted) caller-saved carrier dies at the
				-- call — the smap's poison loop skips conflicted entries, so a
				-- conflict survives it and every later memory-base use through
				-- the register still rejects (the state must stay 3).
				if st2.localCall == nil and (out == 1 or out == 2)
					and not x86.CALLEE_SAVED[regName] then
					out = 0
				end
			end
			-- Outgoing rsp depth (mirrors the depth fixpoint's transitions).
			local ndv = dv
			if type(dv) == "number" then
				if bb == "push" then ndv = dv + 8
				elseif bb == "pop" then ndv = dv - 8
				elseif x86.isFlagStackPush(string.lower(st2.mnemonic)) then ndv = dv + 8
				elseif x86.isFlagStackPop(string.lower(st2.mnemonic)) then ndv = dv - 8
				elseif (bb == "sub" or bb == "add") and ops2[1] and ops2[1].rc == "sp"
					and ops2[2] and ops2[2].kind == "imm" then
					ndv = (bb == "sub") and (dv + immVal(ops2[2])) or (dv - immVal(ops2[2]))
				elseif bb == "lea" and ops2[1] and ops2[1].rc == "sp" and ops2[2]
					and ops2[2].kind == "mem" and ops2[2].base and ops2[2].base.rc == "sp"
					and not ops2[2].index and not ops2[2].dispSym then
					ndv = dv - (ops2[2].dispVal or 0)
				elseif bb == "and" and ops2[1] and ops2[1].rc == "sp" then
					-- `and $-N, %rsp`: the depth fixpoint propagates the depth
					-- unchanged (the alignment machinery owns the and's slack).
				elseif bb == "leave" or insnWritesSp(st2) then
					ndv = "dyn"  -- mov reg,rsp / leave / rbp recoveries: unprovable here
				end
			end
			-- Use checks at the IN state. State 2 (holding a foreign carrier)
			-- checks nothing here: the foreign lea's own scan governs its uses
			-- (and a state-2/state-1 MERGE below is what records the conflict).
			if s == 1 or s == 3 then
				local nUses = 0
				for _, u in c.uses do
					if u.kind == "reg" and u.rc == "gpr" and u.num == regNum then
						nUses += 1
					end
				end
				if nUses > 0 then
					-- The self-xor/self-sub zeroing idiom observes nothing (the
					-- same exemption the carrier discipline's use-check grants).
					local zeroing = (bb == "xor" or bb == "sub") and #ops2 >= 2
						and ops2[1] and ops2[2]
						and ops2[1].kind == "reg" and ops2[2].kind == "reg"
						and ops2[1].num == regNum and ops2[2].num == regNum
					if not zeroing then
						if s == 3 then
							-- A conflicted carrier: the rewrite rejects every
							-- memory access through the register (it cannot
							-- decide stack+offset against heap/argument pointer),
							-- so any memory-base use falls back. A value use
							-- rides the ordinary register webs — legal.
							local mem2, memCount2 = nil, 0
							for _, o in ops2 do
								if o.kind == "mem" then mem2 = o; memCount2 += 1 end
							end
							if mem2 and mem2.base and mem2.base.kind == "reg"
								and mem2.base.rc == "gpr" and mem2.base.num == regNum then
								res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break
							end
						else
							local mem2, memCount2 = nil, 0
							for _, o in ops2 do
								if o.kind == "mem" then mem2 = o; memCount2 += 1 end
							end
							local baseOk = mem2 ~= nil and memCount2 == 1
								and mem2.base and mem2.base.kind == "reg"
								and mem2.base.rc == "gpr" and mem2.base.num == regNum
								and not mem2.index and not mem2.dispSym
							-- A `leaq K(%carrier), %reg` through the tracked
							-- carrier is a DERIVATION (the smap's derived-carrier
							-- feature): the tracked value flows into the new
							-- carrier legally — it is not an observation of the
							-- phantom. The tracked register's state after it is
							-- governed by the def handling below (dest == base
							-- kills the tracked carrier; dest ~= base keeps it);
							-- the derived register's own uses are the smap's.
							if not (baseOk and nUses == 1)
								or (bb == "lea" and not (baseOk and nUses == 1))
								or bb == "call" or bb == "jmp" then
								res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break
							end
							-- The aliasOk window proof (see the fixpoint's
							-- use-check): the access must resolve inside the
							-- frame window.
							if not yoloArgSlotOf(st2, regName, parkedDepth) then
								local ao = (mem2.dispVal or 0) + D0 - parkedDepth
								local afpe = fpKnowledge.lookup(bm)
								local aw = nil
								if isFpInsn(afpe, st2) then
									local okW, ww = pcall(fpKnowledge.accessWidth,
										bm, ops2, st2.raw)
									if not (okW and ww) then res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break end
									aw = ww
								else
									aw = gprStackWidth(bm, mem2)
								end
								if not aw then res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break end
								if ao < -128 or ao + aw > frameSize
									or regionOf(ao, false) then
									res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break
								end
							end
						end
					end
				end
			end
			-- A plain `ret` reads the integer return register in the ABI sense
			-- (the rewrite rejects a %rax carrier live at such a ret).
			if bb == "ret" and not st2.localRet and not st2.tailRet
				and retIsGpr and regNum == 0 and (s == 1 or s == 3) then
				res = false failWhy = "at: " .. (st2 and (st2.raw or st2.mnemonic) or "?") break
			end
			-- Propagate, merging on arrival: holding-states of different provenance
			-- conflict (0+holding -> 3, 1+2 -> 3, 2+2 -> 3, anything+3 -> 3);
			-- identical holding states agree; depths merge to "dyn" on mismatch.
			for _, s2 in successors(i) do
				local curC, curD = instateC[s2], instateD[s2]
				local mergedC
				if curC == nil then mergedC = out
				elseif curC == 3 or out == 3 then mergedC = 3
				elseif curC == 0 and out == 0 then mergedC = 0
				elseif curC == out then mergedC = curC
				else mergedC = 3 end
				local mergedD
				if curD == nil then mergedD = ndv
				elseif curD == "dyn" or ndv == "dyn" then mergedD = "dyn"
				elseif curD == ndv then mergedD = curD
				else mergedD = "dyn" end
				if curC == nil or mergedC ~= curC or mergedD ~= curD then
					instateC[s2] = mergedC
					instateD[s2] = mergedD
					table.insert(work, { s2, mergedC, mergedD })
				end
			end
		end
		break
		end
		leaMemOnlyCache[st] = res
		return res
	end

	-- Are the condition flags DEAD after statement i — nothing reads the
	-- clobbered flags before they are fully rewritten on any control-flow
	-- path? Precise forward scan over the CFG with per-flag granularity
	-- (x86.flagUseMask/flagDefMask): the pending set starts as the flags
	-- statement i fully rewrites; a USE of a pending flag kills the proof, a
	-- full rewrite clears pending flags, and partial writes propagate the
	-- remainder (sound: the flags a later reader sees through them still
	-- include the clobbered value — e.g. `dec` preserves CF, so a later
	-- `adc` still observes the clobber, while a later `jnz` observes only
	-- the `dec`-written ZF). Paths that reach a `ret` (or the body end) are
	-- sound: nothing reads flags there. Used by the mid-function `subq/
	-- `addq $imm, %rsp` and `and $-N, %rsp` drops below (they clobber the
	-- flags). Strictly more precise than the historical coarse scan (a full
	-- rewrite clears the whole pending set, so every historically-accepted
	-- shape stays accepted) while additionally accepting clobbers hidden
	-- behind partial writes whose preserved flags no reader observes (the
	-- chacha B2-clone inner-frame `sub` ahead of a `dec`-counter loop).
	local function flagsDeadAfter(i)
		local start = x86.flagDefMask(body[i])
		if start == 0 then return true end
		-- seen[idx] = union of pending masks already expanded there. Pending
		-- only shrinks along a path, so a pending set covered by the union
		-- adds no new behavior (every continuation was already proven for a
		-- superset of it).
		local seen = {}
		local work = {}
		for _, s in successors(i) do table.insert(work, { s, start }) end
		while #work > 0 do
			local item = table.remove(work)
			local j, pend = item[1], item[2]
			if pend ~= 0 then
				local prev = seen[j] or 0
				if bit32.band(pend, bit32.bnot(prev)) ~= 0 then
					seen[j] = bit32.bor(prev, pend)
					local st = body[j]
					if st and st.kind == "insn" then
						if bit32.band(x86.flagUseMask(st), pend) ~= 0 then return false end
						pend = bit32.band(pend, bit32.bnot(x86.flagDefMask(st)))
						if pend ~= 0 then
							for _, s in successors(j) do table.insert(work, { s, pend }) end
						end
					elseif st then
						for _, s in successors(j) do table.insert(work, { s, pend }) end
					end
				end
			end
		end
		return true
	end
	local depthIn, savesIn, fpIn, smapIn = {}, {}, {}, {}
	do
		-- Local-ret state source (localcall.luau): a local ret resumes its
		-- continuation with the CLONE ENTRY's depth/saves/frame-pointer state —
		-- the clone's own frame machinery (its pushes, its frame sub/add) is
		-- dropped exactly like a top-level frame, so the real rsp never moved
		-- inside the clone, and the continuation executes at the caller's
		-- (callsite's) depth. The smap keeps the clone's flow (carrier
		-- redefinitions inside the clone still poison the caller's carriers;
		-- carriers created inside the clone park the clone's rsp and are
		-- dropped). Because the clone entry is not a CFG predecessor of the ret,
		-- the fixpoint re-enqueues a clone's local rets whenever the entry's
		-- state changes (see the merge below), so the override always evaluates
		-- against the converged entry state.
		local localRetByEntry = {}
		for i, st in body do
			if st.kind == "insn" and st.localRet then
				local e = labelAt[st.localRet.entry]
				if e then
					if not localRetByEntry[e] then localRetByEntry[e] = {} end
					table.insert(localRetByEntry[e], i)
				end
			end
		end
		local firstInsn
		for i, st in body do
			if st.kind == "insn" then firstInsn = i break end
		end
		if firstInsn then
			depthIn[firstInsn] = 0
			savesIn[firstInsn] = {}
			smapIn[firstInsn] = {}
			fpIn[firstInsn] = "no"
			local worklist, inq = { firstInsn }, { [firstInsn] = true }
			while #worklist > 0 do
				local i = table.remove(worklist)
				inq[i] = nil
				local od, osv, osmap, ofp = transfer(body[i], depthIn[i], savesIn[i],
					smapIn[i], fpIn[i], i)
				local st = body[i]
				if st.localRet then
					local e = labelAt[st.localRet.entry]
					if e and depthIn[i] ~= nil then
						od = depthIn[e]
						osv = savesIn[e]
						ofp = fpIn[e]
						local esmap = smapIn[e]
						if esmap then
							local out = {}
							for name, ee in pairs(osmap) do
								if esmap[name] then out[name] = ee end
							end
							osmap = out
						end
					end
				end
				for _, s in successors(i) do
					-- A nil od carries NO state across the edge: it arises only
					-- from the local-ret override above when the clone's entry
					-- has not been reached yet (the clone is entered only
					-- through its callsite, so a local ret can transiently be
					-- dequeued — through a shared clone whose other callsites
					-- ARE reached — before its own entry has state). Propagating
					-- then would poison the continuation: with an unreached
					-- successor a nil "merged" could still be assigned when a
					-- saves/smap/fp component alone differed (enqueueing a
					-- statement with depthIn nil — crashing the transfer), and
					-- with a reached one it would spuriously reset it to "dyn".
					-- The continuation is reached for real once the entry's
					-- state arrives and the ret is re-evaluated (the
					-- localRetByEntry re-enqueue below), so skipping the edge
					-- now loses nothing and keeps the fixpoint's invariant:
					-- depthIn nil means UNREACHED, and is never assigned.
					if od ~= nil then
					local cur = depthIn[s]
					local merged
					if cur == nil then merged = od
					elseif cur == "dyn" or od == "dyn" then merged = "dyn"
					elseif type(cur) == "table" or type(od) == "table" then
						-- A region scope ({anchored} or {dynreg}) survives a merge only
						-- when every path sits in the SAME region's scope (a loop
						-- inside one is fine); anything else is the ordinary
						-- unprovable "dyn". (The two kinds never mix: one field is
						-- always nil on each side, so a kind mismatch fails the
						-- anchored equality.) The exact scope depth must agree too:
						-- a path through the anchoring andq (scopeDepth lost) merging
						-- with one that skipped it must not credit the restore-base
						-- lea with a depth only one path has.
						if type(cur) == "table" and type(od) == "table"
							and cur.anchored == od.anchored and cur.dynreg == od.dynreg
							and cur.scopeDepth == od.scopeDepth then
							merged = cur
						else
							merged = "dyn"
						end
					elseif cur == od then merged = cur
					else merged = "dyn" end
					local mergedSv = mergeSaves(savesIn[s], osv)
					local mergedSmap = mergeSmaps(smapIn[s], osmap)
					local curFp, mergedFp = fpIn[s], nil
					if curFp == nil then mergedFp = ofp
					elseif curFp == "dyn" or ofp == "dyn" then mergedFp = "dyn"
					elseif curFp == ofp then mergedFp = curFp
					else mergedFp = "dyn" end
					if merged ~= cur or mergedSv ~= savesIn[s] or mergedSmap ~= smapIn[s]
						or mergedFp ~= curFp then
						depthIn[s] = merged
						savesIn[s] = mergedSv
						smapIn[s] = mergedSmap
						fpIn[s] = mergedFp
						if not inq[s] then inq[s] = true table.insert(worklist, s) end
						-- A clone entry's state changed: re-evaluate its local rets
						-- (their override reads this state; see above). Only when the
						-- ret's own flow has already reached it.
						for _, lr in (localRetByEntry[s] or {}) do
							if depthIn[lr] ~= nil and not inq[lr] then
								inq[lr] = true table.insert(worklist, lr)
							end
						end
					end
					end
				end
			end
		end
	end

	-- Verify epilogue teardown regions: from the instruction after a teardown starter to a
	-- `ret`, allow only callee-saved pops, `leave`, and ordinary computation that never
	-- touches the stack or control flow. Returns the covered insn indices, or nil.
	local function validateTeardown(afterIdx)
		local marks = {}
		local j = afterIdx
		while j <= #body do
			local st = body[j]
			if st.kind == "label" and targeted[st.name] then return nil end
			if st.kind == "insn" then
				local m = string.lower(st.mnemonic)
				local b = x86.baseMnemonic(m)
				local ops = st.operands
				-- A local call inside a teardown span is control flow the
				-- straight-line proof cannot cover. A local RET is the span's
				-- terminator exactly like a top-level ret (the clone's own epilogue
				-- teardown leads to it): it terminates the span below.
				if st.localCall then return nil end
				-- The terminating `ret` is part of the verified span (the annotation pass
				-- accepts a return at an UNKNOWN depth only inside such a span).
				if b == "ret" then table.insert(marks, j) return marks end
				if b == "jmp" or b == "jcc" or b == "call" or string.match(m, "^loop") then return nil end
				if b == "leave" then
					table.insert(marks, j)
				elseif b == "pop" and ops[1] and ops[1].kind == "reg" and x86.CALLEE_SAVED[ops[1].name] then
					table.insert(marks, j)
				else
					-- An RFLAGS stack transfer inside a teardown span is an unmodeled
					-- stack word (the span's statements are DROPPED, so the word's
					-- slot store would vanish while the depth analysis still counted
					-- it — and a pushfq before the span's `ret` would leave the
					-- real return address one word deep): same class as a push.
					if b == "push" or b == "pop" or x86.isFlagStackXfer(m) or insnWritesSp(st)
						or insnTouchesFrame(st, usesRbp) then return nil end
					table.insert(marks, j)
				end
			end
			j += 1
		end
		return nil
	end

	-- Annotate.
	-- A paired pop of a save whose register was redefined between the push and the
	-- pop still gets DROPPED (the save/restore round-trip is otherwise a no-op), but
	-- its RELOAD is then lost: hardware loads the pushed value into the register,
	-- while the web keeps the redefined value. That is invisible until the register
	-- is OBSERVED after the pop — so walk forward from the pop (CFG, stopping at a
	-- fresh definition of the register) and reject at the first use that would read
	-- the wrong value. The standard prologue/epilogue shapes read nothing after the
	-- epilogue pops and stay accepted. The same walk guards the dropped
	-- movq-restore loads below (there `why` names the shape).
	local function checkLostReload(popIdx, save, why)
		local work, seen = {}, {}
		for _, s in successors(popIdx) do table.insert(work, s) end
		while #work > 0 do
			local i = table.remove(work)
			if seen[i] then continue end
			seen[i] = true
			local st = body[i]
			if st.kind ~= "insn" then continue end
			local c = classOf(st)
			for _, u in c.uses do
				if u.kind == "reg" and u.rc == "gpr" and u.num == save.num then
					error("sarcasm: register " .. save.name .. " is read after the"
						.. " " .. (why or ("transient-pad pop that restores its save (the"
							.. " register was redefined between its push and that pop, so the"
							.. " pop's reload cannot be modeled and this read would see the"
							.. " redefined value; read it before the pop or don't redefine"
							.. " it inside the pad)")) .. ": " .. (st.raw or st.mnemonic))
				end
			end
			local defined = false
			for _, dd in c.defs do
				if dd.kind == "reg" and dd.rc == "gpr" and dd.num == save.num then
					defined = true break
				end
			end
			if not defined then
				for _, s in successors(i) do table.insert(work, s) end
			end
		end
	end
	-- Does the entry signature return an integer-class value (in %rax)? A restore
	-- into %rax can never be proven unobservable (the frame analysis does not model
	-- `ret`'s operand read), and a %rax carrier live at a `ret` is an escaping stack
	-- address — both checks below key off this.
	local retIsGpr = sigDesc and not sigDesc.isVoid
		and not (sigDesc.retTypes[1] and sig.isFpClass(sigDesc.retTypes[1]))
	for i, st in body do
		if st.kind == "insn" then
			st.x86_prologue = i < prologueEnd
			st.x86_depth = depthIn[i]
			-- Whether the frame pointer is provably established (its prologue value
			-- intact) on EVERY path reaching this instruction: "est", "no" (not
			-- established — an early-exit path that skipped the prologue, or a point
			-- after a paired `popq %rbp` restored the caller's frame pointer), "dyn"
			-- (paths disagree), or nil (unreachable code). Gates every rbp-relative
			-- virtualization: a static slot is only what the access addresses when
			-- %rbp provably still holds this function's frame pointer.
			st.x86_fp = fpIn[i]
			-- The outstanding pushed-register save stack at this instruction (or
			-- "dyn" when the paths disagree, or nil for unreachable code). Consumed
			-- by the rewrite: a stack access that aliases one of these save slots
			-- (a `pushq %rbx` inside the prologue pad that has not been popped yet)
			-- must not virtualize into a plain slot web — the slot's content is
			-- mirrored by the pushed register's web, because the matching pop (which
			-- the pairing analysis proves restores exactly this register) is dropped.
			st.x86_saves = savesIn[i]
			-- The prologue rsp-save map at this instruction: consumed by the rewrite
			-- to redirect entry-time reads of the incoming yolo stack arguments
			-- through an entry-rsp alias register (and to gate %rsp recoveries).
			st.x86_smap = smapIn[i]
			-- A `movq %rbx, %rsp` whose source register still holds an UNREDEFINED
			-- prologue rsp save in the converged smap at this point recovers a
			-- known-good stack pointer: the rewrite drops it and the transfer revived
			-- the depth to the saved value (even out of an unknown "dyn" depth — the
			-- smap rides through the poison exactly like the save stack does). The
			if isSpRecoverForm(st, usesRbp) and not (st.allocaPlace or st.allocaOwned) then
				local e = smapIn[i] and smapIn[i][st.operands[2].name]
				-- any live carrier recovers (the recovery is dropped — no register
				-- clobber can be observed through it).
				if e and not e.redefined and not e.slot then
					st.x86_spRecover = true
				end
			end
			-- The lea recovery form (`leaq K(%reg), %rsp`, base not rbp): same proof as
			-- the mov form — the carrier's parked depth minus K is revived.
			if not (st.allocaPlace or st.allocaOwned)
				and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea"
				and st.operands[1] and st.operands[1].kind == "reg" and st.operands[1].rc == "sp"
				and st.operands[2] and st.operands[2].kind == "mem"
				and st.operands[2].base and st.operands[2].base.kind == "reg"
				and st.operands[2].base.name ~= "rbp"
				and not st.operands[2].index and not st.operands[2].dispSym then
				local e = smapIn[i] and smapIn[i][st.operands[2].base.name]
				if e and not e.redefined and not e.slot and e.depth ~= nil
					and e.depth - (st.operands[2].dispVal or 0) >= 0 then
					st.x86_spRecover = true
				end
			end
			-- Phantom saved-rsp flow marks (see the smap comment in the fixpoint):
			-- save-stores and carrier-loads are dropped by the rewrite; a lea-save is
			-- dropped with the save; a restore load through a carrier is dropped like
			-- the epilogue pops (proven unobservable when the save was redefined).
			if depthIn[i] ~= nil and not (st.allocaPlace or st.allocaOwned) then
				-- A recorded `movq %rsp, %reg` save carrier (the fixpoint's isSave,
				-- recomputed exactly): dropped by the rewrite anywhere it appears.
				if isSpSaveForm(st) and not isDynDepth(depthIn[i])
					and (x86.CALLEE_SAVED[st.operands[1].name] ~= nil or yoloCount > 0
						or st.b2clone ~= nil
						or (regions ~= nil and not regionOf(0, false)))
					and (st.x86_prologue or not (st.allocaPlace or st.allocaOwned)) then
					st.x86_spSave = true
				end
				local se = saveStoreOf(st, smapIn[i] or {}, depthIn[i])
				if se then st.x86_saveStore = true end
				if not se and carrierLoadOf(st, smapIn[i] or {}, depthIn[i]) then
					st.x86_carrierLoad = true
				end
				-- A memory access through a carrier register that lands inside the
				-- current anchored region scope (the poly1305 frame-anchor shape —
				-- regionCarrierAccess): passed through by the rewrite and redirected
				-- to the region pointer by the transform. The fixpoint's carrier
				-- use-check already proved the base is the register's ONLY use.
				if not se then
					local mem
					for _, o in (st.operands or {}) do if o.kind == "mem" then mem = o end end
					if mem and mem.base and mem.base.kind == "reg" then
						local e = smapIn[i] and smapIn[i][mem.base.name]
						if e and not e.redefined and not e.slot and not e.roff
							and regionCarrierAccess(st, depthIn[i], e) then
							st.x86_regionCarrier = true
						end
					end
				end
				-- A stack access through a stack+offset alias carrier (see the
				-- transfer's aliasOk check, mirrored here): mark its normalized
				-- frame offset for the rewrite's slot path. Entry-time yolo
				-- argument reads keep their own redirect; anything the transfer
				-- rejected never reaches the rewrite.
				if not se then
					local amem = nil
					for _, o in (st.operands or {}) do if o.kind == "mem" then amem = o end end
					if amem and amem.base and amem.base.kind == "reg" then
						local ae = smapIn[i] and smapIn[i][amem.base.name]
						if ae and not ae.redefined and not ae.slot and not ae.roff and not ae.conflict
							and not yoloArgSlotOf(st, amem.base.name, ae.depth) then
							local ao2 = aliasSlotOf(st, amem, ae)
							if ao2 then st.x86_aliasOff = ao2 st.x86_aliasSaveIdx = ae.saveIdx end
						end
					end
				end
				local rs = restoreLoadOf(st, smapIn[i], savesIn[i])
				if rs then
					if rs.redefined then
						if retIsGpr and st.operands[1].num == 0 then
							error("sarcasm: epilogue restore load into the integer return"
								.. " register cannot be modeled (the register was redefined"
								.. " after its save, so the restore's value and the web's"
								.. " value disagree): " .. (st.raw or st.mnemonic))
						end
						checkLostReload(i, rs,
							"epilogue restore load of a save whose register was redefined"
								.. " after the push (the load cannot be modeled — the slot"
								.. " holds the pre-push value while the register's web holds"
								.. " the redefined one — and this read would observe the"
								.. " web's value)")
					end
					st.x86_dropRestore = true
				end
				-- The general lea-save (`leaq d(%rsp), %reg` parking the entry stack
				-- pointer in a carrier, and the below-rsp `leaq -K(%rsp), %reg` frame
				-- anchor parking a deeper carrier): mark it for the rewrite's drop.
				-- The condition mirrors the fixpoint's isLeaSave computation exactly —
				-- including the +8 rule for clone statements (K = d + 8 parks the
				-- entry rsp) and the provable-depth carrier relaxation (any K at a
				-- provable NUMBER depth off the clone statements — see the fixpoint
				-- comment there).
				if regions ~= nil and not isDynDepth(depthIn[i])
					and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
					local ops = st.operands
					local mem = ops[2]
					if ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
						and (ops[1].name ~= "rbp" or not usesRbp)
						and mem and mem.kind == "mem" and mem.base and mem.base.rc == "sp"
						and not mem.index and not mem.dispSym then
						local k = mem.dispVal or 0
						local pk = st.localClone and (k - 8) or k
						local rkey = st.localClone and (k + D0 - depthIn[i] - 8) or k
						local leaAny = type(depthIn[i]) == "number"
							and (depthIn[i] ~= D0 or ops[1].name == "rbp")
							and not st.b2clone and not st.localClone
						-- Feature (D0 interior lea-save carriers): the mirror of
						-- the fixpoint's `leaD0` relaxation (see the long comment
						-- there) — the same conservative scan, cached per lea
						-- statement, so this mark and the fixpoint's carrier
						-- registration can never disagree.
						local leaD0 = nil
						if type(depthIn[i]) == "number" and depthIn[i] == D0
							and not st.localClone
							and ops[1].name ~= "rbp" and k > 0 and k ~= depthIn[i] then
							leaD0 = leaSaveUsesMemoryOnly(st, i, depthIn[i] - k)
						end
						if (pk == depthIn[i] or pk <= 0 or leaAny or leaD0)
							and not regionOf(rkey, false)
							and not (yoloCount > 0 and k == 0) then
							-- Feature (by-address buffer value leas): the same
							-- exemption the fixpoint's carrier registration
							-- applies (see transfer above) — a buffer-interior
							-- lea is a VALUE definition, not a phantom carrier,
							-- and inside a localcall clone the carrier spelling
							-- cannot name buffer bytes at all (the +8
							-- compensation would be applied twice). This mark
							-- and the fixpoint can never disagree: the
							-- bufProbeRanges set and the memory-only scan are
							-- identical inputs.
							local probe = false
							if bufProbeRanges and type(depthIn[i]) == "number"
								and not isDynDepth(depthIn[i]) then
								if st.b2clone then
									-- The fixpoint's B2 clone probe (see the
									-- transfer): the clone's own raw declared
									-- ranges, tested against the lea's raw
									-- displacement — the mark and the fixpoint
									-- can never disagree.
									local lst = bufB2DeclRaw
										and bufB2DeclRaw[st.b2clone] or nil
									if lst then
										for _, sba in lst do
											if k >= sba.lo and k < sba.hi then
												probe = true break
											end
										end
									end
								else
									local loff = k + D0 - depthIn[i]
										- (st.localClone and 8 or 0)
									for _, r in bufProbeRanges do
										if loff >= r.lo and loff < r.hi then
											probe = true break
										end
									end
								end
							end
							if probe and not bufRepRegs[ops[1].num]
								and (not leaSaveUsesMemoryOnly(st, i, depthIn[i] - k)
									or st.localClone) then
								-- a buffer-interior VALUE lea: no carrier mark
							else
								st.x86_leaSave = true
							end
						end
					end
				end
				-- Feature (derived stack carriers): mark a `leaq K(%carrier),
				-- %reg` whose base holds an unredefined carrier for the rewrite's
				-- drop, mirroring the fixpoint's derived-carrier registration
				-- exactly (same conditions, same converged smap).
				if not st.x86_leaSave and regions ~= nil
					and not isDynDepth(depthIn[i])
					and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
					local dops = st.operands
					local dmem = dops[2]
					if dops[1] and dops[1].kind == "reg" and dops[1].rc == "gpr"
						and (dops[1].name ~= "rbp" or not usesRbp)
						and dmem and dmem.kind == "mem" and dmem.base
						and dmem.base.kind == "reg" and dmem.base.rc == "gpr"
						and (dmem.base.name ~= "rbp" or not usesRbp)
						and not dmem.index and not dmem.dispSym then
						local de = smapIn[i] and smapIn[i][dmem.base.name]
						local dk = dmem.dispVal or 0
						if de and not de.redefined and not de.slot and not de.roff
							and not de.conflict and type(de.depth) == "number"
							and de.depth - dk >= 0 then
							st.x86_leaSave = true
						end
					end
				end
				-- The restore-base leas (D7 — the {anchored}-scope `leaq K(%rsp),
				-- %reg` and the rbp-derived `leaq K(%rbp), %reg`): mark for the
				-- rewrite's drop, mirroring the fixpoint's registration exactly.
				if not st.x86_leaSave and regions ~= nil
					and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
					local ops = st.operands
					local mem = ops[2]
					local dd = depthIn[i]
					if type(dd) == "table" and dd.anchored and dd.scopeDepth
						and ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
						and ops[1].name ~= "rbp"
						and mem and mem.kind == "mem" and mem.base and mem.base.rc == "sp"
						and not mem.index and not mem.dispSym then
						local k = mem.dispVal or 0
						local sd = dd.scopeDepth + (st.localClone and 8 or 0)
						if sd - k >= 0 and k >= dd.anchored.base + dd.anchored.size then
							st.x86_leaSave = true
						end
					end
					if not st.x86_leaSave and usesRbp and fpIn[i] == "est"
						and ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
						and ops[1].name ~= "rbp"
						and mem and mem.kind == "mem" and mem.base and mem.base.kind == "reg"
						and mem.base.name == "rbp" and not mem.index and not mem.dispSym then
						local k = mem.dispVal or 0
						local rkey = rbpNorm + k
						if fpDepth - k >= 0 and not regionOf(rkey, false)
							and (rkey < 0 or rkey >= frameSize) then
							st.x86_leaSave = true
						end
					end
				end
			end
			-- A return reads the integer return register: if %rax still carries the
			-- saved stack pointer at a `ret`, the RETURN VALUE would be the live stack
			-- address — the one carrier read that is never droppable. Reject it.
			-- (A local subroutine's ret is exempt: it does not produce this function's
			-- Fil-C return value — a carrier %rax flowing to the continuation is the
			-- caller's phantom recovery working as intended. A cross-function-jump
			-- synthetic epilogue ret (st.tailRet — tailcall.luau) is exempt the same
			-- way: its return value is the tail-called callee's result, defined by
			-- the call, never the saved stack pointer.)
			if x86.baseMnemonic(string.lower(st.mnemonic)) == "ret" and not st.localRet
				and not st.tailRet and retIsGpr and smapIn[i] then
				local e = smapIn[i]["rax"]
				if e and not e.redefined and not e.slot then
					error("sarcasm: returning while %rax holds the saved stack pointer cannot"
						.. " be proven safe (the return value would be the live stack"
						.. " address; redefine %rax or recover %rsp from it first): "
						.. (st.raw or st.mnemonic))
				end
			end
			-- A pop provably restores a matching save only when the incoming abstract save
			-- stack shows the LAST outstanding push as the same register at the same slot
			-- depth on every path. Anything else (no outstanding push, a different register
			-- on top, an unprovable stack) loads a slot that is not that register's save.
			-- At an UNKNOWN depth (unknown-depth poison) the slot position cannot be
			-- checked; there the pop pairs off the outstanding save stack only inside a
			-- VERIFIED epilogue teardown (validateTeardown above: a straight path to a
			-- `ret` through callee-saved pops and frame-touch-free computation). Every
			-- statement in such a span is dropped and the synthesized epilogue owns the
			-- real %rsp, so the pairing only has to prove that the pop restores the same
			-- register a dropped push saved — the slot it reads is never emitted.
			if x86.baseMnemonic(string.lower(st.mnemonic)) == "pop" and depthIn[i] ~= nil then
				local sv, d = savesIn[i], depthIn[i]
				local n = (st.operands[1] and st.operands[1].kind == "reg")
					and st.operands[1].name or "?"
				if not isDynDepth(d) then
					if sv ~= "dyn" and #sv > 0 and sv[#sv].spill and sv[#sv].depth == d
						and st.operands[1] and st.operands[1].kind == "reg"
						and st.operands[1].rc == "gpr" and st.operands[1].name ~= "rsp"
						and (st.operands[1].width == 64 or st.operands[1].width == nil) then
						-- A pop of an outstanding SPILL slot (a caller-saved register
						-- parked on the stack — the rsaz-avxifma `pushq %r9` /
						-- `popq %r9` shape): hardware loads the slot at (%rsp) into
						-- the destination — an ordinary slot LOAD (the slot web holds
						-- exactly what the spill store wrote, capability included),
						-- never a dropped save restore.
						st.x86_spillPop = true
					elseif sv == "dyn" or #sv == 0
						or sv[#sv].name ~= n or sv[#sv].depth ~= d
						or sv[#sv].spill then
						-- (a spill slot that is not at the exact pop depth is not a
						-- provable slot load — reject below)
						st.x86_unpairedPop = true
					elseif sv[#sv].redefined then
						checkLostReload(i, sv[#sv])
					end
				elseif st.x86_teardown and sv ~= "dyn" and #sv > 0 and sv[#sv].name == n
					and not sv[#sv].spill then
					if sv[#sv].redefined then
						checkLostReload(i, sv[#sv])
					end
				else
					st.x86_unpairedPop = true
				end
			end
			-- A popf/popfq loads the word at (%rsp) into RFLAGS. The word must be
			-- MATERIALIZED frame storage — an outstanding pushfq word (the frame
			-- rewrite files the materialized flags into its slot) or a spill push
			-- of a caller-saved register (the slot web holds the pushed value) —
			-- at the exact current depth on every path; the rewrite then lowers the
			-- statement into a slot load of that word plus the pushq/popfq
			-- round-trip. Popping a DROPPED register save (a callee-saved push's
			-- save slot is never emitted — its value lives only in the register's
			-- web) or an unproven word has no modeled value to load: rejected by
			-- the rewrite (no mark).
			if x86.isFlagStackPop(string.lower(st.mnemonic)) and depthIn[i] ~= nil then
				local sv, d = savesIn[i], depthIn[i]
				if not isDynDepth(d) and sv ~= "dyn" and #sv > 0
					and sv[#sv].spill and sv[#sv].depth == d then
					st.x86_flagsPop = true
				end
			end
			if not st.x86_prologue then
				local b = x86.baseMnemonic(string.lower(st.mnemonic))
				local ops = st.operands
				local isTeardownStart = (b == "add" and ops[1] and ops[1].rc == "sp"
						and ops[2] and ops[2].kind == "imm")
					or (b == "mov" and ops[1] and ops[1].rc == "sp" and ops[2]
						and ops[2].name == "rbp" and usesRbp)
					or (b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2]
						and ops[2].kind == "mem" and ops[2].base and ops[2].base.name == "rbp"
						and usesRbp and not ops[2].index and not ops[2].dispSym)
					or (b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2]
						and ops[2].kind == "mem" and ops[2].base and ops[2].base.rc == "sp"
						and not ops[2].index and not ops[2].dispSym
						and (ops[2].dispVal or 0) >= 0)
					or (b == "leave" and usesRbp)
				if isTeardownStart then
					local marks = validateTeardown(nextInsn[i])
					if marks then
						st.x86_teardownStart = true
						for _, j in marks do body[j].x86_teardown = true end
					end
				end
				-- A constant mid-function rsp adjustment (`subq $imm, %rsp` /
				-- `addq $imm, %rsp`, and the `leaq K(%rsp), %rsp` spellings) that
				-- is not a verified epilogue teardown: sound to drop when the
				-- depth on entry is statically known and the RESULTING depth
				-- stays non-negative — every later slot access keys off the
				-- depth exactly (the perturbed-depth normalization), pops stay
				-- save-pairing-gated, and returns stay depth-0-gated. This is
				-- the fixed-size inner-frame shape: balanced `leaq -168(%rsp),
				-- %rsp` … `leaq 168(%rsp), %rsp` scratch pairs (rsaz-*k-avxifma),
				-- mid-function frees ahead of a re-allocation (poly1305's
				-- `leaq 48(%rsp), %rsp` before jumping into the alloca path),
				-- and the ecp_nistz256 `addq $416, %rsp` shared-tail
				-- frame-reshape. The sub/add forms clobber the flags, so they
				-- additionally require the flags to be provably dead — nothing
				-- reads them before a full flag write on any path; `leaq` is
				-- flags-neutral and needs no such proof. (An adjustment that
				-- would take the depth negative — above the entry rsp — is not
				-- provable and stays rejected.)
				if (b == "sub" or b == "add") and ops[1] and ops[1].rc == "sp"
					and ops[2] and ops[2].kind == "imm" and not st.x86_teardownStart
					and type(depthIn[i]) == "number" then
					local imm = immVal(ops[2])
					local nd = (b == "sub") and (depthIn[i] + imm) or (depthIn[i] - imm)
					if nd >= 0 and flagsDeadAfter(i) then
						st.x86_midSp = true
					end
				end
				if b == "lea" and ops[1] and ops[1].rc == "sp"
					and ops[2] and ops[2].kind == "mem" and ops[2].base
					and ops[2].base.rc == "sp" and not ops[2].index and not ops[2].dispSym
					and not st.x86_teardownStart and type(depthIn[i]) == "number"
					and depthIn[i] - (ops[2].dispVal or 0) >= 0 then
					st.x86_midSp = true
				end
				-- A mid-function `and $-N, %rsp` (N a power of two >= 16) at a
				-- statically known depth: the same rounding the prologue prefix
				-- tolerates, dropped by the rewrite with the alignment recorded.
				-- The and writes the flags, so like the sub/add forms it needs
				-- provably dead flags. Direct rsp-relative slot traffic BEFORE
				-- the and keys pre-and numbering (rejected via x86_preAnd
				-- below); carrier/rbp values are absolute and unaffected.
				if b == "and" and ops[1] and ops[1].rc == "sp"
					and ops[2] and ops[2].kind == "imm" and not st.x86_teardownStart
					and type(depthIn[i]) == "number" then
					local an = -immVal(ops[2])
					if isStackAlign(an) and flagsDeadAfter(i) then
						st.x86_midAnd = true
						if an > frameAlignReq then
							frameAlignReq = an; frameAndDepth = depthIn[i]
						end
					end
				end
			end
		end
	end

	local andDomOf = {}
	local reachedFromAnd = {}  -- see the pre/post-`and` reachability walk below
	-- Direct rsp-relative slot traffic positioned BEFORE the first mid-function
	-- `and $-N, %rsp` keys pre-and numbering, which the and shifts by a dynamic
	-- slack: the rewrite rejects it (alias-carrier and rbp-relative accesses are
		-- absolute and stay legal; yolo-argument reads return before the check).
	do
		local andIdxs = {}
		for i, st in body do
			if st.kind == "insn" and st.x86_midAnd then andIdxs[#andIdxs + 1] = i end
		end
		local firstAnd = andIdxs[1]
		if firstAnd then
			-- Mark every instruction that can REACH a mid-function `and`:
			-- such an access keys pre-and numbering while later same-key
			-- traffic keys post-and numbering shifted by the and's dynamic
			-- slack. The historical layout approximation (every instruction
			-- before the first and) is wrong once B2 shared-tail clones are
			-- appended after the body: each clone is its own path-ENTRY
			-- (reached only through its jump edge, returning through its
			-- rets), so traffic in one clone (or the jumper body) cannot
			-- observe another clone's and slack unless control actually
			-- flows there — exactly what reachability says. Traffic that
			-- CAN flow into an and — within one clone ahead of its own and,
			-- or jumper-header traffic ahead of the dispatch jump — stays
			-- marked and rejected, preserving the protection (including a
			-- define-before-and/use-after-and shape across the jump edge,
			-- whose shared key would name different addresses).
			local isAnd = {}
			for _, a in ipairs(andIdxs) do isAnd[a] = true end
			local pred = {}
			for i = 1, #body do
				if body[i].kind == "insn" then
					for _, s in successors(i) do
						local l = pred[s]
						if not l then l = {}; pred[s] = l end
						table.insert(l, i)
					end
				end
			end
			local reached = {}
			local work = {}
			for _, a in ipairs(andIdxs) do
				if not reached[a] then reached[a] = true; table.insert(work, a) end
			end
			while #work > 0 do
				local j = table.remove(work)
				for _, p in (pred[j] or {}) do
					if not reached[p] then reached[p] = true; table.insert(work, p) end
				end
			end
			for i, st in body do
				if st.kind == "insn" and reached[i] and not isAnd[i] then
					st.x86_preAnd = true
				end
			end
			-- Forward reachability from the mids: an instruction reachable from
			-- an `and` may execute with that and's dynamic slack baked into rsp.
			-- A carrier parked at such a point captures a slack-tainted value,
			-- so a later restore through it is not entry-absolute (this is the
			-- dual of the preAnd walk above, which marks what can reach an
			-- and). A park reached from NO and on any path captures the
			-- slack-free rsp — entry-absolute when parked at the entry depth —
			-- no matter where the ands sit in body order (a dispatch-exclusive
			-- path's and never executes alongside another path's park/use, e.g.
			-- the aesni-mb shared 4x/8x bodies). The set is function-scoped
			-- (reachedFromAnd) because the FP-cluster keying and the pre/post-`and`
			-- slot-web intersection check consume it later: a statement reached
			-- from an `and` keys its rsp-relative coordinates at the and's
			-- slack=0 convention, and a carrier whose PARK site it names keys the
			-- same way. Each such statement is also marked x86_postAnd for the
			-- rewrite (a pre-and-only rsp access is now legal — Feature 2 — while
			-- a statement BOTH reachable-from-an-and and able-to-reach-one still
			-- names two different real addresses for one key and stays rejected).
			do
				local work2 = {}
				for _, a in ipairs(andIdxs) do
					if not reachedFromAnd[a] then
						reachedFromAnd[a] = true; table.insert(work2, a)
					end
				end
				while #work2 > 0 do
					local j = table.remove(work2)
					for _, s in successors(j) do
						if not reachedFromAnd[s] then
							reachedFromAnd[s] = true; table.insert(work2, s)
						end
					end
				end
				for j, st2 in body do
					if st2.kind == "insn" and reachedFromAnd[j] and not isAnd[j] then
						st2.x86_postAnd = true
					end
				end
			end
				-- Governing-`and` dataflow: for every instruction, the depth of the
				-- `and $-N, %rsp` that governs its rsp alignment — the most recent
				-- and on EVERY path reaching it — or "none" (no and yet on any
				-- path), or "dyn" (paths disagree, or an absolute rsp restore
				-- (pop/teardown/recover) intervenes and the slack story is no
				-- longer that and's). Consumed by the FP-cluster keying for
				-- B2-clone statements: a clone's own `and` is NOT the jumper's
				-- prologue note (frameAndDepth), but its slack story is identical,
				-- so post-and clone traffic must key at the clone's own and depth
				-- rather than the SysV base (the aesni-mb shared 4x/8x bodies: the
				-- 8x body is cloned into the 4x function and re-runs its own
				-- `and $-128, %rsp`; keying it SysV placed its vmovdqa counters
				-- spills at 8-mod-16 homes and they faulted at runtime).
				local gand = {}
				local function gandJoin(a, b)
					if a == nil then return b end
					if b == nil then return a end
					if a == b then return a end
					return "dyn"
				end
				local gwork, gqueued = {}, {}
				local gentry
				for i, st in body do
					if st.kind == "insn" then gentry = i break end
				end
				if gentry then
					-- the function entry sees no and yet; the worklist spreads from
					-- there across every edge (clone entries ride their jump edges
					-- like any other edge). Unreached statements are dead code.
					gand[gentry] = "none"
					table.insert(gwork, gentry)
					gqueued[gentry] = true
				end
				while #gwork > 0 do
					local i = table.remove(gwork)
					local st = body[i]
					local out
					if st.kind ~= "insn" then
						out = gand[i]
					elseif isAnd[i] then
						out = depthIn[i]
					elseif depthIn[i] == "dyn" then
						-- already at an unprovable depth: the and story is moot
						-- (table depths are {anchored}/{dynreg} scopes — provable
						-- contexts that carry the and story like numbers do)
						out = "dyn"
					else
						local bm = x86.baseMnemonic(string.lower(st.mnemonic))
						local spDst = st.operands[1] and st.operands[1].rc == "sp"
						if bm == "pop" or bm == "ret" or bm == "leave"
							or (bm == "add" and spDst)
							or ((bm == "mov" or bm == "lea") and spDst) then
							-- rsp moves back UP (epilogue pop, teardown add, absolute
							-- rsp restore through a carrier or the frame pointer): the
							-- and's N-alignment no longer describes rsp, so later
							-- traffic loses the and-keyed residue identity
							out = "dyn"
						else
							-- everything else propagates unchanged: in particular the
							-- frame-setup `subq $N, %rsp` between a clone's and and its
							-- body traffic (the dec8x `and $-256` then `sub $192` shape)
							-- goes DEEPER and keeps rsp N-aligned minus a 16-multiple,
							-- so the and-keyed residue identity survives
							out = gand[i]
						end
					end
					if out ~= nil then
						for _, s2 in successors(i) do
							local merged = gandJoin(gand[s2], out)
							if merged ~= gand[s2] then
								gand[s2] = merged
								if not gqueued[s2] then gqueued[s2] = true; table.insert(gwork, s2) end
							end
						end
					end
				end
				andDomOf = gand
			-- Alias-carrier accesses CROSSING a mid-function `and` cannot be proven
			-- safe: pre- and post-and traffic share one numbering, but
			-- each and shifts rsp by a dynamic slack (0..N-1), so the same key
			-- names different addresses on the two sides. Only a carrier saved
			-- before an and and read at/after it (or saves on mixed sides
			-- reaching one use, whose merged save position is the earliest)
			-- would resolve to one key for two addresses. Saves and uses on
			-- the SAME side stay legal: both-before shares pre-and numbering
			-- with no slack between them, and both-after the nearest preceding
			-- and share post-and numbering with the same slack. With multiple
			-- mids, a save between and#1 and and#2 used after and#2 crosses
			-- and#2's slack, so each use must be checked against its nearest
			-- preceding and, not just the first. Keying the sides separately is
			-- not modeled, so reject the crossing shape only. (Accesses
			-- strictly before the first and stay legal, as do saves after an
			-- and used after it with no further and between: save the stack
			-- pointer after the and and address the slot through it, or address
			-- the slot directly. Prologue `and` notes hold no slot traffic by
			-- construction and never trigger this. Epilogue restores through an
			-- ENTRY-derived carrier (`mov K(%carrier),%reg` reloading a pushed
			-- save, marked x86_dropRestore) are exempt, and ONLY those: they
			-- restore a register web and are dropped, never virtualizing a slot
			-- web, and their save slot is at an absolute entry-relative address
			-- the and slack cannot shift (the save-stack depth match proves it
			-- is that save). The exemption is deliberately narrow — a live
			-- carrier holding the slack-free entry rsp itself (parked at the
			-- entry depth on a path reached from no mid-function and, per the
			-- reachedFromAnd walk above) with a save-depth match re-verified
			-- here. A restore through a carrier parked where an and's slack
			-- was already baked into rsp (reachable from an and), or any other
			-- dropRestore without a live entry carrier, still fails below: the
			-- same key would name different addresses on the two sides. (Body
			-- order alone cannot decide this: a park textually after one and
			-- may sit on a dispatch-exclusive path that never executes it —
			-- the aesni-mb 4x/8x sharing — while a park textually before an
			-- and may still be reached from it around a loop.)
			local function dropRestoreCrossingExempt(i, st)
				if not st.x86_dropRestore then return false end
				local mem = nil
				for _, o in (st.operands or {}) do if o.kind == "mem" then mem = o end end
				if not (mem and mem.base and mem.base.kind == "reg") then return false end
				local e = smapIn[i] and smapIn[i][mem.base.name]
				if not (e and not e.redefined and not e.slot and not e.roff and not e.conflict) then
					return false
				end
				-- Entry-derived: the parked value must be the entry rsp itself
				-- (depth 0 — not a deeper frame address), parked on a path
				-- reached from no mid-function and (so no and slack is baked
				-- into it).
				if type(e.depth) ~= "number" or e.depth ~= 0 then return false end
				local parkIdx = e.saveIdx or st.x86_aliasSaveIdx or 0
				if type(parkIdx) ~= "number" or parkIdx == 0 or reachedFromAnd[parkIdx] then return false end
				-- Save-depth match, re-verified against the incoming save stack
				-- (mirrors restoreLoadOf): the carrier depth minus the
				-- displacement must name exactly the restored register's save.
				local sv = savesIn[i]
				if not (sv and sv ~= "dyn") then return false end
				local want = e.depth - (mem.dispVal or 0)
				for _, s in sv do
					if s.depth == want then
						return s.name == st.operands[1].name
					end
				end
				return false
			end
			for i, st in body do
				if st.kind == "insn" and st.x86_aliasOff ~= nil and not st.x86_preAnd
					and not isAnd[i] and not dropRestoreCrossingExempt(i, st) then
					-- Unknown save position (nil) fails closed: treat as before.
					local sIdx = st.x86_aliasSaveIdx or 0
					-- Nearest preceding and to this use (nil when the use is
					-- before the first and; such uses are preAnd-marked and
					-- skipped above, but handle it for completeness).
					local prevAnd = nil
					for _, a in ipairs(andIdxs) do
						if a < i then prevAnd = a else break end
					end
					local crosses
					if prevAnd == nil then
						crosses = not (sIdx < firstAnd)
					else
						-- Legal only if the save sits after the nearest
						-- preceding and (same side, no and between save and use).
						crosses = not (sIdx > prevAnd)
					end
					if crosses then
						error("sarcasm: stack access through a saved stack-pointer register"
							.. " crossing a mid-function `and $-N, %rsp` cannot be proven safe"
							.. " (the save was before the and but the use is after it, and"
							.. " pre- and post-and traffic share one numbering while the and"
							.. " shifts rsp by a dynamic slack, so the same slot key names"
							.. " different addresses; legal shapes keep save and use on the"
							.. " same side — both before the and, or save after the and and"
							.. " use after it — or address the slot directly): " .. (st.raw or st.mnemonic))
					end
				end
			end
		end
	end
	-- Did the park site at body index `saveIdx` execute after the frame's
	-- alignment `and` (so the parked value carries the and's dynamic slack
	-- and keys the and's slack=0 coordinate system)? A MID-FUNCTION and
	-- answers through the reachedFromAnd walk above (the park site is
	-- post-and iff it is reachable from one). A PROLOGUE and (frameAndIdx)
	-- has no mid-function walk: the prefix is straight-line, so every
	-- statement after the and's position parks the rounded rsp — and every
	-- body statement does (the prefix holds no slot traffic). With both
	-- kinds present the mid-function walk stays authoritative (its ands are
	-- the deeper rounding; a prefix-and park between them keeps its own
	-- slack story, which the walk's reachability already expresses).
	local anyMidAnd = false
	for _, st0 in body do
		if st0.kind == "insn" and st0.x86_midAnd then anyMidAnd = true break end
	end
	local function parkPostOf(saveIdx)
		if saveIdx == nil then return false end
		if reachedFromAnd[saveIdx] then return true end
		-- A park textually after the prologue `and` note executes after it on
		-- every path (the prologue prefix is straight-line from entry), so its
		-- parked value is the rounded rsp — and-keyed — even when OTHER paths
		-- of the merged body carry mid-function ands (the sha1-mb shared
		-- bodies: the scalar body's carrier parks after the prologue note
		-- while the cloned bodies re-run their own mid-function ands). A park
		-- reachable from a mid-function and was already classified post by the
		-- reachedFromAnd walk above, which covers exactly those.
		if frameAndIdx ~= nil and saveIdx > frameAndIdx then
			return true
		end
		return false
	end

	-- Clone subframe balance: a localcall clone's `sub $N,%rsp` scratch
	-- frame (the x86_midSp inner-frame shape) is dropped like the
	-- top-level frame, so the real rsp never moves inside the clone — but
	-- the clone's slot accesses key off the perturbed depth. Every
	-- localRet resumes its continuation at the CALLER's (callsite's)
	-- depth, so a clone that returns with rsp still adjusted would leave
	-- the continuation addressing the wrong slots. The top-level ret
	-- depth-0 gate does not cover localRets (they are exempt — they are
	-- not returns). Require every localRet to sit at exactly its clone
	-- entry's depth; anything else (a `sub` without a matching `add`/`lea`
	-- teardown on some path, or an unprovable "dyn" depth) is a static
	-- error.
	do
		-- Direct-carrier-to-entry-rsp hatch (poly1305 avx512->avx2 shared tail):
		-- both bodies park the entry rsp once (`leaq -8(%rsp),%r11`) and the shared
		-- tail recovers it directly (`leaq 8(%r11),%rsp; vzeroupper; ret`) with no
		-- pushes/pops anywhere. The ret sits at depth 0 while its clone entry sits
		-- at the jump-site depth (320), so the depth-equality branch rejects it, and
		-- there is no verified teardown span (the recovery is a carrier lea, not a
		-- constant add plus paired pops), so the depth-0 hatch rejects it too. The
		-- shape is still sound: with no pushes outstanding there are no saves to
		-- restore (shape + identity are vacuous) and depth 0 is completeness, while
		-- the carrier discipline proves the recovery revives exactly the entry rsp
		-- (the jumper's carrier stays live and unredefined to the ret, and every
		-- frame access was already bounds-proven against the frame). Accept exactly
		-- that shape, fail closed on everything else:
		--   * ret at numeric 0, entry at numeric non-zero (otherwise an earlier
		--     branch already decides);
		--   * save stacks empty at BOTH entry and ret (no pushes to restore, and no
		--     abandonment via recovery truncation);
		--   * NO push/pop anywhere inside this clone and no dyn/non-empty saves at
		--     any clone point (a push abandoned by the recovery would look empty at
		--     the ret but is a lost save);
		--   * a PROVEN carrier recovery to exactly 0 inside this clone (an
		--     x86_spRecover mov/lea whose smap-computed revived depth is 0);
		--   * that same save still live (unredefined, non-slot, non-roff,
		--     non-conflict) at the ret.
		-- In particular a teardown-span epilogue with extra traffic (see
		-- sarcasm-reject-b2-epi-traffic-att) has no carrier and stays rejected, an
		-- imbalanced sub without teardown (sarcasm-reject-b2-imbal-att) never sits
		-- at 0, and any push-carrying clone fails the empty checks.
		local function carrierToZeroOk(retIdx, entryIdx, retSt)
			local dR = depthIn[retIdx]
			local dE = entryIdx and depthIn[entryIdx] or nil
			if type(dR) ~= "number" or dR ~= 0 then return false end
			if type(dE) ~= "number" or dE == 0 then return false end
			local svR = savesIn[retIdx]
			local svE = entryIdx and savesIn[entryIdx] or nil
			if type(svR) ~= "table" or type(svE) ~= "table" then return false end
			if #svR ~= 0 or #svE ~= 0 then return false end
			local cidx = retSt.b2clone
			if type(cidx) ~= "number" then return false end
			for j, cst in body do
				if cst.b2clone == cidx then
					local sv = savesIn[j]
					if sv == nil then
						-- unreachable clone point: dead code needs no proof
					elseif sv == "dyn" then
						return false
					elseif type(sv) == "table" then
						if #sv ~= 0 then return false end
					else
						return false
					end
					if cst.kind == "insn" then
						local b = x86.baseMnemonic(string.lower(cst.mnemonic))
						if b == "push" or b == "pop" then return false end
					end
				end
			end
			local recoverSaveIdx = nil
			for j, cst in body do
				if cst.b2clone == cidx and cst.x86_spRecover and cst.kind == "insn" then
					local ops = cst.operands
					local b = x86.baseMnemonic(string.lower(cst.mnemonic))
					local sm = smapIn[j]
					if type(sm) == "table" then
						if b == "mov" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "reg" then
							local ee = sm[ops[2].name]
							if ee and not ee.redefined and not ee.slot and not ee.roff and not ee.conflict
								and type(ee.depth) == "number" and ee.depth == 0 and ee.saveIdx ~= nil then
								recoverSaveIdx = ee.saveIdx
								break
							end
						elseif b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
							and ops[2].base and ops[2].base.kind == "reg" and not ops[2].index and not ops[2].dispSym then
							local ee = sm[ops[2].base.name]
							if ee and not ee.redefined and not ee.slot and not ee.roff and not ee.conflict
								and type(ee.depth) == "number" and ee.saveIdx ~= nil then
								local rd = ee.depth - (ops[2].dispVal or 0)
								if rd == 0 then
									recoverSaveIdx = ee.saveIdx
									break
								end
							end
						end
					end
				end
			end
			if recoverSaveIdx == nil then return false end
			local smR = smapIn[retIdx]
			if type(smR) ~= "table" then return false end
			for _, ee in pairs(smR) do
				if not ee.slot and not ee.roff and not ee.conflict and not ee.redefined
					and ee.saveIdx == recoverSaveIdx and type(ee.depth) == "number" then
					return true
				end
			end
			return false
		end
		local function depthEq(a, b)
			if a == b then return true end
			if type(a) ~= type(b) then return false end
			if type(a) == "table" then
				return a.anchored == b.anchored and a.dynreg == b.dynreg
					and a.scopeDepth == b.scopeDepth
			end
			return false
		end
		-- CFG reachability for the fail-closed nil check below (independent of
		-- depths: depthIn nil means UNREACHED by the fixpoint invariant, but a
		-- CFG-reachable ret with nil depth would indicate an analysis gap that
		-- must fail closed, not silently pass).
		local reachable = {}
		do
			local firstInsn = nil
			for idx, s in body do
				if s.kind == "insn" then firstInsn = idx break end
			end
			if firstInsn then
				reachable[firstInsn] = true
				local work = { firstInsn }
				while #work > 0 do
					local cur = table.remove(work)
					for _, s in successors(cur) do
						if not reachable[s] then reachable[s] = true table.insert(work, s) end
					end
				end
			end
		end
		for i, st in body do
			-- The cloneRet check is gated on `regions ~= nil`, a vestigial gate:
			-- every analyzeFrame caller (transform.luau's frame-size pre-pass,
			-- the driver's resolution pre-pass, and frame.preprocess's rewrite
			-- run) passes a non-nil regions list — the same always-empty list,
			-- there being no region sources left — so the gate holds on every
			-- run and the check is never skipped. The transform's early run and
			-- the preprocess run even see identical inputs (same body, same
			-- signature, same empty list), so they decide identically. The
			-- rewrite's ret-perturbation exemption (frame.preprocess) consumes
			-- the marks of the run preprocess itself drives, whose proof
			-- therefore backs the exemption; the early run has no rewrite
			-- consumer and its marks are discarded, but running the check there
			-- is pure redundancy, never a rejection hazard: the early run
			-- records the same caller-saved rsp saves (the historical region
			-- corner that withheld them needed the regions), so a B2 clone
			-- carrying its owner's carrier-recovery epilogue (mont's %rax chain,
			-- chacha's %r9 chain: the save lives in the jumper header ahead of
			-- the jump edge) is provable on every run. Runs stay independent:
			-- analyzeFrame clears every x86_* mark at entry, so no
			-- per-statement output (depths, teardown spans, bands) survives into
			-- a later run — each run re-derives them from scratch — and the
			-- only early-run scalar any consumer keeps (the prologue-derived
			-- frameSize/fpOffset/usesFp) is region-invariant by construction
			-- (the prologue scan never consults regions). Double coverage:
			-- every real imbalance is caught by both the transform's early run
			-- and the preprocess run, and both run before any rewrite consumes
			-- a cloneRet mark. localRet keeps its historical unconditional
			-- check (its teardowns are carrier-free constant adjustments,
			-- provable on every run).
			if st.kind == "insn" and (st.localRet or (st.cloneRet and regions ~= nil)) then
				local isClone = st.cloneRet ~= nil
				local mark = st.localRet or st.cloneRet
				local e = mark.entry and labelAt[mark.entry] or nil
				local dRet, dEntry = depthIn[i], (e and depthIn[e] or nil)
				if dRet == nil then
					-- Unreached ret (dead code) needs no balance proof; a
					-- CFG-reachable ret with no depth is an analysis gap: fail closed.
					if reachable[i] then
						error(isClone
							and ("sarcasm: shared-tail clone returns with an unprovable stack-pointer"
								.. " depth (no depth could be proven for the `ret` itself): " .. (st.raw or st.mnemonic))
							or ("sarcasm: local subroutine returns with an unprovable stack-pointer"
								.. " depth (no depth could be proven for the `ret` itself): " .. (st.raw or st.mnemonic)))
					else
						continue
					end
				end
				if dEntry == nil then
					-- The ret is reached (dRet is set) but its clone entry has no
					-- depth: the entry is unreached, missing, or unprovable, so
					-- balance cannot be proven. Fail closed (dead rets already
					-- continued above).
					error(isClone
						and ("sarcasm: shared-tail clone returns with an unprovable clone-entry"
							.. " depth (cannot prove the clone's inner frame was torn"
							.. " down before `ret`): " .. (st.raw or st.mnemonic))
						or ("sarcasm: local subroutine returns with an unprovable clone-entry"
							.. " depth (cannot prove the subroutine's scratch frame was torn"
							.. " down before `ret`): " .. (st.raw or st.mnemonic)))
				end
				if isDynDepth(dRet) or isDynDepth(dEntry) or
					-- A clone may also return at the entry rsp (depth 0) when its
					-- entry depth differs — but only when the ret sits inside a
					-- VERIFIED epilogue teardown span (st.x86_teardown): the span
					-- proof covers exactly the restoring operations (one constant
					-- rsp adjustment plus paired callee-saved pops and
					-- frame-touch-free computation leading straight to this ret),
					-- so the pushed registers are provably restored and the
					-- ordinary return path owns the frame from here. A bare
					-- depth-0 hatch proves no restores (an arbitrary path that
					-- merely lands on 0 — a carrier recovery, an unverified
					-- adjustment — may abandon the pushed saves), so without the
					-- span mark the imbalance stays a static error.
					-- WHY the localcall span proof transfers to B2 epilogues: the
					-- span validator (validateTeardown) guarantees SHAPE — a straight
					-- run of restoring ops with no targeted labels, no calls, no
					-- stack-touching traffic (extra traffic breaks the span: see
					-- sarcasm-reject-b2-epi-traffic-att) — while the pop-pairing
					-- analysis guarantees IDENTITY at every KNOWN depth (a pop whose
					-- register/depth mismatches the outstanding save is
					-- x86_unpairedPop and rejected by the rewrite), and dRet == 0
					-- guarantees COMPLETENESS (every push accounted for). Shape +
					-- identity + completeness is exactly an epilogue, whether the
					-- clone returns to its callsite depth (localcall) or to the
					-- entry rsp (this hatch). A DIRECT carrier recovery to the entry
					-- rsp with no pushes outstanding (carrierToZeroOk above: the
					-- poly1305 avx512->avx2 `leaq 8(%r11),%rsp` shared tail) carries
					-- no saves to restore, so the span's shape+identity proof is
					-- vacuous and depth 0 alone is completeness — accepted without
					-- the span mark.
					(not depthEq(dRet, dEntry) and not (isClone and dRet == 0 and st.x86_teardown)
						and not (isClone and carrierToZeroOk(i, e, st))) then
					if isDynDepth(dRet) or isDynDepth(dEntry) then
						error(isClone
							and ("sarcasm: shared-tail clone returns with an unknown stack-pointer"
								.. " depth (cannot prove the clone's inner frame was torn"
								.. " down before `ret`): " .. (st.raw or st.mnemonic))
							or ("sarcasm: local subroutine returns with an unknown stack-pointer"
								.. " depth (cannot prove the subroutine's scratch frame was torn"
								.. " down before `ret`): " .. (st.raw or st.mnemonic)))
					else
						error(isClone
							and ("sarcasm: shared-tail clone returns with an unbalanced stack frame"
								.. " (the clone must undo every `sub $N,%rsp` on every path before"
								.. " `ret` — back to its clone-entry depth, or all the way to the"
								.. " entry rsp inside a verified epilogue teardown span (one constant"
								.. " `add` plus paired callee-saved pops leading straight to `ret`)"
								.. " when it carries its owner's whole epilogue): "
								.. (st.raw or st.mnemonic))
							or ("sarcasm: local subroutine returns with an unbalanced stack frame"
								.. " (a `sub $N,%rsp` scratch frame inside the subroutine must be"
								.. " torn down with a matching `add $N,%rsp` on every path before"
								.. " `ret`; the continuation resumes at the caller's depth): "
								.. (st.raw or st.mnemonic)))
					end
				end
			end
		end
	end

	-- Epilogue movq-restore loads (the perlasm shape: `movq (%rsp), %r15; movq
	-- 8(%rsp), %r14; ...; addq $K, %rsp; ret`). The save-slot model rewrites a
	-- load from a pushed register's save slot to a read of that register's web
	-- — but when the register was REDEFINED after the push (perlasm uses the
	-- saved registers as scratch inside the body), that web holds the redefined
	-- value while the hardware slot holds the pre-push one, so the model cannot
	-- express the load and the rewrite rejects it. Yet the load's result is dead
	-- weight whenever the register is never read afterwards (the standard
	-- epilogue reads nothing but the return registers before `ret`) — exactly
	-- the situation checkLostReload proves for pops. Detect those loads here
	-- (EXACTLY the rewrite's save-slot alias computation), prove unobservability
	-- with the same forward walk, and mark them x86_dropRestore: the rewrite
	-- then drops them like the pops it already drops. A restore into the
	-- integer return register can never be proven unobservable (the frame
	-- analysis does not model `ret`'s operand read), so it stays rejected.
	for i, st in body do
		if st.kind ~= "insn" then continue end
		local ops = st.operands
		local b = x86.baseMnemonic(string.lower(st.mnemonic))
		if b ~= "mov" or not (ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr") then continue end
		local mem = ops[2]
		if not (mem and mem.kind == "mem" and not mem.index and not mem.dispSym) then continue end
		if not (mem.base and (mem.base.rc == "sp" or (usesRbp and mem.base.name == "rbp"))) then continue end
		if gprStackWidth(string.lower(st.mnemonic), mem) ~= 8 then continue end
		local sv = st.x86_saves
		if not (sv and sv ~= "dyn" and #sv > 0) then continue end
		local off = (mem.dispVal or 0)
		if mem.base.rc == "sp" then
			-- EXACTLY the rewrite's normalization: at a KNOWN perturbed depth (the
			-- early-ret-then-frame shape) the slot sits at disp + D0 - d in frame
			-- coordinates. At an UNKNOWN ("dyn") rsp depth the raw offset is NOT
			-- the slot's frame coordinate — the perturbed rsp points who-knows-where
			-- (an alloca region, a disagreeing merge) — so the load can never be a
			-- proven restore-load here. (The perlasm epilogue-restore shape this
			-- pass exists for runs at a STATIC depth.)
			local d = st.x86_depth
			if d == nil or isDynDepth(d) then continue end
			off = off + D0 - d
		else
			off = rbpNorm + off
		end
		local hit = nil
		for _, s in sv do
			-- (spill slots are ordinary slots — never restore-load candidates)
			if not (usesRbp and s.name == "rbp") and not s.spill then
				if off == D0 - s.depth and s.redefined then hit = s break end
			end
		end
		if not hit then continue end
		if retIsGpr and ops[1].num == 0 then
			error("sarcasm: epilogue restore load into the integer return register"
				.. " cannot be modeled (the register was redefined after its save,"
				.. " so the restore's value and the web's value disagree): "
				.. (st.raw or st.mnemonic))
		end
		checkLostReload(i, { name = ops[1].name, num = ops[1].num },
			"epilogue restore load of a save whose register was redefined after the"
				.. " push (the load cannot be modeled — the slot holds the pre-push"
				.. " value while the register's web holds the redefined one — and"
				.. " this read would observe the web's value)")
		st.x86_dropRestore = true
	end

	-- B2 shared-tail clone coordinate bands (tailcall.luau st.b2clone = the
	-- per-function clone index): a clone executes at the jump-site depth and
	-- carries its owner's own frame setup (`sub`, `and $-N`) and teardown, so
	-- its rsp-relative accesses keyed off the perturbed depth (disp + D0 - d)
	-- would numerically overlap the jumper's own frame slots — same offset,
	-- different addresses on mutually-exclusive paths. Sharing one coordinate
	-- would merge their webs (a clone scratch def kills the jumper slot's
	-- pointer-ness and vice versa, and FP-taint materializes them into one
	-- home). Each clone therefore gets a PRIVATE band: bandedOff =
	-- unbiasedOff + bandBase[idx], which preserves the address<->offset
	-- equivalence exactly (same unbiased offset means same address) while
	-- separating clones from the jumper and each other. Band bases are
	-- multiples of 64, so the taint scan's mod-areq alignment residues are
	-- unaffected: the scan fail-closeds every access whose areq does not
	-- divide 64 (every areq the knowledge module produces — 1, 16, 32, 64 —
	-- does), and a multiple-of-64 base preserves every such residue exactly.
	-- Bases lay out cumulatively after
	-- the prologue frame (each band starts 128 past the previous band's
	-- unbiased extent), so no band overlaps the jumper frame or another band
	-- by construction. MEASURED (pushq %rbx + subq $64 prologue, frameSize 64):
	-- a clone with an inner `sub $64` (unbiased extent [-64,-64]) gets base 256
	-- and frameSize grows 64 -> 200 (delta +136 = the 128 separation + the 8-byte
	-- access width); an inner `sub $128` (extent [-128,-128]) gets base 320 and
	-- maps to the SAME banded extent [192,200) — the base shifts by exactly the
	-- extent shift, so inner-frame depth never changes the backing size. Two such
	-- bands lay out at bases 320, 384 with mapped extents [192,200), [320,328)
	-- (the second band's lo maps exactly to the first band's mapped hi + 128)
	-- and frameSize 328. Only plain rsp-relative accesses band; rbp-relative
	-- accesses address the shared frame pointer, carrier (x86_aliasOff)
	-- accesses resolve absolutely, region members ride region coordinates,
	-- and indexed/symbolic operands are rejected shapes — all keep their
	-- existing keying (such traffic in clones is fail-closed by the
	-- pre-existing rejections when it cannot prove safe).
	local b2base = {}
	do
		local extents = {}  -- idx -> { lo, hi } unbiased-off range
		for _, st in body do
			local idx = st.b2clone
			if st.kind == "insn" and type(idx) == "number" and not st.x86_prologue then
				local d = st.x86_depth
				if d ~= nil and not isDynDepth(d) then
					-- A clone's carrier access whose carrier was parked INSIDE clone
					-- code addresses the clone's PRIVATE frame (the same real bytes
					-- its rsp-relative spellings address) and is banded exactly like
					-- them — so its (unbanded, canonical) extent must join the band's
					-- extent or two bands could overlap. A carrier parked in the
					-- JUMPER resolves to true absolute addresses (the jumper's own
					-- frame) and keeps unbanded keying (see the banding pass below).
					local aliasBand = st.x86_aliasOff ~= nil and not st.x86_dropRestore
						and not st.x86_regionCarrier and st.x86_aliasSaveIdx ~= nil
						and body[st.x86_aliasSaveIdx] ~= nil
						and body[st.x86_aliasSaveIdx].b2clone ~= nil
					for _, o in (st.operands or {}) do
						if o.kind == "mem" and o.base and o.base.kind == "reg"
							and not o.index and not o.dispSym
							and (o.base.rc == "sp" or aliasBand)
							and (aliasBand or not regionOf((o.dispVal or 0), false)) then
							local off
							if aliasBand then
								off = st.x86_aliasOff
							else
								off = (o.dispVal or 0)
								if d ~= D0 then off = off + D0 - d end
							end
							local e = extents[idx]
							if not e then extents[idx] = { lo = off, hi = off }
							else
								if off < e.lo then e.lo = off end
								if off > e.hi then e.hi = off end
							end
						end
					end
				end
			end
		end
		local order = {}
		for idx in extents do table.insert(order, idx) end
		table.sort(order)
		local cursor = frameSize + 128
		for _, idx in order do
			local e = extents[idx]
			-- multiple of 64 (alignment residues preserved); at or past the
			-- cursor, so the band clears every earlier band and the frame.
			local base = cursor - e.lo
			base = math.ceil(base / 64) * 64
			b2base[idx] = base
			cursor = base + e.hi + 128
		end
	end
	-- Band base for a B2-clone statement (0 for anything else).
	local function b2band(st)
		local b = st.b2clone
		if type(b) == "number" then return b2base[b] or 0 end
		return 0
	end

	-- Publish bands as rewrite-visible marks (rewrite is separate).
	for _, st in body do
		if st.kind == "insn" and type(st.b2clone) == "number" then
			st.x86_b2band = b2base[st.b2clone] or 0
		end
	end
	-- Band the clone's own carrier accesses (see the extents comment above): a
	-- carrier parked inside clone code addresses the clone's PRIVATE frame, so
	-- its accesses must land in the same banded homes the clone's rsp-relative
	-- spellings of the same bytes use — otherwise one real byte would map to two
	-- modeled homes (and the clone's scratch would share webs with the jumper's
	-- same-o slots, the exact aliasing the bands exist to separate). A carrier
	-- parked in the JUMPER (b2clone == nil park site) holds a true absolute
	-- address and keeps unbanded keying; dropped traffic (restore loads, region
	-- redirects) never reaches a slot home and is not banded either.
	for _, st in body do
		if st.kind == "insn" and type(st.b2clone) == "number"
			and st.x86_aliasOff ~= nil and not st.x86_dropRestore
			and not st.x86_regionCarrier and st.x86_aliasSaveIdx ~= nil
			and body[st.x86_aliasSaveIdx] ~= nil
			and body[st.x86_aliasSaveIdx].b2clone ~= nil then
			st.x86_aliasOff += b2base[st.b2clone] or 0
		end
	end

	-- Feature (post-`and` frame extent): a prologue `and $-N, %rsp` is DROPPED
	-- from the output (the synthesized frame is emitted N-aligned in its
	-- place), so the post-and traffic's keyed offsets ARE the frame's
	-- coordinates — and the input's `sub` may size the frame SHORTER than the
	-- post-and traffic reaches: the gas path relies on the and's dynamic
	-- rounding-down slack (the OpenSSL poly1305 avx512 body's table spills
	-- reach 0x140 above the post-and rsp while its `sub` is 0x128). That slack
	-- is unobservable in the output — the and is dropped, every post-and
	-- access shifts as a unit, and the and-keyed alignment residues are
	-- preserved by the N-aligned synthesized frame — so cover the traffic:
	-- extend the frame to the widest post-and access extent. Pre-and traffic
	-- keys TRUE entry-relative coordinates and cannot name these bytes (the
	-- pre-/post-and intersection guard rejects any overlap), and clone traffic
	-- rides its own band extents, so nothing else shares them. Runs before the
	-- stack-buffer scan so a buffer declared over post-and traffic is checked
	-- against the covered extent.
	if frameAndDepth ~= 0 then
		for _, st in body do
			if st.kind == "insn" and not st.x86_prologue and not st.localClone
				and not st.b2clone and not st.x86_preAnd
				and type(st.x86_depth) == "number" and st.x86_depth >= frameAndDepth then
				local mem
				for _, o in (st.operands or {}) do if o.kind == "mem" then mem = o end end
				if mem and mem.base and mem.base.kind == "reg" and mem.base.rc == "sp"
					and not mem.index and not mem.dispSym
					and x86.baseMnemonic(string.lower(st.mnemonic)) ~= "lea" then
					local m = string.lower(st.mnemonic)
					local fpe = fp.lookup(m)
					local w = nil
					if isFpInsn(fpe, st) then
						local okW, ww = pcall(fp.accessWidth, m, st.operands, st.raw)
						w = okW and ww or nil
					else
						w = gprStackWidth(m, mem)
					end
					if w then
						local off = (mem.dispVal or 0) + D0 - st.x86_depth
						if off + w > frameSize then frameSize = off + w end
					end
				end
			end
		end
	end

	-- -----------------------------------------------------------------------
	-- STACK BUFFER scan, phase 1 (x86_64 `#! stack buffer (...)`): resolve the
	-- declarations, validate the ranges against this function's frame, and mark
	-- the accesses that must be lowered into the buffer region. The taint scan
	-- below skips buffer accesses (they are redirected, never clustered).
	--
	-- A buffer's declared range must lie inside the function's own carved frame
	-- but EXCLUDE:
	--   * the red zone (normalized offsets < 0) — conservative: a buffer in the
	--     red zone would be clobbered by any call's argument-register spills
	--     (a function with buffers is not necessarily a leaf);
	--   * the transient prologue push pad (the outstanding pushed-register save
	--     slots at [D0 - 8*maxSaves, D0)) — those slots' bytes are the pushed
	--     registers' webs while the pad is outstanding, and the save-slot model
	--     must not compete with the buffer redirect;
	--   * the outgoing call-argument area ([0, 8*maxStackArgs)) — the body's
	--     outgoing stack-argument stores key the callsite marshalling's slot
	--     webs; redirecting them into the buffer would feed the callee garbage;
	--   * any fixed `.alloca` region (region coordinates and buffer coordinates
	--     must not collide).
	-- Overlapping/touching declarations merge into one lowered GROUP (one
	-- region, union size) — "multiple stack buffers may overlap, in which case
	-- you get a single larger stack buffer that encompasses both".
	--
	-- The scan is gated on `regions ~= nil and not sbResolving`. The regions
	-- half is vestigial: every analyzeFrame run carries the same (always empty)
	-- regions list and records the same caller-saved carriers / lea-saves in the
	-- smap, so alias-base declarations resolve on every run. The live gate is
	-- the sbResolving half (see below).
	local sbRanges = {}    -- { {lo, hi, areq, aligns, id} } declarations in this function
	local sbGroups = nil   -- merged groups (phase (d1)); bases assigned in (d2)
	local sbOutgoing = 0   -- largest outgoing stack-argument area (bytes)
	local maxSaves = 0     -- largest outstanding push count anywhere
	local anyDynSaves = false
	-- Feature (stack buffers in localcall clones): per-clone declared ranges
	-- (clone id -> buffer id -> {lo, hi} in the clone's own keyed coordinates),
	-- collected from the clone's long forms, and the ids this body declares
	-- OUTSIDE any clone. A short form inside a clone resolves through the
	-- clone's own declaration (the subroutine's own frame, shifted by the
	-- clone's keying) or through a declaration in the enclosing function's own
	-- body (the caller's frame, which clone accesses key into directly); a
	-- buffer declared only in some third function is out of reach.
	local cloneDeclRanges = {}
	local ownDeclIds = {}
	sbLayout = nil
	-- (During the driver's resolution-only runs (sbResolving) the buffer scan is
	-- skipped entirely: the canonical ranges are not complete yet, so neither
	-- the consistency checks nor the frame-fit checks can run meaningfully —
	-- the driver resolves the long forms directly via resolveStackBufDeclAt.)
	if regions ~= nil and not sbResolving then
	do
		-- Outgoing stack-argument area: every annotated callsite's integer-class
		-- arguments beyond the 6 SysV register arguments travel in slots at
		-- [0, 8*count) in this function's frame.
		for _, st in body do
			if st.kind == "insn" and st.annotation and string.find(st.annotation, "%(") then
				local okcs, cs = pcall(sig.parseSignature, st.annotation)
				if okcs and cs then
					local intWords = 0
					for k = 1, cs.argWords do
						if not sig.isFpClass(cs.argTypes[k]) then intWords += 1 end
					end
					local stackWords = intWords - 6
					if stackWords > 0 and 8 * stackWords > sbOutgoing then
						sbOutgoing = 8 * stackWords
					end
				end
			end
			local sv = st.x86_saves
			if sv == "dyn" then
				anyDynSaves = true
			elseif sv then
				if #sv > maxSaves then maxSaves = #sv end
			end
		end
		local pushTop = D0 - 8 * maxSaves
		-- Feature (stack buffers in localcall clones): each clone's ENTRY depth
		-- (the callsite's model depth — the state flows through the localCall
		-- edge unchanged), keyed by clone id. A clone declaration's upper bound
		-- must stay inside the clone's own sub-frame (below the phantom return
		-- address word of the hardware call it was written against).
		local cloneEntryDepth = {}
		do
			for i, st in body do
				if st.localCloneEntry then
					-- The entry MARK sits on the clone's first statement (a
					-- label, which carries no localClone id or depth): the
					-- entry depth is the clone's first INSTRUCTION's depth (the
					-- state flows through the localCall edge unchanged, so the
					-- first instruction executes at the callsite's depth).
					for j = i, #body do
						local st2 = body[j]
						if st2.kind == "insn" then
							if st2.localClone and st2.x86_depth ~= nil
								and not isDynDepth(st2.x86_depth)
								and cloneEntryDepth[st2.localClone] == nil then
								cloneEntryDepth[st2.localClone] = st2.x86_depth
							end
							break
						end
					end
				end
			end
		end
		local function checkRangeFits(loN, hiN, homeLo, homeHi, id, dDecl, st)
			-- `loN`/`hiN` are the declaring statement's UNBANDED normalized
			-- range; `homeLo`/`homeHi` are the range in the coordinate space
			-- the buffer's home lives in — identical for ordinary statements
			-- and shifted by the statement's B2 band for shared-tail clones
			-- (a clone entered above the jumper's prologue builds its frame
			-- INSIDE the jumper's normalized frame territory on mutually
			-- exclusive paths, so its buffer must not be checked against — or
			-- merged with — the jumper's own slots in unbanded coordinates;
			-- banding moves its home into the clone's private band, exactly
			-- like every other clone access's).
			-- NOTE: there is deliberately NO `and $-N, %rsp` rejection here. A
			-- dynamically-aligned frame used to fail closed for all buffers
			-- ("the dynamic alignment slack breaks the buffer's alignment
			-- identity"), which blocked every OpenSSL-style shared-tail body
			-- that aligns its own frame. The alignment identity is now proven
			-- per access instead: every ALIGNED access contributes its own
			-- offset/requirement pair, and the group placement in phase (d2)
			-- proves the group's offsets are jointly congruent (one output
			-- base exists) and that the synthesized frame backs the widest
			-- requirement. Post-`and` traffic shifts as a unit, so its
			-- relative residues carry the whole story whatever dynamic slack
			-- the `and` baked into rsp.
			-- A buffer may reach BELOW the frame base only down to the declaring
			-- statement's own provably-allocated stack: at a provable rsp depth d
			-- the function's own pushes/subs have allocated every byte from
			-- entry_rsp - d upward, i.e. normalized offsets [D0 - d, ...). The
			-- aesni CBC decrypt shape (the prologue scan ends in the leading
			-- encrypt half, so the decrypt frame's push+sub key as a mid-function
			-- inner frame at normalized [-24, 0)) declares its 16-byte rep-movs
			-- temp exactly at [-24, -8) — the function's own allocated bytes,
			-- ending where the outstanding pushed-register slot begins. Bytes
			-- below that floor were never allocated on the declaring path (red
			-- zone at best) and stay rejected. For an ordinary frame (d == D0)
			-- the floor is 0 — the pre-existing no-red-zone rule. The same holds
			-- for a B2 clone declaration: the clone's own `sub`/`and` are
			-- ordinary statements of the augmented body, so the clone's
			-- declaring depth proves exactly its own allocated bytes.
			local frameFloor = 0
			if dDecl ~= nil and not isDynDepth(dDecl) then frameFloor = D0 - dDecl end
			-- Feature (stack buffers in localcall clones): a clone statement's
			-- model depth overstates the hardware allocation floor by the
			-- return-address word the subroutine was written against (the +8
			-- rule) — the clone's own `sub` allocates [entry_rsp - d - 8, ...)
			-- in hardware coordinates, so its own-frame floor sits 8 lower.
			if st and st.localClone and dDecl ~= nil and not isDynDepth(dDecl) then
				frameFloor = frameFloor - 8
			end
			if loN < frameFloor then
				error("sarcasm: stack buffer (" .. id .. ") reaches below the"
					.. " function's own allocated stack (normalized offset "
					.. tostring(loN) .. " < floor " .. tostring(frameFloor)
					.. (frameFloor == 0
						and "; buffers must live inside the function's own frame"
						or "; the declaring statement's rsp depth " .. tostring(dDecl)
							.. " only proves the bytes at or above " .. tostring(frameFloor))
					.. "): " .. tostring(loN) .. " < " .. tostring(frameFloor))
			end
			if homeLo < D0 and homeHi > pushTop then
				-- Feature (post-`and` frame extent): with an `and $-N, %rsp`
				-- note and NO outstanding pushed-register saves anywhere (an
				-- empty push pad), the post-and traffic keys above the pre-and
				-- frame base — the dropped and's slack is unobservable in the
				-- output (every post-and access shifts as a unit and the
				-- and-keyed alignment residues are preserved by the N-aligned
				-- synthesized frame) — so a buffer may reach up to the grown
				-- frame extent: the bytes it names there are the post-and
				-- traffic's own homes, not the caller's frame. With a non-empty
				-- pad the old rule stands (a buffer crossing the frame base
				-- would overlap the pad's save slots).
				local andTop = (frameAndDepth ~= 0 and maxSaves == 0)
					and frameSize or nil
				if not (andTop ~= nil and homeHi <= andTop) then
					error("sarcasm: stack buffer (" .. id .. ") extends to normalized"
						.. " offset " .. tostring(homeHi) .. ", past the frame's usable top "
						.. tostring(pushTop) .. " (the frame is [0, " .. tostring(D0)
						.. ") and its top " .. tostring(8 * maxSaves) .. " bytes are the"
						.. " outstanding push/callee-save slots): " .. tostring(homeHi)
						.. " > " .. tostring(pushTop))
				end
			end
			-- Feature (stack buffers in localcall clones): a clone-declared
			-- buffer lives in the CLONE's own sub-frame — the bytes the clone's
			-- own `sub` allocated BELOW the phantom return-address word (the +8
			-- rule). Reaching above that word would name the CALLER's frame
			-- bytes (whose slots virtualize separately in every caller); a
			-- buffer with no visible clone entry depth is likewise rejected
			-- (its extent cannot be proven).
			if st and st.localClone then
				local ed = cloneEntryDepth[st.localClone]
				local cloneTop = (ed ~= nil) and (D0 - ed - 8) or nil
				if cloneTop == nil or hiN > cloneTop then
					error("sarcasm: stack buffer (" .. id .. ") is declared inside a"
						.. " local subroutine clone but reaches normalized offset "
						.. tostring(hiN) .. ", above the clone's own sub-frame (top "
						.. tostring(cloneTop) .. "); a clone-declared buffer must live"
						.. " in the subroutine's own frame below its caller's"
						.. " return-address word: " .. (st.raw or st.mnemonic))
				end
			end
			-- The push-pad model above covers the JUMPER's own pushes (their
			-- slots are exactly the pad [D0 - 8*maxSaves, D0)). A B2 clone's own
			-- pushes park their save slots BELOW the jumper's frame — at
			-- normalized [D0 - s.depth, D0 - s.depth + 8) for each outstanding
			-- save (`s.depth` is the post-push rsp depth, matching the save-slot alias
			-- model) — where the pad model cannot see them. A buffer overlapping
			-- such a slot would split the slot's bytes between the save-slot
			-- web model (the rewrite's alias path) and the buffer region — two
			-- homes for one byte. Check the declaring statement's own
			-- outstanding saves exactly. (Saves pushed AFTER the declaration are
			-- deeper than it, so their slots land below the buffer's low bound
			-- and can never intersect it; checking the declaration's state is
			-- therefore sufficient.) Run for non-clone declarations too — it is
			-- a strictly more precise restatement of the pad rule for them.
			if st then
				local sv = st.x86_saves
				if sv == "dyn" then
					error("sarcasm: stack buffer (" .. id .. ") is declared at a"
						.. " statement whose outstanding-save state cannot be proven"
						.. " (the buffer's bytes must be provably disjoint from every"
						.. " pushed-register save slot): " .. (st.raw or st.mnemonic))
				elseif sv then
					for _, s in sv do
						if usesRbp and s.name == "rbp" then continue end  -- saved frame pointer
						if s.spill then continue end  -- a spill slot is an ordinary slot
						local slo, shi = D0 - s.depth, D0 - s.depth + 8
						if loN < shi and hiN > slo then
							error("sarcasm: stack buffer (" .. id .. ") overlaps a"
								.. " pushed register's save slot at normalized ["
								.. tostring(slo) .. ", " .. tostring(shi) .. ") (the"
								.. " slot's bytes are the pushed register's web while"
								.. " the push is outstanding; move the buffer below the"
								.. " push or pop it first): " .. (st.raw or st.mnemonic))
						end
					end
				end
			end
			-- Precise intersection with the outgoing call-argument area [0,
			-- sbOutgoing): a buffer entirely below the frame base (an inner-frame
			-- buffer, see the frameFloor comment) cannot overlap it.
			if homeLo < sbOutgoing and homeHi > 0 then
				error("sarcasm: stack buffer (" .. id .. ") overlaps the outgoing"
					.. " call-argument area [0, " .. tostring(sbOutgoing)
					.. "): " .. tostring(homeLo) .. " < " .. tostring(sbOutgoing))
			end
			for _, r in (regions or {}) do
				if not r.anchored and r.base < homeHi and r.base + r.size > homeLo then
					error("sarcasm: stack buffer (" .. id .. ") overlaps a fixed"
						.. " alloca region at normalized offsets ["
						.. tostring(math.max(r.base, homeLo)) .. ", "
						.. tostring(math.min(r.base + r.size, homeHi))
						.. ") (buffer region and alloca region coordinates must not"
						.. " collide)")
				end
			end
		end
		local function widthOf(st, mem)
			local m = string.lower(st.mnemonic)
			local fpe = fp.lookup(m)
			if isFpInsn(fpe, st) then
				local fw, fa = fp.accessWidth(m, st.operands, st.raw)
				if fw == nil then return nil, nil end
				return fw, fa
			end
			return gprStackWidth(m, mem) or mem.size, nil
		end
		-- Resolve the buffer range a SHORT form names for statement `st` (the
		-- canonical table, adjusted for localcall clones): inside a clone the
		-- subroutine's own declaration wins (the clone's own keyed coordinates,
		-- collected in phase (a)); otherwise a declaration in the enclosing
		-- function's own body or any other function's canonical range applies.
		-- A buffer declared only inside some OTHER subroutine is out of reach.
		local function resolveShortRangeFor(st)
			local sb = st.stackBufAnno
			local canon = sbCanon and sbCanon[sb.id] or nil
			if st.localClone then
				local cm = cloneDeclRanges[st.localClone]
				local r = cm and cm[sb.id] or nil
				if r then return r.lo, r.hi end
				if canon == nil then
					error("sarcasm: `stack buffer (" .. sb.id .. ")` short form has no long"
						.. " form anywhere in this file (declare the buffer with e.g."
						.. " `#! stack buffer (" .. sb.id .. ", %rsp, %rsp + N)` on a buffer"
						.. " access in some function): " .. (st.raw or st.mnemonic))
				end
				if not ownDeclIds[sb.id] then
					error("sarcasm: `stack buffer (" .. sb.id .. ")` is declared inside local"
						.. " subroutine '" .. tostring(canon.fn) .. "' (its bytes live in that"
						.. " subroutine's own frame); only that subroutine's cloned code"
						.. " addresses it - declare a buffer in this function with the"
						.. " long form instead: " .. (st.raw or st.mnemonic))
				end
				return canon.lo, canon.hi
			end
			if canon == nil then
				if sbResolving then return nil, nil end
				error("sarcasm: `stack buffer (" .. sb.id .. ")` short form has no long form"
					.. " anywhere in this file (declare the buffer with e.g. `#! stack buffer ("
					.. sb.id .. ", %rsp, %rsp + N)` on a buffer access in some function): "
					.. (st.raw or st.mnemonic))
			end
			if canon.sub then
				error("sarcasm: `stack buffer (" .. sb.id .. ")` is declared inside local"
					.. " subroutine '" .. tostring(canon.fn) .. "' (its bytes live in that"
					.. " subroutine's own frame); only that subroutine's cloned code"
					.. " addresses it - declare a buffer in this function with the"
					.. " long form instead: " .. (st.raw or st.mnemonic))
			end
			return canon.lo, canon.hi
		end
		-- (a) declarations FIRST (all of them, before any access is classified
		-- against the ranges — a declaration may appear after the accesses that
		-- fall inside it).
		for ia, st in body do
			if st.kind == "insn" and st.stackBufAnno
				and not (st.stackBufAnno.short and st.localClone) then
				-- A short form on a CLONE statement is skipped here: the
				-- canonical range lives in the SUBROUTINE's own frame
				-- coordinates, which this function's frame-fit check cannot
				-- see (the clone's own declaration carried the range in the
				-- clone's keyed coordinates, inserted above); the clone's
				-- short forms resolve per access through the clone's own
				-- declaration or the enclosing function's own.
				local loN, hiN = resolveBufRangeAt(st, st.x86_depth, st.x86_smap,
					st.x86_fp, st.x86_prologue == true)
				if loN then
					if string.lower(st.mnemonic) == "lea" then
						error("sarcasm: `stack buffer` annotation on a lea is not"
							.. " supported (a lea takes no memory access; annotate the"
							.. " accesses or the rep instead): " .. (st.raw or st.mnemonic))
					end
					-- The range the buffer's HOME lives in: unbanded for ordinary
					-- statements, shifted by the statement's B2 band for
					-- shared-tail clones (see checkRangeFits).
					local band = st.x86_b2band or 0
					checkRangeFits(loN, hiN, loN + band, hiN + band,
						st.stackBufAnno.id, st.x86_depth, st)
					table.insert(sbRanges, { lo = loN + band, hi = hiN + band,
						areq = 1, aligns = {}, id = st.stackBufAnno.id,
						-- Absolute-alignment identity bookkeeping (the xor-toggle
						-- feature; see the group identity derivation in (d2)):
						-- the declaring statement's index (for the governing-and
						-- dataflow) and the statement's band (to recover the
						-- declaration's UNBANDED — real-byte — range).
						declIdx = ia, band = band })
					-- Record the declaration for the short-form resolution:
					-- clone statements (the subroutine's own text) key per
					-- clone; the enclosing function's own declarations are the
					-- direct caller-frame ranges.
					if st.localClone then
						local cm = cloneDeclRanges[st.localClone]
						if not cm then
							cm = {}
							cloneDeclRanges[st.localClone] = cm
						end
						cm[st.stackBufAnno.id] = { lo = loN, hi = hiN }
					else
						ownDeclIds[st.stackBufAnno.id] = true
					end
				end
			end
		end
		-- (a0) buffer-interior VALUE leas (Feature A, by-address accesses): a
		-- `leaq K(%rsp), %reg` whose computed normalized offset lands inside a
		-- declared buffer's range and whose destination the carrier scan did NOT
		-- claim (a memory-only-use carrier keeps today's static path — the
		-- transfer only falls out of the carrier when a use shape it cannot
		-- legalize forced it to) defines an ordinary web holding the buffer BYTE
		-- ADDRESS. The rewrite emits it as `leaq disp'(%rsp), %reg` into the
		-- lowered buffer region (tagged stackBuf, finalized by the transform's
		-- buffer-base shift); annotated accesses through the web become
		-- runtime-checked by-address accesses. These leas are excluded from
		-- escape rejection (they name buffer bytes, not frame slots — the
		-- buffer's own lowered region is the lea's real home).
		-- B2 shared-tail clone statements are included (Feature A in clones,
		-- the aesni-mb dec8x shape): their lea offsets key into the clone's
		-- band exactly like every other clone access (+ the statement's band),
		-- so the banded offset lands inside the clone's own banded declaration
		-- ranges. The transfer's raw-displacement probe decided carrier-vs-web
		-- already; this pass re-checks the decision EXACTLY in the banded
		-- coordinates — a statement whose probe hit but whose banded offset
		-- misses every declared range stays an ordinary lea (the escape
		-- detector's business, as today).
		for _, st in body do
			if st.kind == "insn" and not st.x86_leaSave
				and not st.allocaPlace and not st.allocaOwned
				and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
				local ops2 = st.operands
				local d2 = st.x86_depth
				if not st.b2clone or st.x86StackBufLeaValueComputed then
				if ops2[1] and ops2[1].kind == "reg" and ops2[1].rc == "gpr"
					and (ops2[1].name ~= "rbp" or not usesRbp)
					and ops2[2] and ops2[2].kind == "mem" and ops2[2].base
					and ops2[2].base.rc == "sp" and not ops2[2].index
					and not ops2[2].dispSym and type(d2) == "number"
					and not isDynDepth(d2) and not st.x86_preAnd then
					local off2 = (ops2[2].dispVal or 0) + D0 - d2
						+ (st.localClone and -8 or (st.x86_b2band or 0))
					for _, r in sbRanges do
						if off2 >= r.lo and off2 < r.hi then
							st.x86StackBufLeaValue = { off = off2, id = r.id }
							break
						end
					end
				end
				end
			end
		end
		-- Groups holding a COMPUTED buffer-address web (the transfer's
		-- x86StackBufLeaValueComputed fall-out: the lea's uses are not all
		-- memory-only, so the program computes on the address — the aesni-mb
		-- dec8x `xorq $0x80, %base` toggle) need the group's ABSOLUTE alignment
		-- identity preserved (see the (d2) derivation), so record which ranges
		-- own one.
		for _, st in body do
			if st.kind == "insn" and st.x86StackBufLeaValueComputed
				and st.x86StackBufLeaValue then
				local off2 = st.x86StackBufLeaValue.off
				for _, r in sbRanges do
					if off2 >= r.lo and off2 < r.hi then
						r.hasComputed = true
						break
					end
				end
			end
		end
		for _, st in body do
			if st.kind ~= "insn" then continue end
			local mem = nil
			for _, o in st.operands do if o.kind == "mem" then mem = o end end
			-- (b) static accesses: mark the ones inside a declared buffer, reject
			-- the ones straddling a boundary. (rsp-based accesses key at the
			-- frame-base depth, or — at a KNOWN perturbed depth — at disp + D0 - d,
			-- exactly the rewrite's perturbed-depth slot keying and the same
			-- normalization the declaration/rep resolution uses; the aesni CBC
			-- decrypt shape declares its buffer against a mid-function inner
			-- frame, so its stores must key the same way or they silently miss
			-- the buffer. Pre-and accesses stay excluded (the outer gate);
			-- rbp- and alias-based accesses normalize absolutely at any
			-- provable depth.)
			if mem and not mem.index and not mem.dispSym
				and x86.baseMnemonic(string.lower(st.mnemonic)) ~= "lea"
				and st.x86_depth ~= nil and not isDynDepth(st.x86_depth)
				and #sbRanges > 0 then
				local off, wb = nil, nil
				local d = st.x86_depth
				-- Clone statements key into their band's coordinate space, so
				-- their buffer offsets (and their declarations above) live in
				-- the band: the same +band shift every other clone access
				-- gets. Band bases are multiples of 64, so all the alignment
				-- residues the group placement consumes are preserved. A
				-- LOCALCALL clone statement instead keys with the +8
				-- return-address compensation (the +8 rule) and no band — the
				-- same keying every other localcall clone access uses.
				local band = st.x86_b2band or 0
				if mem.base and mem.base.kind == "reg" and mem.base.rc == "sp" then
					if st.localClone then off = (mem.dispVal or 0) + D0 - d - 8
					elseif d == D0 and band == 0 then off = mem.dispVal or 0
					else off = (mem.dispVal or 0) + D0 - d + band end
					if st.x86_preAnd and off ~= nil then
						-- Pre-`and` rsp traffic keys PRE-and numbering while the
						-- buffer's declared range keys on the declaring
						-- statement's depth: on a path that executes the `and`,
						-- the same modeled offset names a real address shifted
						-- by the and's dynamic slack, so a pre-and access
						-- sharing bytes with a buffer would give those bytes
						-- two homes. Fail closed on any intersection; pre-and
						-- traffic that touches no buffer keeps its pre-existing
						-- handling (the rewrite's preAnd rejection).
						local wPre = widthOf(st, mem)
						if wPre then
							for _, r in sbRanges do
								if off < r.hi and off + wPre > r.lo then
									error("sarcasm: stack access before a mid-function"
										.. " `and $-N, %rsp` overlaps a stack buffer"
										.. " (pre-and numbering and the buffer's"
										.. " coordinate system name different real"
										.. " bytes for the same modeled offset): "
										.. (st.raw or st.mnemonic))
								end
							end
						end
						off = nil  -- not buffer-classified; normal handling applies
					end
				elseif mem.base and mem.base.kind == "reg" and mem.base.name == "rbp"
					and usesRbp and st.x86_fp == "est" then
					off = rbpNorm + (mem.dispVal or 0)
				elseif mem.base and mem.base.kind == "reg" and st.x86_aliasOff ~= nil then
					off = st.x86_aliasOff
				end
				if off == nil and st.stackBufAnno then
					-- An annotated access that resolves to no stack address at
					-- all. A SHORT form is the by-ADDRESS mode (Feature A): the
					-- base register is an ordinary web holding the buffer byte
					-- address — the access lowers to a runtime bounds check
					-- against the buffer's lowered range followed by the RAW
					-- instruction. Phase (b3) below validates the base shape and
					-- marks it (nothing silently vanishes). A LONG form falls
					-- through to (b3) as well: the declaration it carries is
					-- independent of the access's base (it is spelled
					-- %rsp-relative and resolves from the statement's own depth
					-- context — phase (a) already registered and frame-checked
					-- it), so the access may DRIVE on a computed base web: a
					-- register holding the buffer byte address (materialized by
					-- a buffer-interior value lea and computed on — the aesni-mb
					-- dec8x xor-toggle base) lowers to a runtime-checked
					-- by-address access exactly like the short form. (b3) owns
					-- the precision: the base must be a general-register web
					-- TAINTED by a buffer value lea — an arbitrary heap or
					-- argument pointer under a long form stays rejected there.
				end
				if off ~= nil then
					local fa
					wb, fa = widthOf(st, mem)
					if wb then
						-- group membership / straddle (against the CURRENT range
						-- list; merged groups are computed after this loop)
						local inside, straddles = false, false
						for _, r in sbRanges do
							if off < r.hi and off + wb > r.lo then
								if off >= r.lo and off + wb <= r.hi then
									inside = true
								else
									straddles = true
								end
							end
						end
						if straddles then
							error("sarcasm: stack access straddles a stack buffer"
								.. " boundary (normalized [" .. tostring(off) .. ", "
								.. tostring(off + wb) .. ") partially overlaps a declared"
								.. " buffer; it cannot be proven which side it is on): "
								.. (st.raw or st.mnemonic))
						end
						if inside then
							-- an explicitly annotated access must sit inside its OWN
							-- buffer's declared range
							if st.stackBufAnno then
								local loN, hiN
								if st.stackBufAnno.short then
									loN, hiN = resolveShortRangeFor(st)
								else
									loN, hiN = resolveBufRangeAt(st, st.x86_depth,
										st.x86_smap, st.x86_fp, st.x86_prologue == true)
								end
								if loN and not (off >= loN + band and off + wb <= hiN + band) then
									error("sarcasm: `stack buffer (" .. st.stackBufAnno.id
										.. ")`-annotated access at normalized ["
										.. tostring(off) .. ", " .. tostring(off + wb)
										.. ") lies outside the buffer's declared range ["
										.. tostring(loN) .. ", " .. tostring(hiN) .. "): "
										.. (st.raw or st.mnemonic))
								end
							end
							-- capabilities never live in buffer memory
							if st.annotation and sig.PTR_ANNOTATIONS[st.annotation] then
								error("sarcasm: `" .. st.annotation .. "` on a stack access"
									.. " inside a stack buffer is not supported (buffer"
									.. " slots cannot hold capabilities; loading from a"
									.. " buffer always yields a null capability): "
									.. (st.raw or st.mnemonic))
							end
							-- Only FP/SIMD forms carry an alignment promise the
							-- lowering must preserve (movdqa-class); the knowledge
							-- module's alignment is authoritative. Scalar GPR accesses
							-- are unaligned-tolerant. The lowering places each buffer
							-- group at one output base, so every ALIGNED access in a
							-- group must land on an aligned byte: record each aligned
							-- access's (offset, requirement) pair and let phase (d2)
							-- prove the group's offsets are jointly congruent (a
							-- single base exists) and that the synthesized frame
							-- backs the widest requirement. This is what makes
							-- buffers work in `and $-N, %rsp`-aligned frames: the
							-- post-`and` traffic's own offsets carry its alignment
							-- identity, whatever dynamic slack the `and` baked into
							-- rsp — all its accesses shift together, so their
							-- relative residues are exactly what the placement
							-- consumes.
							local areq = 1
							local isFP = isFpInsn(fp.lookup(string.lower(st.mnemonic)), st)
							if isFP and fa and fa > 1 then
								areq = fa
								for _, r in sbRanges do
									if off >= r.lo and off + wb <= r.hi then
										table.insert(r.aligns, { off = off, areq = areq })
									end
								end
							end
							st.x86StackBufAccess = { off = off, w = wb, areq = areq }
							-- contribute alignment to the enclosing declaration
							for _, r in sbRanges do
								if off >= r.lo and off + wb <= r.hi and r.areq < areq then
									r.areq = areq
								end
							end
						end
					end
				end
			end
		end
		-- (b3) by-ADDRESS annotated accesses (Feature A): an annotated access
		-- (SHORT form, or LONG form carried on a computed base — see (b)) whose
		-- base register does not statically resolve to a stack address marks a
		-- runtime-checked by-address access — the base web holds the buffer byte
		-- address (materialized by a buffer-interior value lea, computed on,
		-- stored/reloaded, cmov'd, or — short form only — simply an argument),
		-- the runtime check validates it against the buffer's lowered range, and
		-- the ORIGINAL instruction is emitted verbatim with no capability
		-- machinery.
		-- Statically enforced here:
		--   * the base is a general-register web (an FP/SIMD or %rip base has no
		--     byte address to check; %rsp/%rbp that cannot be proven to address
		--     the stack have no web at all) — everything else rejects with a
		--     precise message;
		--   * a LONG form additionally requires the base web to be TAINTED by a
		--     buffer value lea (the walk below: the taint seeds at every phase
		--     (a0) value lea's destination and spreads through every modeled
		--     GPR definition, never dying — the dec8x xor-toggle base is a
		--     tainted web three ALU ops past its lea). A long form on a web the
		--     buffer never touched (an argument or heap pointer) stays rejected
		--     with the historical message: the declaration names stack bytes,
		--     and a by-address check against them would silently turn a typo'd
		--     heap access into a guaranteed runtime trap. A SHORT form keeps
		--     today's shape (any general-register web — the short form IS the
		--     by-address mode by declaration);
		--   * a base that PROVABLY resolves (a live saved-rsp carrier, an
		--     established frame pointer) takes the existing static paths in (b)
		--     instead — no duplicate machinery;
		--   * the access has no index register (an unbounded index cannot ride a
		--     base-only check; the indexed path (c) covers the static bases);
		--   * `#! load/store ptr` on buffer memory stays rejected (buffer slots
		--     cannot hold capabilities);
		--   * the access width is computable, and the buffer id resolves (a
		--     canonical entry declared in this function or in the declaring
		--     subroutine of the clone it sits in) to a lowered group.
		-- The taint walk (buffer value leas flow through ordinary defs): state
		-- per statement = the set of register nums possibly holding a buffer
		-- byte address BEFORE the statement. Over-approximate by design (any
		-- GPR def unions its reg operands' taints); an over-tainted long-form
		-- base only moves a rejection to a runtime check — fail-safe, never
		-- unsound.
		local bufLeaTaint = {}
		do
			local genStmt = {}
			for i, st in body do
				if st.kind == "insn" and st.x86StackBufLeaValue then
					genStmt[i] = st.operands[1]
				end
			end
			if next(genStmt) then
				local work, inq = {}, {}
				local firstInsn = nil
				for i, st in body do
					if st.kind == "insn" then firstInsn = i break end
				end
				if firstInsn then
					bufLeaTaint[firstInsn] = {}
					work[1] = firstInsn
					inq[firstInsn] = true
					while #work > 0 do
						local i = table.remove(work)
						inq[i] = nil
						local st = body[i]
						if not (st and st.kind == "insn") then continue end
						local s = bufLeaTaint[i] or {}
						local okc, c = pcall(classOf, st)
						if not (okc and c) then continue end
						local out = s
						for _, dd in c.defs do
							if dd.kind == "reg" and dd.rc == "gpr" then
								local t = nil
								for _, uu in (c.uses or {}) do
									if uu.kind == "reg" and uu.rc == "gpr" and s[uu.num] then
										t = t or {}
										t[uu.num] = true
									end
								end
								if out == s then
									out = {}
									for k2, v2 in pairs(s) do out[k2] = v2 end
								end
								-- the def kills the destination, then re-tains it
								-- from the operands the def reads
								out[dd.num] = nil
								if t then
									for k2 in t do out[k2] = true end
								end
								-- a def whose value derives from a tainted operand
								-- taints the destination; note the def may also be
								-- the gen (an a0 lea defines the property directly)
								if genStmt[i] and genStmt[i].num == dd.num then
									out[dd.num] = true
								end
							end
						end
						for _, s2 in successors(i) do
							local cur = bufLeaTaint[s2]
							local merged
							if cur == nil then merged = out
							else
								merged = cur
								local grew = false
								for k2 in out do
									if not cur[k2] then
										if merged == cur then
											merged = {}
											for k3, v3 in pairs(cur) do merged[k3] = v3 end
										end
										merged[k2] = true
										grew = true
									end
								end
								if not grew then merged = cur end
							end
							if merged ~= cur then
								bufLeaTaint[s2] = merged
								if not inq[s2] then
									inq[s2] = true
									table.insert(work, s2)
								end
							end
						end
					end
				end
			end
		end
		-- The masked lowering's scratch register: the same "vector register the
		-- body never names" discipline the FP-taint scan uses (the vecUsed scan
		-- below picks the same number, so both agree).
		local bufVecUsed = {}
		for _, st in body do
			if st.kind == "insn" then
				for _, o in (st.operands or {}) do
					if o.kind == "reg" and o.rc == "xmm" then
						local nm = o.name or ""
						local p3 = string.sub(nm, 1, 3)
						if p3 == "xmm" or p3 == "ymm" or p3 == "zmm" then
							bufVecUsed[o.num or 0] = true
						end
					end
				end
			end
		end
		local bufMaskScratch = nil
		for n = 31, 0, -1 do
			if not bufVecUsed[n] then bufMaskScratch = n break end
		end
		for i3, st in body do
			if st.kind == "insn" and st.stackBufAnno
				and st.x86StackBufAccess == nil and st.x86StackBufIdx == nil
				and not st.x86_preAnd
				and x86.baseMnemonic(string.lower(st.mnemonic)) ~= "lea" then
				local mem = nil
				for _, o in st.operands do if o.kind == "mem" then mem = o end end
				if mem and not mem.dispSym then
					if mem.index then
						-- an INDEXED access whose base did not resolve in (c):
						-- the base must have been unresolvable (a live carrier
						-- or %rsp/%rbp base is (c)'s business). By-address with
						-- an unbounded index cannot be checked.
						local bn = mem.base and mem.base.kind == "reg"
							and mem.base.name or "?"
						local rc = mem.base and mem.base.rc
						local e2 = st.x86_smap and st.x86_smap[bn] or nil
						local isStack = (rc == "sp") or (usesRbp and bn == "rbp")
							or (e2 and not e2.redefined and not e2.slot
								and not e2.roff and not e2.conflict)
						if not isStack and rc == "gpr" then
							error("sarcasm: `stack buffer (" .. st.stackBufAnno.id
								.. ")`-annotated indexed access through %" .. bn
								.. " cannot be bounds-checked (the index is unbounded"
								.. " and the base is only known at runtime; use the"
								.. " indexed form through %rsp, or spell the byte"
								.. " address through the base register alone): "
								.. (st.raw or st.mnemonic))
						end
						continue
					end
					local bn = mem.base and mem.base.kind == "reg" and mem.base.name
						or nil
					local rc = mem.base and mem.base.rc
					if bn == nil and not st.stackBufAnno.short then
						-- A LONG form on an absolute (base-less) operand: the
						-- declaration must be carried by an access that at least
						-- names a register to check; an absolute address is no
						-- by-address shape (the historical (b) rejection).
						error("sarcasm: `stack buffer`-annotated access does not address"
							.. " the stack through %rsp, %rbp or a live saved-rsp alias"
							.. " register (a long-form declaration must name stack"
							.. " bytes; use the short form `#! stack buffer ("
							.. st.stackBufAnno.id .. ")` on an access through a"
							.. " computed address to bounds-check it at runtime"
							.. " instead): " .. (st.raw or st.mnemonic))
					end
					if bn then
						local e2 = st.x86_smap and st.x86_smap[bn] or nil
						local liveCarrier = e2 and not e2.redefined and not e2.slot
							and not e2.roff and not e2.conflict or nil
						local isStackBase = (rc == "sp") or (usesRbp and bn == "rbp")
						if isStackBase then
							-- A stack base that (b) could not classify: the depth
							-- or frame-pointer state is unprovable. There is no web
							-- to check and no static home: reject precisely.
							if st.stackBufAnno then
								error("sarcasm: `stack buffer (" .. st.stackBufAnno.id
									.. ")`-annotated access through %" .. bn
									.. " cannot be proven to address the stack here (the"
									.. " stack depth or frame pointer is unprovable, so"
									.. " neither a static home nor a by-address web"
									.. " exists): " .. (st.raw or st.mnemonic))
							end
						elseif rc ~= "gpr" then
							error("sarcasm: `stack buffer (" .. st.stackBufAnno.id
								.. ")`-annotated access base %" .. bn
								.. " is not a general register (a by-address buffer"
								.. " access bounds-checks a byte address held in a"
								.. " general-register web): " .. (st.raw or st.mnemonic))
						elseif liveCarrier then
							-- A provably-resolvable carrier: (b)'s alias path owns
							-- it (static resolution). Nothing to mark.
						else
							-- A LONG form must ride a buffer-tainted web: the
							-- declaration names stack bytes, so driving it on an
							-- arbitrary argument/heap pointer stays rejected.
							if not st.stackBufAnno.short then
								local t = bufLeaTaint and bufLeaTaint[i3] or nil
								local tainted = false
								if t and mem.base and mem.base.kind == "reg" then
									tainted = t[mem.base.num] and true or false
								end
								if not tainted then
									error("sarcasm: `stack buffer`-annotated access does"
										.. " not address the stack through %rsp, %rbp or a"
										.. " live saved-rsp alias register (a long-form"
										.. " declaration must name stack bytes and must"
										.. " ride a register computed from a buffer-interior"
										.. " `lea`; use the short form `#! stack buffer ("
										.. st.stackBufAnno.id .. ")` on an access through"
										.. " a computed address to bounds-check it at"
										.. " runtime instead): " .. (st.raw or st.mnemonic))
								end
							end
							-- the by-address mark
							local wb, fa = widthOf(st, mem)
							if not wb then
								error("sarcasm: cannot determine the access width of a"
									.. " by-address `stack buffer (" .. st.stackBufAnno.id
									.. ")` access: " .. (st.raw or st.mnemonic))
							end
							if st.annotation and sig.PTR_ANNOTATIONS[st.annotation] then
								error("sarcasm: `" .. st.annotation .. "` on a"
									.. " by-address stack-buffer access is not supported"
									.. " (buffer slots cannot hold capabilities; loading"
									.. " from a buffer always yields a null capability): "
									.. (st.raw or st.mnemonic))
							end
							local loN, hiN
							local band3 = st.x86_b2band or 0
							if st.stackBufAnno.short then
								loN, hiN = resolveShortRangeFor(st)
							else
								-- The long form resolves from the statement's own
								-- depth context (its declaration was already
								-- registered and frame-checked in phase (a)); the
								-- mark's range keys in the same space the groups
								-- live in, so band it exactly like the static
								-- accesses and the clone declarations.
								loN, hiN = resolveBufRangeAt(st, st.x86_depth,
									st.x86_smap, st.x86_fp, st.x86_prologue == true)
								if loN == nil then
									error("sarcasm: internal error: long-form by-address"
										.. " access with an unresolvable declaration: "
										.. (st.raw or st.mnemonic))
								end
								loN += band3
								hiN += band3
							end
							local g = nil
							for _, gr in sbRanges do
								if loN >= gr.lo and hiN <= gr.hi then g = gr break end
							end
							if g == nil then
								error("sarcasm: `stack buffer (" .. st.stackBufAnno.id
									.. ")` is not declared in this function (a by-address"
									.. " access checks against the buffer's lowered"
									.. " range, which a long form in this function or in"
									.. " the declaring subroutine must provide): "
									.. (st.raw or st.mnemonic))
							end
							local deco = nil
							for _, o in st.operands do
								if o.deco and string.match(o.deco, "{%%?k[0-7]}") then
									deco = o
								end
							end
							local masked = nil
							if deco then
								local mnem = string.lower(st.mnemonic)
								if not (isFpInsn(fp.lookup(mnem), st) and bufMaskScratch
									and MASKED_STACK_MOVE[mnem]) or mem.bcastN
									or string.find(deco.deco, "{1to", 1, true) then
									error("sarcasm: AVX512 mask-register ({k}) by-address"
										.. " stack-buffer access is not supported for '"
										.. st.mnemonic .. "' (only the masked vector moves"
										.. " lower; masked-off lanes may not touch"
										.. " memory): " .. (st.raw or st.mnemonic))
								end
								masked = { num = bufMaskScratch,
									unal = MASKED_STACK_MOVE[mnem], deco = deco.deco }
							end
							st.x86StackBufAddr = { id = st.stackBufAnno.id,
								reg = bn, dispEff = mem.dispVal or 0,
								w = wb, write = (st.operands[1] == mem)
									or x86.isMemRmw(st),
								areq = (fa and fa > 1) and fa or 1,
								group = g, lo = loN, hi = hiN, masked = masked }
						end
					end
				end
			end
		end
		-- (c) annotated INDEXED accesses: the bounds-check marks. These need the
		-- merged group base, which phase 2 assigns; the mark carries the resolved
		-- range and the access shape, and the rewrite computes the displacement.
		for _, st in body do
			if st.kind == "insn" and st.stackBufAnno
				and string.lower(st.mnemonic) ~= "lea" then
				local mem = nil
				for _, o in st.operands do if o.kind == "mem" then mem = o end end
				if mem and mem.index then
					local d = st.x86_depth
					if d == nil or isDynDepth(d) then
						error("sarcasm: `stack buffer`-annotated indexed access at an"
							.. " unprovable stack depth is not supported: "
							.. (st.raw or st.mnemonic))
					end
					if st.x86_preAnd then
						error("sarcasm: `stack buffer`-annotated indexed access before a"
							.. " mid-function `and $-N, %rsp` cannot be proven safe: "
							.. (st.raw or st.mnemonic))
					end
					local kBase
					-- Clone statements key into their band's coordinate space
					-- (matching the declarations and the static classification).
					-- A LOCALCALL clone statement keys with the +8 return-address
					-- compensation and no band — the same keying every other
					-- localcall clone access uses.
					local band = st.x86_b2band or 0
					if mem.base and mem.base.kind == "reg" and mem.base.rc == "sp" then
						-- The frame-base depth keys disp directly; a KNOWN perturbed
						-- depth keys disp + D0 - d, exactly like the rewrite's slot
						-- keying, the static-buffer classification, and the rep's
						-- alias resolution (the aesni CBC inner-frame shape). Clone
						-- statements add their band (a localcall clone instead
						-- subtracts the return-address word).
						if st.localClone then kBase = D0 - d - 8
						else kBase = D0 - d + band end
					elseif mem.base and mem.base.kind == "reg" and mem.base.name == "rbp"
						and usesRbp and st.x86_fp == "est" then
						kBase = rbpNorm
					elseif mem.base and mem.base.kind == "reg" and st.x86_smap then
						local e = st.x86_smap[mem.base.name]
						if not (e and not e.redefined and not e.slot and not e.roff
							and not e.conflict) then
							error("sarcasm: indexed access base %" .. tostring(mem.base.name)
								.. " is not a live saved-rsp stack alias: "
								.. (st.raw or st.mnemonic))
						end
						kBase = D0 - e.depth
					else
						error("sarcasm: `stack buffer`-annotated indexed access through %"
							.. tostring(mem.base and mem.base.name or "?") .. " is not"
							.. " supported (the base must be %rsp, %rbp or a live saved-rsp"
							.. " alias; an unprovable base register cannot bounds-check an"
							.. " index — use the short form on the same access without the"
							.. " index, or address the bytes through %rsp): "
							.. (st.raw or st.mnemonic))
					end
					local scale = mem.scale or 1
					if scale ~= 1 and scale ~= 2 and scale ~= 4 and scale ~= 8 then
						error("sarcasm: indexed buffer access has scale " .. tostring(scale)
							.. " (only 1/2/4/8 are encodable): " .. (st.raw or st.mnemonic))
					end
					local wb, fa = widthOf(st, mem)
					if not wb then
						error("sarcasm: cannot determine the access width of an indexed"
							.. " buffer access: " .. (st.raw or st.mnemonic))
					end
					local loN, hiN
					if st.stackBufAnno.short then
						loN, hiN = resolveShortRangeFor(st)
						if loN == nil then
							-- A short form during the driver's resolution-only run
							-- (the canonical table is not complete yet): skip; the real
							-- per-function run always resolves (and errors on a
							-- long-form-less id).
							continue
						end
					else
						loN, hiN = resolveBufRangeAt(st, st.x86_depth, st.x86_smap,
							st.x86_fp, st.x86_prologue == true)
						if loN == nil then
							-- A long form during the driver's resolution-only run whose
							-- base could not resolve: skip; the real per-function run
							-- re-resolves (and errors precisely).
							continue
						end
					end
					-- The resolved range is unbanded; band it to the coordinate
					-- space the declarations/groups live in (band-invariant for
					-- the size check below).
					loN, hiN = loN + band, hiN + band
					if wb > hiN - loN then
						error("sarcasm: indexed buffer access is " .. tostring(wb)
							.. " bytes but the buffer (" .. st.stackBufAnno.id .. ") is only "
							.. tostring(hiN - loN) .. " bytes: " .. (st.raw or st.mnemonic))
					end
					-- capabilities never live in buffer memory
					if st.annotation and sig.PTR_ANNOTATIONS[st.annotation] then
						error("sarcasm: `" .. st.annotation .. "` on a stack-buffer access"
							.. " is not supported (buffer slots cannot hold capabilities;"
							.. " loading from a buffer always yields a null capability): "
							.. (st.raw or st.mnemonic))
					end
					local areq = 1
					if fa and fa > 1 then
						-- An indexed access's alignment promise comes from the
						-- FP form and is capped by the index scale (the byte
						-- address moves in scale steps, so a scale-1 index
						-- cannot carry a 16-byte promise). The access's
						-- alignment anchor is its EFFECTIVE displacement
						-- (dispEff — the address is dispEff + idx*scale), which
						-- phase (d2) consumes exactly like a static access's
						-- offset.
						areq = math.min(fa, 64)
						if scale < areq then areq = scale end
						for _, r in sbRanges do
							if loN >= r.lo and hiN <= r.hi then
								table.insert(r.aligns, {
									off = kBase + (mem.dispVal or 0), areq = areq })
							end
						end
					end
					st.x86StackBufIdx = { lo = loN, hi = hiN,
						dispEff = kBase + (mem.dispVal or 0), scale = scale, size = wb,
						write = (st.operands[1] == mem) or x86.isMemRmw(st),
						align = areq }
					-- the range's areq must cover the indexed access's alignment
					for _, r in sbRanges do
						if loN >= r.lo and hiN <= r.hi and r.areq < areq then
							r.areq = areq
						end
					end
				end
			end
		end
		-- (d1) merge overlapping/touching declarations into groups. The groups'
		-- region-relative placeholder BASES are assigned in phase (d2), after
		-- the FP cluster/yolo layout has run: the buffer region sits above the
		-- materialized clusters in the synthesized frame, so a group's
		-- alignment residue needs the region-start residue (phase (d2) below
		-- for the full identity).
		table.sort(sbRanges, function(a, b)
			if a.lo ~= b.lo then return a.lo < b.lo end
			return a.hi < b.hi
		end)
		sbGroups = {}
		for _, r in sbRanges do
			local last = sbGroups[#sbGroups]
			if last and r.lo <= last.hi then
				if r.hi > last.hi then last.hi = r.hi end
				if r.areq > last.areq then last.areq = r.areq end
				for _, a in r.aligns do table.insert(last.aligns, a) end
				if r.hasComputed then last.hasComputed = true end
				if r.declIdx ~= nil then
					if last.decls == nil then last.decls = {} end
					table.insert(last.decls, { declIdx = r.declIdx, band = r.band,
						lo = r.lo, hi = r.hi })
				end
			else
				table.insert(sbGroups, { lo = r.lo, hi = r.hi, areq = r.areq,
					aligns = { table.unpack(r.aligns) }, base = 0,
					hasComputed = r.hasComputed or nil,
					decls = (r.declIdx ~= nil)
						and { { declIdx = r.declIdx, band = r.band,
							lo = r.lo, hi = r.hi } } or nil })
			end
		end
		-- Absolute-alignment identity for COMPUTED-base groups (the xor-toggle
		-- feature). A buffer whose base web the program computes on (the dec8x
		-- `xorq $0x80, %base` base flip) keys its arithmetic on ABSOLUTE address
		-- bits: the input's `and $-N, %rsp` pins rsp mod N, so the input buffer's
		-- base byte sits at a fixed residue mod N and the toggle's bit-flip lands
		-- on the buffer's other half deterministically. The output frame must
		-- reproduce that residue or the flipped base lands outside the lowered
		-- group and every toggle-shaped access traps on valid input. When the
		-- declaring statement is governed by a provable `and $-N, %rsp` (the
		-- andDomOf dataflow: the most recent and on EVERY path, no rsp restore
		-- since), the buffer base's residue mod N is
		--     res = (d_and - D0 + o_lo)  (mod N)
		-- (entry_rsp ≡ d_and mod N because rsp(d_and) = entry_rsp - d_and is the
		-- and-aligned rsp; a byte's absolute address is entry_rsp - D0 + o for
		-- normalized offset o — D0 cancels against the entry term), and the
		-- transform pads the buffer region so the lowered group's base lands on
		-- exactly that residue (the output rsp is N-aligned too: the identity's
		-- mod raises the frame's effective alignment). Groups without a computed
		-- base keep today's placement byte for byte (the enc8x carrier shape has
		-- no computed web and no identity needs).
		do
			-- Governing-and modulus by and depth: `and $-N, %rsp` statements'
			-- N keyed by their (provable) depth; two different Ns at one depth
			-- make the residue ambiguous — no identity.
			local andNByDepth = {}
			for i, st in body do
				if st.kind == "insn" and st.x86_midAnd then
					local dd = depthIn[i]
					if type(dd) == "number" and st.operands and st.operands[2]
						and st.operands[2].kind == "imm" then
						local n = -immVal(st.operands[2])
						if isStackAlign(n) then
							if andNByDepth[dd] ~= nil and andNByDepth[dd] ~= n then
								andNByDepth[dd] = nil
								andNByDepth[dd .. "_ambig"] = true
							elseif andNByDepth[dd .. "_ambig"] == nil then
								andNByDepth[dd] = n
							end
						end
					end
				end
			end
			for _, g in sbGroups do
				if g.hasComputed and g.decls then
					local anchor = nil
					for _, dm in g.decls do
						local gd = andDomOf[dm.declIdx]
						local n = (type(gd) == "number") and andNByDepth[gd] or nil
						if n then
							local res = ((gd - D0 + (dm.lo - dm.band)) % n + n) % n
							if anchor == nil then
								anchor = { res = res, mod = n, lo = dm.lo }
							else
								-- cross-check: the members' real-byte deltas must
								-- match their residue deltas (one and governs all)
								if n == anchor.mod then
									local dRes = ((res - anchor.res) % n + n) % n
									local dLo = ((dm.lo - anchor.lo) % n + n) % n
									if dRes ~= dLo then
										error("sarcasm: stack buffer group ["
											.. tostring(g.lo) .. ", " .. tostring(g.hi)
											.. ") holds declarations whose absolute"
											.. " alignment identities disagree (the"
											.. " xor-toggle residue story is"
											.. " ambiguous); the buffer cannot be"
											.. " lowered with a computed base")
									end
								else
									anchor = nil break
								end
							end
						end
					end
					if anchor ~= nil then
						-- re-anchor the residue at the GROUP's low byte
						g.abs = { res = ((anchor.res - (anchor.lo - g.lo))
							% anchor.mod + anchor.mod) % anchor.mod,
							mod = anchor.mod }
					end
				end
			end
		end
	end
	end

	-- FP/SIMD stack-slot taint scan. FP/vector instructions cannot ride the GPR slot
	-- virtualization (their operands are real vector registers, not temps — the slot
	-- pseudo-register would render as garbage), so every stack range an FP
	-- instruction touches is MATERIALIZED: the instruction is kept and its memory
	-- operand is rewritten to a real disp(%rsp) into a reserved region of sarcasm's
	-- synthesized frame (see rewrite). GPR accesses overlapping an FP-touched range
	-- are materialized too (aliasing: the overlapped bytes must have exactly one
	-- home). This scan computes the tainted clusters and assigns their synthesized
	-- offsets; rewrite's fpInfo.lookup(off, w) then maps each materialized access
	-- to its final offset.
	local intervals = {}
	for si, st in body do
	if st.kind == "insn" then
			local mem
			for _, o in (st.operands or {}) do if o.kind == "mem" then mem = o end end
			-- Mirror exactly the rewrite's stack-mem gate: stack base, no symbolic
			-- displacement, no index register (the rewrite rejects those cleanly).
			-- Heap/global accesses are irrelevant here. An entry-time read of an
			-- incoming yolo stack argument (a depth-0 %rsp read) is irrelevant too:
			-- it is redirected to the arguments' own materialized area, not to an
			-- FP cluster.
			-- A lea has no memory traffic (it computes an address): it never
			-- materializes into a cluster home (the rewrite drops lea-saves and
			-- rejects other stack-address leas; the spill sweep below skips leas
			-- for the same reason), so it contributes no interval here.
			if mem and x86.baseMnemonic(string.lower(st.mnemonic)) ~= "lea"
				and (isStackReg(mem.base, usesRbp) or st.x86_aliasOff ~= nil) and not mem.dispSym and not mem.index
				and not (mem.base.rc == "sp" and st.x86_depth ~= nil and not isDynDepth(st.x86_depth)
					and yoloArgSlotOf(st, "rsp", st.x86_depth))
				-- Phantom saved-rsp traffic (save-stores / carrier-loads) is dropped by
				-- the rewrite — no real memory exists for it — and an access landing
				-- in a fixed alloca region rides the region pointer instead of a
				-- frame slot (region coordinates never collide with cluster offsets).
				and not st.x86_saveStore and not st.x86_carrierLoad
				-- a lea-save is dropped by the rewrite too (no real memory exists
				-- for its phantom 8-byte "read"), so it must not join a cluster
				and not st.x86_leaSave
				-- A stack-buffer access is redirected into the buffer region by
				-- the rewrite — never materialized into an FP cluster.
				and st.x86StackBufAccess == nil then
				local m = string.lower(st.mnemonic)
				local fpe = fp.lookup(m)
				-- Region membership in the rewrite's exact coordinates/classification.
				local rOff = st.x86_aliasOff or (mem.dispVal or 0)
				if st.x86_aliasOff == nil and mem.base.rc ~= "sp" then rOff = rbpNorm + rOff end
				local rDyn = st.x86_aliasOff == nil and mem.base.rc == "sp" and not st.x86_prologue and type(st.x86_depth) == "table"
				-- A dynamic-region scope access rides the region pointer exactly
				-- like a fixed-region one (region coordinates never collide with
				-- cluster offsets) — never materialized.
				local rDynReg = rDyn and st.x86_depth.dynreg ~= nil
				if not (fpe and fpe.noCheck) and not regionOf(rOff, rDyn) and not rDynReg then  -- prefetch: memory-neutral, needs no slot
					-- Normalized rsp-relative frame offset: EXACTLY the rewrite's
					-- formula — including the perturbed-depth normalization (a
					-- mid-function inner frame's scratch traffic keys at
					-- disp + D0 - d) and the clone +8 rule.
					local off = st.x86_aliasOff or (mem.dispVal or 0)
					if st.x86_aliasOff == nil then
					local scand = st.x86_depth
					if mem.base.rc ~= "sp" then off = rbpNorm + off
					elseif st.localClone and scand ~= nil and not isDynDepth(scand) then
						off = off + D0 - scand - 8
					elseif st.b2clone and scand ~= nil and not isDynDepth(scand) then
						off = off + D0 - scand + b2band(st)
					elseif not st.x86_prologue and scand ~= nil and scand ~= D0
						and not isDynDepth(scand) then
						off = off + D0 - scand
					end
					end
					local isFP = isFpInsn(fpe, st)
					local w, areq = nil, 1
					if isFP then
						-- The knowledge module's width is AUTHORITATIVE for FP/SIMD
						-- (broadcast decorators, ISA-fixed vector widths, and the
						-- size-driven x87/PTR forms all resolved inside); a raw
						-- Intel mem.size must never under-size the slot, and
						-- accessWidth cleanly rejects a contradicting PTR
						-- annotation. nil: the rewrite produces the clean error.
						local fw, fa = fp.accessWidth(m, st.operands, st.raw)
						w = fw
						if fa and fa > 1 then areq = math.min(fa, 64) end  -- cap at 64
					else
						w = gprStackWidth(m, mem)
					end
					-- A full-slot (8-byte) GPR access at exactly an outstanding pushed
					-- register's save slot is NOT slot traffic: the rewrite's save-slot
					-- model turns it into an operation on that register's web (a load
					-- reads the web, a full-width store re-defines it) — no frame bytes
					-- are touched, so it owns no home and takes no part in the
					-- pre-/post-`and` intersection check (the poly1305 avx2 dispatch
					-- restore loads sit pre-and while the vector scratch is post-and;
					-- they never share bytes with it because they share none with
					-- anything).
					local saveAliasOp = false
					if not isFP and w == 8 then
						local sv = st.x86_saves
						if sv and sv ~= "dyn" and #sv > 0 then
							for _, s in sv do
								if not (usesRbp and s.name == "rbp") and not s.spill then
									local sd = D0 - s.depth
									if off == sd then
										if not s.redefined or st.operands[1] == mem then
											saveAliasOp = true
										end
										break
									end
								end
							end
						end
					end
					if w then
						-- B2 band bases are multiples of 64: a banded offset
						-- (unbiased + base) preserves the input access's
						-- mod-areq residue — and hence its synthesized
						-- alignment — iff areq divides 64. Every areq the
						-- knowledge module can produce (1, 16, or a vector
						-- width 16/32/64, capped at 64 above) does — but if
						-- one ever does not, banding would silently
						-- misalign it, so fail closed here rather than emit
						-- a bad frame.
						if areq ~= 1 and 64 % areq ~= 0 then
							error("sarcasm: stack access with a " .. areq .. "-byte alignment"
								.. " requirement cannot be proven safe (B2 shared-tail bands"
								.. " are multiples of 64, which preserves only alignment"
								.. " requirements that divide 64): "
								.. (body[si].raw or body[si].mnemonic))
						end
						-- An aligned access needing more than the guaranteed
						-- 16-byte stack alignment (a vmovdqa32/64-class access):
						-- the frame must carry an `and $-N, %rsp` note with
						-- N >= areq, and the input offset must itself be
						-- aligned to (D0 - andDepth), the note's base.
						if isFP and areq > 16 then
							local alignedOff = ((off - (D0 - frameAndDepth)) % areq + areq) % areq == 0
							if frameAlignReq < areq or not alignedOff then
								error("sarcasm: aligned " .. areq .. "-byte stack access cannot be proven safe"
									.. " (the stack is only guaranteed 16-byte aligned;"
									.. (frameAlignReq < areq
										and " align the frame with `and $-" .. areq .. ", %rsp` in the prologue, or else"
										or " use an aligned offset,")
									.. " or use the unaligned form (e.g. vmovdqu/vmovups)): "
									.. (body[si].raw or body[si].mnemonic))
							end
						end
						-- Which alignment coordinate system the access keys off
						-- (drives the cluster residue below). An rsp-relative
						-- access in a frame WITH an `and $-N,%rsp` note executes
						-- at the POST-AND rsp -- N-aligned (N >= 32 is what gets
						-- recorded), so its alignment residue is `disp mod areq`,
						-- keyed at the note's base. Every other access keys at
						-- the SysV base: rsp-relative without a note, normalized
						-- rbp-relative (absolute -- rbp = entry_rsp - fpDepth),
						-- and the carrier/clone coordinate spaces. The cluster
						-- placement must use the right base for its members and
						-- refuses to mix them (one cluster, one base).
						local keyed
						local andBase
						if frameAndDepth == 0 then
							-- no `and $-N,%rsp` note: every access keys at the SysV base
							keyed = "d0"
						-- A stack-alias carrier access (a GPR base holding a parked rsp+K
						-- value): the access is and-keyed iff the SAVE executed after the
						-- and (a post-and save parks the rounded rsp); a pre-and save
						-- parks the unrounded (SysV-keyed) value. A dyn save depth cannot
						-- be proven either way. "Executed after the and" is decided by
						-- the park SITE's reachedFromAnd fact (the park index the smap
						-- records), NOT by the parked depth: a post-and `leaq K(%rsp)`
						-- parks at depth d - K which can sit well ABOVE the and's depth
						-- (poly1305 avx2's `leaq 0x90(%rsp),%rax` at and-depth 0x128
						-- parks at 0xb0) while its VALUE is exactly as slack-tainted as
						-- the surrounding post-and rsp-relative traffic — keying it
						-- SysV would split one cluster's coordinate systems and reject
						-- the function.
						elseif st.x86_aliasOff ~= nil then
							local e = st.x86_smap and st.x86_smap[mem.base.name]
							local parkPost = e and e.saveIdx ~= nil
								and parkPostOf(e.saveIdx) or false
							if parkPost then
								keyed = "and"
								andBase = frameAndDepth
							elseif e and type(e.depth) == "number" and e.depth >= frameAndDepth then
								keyed = "and"
							elseif e and type(e.depth) == "number" then
								keyed = "d0"
							else
								keyed = "ambig"
							end
						-- A direct rsp access at or below the note's depth executes at the
						-- rounded (post-and) rsp; the access depth cancels in the residue
						-- identity (the model's off carries disp + D0 - d). Pre-and-only
						-- accesses (Feature 2's new accepts — they were hard-rejected
						-- before) sit at the SAME depth as the and but never execute
						-- after it, so they must not take the and-keyed residue: they
						-- fall to the TRUE-coordinate branch below.
						elseif mem.base.rc == "sp" and not st.localClone and not st.b2clone
						and type(st.x86_depth) == "number" and st.x86_depth >= frameAndDepth
						and not st.x86_preAnd then
							keyed = "and"
							andBase = frameAndDepth
						elseif mem.base.rc == "sp" and not st.localClone
						and st.x86_preAnd and not st.x86_postAnd
						and type(st.x86_depth) == "number" then
							-- Feature 2: a pre-and-only rsp access executes STRICTLY
							-- before every and on its path — TRUE entry-relative
							-- coordinates, keyed at the SysV base exactly like
							-- rbp-relative and pre-and-parked carrier traffic.
							keyed = "d0"
						elseif mem.base.rc == "sp" and st.b2clone
						and type(st.x86_depth) == "number" then
							-- A B2 clone re-runs the frame setup of the body it clones,
							-- including its own `and $-N, %rsp` — which is not the
							-- jumper's prologue note. When the clone's governing and is
							-- provable, post-and clone traffic keys at THAT and's depth
							-- (same slack identity as the jumper's note); otherwise it
							-- keeps the historical ambig (SysV-residue) keying.
							local g = andDomOf[si]
							if type(g) == "number" and st.x86_depth >= g
								and not st.x86_preAnd then
								keyed = "and"
								andBase = g
							elseif st.x86_preAnd and not st.x86_postAnd
								and type(st.x86_depth) == "number" then
								-- Feature 2 (clone side): a pre-and-only clone access
								-- keys TRUE (pre-band) coordinates at the SysV base; the
								-- band's multiple-of-64 base preserves the residue.
								keyed = "d0"
							else
								keyed = "ambig"
							end
						else
							keyed = "ambig"
						end
						table.insert(intervals, { lo = off, hi = off + w, fp = isFP, areq = areq, si = si, keyed = keyed, andBase = andBase,
						dropped = saveAliasOp or st.x86_dropRestore or nil,
						pre = not saveAliasOp and not st.x86_dropRestore
							and mem.base.rc == "sp" and st.x86_aliasOff == nil
							and st.x86_preAnd and not st.x86_postAnd,
						slack0 = not saveAliasOp and not st.x86_dropRestore
							and ((mem.base.rc == "sp" and st.x86_aliasOff == nil
								and st.x86_postAnd)
							or (st.x86_aliasOff ~= nil and st.x86_aliasSaveIdx ~= nil
								and parkPostOf(st.x86_aliasSaveIdx) or false)) })
					end
				end
			end
		end
	end
	-- Sweep-merge strictly-overlapping intervals into clusters; a cluster is TAINTED
	-- iff any member is an FP access (this is the aliasing guarantee: a GPR range
	-- overlapping an FP range joins the cluster and is materialized with it).
	-- Pre-/post-`and` slot-web intersection (Feature 2's soundness guard): a
	-- pre-and rsp-relative access keys TRUE entry-relative coordinates while
	-- every post-and access (rsp-relative, or through a carrier parked after
	-- the and) keys the and's slack=0 convention — the same modeled byte range
	-- on the two sides names real addresses a dynamic slack apart, and one
	-- virtual slot web / materialized cluster cannot hold both. (The input
	-- program cannot rely on the slack-dependent distance between the two
	-- sides, so giving them separate homes is always sound; sharing one home
	-- is not.) Reject any overlap between a pre-and access's range and a
	-- post-and access's range. Both-TRUE and both-slack0 overlaps keep their
	-- historical handling (same coordinates: one address, one home).
	for _, iv in intervals do
		if not iv.pre then continue end
		for _, jv in intervals do
			if jv ~= iv and jv.slack0 and iv.lo < jv.hi and iv.hi > jv.lo then
				error("sarcasm: stack access before a mid-function `and $-N, %rsp`"
					.. " overlaps post-and traffic in the modeled frame (pre-and keys"
					.. " true entry-relative coordinates while post-and keys the and's"
					.. " slack=0 convention, so one modeled range would name two real"
					.. " addresses a dynamic slack apart): "
					.. (body[iv.si].raw or body[iv.si].mnemonic))
			end
		end
	end

	table.sort(intervals, function(x, y) return x.lo < y.lo end)
	local tainted = {}
	do
		local cur = nil
		for _, iv in intervals do
			if cur and iv.lo < cur.hi then
				if iv.hi > cur.hi then cur.hi = iv.hi end
				if iv.fp then cur.fp = true end
				if iv.areq > cur.areq then cur.areq = iv.areq end
				-- Memory-neutral members (dropped epilogue restore loads and
				-- save-slot-model register operations touch no frame bytes) carry no
				-- coordinate system of their own: they inherit the cluster's keying
				-- instead of forcing the keyed-mismatch rejection. Their RANGES still
				-- extend the cluster exactly as before (the placement over-covers
				-- them, as it always did).
				if iv.dropped then
					-- inherits cur's keying
				elseif cur.dropped then
					cur.keyed = iv.keyed
					cur.andBase = iv.andBase
					cur.dropped = nil
				elseif iv.keyed ~= cur.keyed or (iv.keyed == "and" and iv.andBase ~= cur.andBase) then
					-- One cluster, one alignment base: rsp-relative traffic
					-- under an `and $-N,%rsp` note aligns to the note's base,
					-- absolute/rbp-relative traffic to the SysV base. A cluster
					-- mixing them has no single correct placement (fail closed;
					-- the old code silently picked the SysV base and could emit
					-- a misaligned movdqa-class access that traps at runtime).
					error("sarcasm: stack accesses through different coordinate systems"
						.. " (post-`and` rsp-relative and absolute/rbp-relative) share one"
						.. " materialized cluster and cannot be proven safe (split the"
						.. " vector scratch area): "
						.. (body[iv.si].raw or body[iv.si].mnemonic))
				end
			else
				cur = { lo = iv.lo, hi = iv.hi, fp = iv.fp, areq = iv.areq, keyed = iv.keyed, andBase = iv.andBase, dropped = iv.dropped }
				table.insert(tainted, cur)
			end
		end
		-- drop untainted clusters
		for i = #tainted, 1, -1 do
			if not tainted[i].fp then table.remove(tainted, i) end
		end
	end

	-- Live-window spill/cluster exclusion. A mid-function spill slot's home is
	-- its virtual web (the pop's reload, capability included); an overlapping
	-- FP-tainted cluster's home is real materialized memory. The spill push
	-- dual-stores both, so they agree until some OTHER cluster-materialized
	-- access touches the slot's range while its spill entry is live (between
	-- the push and the matching pop on the abstract save stack): a cluster
	-- write would leave the pop's web stale against it, a cluster read could
	-- not see the pushed bytes. Reject any such overlap. (Dropped epilogue
	-- restore loads and lea address computations execute no memory traffic and
	-- are excluded.)
	for _, iv in intervals do
		if x86.baseMnemonic(string.lower(body[iv.si].mnemonic)) == "lea" then continue end
		if not iv.fp then
			-- a GPR interval is materialized only when it joined a TAINTED cluster
			local inTainted = false
			for _, c in tainted do
				if iv.lo >= c.lo and iv.hi <= c.hi then inTainted = true break end
			end
			if not inTainted then continue end
		end
		local sv = savesIn[iv.si]
		if sv and sv ~= "dyn" then
			for _, e in sv do
				if e.spill then
					local slo, shi = D0 - e.depth, D0 - e.depth + 8
					if iv.lo < shi and iv.hi > slo
						and not body[iv.si].x86_dropRestore then
						error("sarcasm: FP/vector stack traffic overlapping a live spill"
							.. " slot cannot be proven safe (the slot is virtual while the"
							.. " vector range is materialized into real frame memory; move"
							.. " the spill outside the vector scratch area): "
							.. (body[iv.si].raw or body[iv.si].mnemonic))
					end
				end
			end
		end
	end

	-- Widest xmm-class operand width/8 anywhere in the function (16/32/64): sizes
	-- the reserved vector-save area above the materialized clusters.
	local vecBytes = nil
	-- Whether the body names any xmm/ymm/zmm register >= 16 (AVX512 EVEX-only
	-- registers) or any opmask register k0-7. The injected runtime calls the
	-- FP/SIMD save/restore machinery brackets CAN reach EVEX-compiled code
	-- (libyolocimpl's _evex string/memory routines use zmm16-31 and k0-7 as
	-- scratch), so live state in those registers must be saved too — see the
	-- long comment in x86_64_codegen.luau's expandFpSaves.
	local usesHiVec = false
	local usesK = false
	-- Feature ({k}-masked stack lowering): the highest-numbered vector register
	-- (zmm31 down) the body never names. The rewrite lowers a supported
	-- {%kN}-masked stack access into unmasked load / register-masked move /
	-- unmasked store sequences riding this scratch (mask merge-masking needs the
	-- old destination/memory value, and vector state has no register allocator —
	-- every vector operand passes through on its hardware register — so the
	-- scratch must be a register the program provably never touches). nil when
	-- all 32 are used: masked stack forms then stay rejected (fail closed).
	local vecUsed = {}
	for _, st in body do
		if st.kind == "insn" then
			local vb = fp.widestVecBytes(st.operands or {})
			if vb and (not vecBytes or vb > vecBytes) then vecBytes = vb end
			do
				for _, o in (st.operands or {}) do
					if o.kind == "reg" and o.rc == "xmm" then
						local nm = o.name or ""
						local p3 = string.sub(nm, 1, 3)
						if (p3 == "xmm" or p3 == "ymm" or p3 == "zmm") then
							vecUsed[o.num or 0] = true
							if (o.num or 0) >= 16 then
								usesHiVec = true
							end
						elseif string.match(nm, "^k%d$") then
							usesK = true
						end
					end
				end
			end
		end
	end
	maskScratchNum = nil
	for n = 31, 0, -1 do
		if not vecUsed[n] then maskScratchNum = n break end
	end
	-- Force the reservation from an FP entry signature alone (see the sigHasFp
	-- comment above): 16 bytes per xmm0..xmm15 slot, exactly VEC_SAVE_BYTES below.
	-- A body that already touches vector registers reserves at least this much on
	-- its own (a ymm/zmm body reserves 32/64-byte slots).
	if not vecBytes and sigHasFp then vecBytes = 16 end
	-- A body using opmask registers but no vector registers still needs the
	-- save/restore machinery active for the k-register saves (16-byte slots are
	-- big enough for that; the k area is appended after the vector slots).
	if not vecBytes and usesK then vecBytes = 16 end
	local saveSlots = usesHiVec and 32 or 16
	local kBytes = usesK and 64 or 0
	local saveBytes = vecBytes and (saveSlots * vecBytes + kBytes) or 0
	local base0 = 16 + saveBytes
	-- A cluster needing more than 16-byte alignment forces the output frame
	-- onto a dynamic `and` alignment (see computeLayout): the prologue saves
	-- the pre-alignment rsp in 8 reserved bytes at the start of the cluster
	-- area, so clusters begin 8 higher. Residues are unaffected (recomputed
	-- per cluster below); the reserved bytes ride fpBytes into the layout.
	local needAlignSave = false
	for _, c in tainted do if c.areq > 16 then needAlignSave = true break end end
	-- A `#! stack buffer` group needing more than 16-byte alignment drives the
	-- output frame's dynamic alignment too (the transform folds the groups'
	-- requirements into tr.frameAlignReq), so the 8-byte pre-alignment rsp
	-- save must be reserved for it exactly as for a wide cluster — otherwise
	-- the prologue's save would land on the first cluster's bytes.
	if not needAlignSave and sbGroups then
		for _, g in sbGroups do
			if g.areq > 16 then needAlignSave = true break end
		end
	end
	local cursor = base0 + (needAlignSave and 8 or 0)
	for _, c in tainted do
		local areq = math.max(c.areq, 1)
		-- The synthesized offset must preserve every input-guaranteed alignment. An
		-- input access with normalized offset off has address entry_rsp - D0 + off,
		-- and entry_rsp ≡ 8 (mod 16) (SysV: rsp+8 is 16-aligned at the call), so an
		-- input access aligned to areq (areq ≤ 16) has off ≡ D0 - 8 (mod areq) —
		-- the SAME identity for rsp-relative and normalized rbp-relative offsets.
		-- The output address is body_rsp + finalBase + (off - c.lo) with body_rsp
		-- ≡ 0 (mod 16) (computeLayout), so finalBase ≡ c.lo - (D0 - 8) (mod areq)
		-- maps every input-aligned access to an output-aligned address.
		-- Clusters needing more than 16-byte alignment (covered by an
		-- `and $-N, %rsp` note): input offsets align to (D0 - andDepth),
		-- the note's base, rather than the SysV (D0 - 8) residue.
		-- The residue base: post-`and` rsp-relative accesses align to the
		-- note's base (the post-and rsp is N-aligned, N >= 32); everything
		-- else to the SysV entry_rsp = 8 (mod 16) base. (Keying was decided
		-- per interval above and checked for conflicts at cluster merge.)
		local baseRes = (c.keyed == "and") and (c.andBase or frameAndDepth) or 8
		local residue = ((c.lo - D0 + baseRes) % areq + areq) % areq
		local delta = ((residue - cursor % areq) % areq + areq) % areq
		c.finalBase = cursor + delta
		cursor = c.finalBase + (c.hi - c.lo)
	end
	local fpBytes = cursor - base0

	-- Incoming yolo stack-argument FP/vector reads: scan for FP-class instructions
	-- whose entry-time read maps to a yolo slot (EXACTLY the rewrite's detection:
	-- a depth-0 %rsp read, or a read through an entry-rsp alias the smap proves
	-- unredefined). When any exist, reserve the materialized area AFTER the clusters,
	-- 16-aligned (the shift past the GC root area is always a multiple of 16 — see
	-- transform.luau / computeLayout — so a mod-16 base here keeps the input's
	-- movaps-class alignment of the entry_rsp+8 word pairs: entry_rsp ≡ 8 (mod 16),
	-- hence slot pair starts ≡ 0). GPR reads of the same slots ride the virtual slot
	-- webs instead; the area is initialized by the transform's entry unpack, so both
	-- views always agree (it is read-only afterwards — stores stay rejected).
	local fpYolo = {}
	local hasFpYolo = false
	if yoloCount > 0 then
		for _, st in body do
			if st.kind == "insn" and not (st.allocaPlace or st.allocaOwned) then
				local mem
				for _, o in (st.operands or {}) do if o.kind == "mem" then mem = o end end
				local i = nil
				if mem and mem.base then
					if mem.base.rc == "sp" and st.x86_depth ~= nil and not isDynDepth(st.x86_depth) then
						i = yoloArgSlotOf(st, "rsp", st.x86_depth)
					elseif st.x86_smap then
						local e = st.x86_smap[mem.base.name]
						if e and not e.redefined then i = yoloArgSlotOf(st, mem.base.name, e.depth) end
					end
				end
				if i then
					local fpe = fp.lookup(string.lower(st.mnemonic))
					if isFpInsn(fpe, st) then
						-- mark every slot the access can touch: a wider-than-8-byte
						-- read (movaps/movups/vmovdqu...) spans consecutive slots —
						-- the entry unpack must fill them all.
						local w = fp.accessWidth(string.lower(st.mnemonic), st.operands, st.raw) or 8
						local last = math.min(yoloCount - 1, i + math.ceil(w / 8) - 1)
						for j = i, last do fpYolo[j] = true end
						hasFpYolo = true
					end
				end
			end
		end
	end
	local yoloBytes = 0
	if hasFpYolo then
		yoloBase = math.ceil(cursor / 16) * 16
		yoloBytes = 8 * yoloCount
	end

	-- (d2) STACK BUFFER group base assignment (runs after the FP cluster and
	-- yolo layout above, whose sizes the buffer region's start depends on).
	-- The buffer region sits in the synthesized frame at
	--   regionStart = 16 + 8*numRoots + fpPad + fpSaveBytes + fpBytes + yoloBytes
	-- (mirroring computeLayout / the transform's bufShift), and every
	-- stackBuf-tagged operand is shifted by exactly that once numRoots is
	-- known. fpPad is widened so (8*numRoots + fpPad) ≡ 0 (mod A) with A the
	-- effective frame alignment (the widest materialized cluster requirement,
	-- 16 when none needs more) — so modulo any areq dividing A,
	--   regionStart ≡ 16 + fpSaveBytes + fpBytes + yoloBytes   (=: T)
	-- which is fully known here. A group placed at base g receives an access
	-- at absolute frame offset regionStart + g.base + (off - g.lo); an access
	-- whose ISA form promises areq-byte alignment must land on an areq-aligned
	-- byte, i.e. the group base must satisfy
	--   g.base ≡ g.lo - off_i   (mod areq_i)     for every aligned access i
	-- — solvable exactly when the accesses' offsets are jointly congruent
	-- (checked below by pairwise CRT over the power-of-two requirements).
	-- This is what makes buffers work in `and $-N, %rsp`-aligned frames: the
	-- `and` shifts every post-and rsp by one dynamic slack, so all of a
	-- buffer's post-and accesses shift together and their RELATIVE residues
	-- carry the whole alignment story; a group mixing incompatible residues
	-- (e.g. and-shifted and and-free traffic with disagreeing offsets) is
	-- rejected here rather than silently misaligned. (The historical formula
	-- was the special case T ≡ 0 (mod 16) with a single SysV residue; groups
	-- with areq 1 are indifferent to all of this.) A group whose widest
	-- requirement does not divide A cannot be placed — the fpPad widening
	-- would not cancel numRoots, which is not known here — so it is rejected
	-- (fail closed).
	if sbGroups and #sbGroups > 0 then
		local alignA = 16
		for _, c in tainted do
			if c.areq and c.areq > alignA then alignA = math.min(c.areq, 64) end
		end
		-- The buffer groups drive the output frame's effective alignment too
		-- (the transform folds them into tr.frameAlignReq the same way), so a
		-- group's requirement is placeable whenever it divides the widest
		-- requirement in the body — which its own contribution guarantees for
		-- the power-of-two requirements the ISA produces.
		for _, g2 in sbGroups do
			if g2.areq > alignA then alignA = math.min(g2.areq, 64) end
		end
		local sbCursor = 0
		for _, g in sbGroups do
			if g.areq > 1 then
				if g.areq > alignA or alignA % g.areq ~= 0 then
					error("sarcasm: stack buffer group ["
						.. tostring(g.lo) .. ", " .. tostring(g.hi)
						.. ") needs " .. tostring(g.areq) .. "-byte alignment but"
						.. " the synthesized frame only guarantees "
						.. tostring(alignA) .. " (a materialized cluster would"
						.. " have to need the same width); use unaligned or"
						.. " narrower accesses into this buffer")
				end
			end
			-- CRT over the aligned accesses' requirements (all powers of two):
			-- `want` is the required g.base residue modulo `wantMod`, widened
			-- as accesses with wider requirements are consumed. Groups with no
			-- aligned accesses (areq 1) are indifferent to the base and keep
			-- the historical placement exactly.
			local want, wantMod = nil, 1
			for _, a in g.aligns do
				local w2 = ((g.lo - a.off) % a.areq + a.areq) % a.areq
				if want == nil then
					want, wantMod = w2, a.areq
				elseif a.areq >= wantMod then
					if w2 % wantMod ~= want then
						error("sarcasm: stack buffer group ["
							.. tostring(g.lo) .. ", " .. tostring(g.hi)
							.. ") holds aligned accesses whose offsets cannot all"
							.. " land on aligned bytes (one output base exists but"
							.. " the accesses' alignment residues disagree); split"
							.. " the buffer or use unaligned forms")
					end
					want, wantMod = w2, a.areq
				else
					if want % a.areq ~= w2 then
						error("sarcasm: stack buffer group ["
							.. tostring(g.lo) .. ", " .. tostring(g.hi)
							.. ") holds aligned accesses whose offsets cannot all"
							.. " land on aligned bytes (one output base exists but"
							.. " the accesses' alignment residues disagree); split"
							.. " the buffer or use unaligned forms")
					end
				end
			end
			if want == nil then
				-- No aligned accesses (areq 1): the base is indifferent; keep
				-- the historical SysV placement (which is a no-op modulo 1).
				wantMod = g.areq
				want = ((g.lo - D0 + 8) % wantMod + wantMod) % wantMod
			end
			local t = (16 + saveBytes + fpBytes + yoloBytes) % wantMod
			local residue = ((want - t) % wantMod + wantMod) % wantMod
			local delta = ((residue - sbCursor % wantMod) % wantMod + wantMod) % wantMod
			g.base = sbCursor + delta
			sbCursor = g.base + (g.hi - g.lo)
		end
		sbLayout = { groups = sbGroups, bytes = sbCursor }
		-- (e2) Finalize the buffer-interior VALUE leas' lowered displacements
		-- (Feature A): each marked lea emits `leaq disp'(%rsp), %reg` into the
		-- lowered buffer region at its byte's group-relative position (tagged
		-- stackBuf, shifted by the transform once the frame base is known), so
		-- the web's runtime value is exactly the output address of the buffer
		-- byte the input lea computed — and the by-address checks (which compare
		-- against the same group base) agree with it by construction.
		for _, st in body do
			if st.kind == "insn" and st.x86StackBufLeaValue then
				local lv = st.x86StackBufLeaValue
				local g2 = nil
				for _, gr in sbGroups do
					if lv.off >= gr.lo and lv.off < gr.hi then g2 = gr break end
				end
				if g2 == nil then
					error("sarcasm: internal error: buffer value lea at normalized "
						.. tostring(lv.off) .. " has no lowered group: "
						.. (st.raw or st.mnemonic))
				end
				lv.disp = g2.base + (lv.off - g2.lo)
			end
		end
		-- (e) Materialize the defining save of each rep's stack-alias side. The
		-- save (`mov %rsp, %reg` / `lea K(%rsp), %reg` / a carrier copy) would be
		-- dropped as a phantom value; an annotated rep CONSUMES the address, so
		-- the rewrite emits a real `lea disp'(%rsp), %reg` in its place (tagged
		-- stackBuf, shifted by the transform once the frame base is known). The
		-- rep's implicit register then has a FRESH web (defined by that lea)
		-- instead of the shared entry-argument web, so the pin moves around the
		-- rep cannot corrupt an argument value that happens to share the
		-- register, and the post-rep (hardware-advanced) value lands in the
		-- fresh web honestly.
		local function groupBaseFor(lo, hi)
			for _, g in sbGroups do
				if lo >= g.lo and hi <= g.hi then return g.base end
			end
			error("sarcasm: internal error: rep buffer range has no group")
		end
		for _, st in body do
			if st.kind == "insn" and st.x86StackBufRep then
				local rep = st.x86StackBufRep
				local base = groupBaseFor(rep.lo, rep.hi)
				for reg, side in pairs(rep) do
					if type(side) == "table" and side.K then
						local e = st.x86_smap and st.x86_smap[reg] or nil
						local saveSt = (e and e.saveIdx and body[e.saveIdx]) or nil
						local okSave = false
						if saveSt and saveSt.kind == "insn" and saveSt.operands
							and saveSt.operands[1] and saveSt.operands[1].kind == "reg"
							and saveSt.operands[1].name == reg
							and (x86.baseMnemonic(string.lower(saveSt.mnemonic)) == "mov"
								or x86.baseMnemonic(string.lower(saveSt.mnemonic)) == "lea") then
							okSave = true
						end
						if not okSave then
							error("sarcasm: cannot materialize the defining stack-alias"
								.. " save for a rep's %" .. reg .. " side: "
								.. (st.raw or st.mnemonic))
						end
						local disp = base + (side.K - rep.lo)
						if saveSt.x86StackBufLea and saveSt.x86StackBufLea.disp ~= disp then
							error("sarcasm: one stack-alias register cannot be"
								.. " materialized at two different buffer offsets ("
								.. tostring(saveSt.x86StackBufLea.disp) .. " vs "
								.. tostring(disp) .. "): " .. (st.raw or st.mnemonic))
						end
						saveSt.x86StackBufLea = { reg = reg, disp = disp }
						side.materialized = true
					end
				end
			end
		end
		-- (f) Unannotated accesses through buffer-address webs (Feature A). A web
		-- that provably or possibly holds a buffer byte address (an (a0) value
		-- lea, propagated through full-width register copies) must not be
		-- dereferenced WITHOUT the annotation: the web is an ordinary integer
		-- holding a capability-less stack address, so the ordinary checked path
		-- would trap at runtime. Reject statically with the buffer named — the
		-- `#! stack buffer (id)` annotation on the access is the fix (it lowers
		-- to the runtime-checked by-address mode). A redefinition by ALU drops
		-- the property (an ordinary web again — the historical unknown-base
		-- handling applies, and by-address mode accepts any web anyway).
		do
			local genStmt = {}
			for i, st in body do
				if st.kind == "insn" and st.x86StackBufLeaValue then
					genStmt[i] = st.operands[1]
				end
			end
			if next(genStmt) then
				-- state[i] = map (reg num -> buffer id) of registers that
				-- possibly hold a buffer byte address BEFORE statement i.
				local state, work, inq = {}, {}, {}
				local firstInsn = nil
				for i, st in body do
					if st.kind == "insn" then firstInsn = i break end
				end
				local function union(a, b)
					local u = {}
					for k2, v2 in pairs(a) do u[k2] = v2 end
					for k2, v2 in pairs(b) do u[k2] = v2 end
					return u
				end
				local function sameSet(a, b)
					for k2 in a do if b[k2] == nil then return false end end
					for k2 in b do if a[k2] == nil then return false end end
					return true
				end
				if firstInsn then
					local first = {}
					work[1] = firstInsn
					inq[firstInsn] = true
					state[firstInsn] = first
					while #work > 0 do
						local i = table.remove(work)
						inq[i] = nil
						local st = body[i]
						if not (st and st.kind == "insn") then continue end
						local s = state[i] or {}
						local okc, c = pcall(classOf, st)
						if not (okc and c) then continue end
						local ops2 = st.operands or {}
						local out = s
						-- kill: every modeled GPR definition drops the property
						for _, dd in c.defs do
							if dd.kind == "reg" and dd.rc == "gpr" and out[dd.num] then
								if out == s then
									out = {}
									for k2, v2 in pairs(s) do out[k2] = v2 end
								end
								out[dd.num] = nil
							end
						end
						-- copy: a full-width reg-reg mov propagates the property
						if x86.baseMnemonic(string.lower(st.mnemonic)) == "mov"
							and ops2[1] and ops2[1].kind == "reg" and ops2[1].rc == "gpr"
							and (ops2[1].width == 64 or ops2[1].width == nil)
							and ops2[2] and ops2[2].kind == "reg" and ops2[2].rc == "gpr"
							and (ops2[2].width == 64 or ops2[2].width == nil) then
							local srcId = out[ops2[2].num]
							if srcId and not out[ops2[1].num] then
								if out == s then
									out = {}
									for k2, v2 in pairs(s) do out[k2] = v2 end
								end
								out[ops2[1].num] = srcId
							end
						end
						-- gen: the buffer value lea defines the property
						local g2 = genStmt[i]
						if g2 then
							if out == s then
								out = {}
								for k2, v2 in pairs(s) do out[k2] = v2 end
							end
							out[g2.num] = st.x86StackBufLeaValue.id
						end
						for _, s2 in successors(i) do
							local cur = state[s2]
							local merged
							if cur == nil then
								merged = out
							elseif sameSet(cur, out) then
								merged = cur
							else
								merged = union(cur, out)
							end
							if merged ~= cur then
								state[s2] = merged
								if not inq[s2] then
									inq[s2] = true
									table.insert(work, s2)
								end
							end
						end
					end
				end
				-- the rejection walk (an address-taking lea through the web is an
				-- ordinary value computation — only memory accesses reject)
				for i, st in body do
					if st.kind ~= "insn" or st.stackBufAnno then continue end
					if x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then continue end
					local s = state[i]
					if not s or not next(s) then continue end
					for _, o in (st.operands or {}) do
						if o.kind == "mem" and o.base and o.base.kind == "reg"
							and o.base.rc == "gpr" and s[o.base.num] then
							error("sarcasm: memory access through %" .. o.base.name
								.. " holds the address of stack buffer '"
								.. tostring(s[o.base.num]) .. "' (a buffer-interior"
								.. " `leaq` computed it, and its value is an ordinary"
								.. " integer here); annotate the access with"
								.. " `#! stack buffer (" .. tostring(s[o.base.num])
								.. ")` so it lowers to a runtime-checked raw access,"
								.. " or redefine the register first: "
								.. (st.raw or st.mnemonic))
						end
					end
				end
			end
		end
	end

	local fpInfo = {
		clusters = tainted,
		alignReq = frameAlignReq,
		bytes = fpBytes,
		saveBytes = saveBytes,
		vecBytes = vecBytes,
		saveSlots = saveSlots,
		kBytes = kBytes,
		usesK = usesK,
		-- finalOff for a materialized access, or nil when it hits no tainted cluster.
		lookup = function(off, w)
			for _, c in tainted do
				if off >= c.lo and off + w <= c.hi then
					return c.finalBase + (off - c.lo)
				end
			end
			return nil
		end,
	}
	-- B2 shared-tail clone lea carriers (the provable-depth relaxation, clone
	-- side): a clone statement's frame-interior `leaq K(%rsp), %reg` (0 < K < d
	-- — the chacha 4x/4xop/8x bodies cloned into each other, each re-running its
	-- own `leaq 0x100(%rsp),%rcx` behind the cloned dispatch) parks a carrier
	-- keyed in the clone's BANDED coordinate space (see the banding pass below):
	-- the clone executes in the jumper's stack context with the jumper's frame
	-- dead, so its private frame is displaced into the band — the same
	-- displacement every clone rsp-relative access gets — and the carrier's
	-- accesses resolve statically to those banded homes (the carrier spelling
	-- and the rsp-relative spelling of one byte land on one home: the K offset
	-- cancels between the parked depth and the displacement). Marking the lea
	-- x86_leaSave also excludes it from escape rejection (a non-carrier lea of
	-- a stack address is rejected — there is no promotion anymore), which the
	-- carrier's legality proof makes sound. A carrier use that is NOT a
	-- legal single-memory-base access still fails closed at the use, exactly
	-- like every other carrier.
	for i, st in body do
		if st.kind == "insn" and st.b2clone and st.x86_leaSave == nil
			and not st.x86StackBufLeaValue
			and not st.x86StackBufLeaValueComputed
			and not (st.allocaPlace or st.allocaOwned)
			and x86.baseMnemonic(string.lower(st.mnemonic)) == "lea" then
			local dd = depthIn[i]
			local ops = st.operands
			local mem = ops[2]
			if type(dd) == "number" and not isDynDepth(dd) and (dd ~= D0 or ops[1].name == "rbp")
				and ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
				and (ops[1].name ~= "rbp" or not usesRbp)
				and mem and mem.kind == "mem" and mem.base and mem.base.rc == "sp"
				and not mem.index and not mem.dispSym then
				local k = mem.dispVal or 0
				local pk = st.localClone and (k - 8) or k
				if pk ~= dd and pk > 0 then
					st.x86_leaSave = true
				end
			end
		end
	end

	-- B2 shared-tail inner-frame extent (tailcall.luau st.b2clone): clone
	-- accesses key into per-clone bands (b2band above), so extend the frame
	-- to cover the banded extents: every banded access addresses scratch its
	-- own `sub` allocated below the jump-site rsp (or the red zone), so backing
	-- [off, off+width) with real synthesized-frame space is sound. The width is
	-- the access's own width — the taint scan's exact computation (GPR suffix /
	-- PTR size, FP knowledge width), falling back to the maximum vector width
	-- (64) when it is unknown or uncomputable — so the extension covers exactly
	-- what each access touches and over-covers only the unknowable remainder.
	-- BLOAT BOUND: frameSize grows past the true need by at most 63 bytes (one
	-- fallback-width access), plus each band's 128-byte separation gap from the
	-- layout above; bands never overlap by construction, so the total waste is
	-- bounded by 64 + 128 * (#bands). Runs after all annotation, so the
	-- carrier-normalized x86_aliasOff is used when present (absolute,
	-- unbanded), exactly like the rewrite's slot path.
	do
		local maxNeed = frameSize
		for _, st in body do
			if st.kind == "insn" and st.b2clone and not st.x86_prologue then
				local d = st.x86_depth
				if d ~= nil and not isDynDepth(d) then
					for _, o in (st.operands or {}) do
						if o.kind == "mem" and o.base and o.base.kind == "reg"
							and not o.index and not o.dispSym
							and (o.base.rc == "sp" or st.x86_aliasOff ~= nil) then
							local off = (st.x86_aliasOff ~= nil) and st.x86_aliasOff
								or ((o.dispVal or 0) + D0 - d + b2band(st))
							-- Exactly the taint scan's width (pcall: the scan
							-- never saw some shapes this loop does — an FP
							-- read of an incoming yolo argument, a dropped
							-- save-store — and accessWidth errors on a
							-- contradicting PTR annotation, which must not
							-- newly reject an otherwise accepted shape here).
							local w = nil
							do
								local ok, ww = pcall(function()
									local m = string.lower(st.mnemonic)
									local fpe = fpKnowledge.lookup(m)
									if isFpInsn(fpe, st) then
										return fpKnowledge.accessWidth(m, st.operands, st.raw)
									end
									return gprStackWidth(m, o)
								end)
								if ok then w = ww end
							end
							if type(w) ~= "number" then w = 64 end
							if off + w > maxNeed then maxNeed = off + w end
						end
					end
				end
			end
		end
		frameSize = maxNeed
	end

	-- Lea-save target window (the provable-depth carrier relaxation): a
	-- `leaq K(%rsp), %reg` recognized as a carrier ONLY BY the new provable-
	-- depth case (a frame-interior or above-entry anchor at a perturbed
	-- provable depth — or any such lea into %rbp when %rbp is not the frame
	-- pointer) must anchor INSIDE the frame's real extent — the same window
	-- the rewrite's escape rejection uses for the prologue-depth leas it
	-- rejects (`off >= 0 and off < frameSize`), extended below the frame base by the
	-- access window's red-zone rule (`min(-128, D0 - d - 128)`, the same
	-- lower bound the carrier's accesses are bounds-checked against at the
	-- lea's own depth). A lea anchoring OUTSIDE that window behaves exactly
	-- as before the relaxation: it is not a carrier, and the rewrite rejects
	-- it as a frame-address escape. Runs here so `frameSize` is the FINAL
	-- extent (the B2 band extension above can grow it): a deep anchor inside
	-- the banded extent is a carrier, matching the banded accesses'
	-- bounds-checking. The historical forms (K == d: the entry-rsp park;
	-- K <= 0: the below-rsp anchor) predate the relaxation and keep their
	-- unconditional recognition — their windows were accepted shapes before.
	do
		for i, st in body do
			if st.kind == "insn" and st.x86_leaSave then
				local dd = depthIn[i]
				if type(dd) == "number" and not isDynDepth(dd) then
					local ops = st.operands
					local mem = ops[2]
					if ops[1] and ops[1].kind == "reg" and mem and mem.kind == "mem" then
						local k = mem.dispVal or 0
						local pk = st.localClone and (k - 8) or k
						-- Feature (D0 interior lea-save carriers): the D0 relaxation
						-- is a "new" form too — its window is decided here, with the
						-- same interior rule (the lea's normalized offset K must lie
						-- inside the frame's real extent). The scan guard mirrors
						-- the fixpoint's `leaD0` conditions exactly.
						local isD0 = not st.localClone and not st.b2clone
							and dd == D0 and ops[1].name ~= "rbp"
							and k > 0 and k ~= dd
							and leaSaveUsesMemoryOnly(st, i, dd - k)
						local isNew = not st.localClone
							and (ops[1].name == "rbp" or isD0
								or (dd ~= D0 and not (pk == dd or pk <= 0)))
						if isNew then
							local ot = k + D0 - dd
							local otLow = math.min(-128, D0 - dd - 128)
							if ot < otLow or ot >= frameSize then
								error("sarcasm: `leaq " .. k .. "(%rsp)` anchors outside the"
									.. " frame (normalized offset " .. ot .. ", window ["
									.. otLow .. ", " .. frameSize .. ")) — taking the address"
									.. " of the stack frame cannot be proven safe: "
									.. (st.raw or st.mnemonic))
							end
						end
					end
				end
			end
		end
	end

	-- Containing-granule scan (partial slot access support; see slotGranules):
	-- record every normalized frame offset whose 8-byte plain GPR stack access
	-- virtualizes into a slot web (the rewrite's final virtualization). Runs LAST so
	-- every x86_* mark and the FP-taint clusters (fpInfo) are final. The conditions
	-- mirror the rewrite's slot path for width-8 GPR accesses; any shape this scan is
	-- unsure about is left UNMARKED — a false negative keeps the historical per-offset
	-- web behavior (conservative), while a false positive would misroute a narrow
	-- access into a web that does not exist, so every excluded shape here is one the
	-- rewrite does not virtualize.
	do
		for _, st in body do
			if st.kind == "insn" and not st.x86_prologue and not st.localClone
				and not st.b2clone and st.x86_depth ~= nil
				and not st.x86_spSave and not st.x86_saveStore and not st.x86_carrierLoad
				and not st.x86_leaSave and not st.x86_dropRestore
				and not st.x86_regionCarrier and not st.x86_spRecover
				and not st.x86StackBufAccess and not st.x86StackBufIdx
				and not st.x86StackBufRep and not st.x86StackBufLea
				and not st.x86_preAnd then
				local bm = x86.baseMnemonic(string.lower(st.mnemonic))
				-- leas never virtualize (address computation); push/pop spell their
				-- slot traffic through dedicated rewrite paths (a spill push/pop's
				-- slot stays outside the granule map — conservative).
				if bm ~= "lea" and bm ~= "push" and bm ~= "pop" and bm ~= "leave"
					and bm ~= "cmpxchg8b" and bm ~= "cmpxchg16b" then
					local mem, memCount = nil, 0
					for _, o in (st.operands or {}) do
						if o.kind == "mem" then mem = o; memCount += 1 end
					end
					local fpe = fpKnowledge.lookup(string.lower(st.mnemonic))
					if memCount == 1 and mem and mem.base and mem.base.kind == "reg"
						and not mem.index and not mem.dispSym and not isFpInsn(fpe, st) then
						local w = mem.size or gprStackWidth(string.lower(st.mnemonic), mem)
						if w == 8 then
							local off
							local d = st.x86_depth
							local yoloRead = false
							if st.x86_aliasOff ~= nil then
								off = st.x86_aliasOff
							elseif mem.base.rc == "sp" then
								-- rsp-relative needs a provable depth (a "dyn" depth cannot
								-- key a slot; the rewrite rejects those accesses anyway)
								if not isDynDepth(d) then
									off = (mem.dispVal or 0) + D0 - d
									-- a direct entry-time read of an incoming yolo stack
									-- argument redirects to the argument's slot band, not
									-- an ordinary frame slot (the rewrite's yolo gate)
									yoloRead = yoloArgSlotOf(st, "rsp", d) ~= nil
								end
							elseif usesRbp and mem.base.name == "rbp" and st.x86_fp == "est" then
								-- rbp-relative: static regardless of the rsp depth (rbp is
								-- the frame pointer) — the rewrite's second exception
								off = rbpNorm + (mem.dispVal or 0)
							end
							if off ~= nil and not yoloRead and not regionOf(off, false)
								and not (fpInfo and fpInfo.lookup(off, 8)) then
								-- Save-slot overlap (the rewrite's alias model: an access
								-- overlapping an outstanding pushed save never virtualizes
								-- into an ordinary web). A "dyn" save state cannot be
								-- checked here — leave unmarked (conservative).
								local aliased = false
								local sv = st.x86_saves
								if sv ~= nil and sv ~= "dyn" then
									for _, s in sv do
										if usesRbp and s.name == "rbp" then continue end
										if s.spill then continue end
										local sd = D0 - s.depth
										if off < sd + 8 and off + 8 > sd then aliased = true break end
									end
								end
								if not aliased then slotGranules[off] = true end
							end
						end
					end
				end
			end
		end
	end

	-- 5th return: the incoming yolo stack-argument info for the transform (slot
	-- count, which slots FP instructions read, the materialized area's base offset
	-- and byte size — base is nil when no FP instruction reads one). (The
	-- historical 6th return — the fixed-frame escape flag that fed the removed
	-- D9 promotion — is gone.)
	return frameSize, 0, usesRbp, fpInfo,
		{ count = yoloCount, fpRead = fpYolo, base = yoloBase, bytes = yoloBytes }
end

function x86_64_frame.rewrite(st, ctx)
	local emit = ctx.emit
	local usesRbp = ctx.usesFp
	local m = string.lower(st.mnemonic)
	local b = x86.baseMnemonic(m)
	local ops = st.operands

	-- `enter`/`enterq` (push rbp; mov rsp,rbp; sub imm,rsp as ONE instruction) is frame
	-- setup sarcasm does not model: passed through it would shift rsp and clobber rbp
	-- inside the synthesized Fil-C frame (the frame-link restore would read garbage and
	-- `ret` would pop a wild address). Reject it — frame setup must be the canonical
	-- push %rbp / mov %rsp,%rbp / sub $imm,%rsp form the prologue analysis recognizes.
	if b == "enter" or b == "enterq" then
		error("sarcasm: `enter` is not supported (cannot prove safety; use the canonical"
			.. " push %rbp / mov %rsp,%rbp / sub $imm,%rsp frame setup): " .. (st.raw or m))
	end

	-- RFLAGS stack transfers (pushf/pushfq[/pushfd], popf/popfq[/popfd]; the
	-- 16-bit pushfw/popfw stay rejected in x86_64_fp). The word is a SPILL stack
	-- word (the transfer analysis models it exactly like a caller-saved
	-- register's spill push, so the save-slot alias model and the dropped-save
	-- model stay away from it and ordinary slot traffic at the same depth shares
	-- the word's slot web). The value rides the virtual FLAGS register
	-- (x86.FLAGS_REG_NUM): the transform lowers the kept statement into the
	-- pushfq/popq + pushq/popfq round-trip its own injected checks use, and the
	-- file/load below connects that value to the word's slot web.
	if x86.isFlagStackPush(m) then
		local d = st.x86_depth
		if d == nil then return end  -- unreachable (dead code): drop like any dead statement
		if isDynDepth(d) then
			error("sarcasm: " .. m .. " while the stack pointer is perturbed or unknown"
				.. " (the flags word's slot cannot be proven; cannot prove safety): "
				.. (st.raw or m))
		end
		local off = D0 - (d + 8)
		if not slotOffSafe(off) then
			error("sarcasm: " .. m .. " frame area collides with the incoming"
				.. " stack-argument slot band (cannot prove safety): " .. (st.raw or m))
		end
		-- The statement stays: the transform materializes RFLAGS into the flags
		-- web here (pushfq; pop <web>), then this file store parks it in the
		-- word's slot.
		emit(st)
		emit({ kind = "insn", mnemonic = "movq",
			operands = { ctx.slotReg(off, 64), x86.flagsWordOperand() },
			raw = st.raw, line = st.line })
		return
	end
	if x86.isFlagStackPop(m) then
		local d = st.x86_depth
		if d == nil then return end  -- unreachable (dead code): drop like any dead statement
		if not st.x86_flagsPop then
			error("sarcasm: " .. m .. " with no provable stack word on top of the stack"
				.. " (the word it loads must be an outstanding pushfq word or a"
				.. " materialized spill push at the current depth on every path; popping"
				.. " a dropped register save, an empty or divergent stack, or a word at"
				.. " an unknown depth cannot be proven safe): " .. (st.raw or m))
		end
		local off = D0 - d
		if not slotOffSafe(off) then
			error("sarcasm: " .. m .. " frame area collides with the incoming"
				.. " stack-argument slot band (cannot prove safety): " .. (st.raw or m))
		end
		-- Reload the word into the flags web first (an ordinary slot load — the
		-- value flows slot web -> flags web), then the statement: the transform
		-- lowers it into push <web>; popfq (stack-neutral on its own, so a path
		-- that skips it keeps rsp balanced).
		emit({ kind = "insn", mnemonic = "movq",
			operands = { x86.flagsWordOperand(), ctx.slotReg(off, 64) },
			raw = st.raw, line = st.line })
		emit(st)
		return
	end

	-- A stack-register name that did not parse as a REGISTER: in an AT&T-parsed file
	-- a bare (un-%-ed) operand field falls through to the symbol parse, so a
	-- dest-first Intel spelling inside an AT&T file (`lea rsp, [rax+15]`) parses as
	-- a symbol source over a non-register destination. Every other mnemonic treats
	-- a symbol operand as memory and is rejected as global data access, but a lea's
	-- symbol operands are address materializations — so the model here sees neither
	-- a %rsp write nor a memory access: the rsp-write enforcement, the depth
	-- analysis, and the escape scan all pass, and the statement is emitted
	-- VERBATIM — a live hardware-%rsp write wherever the text assembles, and a bare
	-- `as` "operand type mismatch" (no sarcasm diagnostic) where it does not. A lea
	-- whose destination is not a register never assembles anyway (GAS lea computes
	-- into a register), so gating on that keeps every assemblable lea (including
	-- `leaq myglobal, %rax` address materializations) working while any
	-- stack-register name hiding in the operands of the malformed spelling — an
	-- unmodeled %rsp write, whatever operand carries it — is rejected with the
	-- stack-pointer-write error. (The prologue prefix scan treats such a statement
	-- as transparent, but the rewrite runs on prologue statements too, so this
	-- rejection covers the prefix as well.)
	if b == "lea" and not (ops[1] and ops[1].kind == "reg") then
		for _, o in ops do
			if o.kind == "sym" then
				local t = string.gsub(string.lower(o.text or ""), "^%%", "")
				t = string.gsub(string.gsub(t, "^%[", ""), "%]$", "")
				if string.match(t, "^[re]?[sb]p%f[^%w]") then
					error("sarcasm: writing the stack pointer through an operand that parsed"
						.. " as a symbol rather than a register (mixed AT&T/Intel operand"
						.. " syntax) cannot be proven safe: " .. (st.raw or m))
				end
			end
		end
	end

	-- Vector-indexed memory (gather/scatter: an xmm-class register as the memory base
	-- or index) touches multiple discrete addresses that cannot be bounds-checked.
	-- Reject it cleanly. (The x86_64_fp table also hard-rejects the gather/scatter
	-- mnemonics; this catches any vector-indexed form regardless of mnemonic.)
	for _, o in ops do
		if o.kind == "mem"
			and ((o.base and o.base.rc == "xmm") or (o.index and o.index.rc == "xmm")) then
			error("sarcasm: vector-indexed memory access (gather/scatter) is not supported"
				.. " (cannot prove memory safety): " .. (st.raw or m))
		end
	end

	-- A stack-alias save that an annotated rep consumes (see the buffer scan's
	-- phase (e)): emitted as a REAL `lea disp'(%rsp), %reg` into the lowered
	-- buffer region instead of being dropped as a phantom value. The lea gives
	-- the register a fresh web whose value is the materialized buffer address;
	-- the transform's rep lowering pins that web into the implicit register.
	if st.x86StackBufLea then
		local ml = st.x86StackBufLea
		local memop = { kind = "mem",
			base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
			scale = 1, dispVal = ml.disp, stackBuf = true }
		emit({ kind = "insn", mnemonic = "lea", operands = { ops[1], memop },
			raw = st.raw, line = st.line })
		return
	end

	-- `.alloca size, align, result`: convert frame-relative (spill-slot) memory
	-- operands to slot pseudo-registers in place, keeping the mnemonic. Only plain
	-- 8-byte slot references convert: symbolic/indexed displacements, non-frame
	-- bases, unknown depths, FP-materialized ranges, yolo-argument-band overlap,
	-- and (for the result) push-save overlap are clean rejections. A size/align
	-- read exactly aliasing a live (non-redefined) push
	-- save names the pushed register itself (its web holds the slot's value).
	if m == "alloca" then
		local ad = st.x86_depth
		local anewops = {}
		for oi, o in ops do
			if o.kind ~= "mem" then table.insert(anewops, o)
			else
				local isResult = (oi == 3)
				if o.dispSym or o.index then
					error("sarcasm: `.alloca` spill-slot operand must be a plain disp(%rsp)/disp(%rbp)"
						.. " reference (no symbol, no index): " .. (st.raw or m))
				end
				local frameBase = (o.base and o.base.rc == "sp")
				or (usesRbp and o.base and o.base.name == "rbp" and st.x86_fp == "est")
				if not frameBase then
					error("sarcasm: `.alloca` operand is not a register, immediate, or frame spill slot"
						.. " (a frame-relative disp(%rsp)/disp(%rbp) reference): " .. (st.raw or m))
				end
				if ad == nil or isDynDepth(ad) then
					error("sarcasm: `.alloca` with spill-slot operands while the stack pointer"
						.. " is perturbed or unknown (cannot prove which frame slot this is): " .. (st.raw or m))
				end
				local perturbed = false
				if not st.x86_prologue and ad ~= D0 then perturbed = true end
				local aoff = (o.dispVal or 0)
				if o.base.rc ~= "sp" then aoff = rbpNorm + aoff
				elseif st.localClone then aoff = aoff + D0 - ad - 8
				elseif perturbed then aoff = aoff + D0 - ad end
				local lowB = -128
				if perturbed then lowB = math.min(-128, D0 - ad - 128) end
				ctx.checkBounds(aoff, 8, isResult, st.raw, lowB)
				if ctx.fpInfo and ctx.fpInfo.lookup(aoff, 8) then
					error("sarcasm: `.alloca` spill-slot operand overlaps an FP-materialized stack range"
						.. " (cannot share one home): " .. (st.raw or m))
				end
				if not slotOffSafe(aoff) then
					error("sarcasm: `.alloca` spill-slot operand reaches the incoming stack-argument"
						.. " slot band (cannot prove safety): " .. (st.raw or m))
				end
				if ctx.regionOf(aoff, false) then
					-- Unreachable today: regions have no sources, so the lookup
					-- always misses (kept for the region plumbing's sake).
					error("sarcasm: `.alloca` spill-slot operand lands in a GC-allocation region"
						.. " (slots there do not virtualize; historical path — the"
						.. " region plumbing is inert): " .. (st.raw or m))
				end
				-- Push-save overlap: the slot holds a pushed register, not an ordinary spill.
				local asv = st.x86_saves
				if asv and asv ~= "dyn" and #asv > 0 then
					for _, se in asv do
						if not (usesRbp and se.name == "rbp") and not se.spill then
							local sd = D0 - se.depth
							if aoff < sd + 8 and aoff + 8 > sd then
								if aoff ~= sd or isResult or se.redefined then
									error("sarcasm: `.alloca` spill-slot operand overlaps a pushed register's"
										.. " save slot (spill to an ordinary frame slot instead): " .. (st.raw or m))
								else
									-- Exact 8-byte read of a live save: name the pushed register itself.
									local parsed = x86parse.classifyReg(se.name)
									if not (parsed and parsed.rc == "gpr") then
										error("sarcasm: `.alloca` spill-slot operand overlaps an unmodelable save"
											.. " slot: " .. (st.raw or m))
									end
									table.insert(anewops, { kind = "reg", rc = "gpr", num = parsed.num,
										width = 64, name = parsed.name })
									asv = nil
								end
							end
						end
					end
				end
				if asv then table.insert(anewops, ctx.slotReg(aoff, 64)) end
			end
		end
		emit({ kind = "insn", mnemonic = st.mnemonic, operands = anewops,
			raw = st.raw, line = st.line, annotation = st.annotation, dotAlloca = st.dotAlloca })
		return
	end

	-- Post-prologue enforcement (annotations from analyzeFrame; see the file header). The
	-- prologue prefix itself is handled by the legacy drops below — frame setup there is
	-- accounted for in the frame geometry and simply dropped.
	local d = st.x86_depth
	if not st.x86_prologue then
		if b == "leave" then
			if not usesRbp then
				error("sarcasm: `leave` without a frame pointer established in the prologue"
					.. " cannot be proven safe: " .. (st.raw or m))
			end
			-- `leave` is an epilogue teardown only when the teardown analysis proved that
			-- it STARTS (or sits inside) a region leading straight to a `ret` through
			-- callee-saved pops and frame-touch-free computation, with the frame pointer
			-- provably established on entry. Any other `leave` is a MID-FUNCTION one and
			-- stays rejected even though the plain rsp-recovery forms (`movq %rbp, %rsp`,
			-- `leaq -N(%rbp), %rsp`) are now accepted mid-function: hardware `leave` also
			-- restores the CALLER's frame pointer into %rbp, breaking the static-frame-
			-- pointer premise every later rbp-relative virtualization rests on — silently
			-- miscompiled reads and writes. The plain recoveries touch only %rsp, which
			-- is why they can be dropped anywhere.
			if not (st.x86_teardownStart or st.x86_teardown) or st.x86_fp ~= "est" then
				error("sarcasm: mid-function `leave` cannot be proven safe (a leave that does"
					.. " not lead directly to a ret may observe the caller's frame): "
					.. (st.raw or m))
			end
			return
		end
		if (b == "sub" or b == "add") and ops[1] and ops[1].rc == "sp" then
			if ops[2] and ops[2].kind == "imm"
				and (st.x86_teardownStart or st.x86_midSp) then
				return  -- verified epilogue teardown / proven mid-function adjustment: drop
			end
			error("sarcasm: mid-function stack-pointer adjustment cannot be proven safe"
				.. " (frame teardown must be an epilogue `addq $imm, %rsp` leading to `ret`;"
				.. " dynamic allocation must use the `.alloca` directive`): " .. (st.raw or m))
		end
		if b == "and" and ops[1] and ops[1].rc == "sp" then
			-- A proven mid-function `and $-N, %rsp` (power of two N >= 16 at a
			-- statically known depth with dead flags): rounding for vector
			-- alignment, dropped with the alignment recorded by the analysis.
			if st.x86_midAnd then return end
			error("sarcasm: mid-function `and` on the stack pointer cannot be proven safe"
				.. " (only `and $-N, %rsp` for a power of two N >= 16 at a known depth"
				.. " with dead flags is supported; dynamic allocation must use the"
				.. " `.alloca` directive`): " .. (st.raw or m))
		end
		if b == "mov" and ops[1] and ops[1].name == "rbp" and ops[2] and ops[2].rc == "sp" then
			error("sarcasm: establishing %rbp as a frame pointer outside the prologue cannot"
				.. " be proven safe: " .. (st.raw or m))
		end
		if b == "lea" and ops[1] and ops[1].kind == "reg" and ops[1].name == "rbp"
			and ops[2] and ops[2].kind == "mem" and ops[2].base and ops[2].base.rc == "sp"
			and not ops[2].index and not ops[2].dispSym and (ops[2].dispVal or 0) == 0 then
			-- `leaq 0(%rsp), %rbp` mid-body: the lea spelling of the rejected
			-- re-establishment (same proof failure as the mov form above).
			error("sarcasm: establishing %rbp as a frame pointer outside the prologue cannot"
				.. " be proven safe: " .. (st.raw or m))
		end
		if b == "mov" and ops[1] and ops[1].rc == "sp" then
			if ops[2] and ops[2].name == "rbp" and usesRbp then
				-- Recovering %rsp from the frame pointer is sound wherever the frame
				-- pointer is provably established, and is DROPPED: the recovery cannot
				-- perturb any rbp-relative virtualization (rbp is untouched), every
				-- rsp-relative access stays gated on the revived depth, every pop stays
				-- gated on save pairing, and every return stays gated on depth 0. This
				-- accepts mid-function alloca "frees" as readily as the classic epilogue
				-- teardown — no straight-line-to-ret proof is required. (When rbp is
				-- NOT the frame pointer it is an ordinary carrier register: the plain
				-- saved-rsp recovery path below handles it.)
				if st.x86_fp == "est" then return end
				error("sarcasm: restoring %rsp from %rbp while the frame pointer is not"
					.. " provably established cannot be proven safe: " .. (st.raw or m))
			end
			if st.x86_spRecover then
				-- Recovery from a register the prologue saved %rsp into: the value is
				-- fixed at function entry (the fixpoint proved the save still reaches
				-- this point unredefined), so dropping the write is exactly as sound as
				-- dropping the rbp forms.
				return
			end
			local rn = (ops[2] and ops[2].name) and "%" .. ops[2].name or "a register"
			error("sarcasm: restoring %rsp from " .. rn .. " requires a prologue"
				.. " `movq %rsp, " .. rn .. "` that has not been redefined (or recover it"
				.. " from %rbp, or allocate with the `.alloca` directive): "
				.. (st.raw or m))
		end
		if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
			and ops[2].base and ops[2].base.name == "rbp" and usesRbp
			and not ops[2].index and not ops[2].dispSym then
			-- Same proof as the movq form: a frame-pointer-relative rsp recovery, dropped.
			if st.x86_fp == "est" then return end
			error("sarcasm: restoring %rsp from %rbp via lea while the frame pointer is not"
				.. " provably established cannot be proven safe: " .. (st.raw or m))
		end
		if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
			and not ops[2].index and not ops[2].dispSym then
			if ops[2].base and ops[2].base.rc == "sp" then
				-- `leaq +N(%rsp), %rsp`: the lea spelling of the constant epilogue
				-- teardown — accepted exactly like `addq $imm, %rsp` when the teardown
				-- analysis verified the span. `leaq ±N(%rsp), %rsp` at a statically
				-- known depth with a non-negative resulting depth is a proven
				-- mid-function adjustment (the fixed-size inner-frame shape);
				-- anything else is unprovable.
				if (ops[2].dispVal or 0) >= 0 and st.x86_teardownStart then return end
				if st.x86_midSp then return end
				error("sarcasm: mid-function stack-pointer adjustment cannot be proven safe"
					.. " (frame teardown must be an epilogue `addq $imm, %rsp` /"
					.. " `leaq $imm(%rsp), %rsp` leading to `ret`; dynamic allocation must"
					.. " use `alloca`): " .. (st.raw or m))
			end
			if ops[2].base and ops[2].base.kind == "reg"
				and (ops[2].base.name ~= "rbp" or not usesRbp) then
				-- `leaq K(%reg), %rsp`: recovery from a saved-rsp carrier (the analysis
				-- proved the base register holds the save value); dropped with it.
				if st.x86_spRecover then return end
				local rn = "%" .. ops[2].base.name
				error("sarcasm: restoring %rsp from " .. rn .. " requires a prologue"
					.. " `movq %rsp, " .. rn .. "` (or lea) save that has not been redefined"
					.. " (or recover it from %rbp, or allocate with the `.alloca` directive): "
					.. (st.raw or m))
			end
		end
		if b == "push" then
			-- Balanced callee-saved save/restore pairs are dropped; the rsp-depth analysis
			-- rejects any stack access or return that would observe the shifted rsp.
			if ops[1] and ops[1].kind == "reg" and x86.CALLEE_SAVED[ops[1].name] then return end
			-- A mid-function SPILL push (a caller-saved register parked on the stack
			-- and reloaded by a matching pop-as-load — the rsaz-avxifma `pushq %r9` /
			-- `pushq %r8` shape) at a statically known depth: the slot is an ordinary
			-- frame slot at normalized offset D0 - (d+8). The store ALWAYS defines
			-- the slot's virtual web — the web is what the matching pop reloads, and
			-- it carries the pushed value's capability (a spilled POINTER keeps its
			-- lower, which a raw memory store would silently drop). When the slot
			-- falls inside an FP-tainted cluster the store ADDITIONALLY writes the
			-- real materialized bytes, so the cluster's real-memory view of the
			-- address stays exact (a live-overlap with the cluster's own traffic is
			-- rejected by analyzeFrame; a partial overlap cannot keep one home).
			if ops[1] and ops[1].kind == "reg" and ops[1].rc == "gpr"
				and (ops[1].width == 64 or ops[1].width == nil)
				and d ~= nil and not isDynDepth(d) then
				local off = D0 - (d + 8)
				if not slotOffSafe(off) then
					error("sarcasm: mid-function spill-push frame area collides with the"
						.. " incoming stack-argument slot band (cannot prove safety): "
						.. (st.raw or m))
				end
				local slot = ctx.slotReg(off, 64)
				emit({ kind = "insn", mnemonic = "movq", operands = { slot, ops[1] },
					raw = st.raw, line = st.line })
				local final = ctx.fpInfo and ctx.fpInfo.lookup(off, 8)
				if final then
					emit({ kind = "insn", mnemonic = "movq", operands = {
						{ kind = "mem",
							base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
							scale = 1, dispVal = final, size = 8, frameSlot = true },
						ops[1] },
						raw = st.raw, line = st.line })
				end
				return
			end
			error("sarcasm: mid-function push of a non-callee-saved operand cannot be proven"
				.. " safe: " .. (st.raw or m))
		end
		if b == "pop" then
			-- A pop of an outstanding spill slot (the annotation pass proved the
			-- slot at the current depth is a caller-saved spill, not a dropped
			-- save): an ordinary slot LOAD into the destination. The load ALWAYS
			-- reads the slot's virtual web — the web holds exactly what the spill
			-- store wrote, WITH its capability (a spilled pointer keeps its
			-- lower). analyzeFrame rejects any FP-tainted cluster traffic that
			-- would overlap the slot while its entry is live, so the web view
			-- can never go stale against the cluster's real memory.
			if st.x86_spillPop then
				local off = D0 - d
				if not slotOffSafe(off) then
					error("sarcasm: mid-function spill-pop frame area collides with the"
						.. " incoming stack-argument slot band (cannot prove safety): "
						.. (st.raw or m))
				end
				local slot = ctx.slotReg(off, 64)
				emit({ kind = "insn", mnemonic = "movq", operands = { ops[1], slot },
					raw = st.raw, line = st.line })
				return
			end
			if ops[1] and ops[1].kind == "reg" and x86.CALLEE_SAVED[ops[1].name] then
				-- Popping the frame pointer is sound only as its save's restore: inside a
				-- verified epilogue teardown, or provably paired with the outstanding save
				-- of rbp (e.g. the leaf `popq %rbp; ret` with no frame cleanup — rsp still
				-- sits at the save slot). After such a pop the depth analysis rejects any
				-- further stack access (rsp/rbp-relative) and any unbalanced return, so
				-- only ordinary computation may follow. An UNPAIRED rbp-pop reads a slot
				-- that is not the saved frame pointer (it can be another register's save
				-- or the return address): reject.
				if ops[1].name == "rbp" and usesRbp and not st.x86_teardown and st.x86_unpairedPop then
					error("sarcasm: popping the frame pointer with no matching earlier save"
						.. " cannot be proven safe (it is not an epilogue restore): "
						.. (st.raw or m))
				end
				-- Dropping the pop is sound only when it provably restores a matching
				-- dropped push of the same register on every path (balanced save region,
				-- epilogue restore of a prologue save, or verified teardown). An unpaired
				-- pop's load would be silently lost — and the slot it reads is not that
				-- register's save (it can be the saved frame pointer or the return
				-- address).
				if st.x86_unpairedPop then
					error("sarcasm: pop with no matching earlier push of the same register"
						.. " cannot be proven safe (dropping it would lose the load): "
						.. (st.raw or m))
				end
				return
			end
			error("sarcasm: mid-function pop into a non-callee-saved register cannot be"
				.. " proven safe: " .. (st.raw or m))
		end
		-- d is nil for unreachable instructions (dead code: no depth checks — the drops and
		-- rejections above still apply, and surviving accesses stay virtualized).
		-- A return at an UNKNOWN depth is accepted only inside a verified epilogue
		-- teardown (x86_teardown: everything in the span is dropped and the synthesized
		-- epilogue owns the real %rsp — see the pop pairing above); anywhere else a
		-- return must provably sit at the entry rsp.
		-- A local subroutine's ret (st.localRet) is exempt: it is not a return — the
		-- no-push clone executes at the caller's depth and branches to the
		-- continuation, which resumes at exactly that depth. A cross-function-jump
		-- synthetic epilogue ret (st.tailRet — tailcall.luau) is exempt too: it sits
		-- at the tail branch's (arbitrary) virtual depth, but the body's stack is
		-- virtual, so the synthesized epilogue — which owns the real %rsp — tears
		-- the frame down correctly regardless. A shared-tail clone's ret
		-- (st.cloneRet — tailcall.luau) is exempt the same way: the clone-balance
		-- check above (which runs on every analyzeFrame run — every run passes
		-- the same always-empty regions list, including the run whose marks this
		-- rewrite consumes) proved it sits at exactly its clone entry's depth
		-- (the jump-site depth, with the clone's inner frame torn down) — or at
		-- the entry rsp inside a verified epilogue teardown span, which restores
		-- the pushed saves itself — and the body's stack is virtual, so the
		-- ordinary return path tears the frame down correctly regardless.
		-- MACHINE CHECK: the cloneRet exemption below asserts the balance proof ran
		-- on the marks this rewrite consumes (cloneRetProofRan — set by analyzeFrame;
		-- its `regions ~= nil` gate is vestigial: every run passes the always-empty
		-- list, so the flag is set on every run — the assert stays loud only if a
		-- future caller ever passes nil).
		if st.cloneRet then
			assert(cloneRetProofRan,
				"sarcasm: internal error: shared-tail clone return consumed without"
				.. " the clone-balance proof (region-less analysis marks reached the"
				.. " rewrite): " .. (st.raw or m))
		end
		if b == "ret" and not st.localRet and not st.tailRet and not st.cloneRet and d ~= nil and d ~= 0
			and not (isDynDepth(d) and st.x86_teardown) then
			error("sarcasm: return with a perturbed or unknown stack pointer cannot be proven"
				.. " safe: " .. (st.raw or m))
		end
		if b == "jmp" and d ~= nil and d ~= 0 and not (ops[1] and ops[1].kind == "sym") then
			error("sarcasm: indirect tail jump with a perturbed or unknown stack pointer"
				.. " cannot be proven safe: " .. (st.raw or m))
		end
	end

	-- drop CET marker and frame setup/teardown
	if b == "nop" then
		if m == "endbr64" then return end
		-- nop/nopw/nopl padding: zero-effect passthrough. Their operand (when
		-- present — `nopw 0x0(%rax,%rax,1)`) is a dummy encoding hint, NOT a memory
		-- access, so it must not ride the slot-virtualization / bounds-check
		-- machinery below: a virtualized slot would render a random web's register
		-- into the padding encoding, and a dummy (%rsp) operand would be treated as
		-- a real frame access.
		emit(st)
		return
	end
	-- (A `leave` is never prologue-transparent — the prologue prefix scan above ends at
	-- one — and every post-prologue `leave` is handled by the enforcement block above, so
	-- this legacy drop cannot be reached for it. It stays consistent with that block so a
	-- future prologue-scan change cannot silently drop an unproven `leave` here. The same
	-- holds for the %rsp-recovery forms below: the prologue scan ends at any rsp write,
	-- so they too are always handled — and gated on the frame-pointer state — above.)
	if b == "leave" then
		if not (st.x86_teardownStart or st.x86_teardown) or st.x86_fp ~= "est" then
			error("sarcasm: mid-function `leave` cannot be proven safe (a leave that does"
				.. " not lead directly to a ret may observe the caller's frame): "
				.. (st.raw or m))
		end
		return
	end
	if (b == "sub" or b == "add") and ops[1] and ops[1].rc == "sp" then return end
	-- Prologue `and $-N, %rsp` alignment (accepted by the prefix scan): dropped,
	-- the recorded alignment keeps the synthesized frame aligned.
	if b == "and" and ops[1] and ops[1].rc == "sp" and st.x86_prologue then return end
	-- `leaq -N(%rsp), %rsp` prologue allocation / `leaq +N(%rsp), %rsp` teardown (the
	-- lea spellings; post-prologue ones were handled — and gated — above).
	if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
		and ops[2].base and ops[2].base.rc == "sp" and not ops[2].index and not ops[2].dispSym then
		if st.x86_prologue then return end
		if (ops[2].dispVal or 0) >= 0 and st.x86_teardownStart then return end
		error("sarcasm: mid-function stack-pointer adjustment cannot be proven safe"
			.. " (frame teardown must be an epilogue `addq $imm, %rsp` / `leaq $imm(%rsp),"
			.. " %rsp` leading to `ret`; dynamic allocation must use the `.alloca` directive`): "
			.. (st.raw or m))
	end
	if b == "mov" and ops[1] and ops[1].name == "rbp" and ops[2] and ops[2].rc == "sp" then return end
	-- `leaq 0(%rsp), %rbp` in the prologue prefix: the lea spelling of the frame-pointer
	-- establishment (the prefix scan recognized it and set usesRbp/fpDepth from it) —
	-- dropped like the mov form. Post-prologue ones were handled — and rejected — above.
	if b == "lea" and ops[1] and ops[1].kind == "reg" and ops[1].name == "rbp"
		and ops[2] and ops[2].kind == "mem" and ops[2].base and ops[2].base.rc == "sp"
		and not ops[2].index and not ops[2].dispSym and (ops[2].dispVal or 0) == 0 then return end
	if b == "mov" and ops[1] and ops[1].rc == "sp" then
		if (ops[2] and ops[2].name == "rbp" and usesRbp and st.x86_fp == "est")
			or st.x86_spRecover then
			return
		end
		local rn = (ops[2] and ops[2].name) and "%" .. ops[2].name or "a register"
		error("sarcasm: restoring %rsp from " .. rn .. " requires a prologue"
			.. " `movq %rsp, " .. rn .. "` that has not been redefined (or recover it"
			.. " from %rbp, or allocate with the `.alloca` directive): "
			.. (st.raw or m))
	end
	if b == "lea" and ops[1] and ops[1].rc == "sp" and ops[2] and ops[2].kind == "mem"
		and ops[2].base and ops[2].base.name == "rbp" and usesRbp
		and not ops[2].index and not ops[2].dispSym then
		if st.x86_fp == "est" then return end
		error("sarcasm: restoring %rsp from %rbp via lea while the frame pointer is not"
			.. " provably established cannot be proven safe: " .. (st.raw or m))
	end

	-- A prologue SPILL push (a caller-saved register parked on the stack — see
	-- the prologue scan): virtualize as an ordinary slot STORE. The pushed
	-- value sits at depth d+8, i.e. normalized frame offset D0-(d+8); the
	-- slot packs into the spillBytes reservation (bounds-proven there), and
	-- reloads read the slot web like any other frame slot.
	if b == "push" and st.x86_prologue and ops[1] and ops[1].kind == "reg"
		and ops[1].rc == "gpr" and not x86.CALLEE_SAVED[ops[1].name] then
		local d = st.x86_depth
		if d ~= nil and not isDynDepth(d) then
			local slot = ctx.slotReg(D0 - (d + 8), 64)
			emit({ kind = "insn", mnemonic = "movq", operands = { slot, ops[1] },
				raw = st.raw, line = st.line })
			return
		end
		-- unreachable / unprovable depth: drop (dead code)
		return
	end

	-- push/pop of callee-saved registers (and rbp frame pointer): save/restore round-trips -> drop.
	if b == "push" and ops[1] and ops[1].kind == "reg" and (x86.CALLEE_SAVED[ops[1].name] or ops[1].name == "rbp") then return end
	if b == "pop" and ops[1] and ops[1].kind == "reg" and (x86.CALLEE_SAVED[ops[1].name] or ops[1].name == "rbp") then return end

	-- rbp/rsp-relative memory access -> slot pseudo-register (unless it lands in a region).
	local mem
	for _, o in ops do if o.kind == "mem" then mem = o end end
	-- A stack-relative operand with a SYMBOLIC displacement (`movq foo(%rsp), %rax`) has an
	-- unknown-at-compile-time target: it cannot be bounds-checked or virtualized, so reject
	-- it cleanly (both the AT&T and Intel parsers set mem.dispSym on the memory operand).
	if mem and isStackReg(mem.base, usesRbp) and mem.dispSym then
		error("sarcasm: stack-relative memory access with a symbolic displacement cannot be"
			.. " proven safe: " .. (st.raw or m))
	end

	-- Emit the rewrite of an entry-time read of incoming yolo stack-argument slot i
	-- (yoloArgSlotOf already proved the shape: a load-like instruction, plain
	-- base+disp memory operand). GPR-class reads ride a virtual slot web — the
	-- transform seeds it from the incoming fast-CC argument word (with its
	-- capability lower for a pointer argument, so pointer flow propagates exactly
	-- like a register argument). FP/vector-class reads cannot name a GPR
	-- pseudo-register, so they materialize into the reserved area the transform
	-- fills with the same incoming words (see analyzeFrame).
	local function emitYoloRead(i)
		-- The `;! ... ptr` annotation family: unlike an ordinary (writable) frame
		-- slot, an incoming stack-argument read has no store direction to seed
		-- (the argument's capability comes from the entry unpack, not from any
		-- annotated access), so no family member is honored here — reject it.
		if st.annotation and sig.PTR_ANNOTATIONS[st.annotation] then
			error("sarcasm: `" .. st.annotation .. "` on an incoming stack-argument read is not"
				.. " supported (the argument is redirected to a frame slot, not real memory): "
				.. (st.raw or m))
		end
		if st.lockPrefix then
			error("sarcasm: lock prefix on an incoming stack-argument read is not supported"
				.. " (the argument is redirected to a thread-confined frame slot): " .. (st.raw or m))
		end
		for _, o in ops do
			if o.deco and string.match(o.deco, "{%%?k[0-7]}") then
				error("sarcasm: AVX512 mask-register ({k}) incoming stack-argument reads are not"
					.. " supported (masked-off lanes may not touch memory; cannot prove memory"
					.. " safety): " .. (st.raw or m))
			end
		end
		local mem
		for _, o in ops do if o.kind == "mem" then mem = o end end
		local fpEntry = fp.lookup(m)
		if isFpInsn(fpEntry, st) then
			-- FP/vector read: materialize into the reserved area (analyzeFrame
			-- assigned yoloBase precisely because this read exists). The width is
			-- the knowledge module's authoritative one (it rejects a contradicting
			-- PTR annotation); aligned >16-byte forms are rejected like anywhere.
			local wb, fa = fp.accessWidth(m, ops, st.raw)
			if not wb then
				error("sarcasm: cannot determine the memory access width of '" .. m
					.. "' (cannot prove memory safety): " .. (st.raw or m))
			end
			if fa and fa > 16 then
				error("sarcasm: aligned 32/64-byte incoming stack-argument reads are not supported"
					.. " (the stack is only guaranteed 16-byte aligned); use the unaligned form: "
					.. (st.raw or m))
			end
			local mat = { kind = "mem",
				base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
				scale = 1, dispVal = yoloBase + 8 * i, size = wb, frameSlot = true,
				deco = mem.deco, bcastN = mem.bcastN }
			local newops = {}
			for _, o in ops do table.insert(newops, (o == mem) and mat or o) end
			emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops,
				raw = st.raw, line = st.line, annotation = st.annotation })
			return
		end
		-- GPR read: a virtual slot web, keyed by slot index (shared across widths,
		-- exactly like the ordinary frame-slot virtualization).
		local wb = gprStackWidth(m, mem)
		local slot = frameShared.yoloSlotReg(i, wb * 8)
		local newops = {}
		for _, o in ops do table.insert(newops, (o == mem) and slot or o) end
		emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops,
			raw = st.raw, line = st.line, annotation = st.annotation })
	end

	-- Memory access THROUGH a saved-rsp carrier register. Three legal shapes:
	--   * an entry-time read of an incoming yolo stack argument through an entry-rsp
	--     alias (`movq %rsp, %rax` / `leaq 0(%rsp), %rax`, then `8(%rax)` — the
	--     aesni_ocb / bn_mul_mont_gather5 prologue shape): the smap proves the
	--     register still holds the parked stack pointer on every path; redirect the
	--     read to the argument's slot;
	--   * an epilogue restore load through the carrier (the perlasm movq-restore
	--     riding the recovered pointer, `movq -48(%rsi), %r15`): the analysis proved
	--     it a save/restore pair (and unobservable when the save was redefined) and
	--     marked it x86_dropRestore — dropped like the epilogue pops;
	--   * anything else is the same escape class as reading the saved stack pointer
	--     itself (the transfer's use-check rejects those first — this is the
	--     defensive mirror).
	-- Phantom saved-rsp flow: save-stores and carrier-loads are DROPPED, whatever
	-- base spelling carries the slot (a stack base below, or a region-pointer-
	-- derived register — the wp `movq %rax, 32(%r10)`): the value is phantom —
	-- sarcasm owns the real frame — so no real memory exists for it (the analysis
	-- proved the flow and marked the statements).
	if st.x86_saveStore or st.x86_carrierLoad then return end
	-- Normalized frame offset for a stack access through a stack+offset alias
	-- carrier (marked x86_aliasOff by the analysis): consumed by the ordinary
	-- slot path below, which keys everything off it (region lookup, bounds,
	-- taint/materialization, virtualization). The carrier base itself never
	-- reaches the output: the slot path only reads the memory operand for its
	-- width/decorators and emits a slot pseudo-register or a real disp(%rsp).
	local aliasOff = st.x86_aliasOff
	if mem and mem.base and not isStackReg(mem.base, usesRbp) and not mem.index and not mem.dispSym then
		-- A by-ADDRESS stack-buffer access (Feature A): the base is an ordinary
		-- web (possibly one holding a buffer byte address, possibly any other
		-- integer/pointer — the transform's runtime check is the whole story).
		-- Emit the statement untouched: no smap lookup, no conflict rejection,
		-- no capability decision.
		if st.x86StackBufAddr then
			emit(st)
			return
		end
		local e = st.x86_smap and st.x86_smap[mem.base.name]
		if e and e.conflict then
			-- A heap-form access with an explicit pointer capability
			-- (`;! load/store ptr` and the atomic variants) through a
			-- conflicted base stays legal: the transform bounds-checks it at
			-- runtime against the capability, fail-closed (a stack+offset
			-- value on some path has no valid lower, so that path traps
			-- rather than accessing out of bounds). This is the
			-- "ordinary runtime-checked uses stay legal" half of the
			-- conflict contract. Every other memory form through a
			-- conflicted base — plain loads/stores/RMWs (which would need a
			-- static stack-or-heap decision this pass cannot make) and lea
			-- address-taking — stays a static error.
			if b ~= "lea" and st.annotation and sig.PTR_ANNOTATIONS[st.annotation] then
				emit(st); return
			end
			error("sarcasm: memory access through %" .. mem.base.name .. " cannot be proven safe"
				.. " (control flow leaves it ambiguous whether the register holds stack+offset"
				.. " or a heap/alloca/argument pointer): " .. (st.raw or m))
		end
		if e and not e.redefined and not e.slot and not e.roff and not e.conflict then
			if st.x86_dropRestore then return end
			-- A derived-carrier lea (`leaq K(%carrier), %reg`) rides the carrier
			-- discipline: dropped like the save itself (the x86_leaSave mark the
			-- annotation pass computed for it), never reaching the alias/slot
			-- path below (a lea has no alias slot — aliasSlotOf refuses leas).
			if st.x86_leaSave and b == "lea" then return end
			-- A memory access through a frame-anchor carrier that lands inside the
			-- current anchored region scope (the poly1305 `leaq -88(%rsp), %r11`
			-- shape): pass it through — the transform redirects it to the region
			-- pointer at offset scopeDepth - carrier depth + disp.
			if st.x86_regionCarrier then emit(st); return end
			local i = yoloArgSlotOf(st, mem.base.name, e.depth)
			if i then
				emitYoloRead(i)
				return
			end
			-- A stack access through a stack+offset alias carrier: resolve to
			-- its normalized frame offset (the transfer proved the single
				-- memory-base use lands inside the frame) and fall through to
				-- the ordinary slot path.
			if aliasOff == nil then
				error("sarcasm: memory access through the saved stack pointer that is not"
					.. " an incoming stack-argument read or a saved-register restore cannot"
					.. " be proven safe: " .. (st.raw or m))
			end
			if ctx.regionOf(aliasOff, false) then
				-- Unreachable today: regions have no sources, so the lookup
				-- always misses (kept for the region plumbing's sake).
				error("sarcasm: stack access through stack+offset lands in a GC-allocation region"
					.. " (slots there do not virtualize; historical path — the"
					.. " region plumbing is inert): " .. (st.raw or m))
			end
		end
	end

	if mem and (isStackReg(mem.base, usesRbp) or aliasOff ~= nil
		-- An annotated INDEXED stack-buffer access (the base may be %rsp, %rbp or
		-- an alias carrier; the buffer scan marked it): handled inside the block
		-- below by the dedicated indexed branch.
		or (mem.index and st.x86StackBufIdx ~= nil)) and not mem.dispSym then
		-- A `lock` prefix on a stack-frame access can never be honored: this rewrite
		-- runs BEFORE classify (which owns the lockAllows validation), and the slot
		-- is about to be virtualized into a pseudo-register or materialized into the
		-- thread-confined synthesized frame — either way the emitted instruction
		-- would silently lose the prefix (a locked memory RMW degenerating into an
		-- unlocked register operation, e.g. `lock addq $1, -8(%rbp)`), or carry a
		-- meaningless prefix on thread-confined memory. Reject it, mirroring the
		-- classify rejection of `lock` on everything but the modeled heap mem-RMWs.
		if st.lockPrefix then
			error("sarcasm: lock prefix on a stack-frame access is not supported (frame"
				.. " slots are thread-confined — virtualized or materialized — so `lock`"
				.. " there is meaningless; it is only allowed on the modeled"
				.. " memory-destination RMWs on real memory): " .. (st.raw or m))
		end
		-- A stack memory operand carrying an AVX512 {kN} mask decorator. On HEAP
		-- accesses the supported masked forms (vector moves, truncating stores,
		-- expand/compress) get a mask-aware bounds check from the transform
		-- (fp.maskedAccessInfo). A STACK slot is virtualized/materialized HERE,
		-- before the transform, so the masked instruction cannot ride through —
		-- but the masked VECTOR-MOVE family (vmovdqu8/16/32/64, vmovdqa32/64,
		-- vmovups/upd, vmovaps/apd) can be LOWERED exactly: an unmasked full-width
		-- load of the slot's old bytes, the ORIGINAL register-register masked move
		-- (merge-masking: k=1 lanes take the new value, k=0 keep the old — {z}
		-- forms zero them) riding a scratch vector register the body never names,
		-- and an unmasked full-width store back. The slot's cluster was sized by
		-- the FULL vector width (fp.accessWidth is mask-blind), so the widened
		-- footprint is bounds-proven, and the masked-off lanes' bytes are read
		-- and written back UNCHANGED — memory content is exactly what the masked
		-- instruction leaves. Every other masked form (expand/compress, truncating
		-- stores, ALU memory sources) stay rejected.
		local fpEntry = fp.lookup(m)
		local maskedStackLower = nil
		for _, o in ops do
			if o.deco and string.match(o.deco, "{%%?k[0-7]}") then
				if isFpInsn(fpEntry, st) and maskScratchNum
					and MASKED_STACK_MOVE[m]
					and not mem.bcastN and not string.find(o.deco, "{1to", 1, true) then
					maskedStackLower = o
				else
					error("sarcasm: AVX512 mask-register ({k}) stack-frame memory accesses are not"
						.. " supported for '" .. m .. "' (only the masked vector moves lower"
						.. " here; masked-off lanes may not touch memory; cannot prove memory"
						.. " safety): "
						.. (st.raw or m))
				end
			end
		end
		-- The lowering itself (both emission points below call this): see the gate
		-- comment above for the exactness argument. `mat` is the materialized
		-- slot operand (its deco carries the original {kN}{z} decorators — the
		-- unmasked helper traffic strips them); ops are dest-first; `deco` is the
		-- mask decorator text (on the memory operand for stores, on the register
		-- destination for loads — wherever the input spelled it). Returns the
		-- sequence WITHOUT emitting it, so an indexed buffer access can attach
		-- its runtime bounds-check mark to the first statement before emission.
		local function buildMaskedStackLower(mm, mat, sops, smem, mdeco)
			local unal = MASKED_STACK_MOVE[mm]
			local isStore = (sops[1] == smem)
			local vop = isStore and sops[2] or sops[1]
			local w = (vop.width or 128) // 8
			local pfx = (w >= 64) and "zmm" or (w >= 32) and "ymm" or "xmm"
			local scratch = { kind = "reg", rc = "xmm", num = maskScratchNum,
				width = w * 8, name = pfx .. maskScratchNum }
			local matU = {}
			for kk, vv in pairs(mat) do matU[kk] = vv end
			matU.deco = nil
			local out = {}
			-- unmasked full-width traffic of the old/new slot bytes
			if isStore then
				-- scratch = old memory; scratch = k ? src : scratch; store scratch
				table.insert(out, { kind = "insn", mnemonic = unal,
					operands = { scratch, matU }, raw = st.raw, line = st.line })
				local dst = {}
				for kk, vv in pairs(scratch) do dst[kk] = vv end
				dst.deco = mdeco
				table.insert(out, { kind = "insn", mnemonic = st.mnemonic,
					operands = { dst, vop }, raw = st.raw, line = st.line,
					annotation = st.annotation })
				table.insert(out, { kind = "insn", mnemonic = unal,
					operands = { matU, scratch }, raw = st.raw, line = st.line })
			else
				-- scratch = full load; dest = k ? scratch : dest (merge) / {z} zeroes
				table.insert(out, { kind = "insn", mnemonic = unal,
					operands = { scratch, matU }, raw = st.raw, line = st.line })
				local dst = {}
				for kk, vv in pairs(vop) do dst[kk] = vv end
				dst.deco = mdeco
				table.insert(out, { kind = "insn", mnemonic = st.mnemonic,
					operands = { dst, scratch }, raw = st.raw, line = st.line,
					annotation = st.annotation })
			end
			return out
		end
		local function emitMaskedStackLower(mm, mat, sops, smem, mdeco)
			for _, e in buildMaskedStackLower(mm, mat, sops, smem, mdeco) do
				emit(e)
			end
		end
		-- Memory-neutral hints (prefetch*) need no bounds check and no slot: the real
		-- stack address is always safe to hint at (a prefetch can never fault).
		if fpEntry and fpEntry.noCheck then emit(st); return end
		-- Region lookup (a static fact even at a perturbed/"dyn" depth): an access
		-- landing in a fixed alloca region would pass through for the transform's
		-- region redirect (a region-pointer-relative checked access), whatever the
		-- depth. The lookup rides the access's own coordinate space: inside an
		-- {anchored} region scope (the depth is the region's table — raw
		-- displacements are that region's coordinates); plain "dyn" has NO valid
		-- coordinates (a later rsp write moved the base); and everything else
		-- matches the static regions in normalized frame coordinates (rbp-relative
		-- offsets are static at any depth). A CLONE statement's rsp-relative
		-- displacement at a static depth keys with the +8 rule (the return-address
		-- compensation: hardware has the pushed return address between the clone's
		-- rsp and the caller's frame — localcall.luau). UNREACHABLE today: regions
		-- have no sources anymore (no `;! alloca` annotations, no D9 fixed-frame
		-- escape promotion), so the static lookups always miss — every stack access
		-- proceeds to the slot virtualization / rejection paths below.
		local region = nil
		local rOff = aliasOff or (mem.dispVal or 0)
		if aliasOff ~= nil then
			-- (region membership was decided in the carrier gate above:
			-- historically an alias access landing in a promoted region was
			-- rejected there; today the lookup always misses because regions
			-- have no sources. Anchored/dynreg scopes never
			-- apply: the anchor shapes ride regionCarrierAccess instead.)
			region = ctx.regionOf(rOff, false)
			assert(region == nil)
		end
		if aliasOff == nil then
		if mem.base.rc ~= "sp" then rOff = rbpNorm + rOff
		elseif st.localClone and d ~= nil and not isDynDepth(d) then rOff = rOff + D0 - d - 8 end
		end
		if aliasOff == nil and mem.base.rc == "sp" and not st.x86_prologue and type(d) == "table" then
			if d.anchored and rOff >= d.anchored.base and rOff < d.anchored.base + d.anchored.size then
				region = d.anchored
			elseif d.dynreg then
				-- Dynamic-region scope: rsp = region base + bias exactly, so
				-- EVERY rsp-relative access here is a region access (indexed
				-- forms included — mont's `movq %r13,-16(%rsp,%r15,8)`). The
				-- region's extent is the dynamic allocation size, so membership
				-- is not a static fact: the redirect rewrites the operand to
				-- ride the region temp, and the ordinary checked path's runtime
				-- bounds check covers [0, size).
				region = d.dynreg
			end
		else
			region = ctx.regionOf(rOff, false)
		end
		if region then
			-- An aligned FP/vector access needing MORE than the region's provable
			-- alignment: reject at compile time (deterministic — the alternative is a
			-- runtime alignment trap that fires only when the GC payload happens to
			-- be under-aligned). Ordinary regions are 16-aligned (the GC payload);
			-- align=A regions (and the andq-anchored forms) provably cover A.
			local _, fa = fp.accessWidth(m, ops, st.raw)
			if fa and fa > (region.align or 16) then
				error("sarcasm: aligned " .. fa .. "-byte access into a " .. (region.align or 16)
					.. "-byte-aligned region cannot be proven safe (use the unaligned form): "
					.. (st.raw or m))
			end
			-- A masked form on a REGION access rides the transform's region redirect
			-- verbatim, whose plain (full-width) check is what proves it — the
			-- stack-lowering below owns only the virtualized/materialized slots.
			-- The gate above already rejected every non-lowerable masked form, but
			-- a lowerable one must not silently lose its mask on the region path.
			if maskedStackLower then
				error("sarcasm: AVX512 mask-register ({k}) memory access into a promoted"
					.. " region is not supported (the region redirect cannot lower the"
					.. " mask; use an unmasked form): " .. (st.raw or m))
			end
			emit(st); return
		end
		-- A lea-save (the entry-rsp carrier materializations, INCLUDING the D7
		-- restore-base leas — the {anchored}-scope and rbp-derived forms, which
		-- execute at a perturbed/region-scope depth the slot-keying below
		-- rejects): dropped with the save — the carrier discipline rejects every
		-- non-carrier use. This drop must precede the perturbed-depth rejection
		-- (the D7 forms never pass it); the region pass-through above already
		-- claimed every in-region lea (the D7 forms exclude those by
		-- construction), so nothing here loses a redirect.
		if st.x86_leaSave then return end
		-- A stack access is only keyable to a frame slot when rsp provably sits at the
		-- post-prologue frame base; anything else (mid-function push/sub region, dynamic
		-- alloca aftermath, unbalanced merge) is unprovable: reject. (nil depth = dead code.)
		-- EXCEPTION: an rbp-relative access while rbp provably holds the frame pointer
		-- (x86_fp == "est"). rbp's value is static — established once in the prologue,
		-- with every re-establishment, clobber, and restore rejected or tracked by the
		-- frame-pointer state — so the slot this access touches is keyed by a static
		-- offset regardless of the rsp depth. This is what lets alloca-using code spill
		-- and reload locals (the size, the buffer pointer) through rbp-relative slots
		-- AFTER the alloca perturbed rsp — hand-written code and optimized builds alike
		-- rely on it — and the arm64 backend has always allowed the analogous
		-- x29-relative accesses there.
		-- SECOND EXCEPTION: an rsp-relative access at a KNOWN perturbed depth (the
		-- early-ret-then-frame shape: an early return before the pushes leaves the
		-- framed path's pushes POST-prologue, so D0 stays 0 while the body executes
		-- at depth 8k) may still alias an outstanding pushed register's save slot —
		-- the save-slot model below keys it in normalized coordinates
		-- (disp + D0 - d) exactly like the transient prologue pad; a non-aliased
		-- access at such a depth stays rejected.
		local perturbedRsp = false
		if aliasOff == nil and not st.x86_prologue and d ~= nil and d ~= D0
			and not (usesRbp and mem.base.name == "rbp" and st.x86_fp == "est") then
			if not isDynDepth(d) and mem.base.rc == "sp" then
				perturbedRsp = true
			else
				error("sarcasm: stack access while the stack pointer is perturbed or unknown"
					.. " (cannot prove which frame slot this is): " .. (st.raw or m))
			end
		end
		-- Normalize to an rsp-relative frame offset: rbp, when it is the frame pointer,
		-- sits at the frame TOP (rbp = rsp + frameSize), so an rbp-relative displacement d
		-- is frame offset frameSize + d. The slot key, region lookup, and bounds check all
		-- use this NORMALIZED offset, so rbp- and rsp-relative accesses to the same address
		-- (e.g. -8(%rbp) and 8(%rsp) with a 16-byte frame) share one slot web, while
		-- distinct addresses (e.g. -8(%rbp) vs -8(%rsp)) never alias. A CLONE statement's
		-- rsp-relative displacement additionally keys 8 lower (the +8 rule: hardware has
		-- the pushed return address between the clone's rsp and the caller's frame — at
		-- the callsite depth this is exactly disp-8).
		-- A B2-clone access overlapping a pushed slot addresses the jumper's
		-- slot (the x25519 `mov 16(%rsp),%rdi # restore dst` and the shared
		-- epilogue restores): it must share the slot's web, so it keys unbanded.
		-- This covers callee-saved saves (paired by the save-slot model below
		-- with the jumper's save) and caller-saved push-spills like the offloaded
		-- first argument (ordinary slot webs, shared by coordinate exactly like
		-- the jumper's own accesses). All other clone traffic rides the private
		-- band (the pinned sarcasm-tail-b2-addrcompare-att semantics:
		-- same-spelled scratch in the clone and the jumper are different
		-- addresses). This pre-scan mirrors the save-slot model's hit test
		-- exactly; the model itself runs below on the unbanded offset and
		-- accepts or rejects exactly like ordinary code.
		local function b2saveHit(unbanded)
			local sv = st.x86_saves
			if sv == nil or sv == "dyn" or #sv == 0 then return false end
			if isFpInsn(fpEntry, st) then return false end
			local w0 = gprStackWidth(m, mem)
			if not w0 then return false end
			for _, s in sv do
				if usesRbp and s.name == "rbp" then continue end
				local sd = D0 - s.depth
				if unbanded < sd + 8 and unbanded + w0 > sd then return true end
			end
			return false
		end
		local off = aliasOff or (mem.dispVal or 0)
		if aliasOff == nil then
		if mem.base.rc ~= "sp" then off = rbpNorm + off
		elseif st.localClone and d ~= nil and not isDynDepth(d) then off = off + D0 - d - 8
		elseif st.b2clone and d ~= nil and not isDynDepth(d) then
			off = off + D0 - d
			if not b2saveHit(off) then off = off + (st.x86_b2band or 0) end
		elseif perturbedRsp then off = off + D0 - d end
		end
		-- An entry-rsp materialization for reading the incoming yolo stack arguments
		-- (`leaq 0(%rsp), %reg` at a provable depth — the aesni_ocb shape): dropped;
		-- reads through the register are redirected by the alias handling above.
		if b == "lea" and isYoloLeaSave(st, d) then return end
		-- (The lea-save drop — the ghash/aesni-gcm entry-rsp carrier
		-- materializations and the D7 restore-base leas — now lives before the
		-- perturbed-depth rejection above: the D7 forms execute at a perturbed
		-- depth and never reach this point.)
		-- A DIRECT entry-time read of an incoming yolo stack argument: %rsp-based
		-- at a PROVABLE depth d (before the body's own frame setup, in a frameless
		-- body, or — the compiler style — above the established frame), e.g.
		-- `movq 8(%rsp), %rbx` / `movd 8(%rsp), %xmm5`: at depth d the argument
		-- sits at d+8+8*i(%rsp). The address is above the caller's frame, so the
		-- ordinary bounds check below would reject it; redirect it to the
		-- argument's slot instead.
		if mem.base.rc == "sp" and d ~= nil and not isDynDepth(d) and not mem.index then
			local i = yoloArgSlotOf(st, "rsp", d)
			if i then emitYoloRead(i); return end
		end
		-- A direct rsp-relative access positioned before a mid-function
		-- `and $-N, %rsp` keys pre-and numbering, which the and shifts by a
		-- dynamic slack (alias-carrier, rbp-relative, and yolo-argument reads
		-- above are absolute and exempt). A statement that can ONLY execute
		-- before the and (preAnd-marked, never reached from one — x86_postAnd
		-- unset) names a single real address per key on every execution: its
		-- accesses key TRUE entry-relative coordinates like every other pre-and
		-- slot, and analyzeFrame's interval check proves no post-and (slack=0-
		-- keyed) traffic shares those modeled bytes. A statement BOTH able to
		-- reach an and and reachable from one executes on both sides of the
		-- and — one key, two slack-dependent addresses — and stays rejected.
		if st.x86_preAnd and st.x86_postAnd and aliasOff == nil and mem.base.rc == "sp" then
			error("sarcasm: stack access both before and after a mid-function `and $-N, %rsp`"
				.. " cannot be proven safe (the access executes on both sides of the and, and"
				.. " the and shifts rsp by a dynamic slack, so one slot key names two"
				.. " different real addresses; move the access wholly to one side, or address"
				.. " the slot through a saved stack-pointer register): " .. (st.raw or m))
		end
		-- A buffer-interior VALUE lea (Feature A, by-address accesses): the
		-- destination web holds the buffer byte ADDRESS — emit it as a real
		-- `leaq disp'(%rsp), %reg` into the lowered buffer region (tagged
		-- stackBuf, so the transform's buffer-base shift finalizes it into the
		-- output-frame address of exactly the byte the input lea computed). The
		-- web is an ordinary integer web; annotated accesses through it become
		-- runtime-checked by-address accesses, and value uses (ALU, cmov,
		-- stores) are ordinary integer arithmetic. (The phase (e2) pass computed
		-- the group-relative displacement; a value lea never lands in a region —
		-- a buffer's range and an alloca region's coordinates cannot collide.)
		if st.x86StackBufLeaValue then
			local lv = st.x86StackBufLeaValue
			local mat = { kind = "mem",
				base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
				scale = 1, dispVal = lv.disp, stackBuf = true }
			emit({ kind = "insn", mnemonic = "lea", operands = { ops[1], mat },
				raw = st.raw, line = st.line })
			return
		end
		-- lea of a stack address computes/escapes a pointer into the frame: reject.
		if b == "lea" then ctx.rejectEscape(st.raw or m) end
		-- ANNOTATED INDEXED stack-buffer access (`#! stack buffer (...)` on an
		-- indexed stack-relative memory operand): the scan marked it with its
		-- resolved range and access shape. Rewrite the operand in place to a
		-- disp'(%rsp,%idx,s) into the buffer region (tagged stackBuf so the
		-- transform emits it verbatim — no capability checks, no slot webs — and
		-- shifts it once the frame's root count is known), and mark the
		-- statement for the transform's runtime bounds check on the index.
		if mem.index and st.x86StackBufIdx then
			local bi = st.x86StackBufIdx
			local g = nil
			for _, gr in (sbLayout and sbLayout.groups or {}) do
				if bi.lo >= gr.lo and bi.hi <= gr.hi then g = gr break end
			end
			if not g then
				error("sarcasm: internal error: indexed buffer access range ["
					.. tostring(bi.lo) .. ", " .. tostring(bi.hi)
					.. ") has no lowered group: " .. (st.raw or m))
			end
			local mat = { kind = "mem",
				base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
				index = mem.index, scale = mem.scale or 1,
				dispVal = g.base + (bi.dispEff - g.lo), size = bi.size,
				stackBuf = true, deco = mem.deco, bcastN = mem.bcastN }
			local newops = {}
			for _, o in ops do table.insert(newops, (o == mem) and mat or o) end
			-- Feature (indexed masked stack-buffer lowering): a lowerable masked
			-- vector move on a REDIRECTED buffer operand lowers exactly like the
			-- static-slot case — the unmasked full-width old-bytes load, the
			-- original masked register-register move, and the unmasked store-back
			-- (loads: the full load, then the masked move into the destination) —
			-- all riding the redirected `disp'(%rsp,%idx,s)` operand. The runtime
			-- bounds-check mark rides the FIRST emitted statement (the unmasked
			-- old-bytes load), so the index is proven in range before ANY byte of
			-- the sequence executes — an out-of-range index traps without
			-- touching the buffer, exactly like the unmasked indexed form. The
			-- scratch register rule holds unchanged (the body never names the
			-- scratch web), and the masked-off lanes' bytes are read and written
			-- back unchanged, so the buffer's memory content is exactly what the
			-- masked instruction leaves. Capabilities never live in buffer
			-- memory (the scan already rejected the ptr annotations), so the
			-- index web's pointer-ness is irrelevant here — the bounds check is
			-- the whole story.
			if maskedStackLower then
				local lowered = buildMaskedStackLower(m, mat, ops, mem,
					maskedStackLower.deco)
				lowered[1].x86StackBufIdx = bi
				for _, e in lowered do emit(e) end
				return
			end
			emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops,
				raw = st.raw, line = st.line, x86StackBufIdx = bi })
			return
		end
		-- only plain base+disp slots are virtualizable (no index register)
		if mem.index then
			error("sarcasm: indexed stack-relative access is not supported on x86"
				.. " (annotate it with `#! stack buffer (name, %rsp, %rsp + N)` to"
				.. " lower it into a bounds-checked stack buffer): " .. (st.raw or m))
		end
		-- The slot virtualization rewrites ONLY the memory operand. Any other operand that
		-- is (or contains) a stack register would leak the live sp/fp value into a virtual
		-- temp — `movq %rsp, (%rsp)`, `movq %rbp, -8(%rbp)`, `movq (%rsp), %rsp` — the same
		-- escape class as `movq %rsp, %rax`, which the fall-through scan below catches only
		-- because no stack-mem operand virtualizes first. Reject it here.
		for _, o in ops do
			if o ~= mem then
				if isStackReg(o, usesRbp) then ctx.rejectEscape(st.raw or m) end
				if o.kind == "mem" and (isStackReg(o.base, usesRbp) or isStackReg(o.index, usesRbp)) then
					ctx.rejectEscape(st.raw or m)
				end
			end
		end
		-- The `;! ... ptr` annotation family. A VIRTUALIZED frame slot is an
		-- ordinary GPR web, so a capability rides the same pointer flow a
		-- register move uses: `store ptr` / `load ptr` on a plain 8-byte
		-- register<->slot move are ACCEPTED — the move's pointer-flow
		-- propagation IS the capability transfer. A store hands the source
		-- web's capability to the slot web; a load hands the slot web's
		-- capability to the destination web; two different origins stored into
		-- one slot widen the web to a dynamic (lockstep) lower exactly like
		-- conflicting register moves; and every lower stays rooted at its seed,
		-- so GC safety is unchanged. The annotation itself is dropped below —
		-- the plain move is the whole story (no invisicap sequence is involved).
		-- A narrower scalar store to the same offset is a FULL def of the slot
		-- web (the colored register zero-extends), so it kills the slot's
		-- pointer-ness on that path: a later `load ptr` then carries a null
		-- capability and traps at the deref — sound. Everything else in the
		-- family (the atomic forms, `load store ptr`) stays rejected: a frame
		-- slot is virtualized/materialized, not REAL memory, so the invisicap
		-- sidecar-byte protocol has nothing to point at.
		local slotPtrAnno = nil
		if st.annotation and sig.PTR_ANNOTATIONS[st.annotation] then
			if st.annotation == "store ptr" or st.annotation == "load ptr" then
				slotPtrAnno = st.annotation
				if st.lockPrefix then
					error("sarcasm: `lock`-prefixed `" .. st.annotation .. "` on a stack-frame"
						.. " access is not supported (the slot is virtualized, not real"
						.. " memory — atomics need real memory): " .. (st.raw or m))
				end
				if isFpInsn(fpEntry, st) then
					error("sarcasm: `" .. st.annotation .. "` on an FP/SIMD stack-frame access is"
						.. " not supported (a capability is an 8-byte general-register value;"
						.. " vector slots materialize into real memory and cannot hold one): "
						.. (st.raw or m))
				end
				if b ~= "mov" then
					error("sarcasm: `" .. st.annotation .. "` on a stack-frame access requires a"
						.. " plain mov (the slot virtualizes into a register, so only a"
						.. " register<->slot move can carry the capability): " .. (st.raw or m))
				end
				local w0 = gprStackWidth(m, mem)
				if w0 ~= 8 then
					error("sarcasm: `" .. st.annotation .. "` on a stack-frame access requires an"
						.. " 8-byte (64-bit) move (a capability is an 8-byte GPR value); this"
						.. " access is " .. tostring(w0) .. " bytes: " .. (st.raw or m))
				end
				local regOp = (ops[1] == mem) and ops[2] or ops[1]
				if not (regOp and regOp.kind == "reg" and regOp.rc == "gpr"
						and (regOp.width or 64) == 64) then
					error("sarcasm: `" .. st.annotation .. "` on a stack-frame access requires the"
						.. " other operand to be a 64-bit general register (a capability is an"
						.. " 8-byte GPR value): " .. (st.raw or m))
				end
			else
				error("sarcasm: `" .. st.annotation .. "` on a stack-frame access is not supported"
					.. " (only `store ptr` / `load ptr` on a plain 8-byte register<->slot move"
					.. " hold a capability; frame slots are virtualized/materialized, not real"
					.. " memory, so the atomic/invisicap forms have nothing to point at): "
					.. (st.raw or m))
			end
		end
		-- cmpxchg8b/cmpxchg16b have no register form: a frame slot cannot be
		-- virtualized for them (a slot pseudo-register is not a memory operand),
		-- and GPR-only stack slots are never materialized. Reject cleanly instead
		-- of mis-sizing the slot (the bare mnemonic has no width suffix).
		if m == "cmpxchg8b" or m == "cmpxchg16b" then
			error("sarcasm: " .. m .. " on a stack-frame access is not supported (frame slots"
				.. " are virtualized as registers and " .. m .. " has no register form): "
				.. (st.raw or m))
		end
		-- Outstanding pushed-register save slots (the transient prologue pad). When a
		-- `pushq %rbx` sits inside the prologue prefix and has not been popped yet,
		-- the bytes it saved are NOT an ordinary frame slot: their content is mirrored
		-- by the pushed register's web, because the matching pop — which the
		-- save-pairing analysis proves restores exactly this register — is dropped.
		-- Virtualizing an access that aliases such a slot into an unrelated slot web
		-- silently desyncs it from the register (a store to the slot would never reach
		-- the register, and the dropped pop would resurrect the stale pre-push value).
		-- Model it instead:
		--   * a full-slot (8-byte) GPR access is rewritten to the register itself: a
		--     store then defines the pushed register's web (so the dropped pop
		--     naturally yields the stored value, matching real x86 where the pop
		--     overwrites the register with the stored value and the pre-push value is
		--     lost), a read-modify-write operates on that web, and a load — of any
		--     width, at the slot base — reads it (before any store that IS the pushed
		--     value);
		--   * anything else that overlaps a save slot — a partial-width store (the web
		--     model has no subregister view), a vector/x87 access (it cannot name a
		--     GPR web), or an instruction whose register form is not exactly modeled
		--     (checked by classifying the rewritten instruction below) — is rejected.
		-- Multi-register pads work the same way: the FIRST push sits at the HIGHEST
		-- address, so displacement (depth - save.depth) selects the slot (0 = most
		-- recent push, 8 = the one before it, ...). The saved frame pointer (a
		-- `pushq %rbp` establishing the frame) is not a value slot and never aliases
		-- here — it sits above the frame and the bounds check rejects accesses to it.
		local sv = st.x86_saves
		if sv == "dyn" then
			-- The save state is unprovable here: the paths disagree on which registers
			-- are pushed (or a push happened while rsp was poisoned). The access cannot
			-- be checked against individual save slots — but it also cannot DESYNC one
			-- in any function sarcasm accepts: an unprovable save state always carries
			-- an unprovable rsp depth with it (each outstanding push is 8 bytes of
			-- depth, and paths that disagree on outstanding pushes disagree on depth),
			-- so this point is reachable only for rbp-relative accesses (rsp-relative
			-- ones were already rejected above at d ~= D0), and every push that made
			-- the state unprovable makes its own pop unpairable — a body with an
			-- unpairable pop or a return at a poisoned depth is rejected downstream.
			-- Virtualize without the aliasing model.
		end
		local aliasHit, aliasDisp, aliasW = nil, nil, nil
		if sv and sv ~= "dyn" and #sv > 0 and not isFpInsn(fpEntry, st) then
			local w0 = gprStackWidth(m, mem)
			if w0 then
				for _, s in sv do
					if usesRbp and s.name == "rbp" then continue end  -- saved frame pointer
					if s.spill then continue end  -- a spill slot is an ordinary slot
					-- The save slot's NORMALIZED frame offset: its displacement from the
					-- post-prologue frame base (D0). Computed from D0 rather than the
					-- current depth d so the aliasing identity holds in normalized
					-- coordinates even for an rbp-relative access at a perturbed/unknown
					-- rsp depth (both the access's offset and the save slot's are static
					-- in these coordinates).
					local sd = D0 - s.depth  -- that save slot's frame offset (>= 0)
					if off < sd + 8 and off + w0 > sd then
						aliasHit, aliasDisp, aliasW = s, sd, w0
						break
					end
				end
			end
		end
		if aliasHit then
			local function aliasReject(why)
				error("sarcasm: stack access overlapping a pushed register's save slot inside"
					.. " the transient prologue pad " .. why .. " (the slot's content is the"
					.. " pushed register's web; use a full 8-byte access or pop the pad"
					.. " first): " .. (st.raw or m))
			end
			if off ~= aliasDisp then
				aliasReject("at a non-slot-aligned displacement")
			end
			if aliasW ~= 8 and ops[1] == mem then
				-- A narrower store cannot define the register's web (the model has no
				-- subregister view) while the remaining slot bytes keep the pushed
				-- value the dropped pop would restore.
				aliasReject("with a partial-width store")
			end
			if aliasHit.redefined and not (ops[1] == mem and aliasW == 8 and b == "mov") then
				-- The pushed register was redefined between its push and this access:
				-- the slot holds the value pushed BEFORE the redefinition, while the
				-- register's web now holds the redefined value — reading the slot (a
				-- load, or a read-modify-write) would see the wrong value. The ONE
				-- exception is a full-width plain STORE: it overwrites the slot and
				-- the model defines the register's web with exactly that value, so
				-- the slot and the web agree again (the analysis clears the save's
				-- redefined state from there on). The OTHER exception is an epilogue
				-- movq-restore load that analyzeFrame proved unobservable (its
				-- x86_dropRestore walk found no later read of the destination): it
				-- is dropped like the epilogue pops.
				if st.x86_dropRestore then return end
				error("sarcasm: stack access overlapping a pushed register's save slot inside"
					.. " the transient prologue pad whose register was redefined after the"
					.. " push (the slot still holds the pre-redefinition value, so the"
					.. " save-slot model cannot express this access; pop the pad first or"
					.. " don't redefine the register inside it): " .. (st.raw or m))
			end
			local parsed = x86parse.classifyReg(aliasHit.name)
			if not (parsed and parsed.rc == "gpr") then
				aliasReject("for an unmodelable register")
			end
			local regOp = { kind = "reg", rc = "gpr", num = parsed.num,
				width = aliasW * 8, name = parsed.name }
			local newops = {}
			for _, o in ops do table.insert(newops, (o == mem) and regOp or o) end
			-- The rewritten instruction's REGISTER form must be exactly modeled: the
			-- conservative first-reg-def fallback (e.g. shld/shrd) would silently
			-- desync the slot from the register. Classify the rewritten (now
			-- register-only) instruction; it flags `unknown` for the fallback, and
			-- anything it rejects outright is rejected with that instruction's own
			-- precise message.
			local okC, c2 = pcall(x86.classify, {
				mnemonic = st.mnemonic, operands = newops, opsize = st.opsize,
				raw = st.raw, syntax = st.syntax,
			})
			if not okC or (c2 and c2.unknown) then
				aliasReject("by an instruction whose register form sarcasm does not model exactly")
			end
			-- A `store ptr` / `load ptr` on an aliasing access stays sound with the
			-- annotation DROPPED: the slot's content IS the pushed register's web, and
			-- the rewritten register<->register move propagates the capability through
			-- pointer flow exactly like the slot move would.
			local aliasAnno = st.annotation
			if slotPtrAnno then aliasAnno = nil end
			emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops,
				raw = st.raw, line = st.line, annotation = aliasAnno })
			return
		end
		-- A KNOWN-perturbed-depth rsp-relative access that aliases no outstanding save
		-- slot virtualizes like any other slot: the normalized offset (disp + D0 - d)
		-- is exact at a statically-known depth, so the slot web connects every access
		-- to the same address across the depth change, the bounds check below gates
		-- the frame extent, and pops/returns stay pairing/depth-gated. (Historically
		-- this was rejected: with mid-function `addq $imm, %rsp` decreases accepted
		-- above — the ecp_nistz256 shared-tail frame-reshape shape — static
		-- perturbed depths are ordinary working points, not just the early-ret
		-- transient the save-alias model was built for. An UNKNOWN depth — a
		-- an unanalyzable adjustment — stays
		-- rejected above.)
		-- Replace the stack-slot memory operand in place with a slot pseudo-register of the
		-- access width, keeping the original mnemonic. This uniformly handles plain moves
		-- (movq), zero/sign-extending loads (movzwl/movsbl/...), and other reg<->slot forms:
		-- e.g. `movzwl 40(%rsp), %r14d` -> `movzwl %<slot:16>, %r14d`. The slot is keyed by
		-- byte offset, so a 64-bit store and a 16-bit reload of the same offset share one web.
		-- FP/SIMD/x87 instructions get their width from the knowledge module (the AT&T
		-- suffix convention does not apply to them: movsd is 8, movss 4, fldt 10, ...).
		-- That width is AUTHORITATIVE: a raw Intel mem.size must never under-size the
		-- slot — the emitted instruction keeps its true (wider) width and would
		-- overflow the materialized frame region into the caller's stack. accessWidth
		-- itself cleanly rejects a PTR annotation contradicting the ISA-fixed width
		-- (the size-driven x87/PTR forms take the annotation as their width). GPR
		-- instructions keep the mem.size-first behavior.
		local isFP = isFpInsn(fpEntry, st)
		local alignReq = 1
		local fw, fa = fp.accessWidth(m, ops, st.raw)
		local wb
		if isFP then
			wb = fw
			if fa and fa > 1 then alignReq = fa end
		else
			wb = mem.size or fw
		end
		if not wb then
			-- An FP-class access whose width is unknowable can neither be
			-- materialized nor virtualized soundly (a guessed width could
			-- under-cover the access). Reject cleanly. (Reject-listed entries
			-- fall through to the default width: the lift's fp.classify
			-- produces their more precise error.)
			if isFP and not (fpEntry and fpEntry.reject) then
				error("sarcasm: cannot determine the memory access width of '" .. m
					.. "' (cannot prove memory safety): " .. (st.raw or m))
			end
			wb = gprStackWidth(m, mem)
		end
		-- Bounds-check against the input frame (mirrors arm64's per-access checkBounds).
		-- rsp-relative offsets address the frame [0, frameSize) plus the 128-byte SysV red
		-- zone below rsp. NORMALIZED rbp-relative offsets range over the same [-128,
		-- frameSize): with rbp = rsp + frameSize, an rbp-relative d in [-frameSize-128,
		-- -frameSize) — e.g. the leaf-function shape `-8(%rbp)` with no `subq` at all — IS
		-- the red zone. (Pre-normalization rbp-relative d >= 0 hits the saved rbp, the
		-- return address, or the caller's argument area.) At a KNOWN perturbed depth
		-- (a mid-function inner frame — the rsaz-avxifma scratch pairs) the valid
		-- window additionally extends down to the current rsp (and its red zone):
		-- offsets [D0 - d - 128, frameSize). Anything outside cannot be proven safe:
		-- reject (a virtualized slot would silently fabricate garbage reads / dead
		-- writes).
		local lowB = -128
		if perturbedRsp then lowB = math.min(-128, D0 - d - 128) end
		-- A stack+offset alias carrier access gets the SAME depth-aware window as
		-- the direct rsp-relative spelling at this point (the carrier's key names
		-- the same address — see the transfer's aliasOk window comment). A "dyn"
		-- or scope depth keeps the historical [-128, frameSize) window.
		if aliasOff ~= nil and type(d) == "number" then
			lowB = math.min(lowB, D0 - d - 128)
		end
		ctx.checkBounds(off, wb, ops[1] == mem, st.raw, lowB)
		-- A STATIC stack access whose normalized byte range falls fully inside a
		-- declared stack-buffer group is lowered into the buffer region: real
		-- memory, no slot web, no capability (the scan rejected capabilities and
		-- boundary straddles already). The operand becomes disp'(%rsp) — even for
		-- an rbp- or alias-based original, the buffer lives at a fixed offset of
		-- the synthesized frame's %rsp — tagged stackBuf so the transform passes
		-- it through verbatim and shifts it once the root count is known.
		if st.x86StackBufAccess then
			local ba = st.x86StackBufAccess
			local g = nil
			for _, gr in (sbLayout and sbLayout.groups or {}) do
				if ba.off >= gr.lo and ba.off + ba.w <= gr.hi then g = gr break end
			end
			if not g then
				error("sarcasm: internal error: buffer access at normalized offset "
					.. tostring(ba.off) .. " has no lowered group: " .. (st.raw or m))
			end
			local mat = { kind = "mem",
				base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
				scale = 1, dispVal = g.base + (ba.off - g.lo), size = wb,
				stackBuf = true, deco = mem.deco, bcastN = mem.bcastN }
			local newops = {}
			for _, o in ops do table.insert(newops, (o == mem) and mat or o) end
			if maskedStackLower then
				emitMaskedStackLower(m, mat, ops, mem, maskedStackLower.deco)
				return
			end
			emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops,
				raw = st.raw, line = st.line, annotation = st.annotation })
			return
		end
		-- Aligned >16-byte stack accesses cannot be materialized at their required
		-- alignment: the synthesized frame guarantees 16-byte body-rsp alignment,
		-- or the recorded `and $-N, %rsp` alignment when the analysis saw one.
		-- The taint scan already proved coverage (an `and $-N, %rsp` note with
			-- N >= alignReq plus an aligned input offset); this is the backstop
			-- for shapes the scan mirrors. Reject cleanly; the unaligned forms
			-- (vmovdqu/vmovups/...) encode no alignment promise and work fine.
		if isFP and alignReq > 16 then
			local alignedOff = ((off - (D0 - frameAndDepth)) % alignReq + alignReq) % alignReq == 0
			if frameAlignReq < alignReq or not alignedOff then
				error("sarcasm: aligned 32/64-byte stack accesses are not supported (the stack is only"
					.. " guaranteed 16-byte aligned); use the unaligned form (e.g. vmovdqu/vmovups): "
					.. (st.raw or m))
			end
		end
		-- An FP/SIMD-touched stack range is MATERIALIZED into sarcasm's synthesized
		-- frame (see analyzeFrame's taint scan): keep the instruction and rewrite its
		-- memory operand to a real disp(%rsp). GPR accesses overlapping the range
		-- land here too (aliasing). The rc="sp" base keeps the operand out of the
		-- register-web analysis (classify's isGpr skips it); the renderer's
		-- baseRegName fallback renders it as %rsp. size=wb lets the renderer
		-- synthesize AT&T suffixes for reg-less forms (e.g. `movl $5, O(%rsp)`).
		local final = ctx.fpInfo and ctx.fpInfo.lookup(off, wb)
		if final then
			-- A GPR access materialized into an FP-tainted cluster becomes a real
			-- disp(%rsp) memory access: a lying Intel PTR annotation that
			-- contradicts the register-defined access width would under-size the
			-- cluster while the materialized instruction writes past it (into the
			-- GC root slots / spill slots). Reject it, mirroring the heap path
			-- (x86_64_codegen.accessSizeAlign). Virtualized GPR slots keep the
			-- mem.size-first behavior (no real memory is accessed).
			if not isFP then
				local regW = x86.gprAccessRegWidth(st)
				if regW and mem.size and mem.size ~= regW then
					error("sarcasm: PTR size annotation (" .. tostring(mem.size)
						.. " bytes) contradicts the memory access width (" .. tostring(regW)
						.. " bytes) of '" .. m .. "': " .. (st.raw or m))
				end
			end
			-- A `store ptr` / `load ptr` on a MATERIALIZED slot access is rejected:
			-- the access rides real memory in sarcasm's synthesized frame, and the
			-- invisicap sidecar bytes have no home there (only the transform's
			-- heap/region memory does). Move the pointer into a virtualized slot
			-- (outside the FP-tainted ranges) or through a real pointer instead.
			if slotPtrAnno then
				error("sarcasm: `" .. slotPtrAnno .. "` on a stack-frame access that"
					.. " materializes into real frame memory (an FP/vector scratch cluster)"
					.. " is not supported: " .. (st.raw or m))
			end
			-- AVX512 decorators ({1toN} embedded broadcast) MUST ride along on the
			-- materialized operand: the slot was sized by the DECORATED access width
			-- (fp.accessWidth applies bcastN), so dropping the decorator would emit a
			-- full-width vector access to an element-sized slot. ({kN} masks lower
			-- here — see the gate above and emitMaskedStackLower.)
			local mat = { kind = "mem",
				base = { kind = "reg", rc = "sp", num = 4, width = 64, name = "rsp" },
				scale = 1, dispVal = final, size = wb, frameSlot = true,
				deco = mem.deco, bcastN = mem.bcastN }
			local newops = {}
			for _, o in ops do table.insert(newops, (o == mem) and mat or o) end
			if maskedStackLower then
				emitMaskedStackLower(m, mat, ops, mem, maskedStackLower.deco)
				return
			end
			emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops, raw = st.raw, line = st.line, annotation = st.annotation })
			return
		end
		-- The accepted `store ptr` / `load ptr` shape (plain 8-byte GPR
		-- register<->slot move, virtualized slot): drop the annotation — the
		-- move's pointer-flow propagation IS the capability transfer (see the
		-- acceptance comment above). Everything else about the rewrite is
		-- unchanged; a narrower scalar store to the same offset is a full
		-- zero-extending def of the slot web, so it kills the capability on its
		-- path and a later `load ptr` traps at the deref (null capability).
		local emitAnno = st.annotation
		if slotPtrAnno then emitAnno = nil end
		if not slotOffSafe(off) then
			error("sarcasm: mid-function inner frame reaches the incoming stack-argument"
				.. " slot band (a frame slot there would alias an argument slot; cannot"
				.. " prove safety): " .. (st.raw or m))
		end
		-- Containing-granule routing (partial slot access): a narrow (1/2/4-byte)
		-- access whose byte range falls inside a granule that (per the analysis's
		-- slotGranules map) carries 8-byte slot traffic at its 8-byte-aligned base is
		-- routed to the CONTAINING slot web with a partial-access marker, instead of
		-- creating a fresh web at its own offset. The marker (x86_slotPartial: the
		-- sub-offset, the width, and the direction) drives the transform's extract /
		-- read-modify-write lowering, which makes the virtualized web behave exactly
		-- like the memory it stands for (a narrow store updates bytes of the enclosing
		-- qword; a narrow load reads them). Fully-containing only (sub + w <= 8): a
		-- wider-than-remainder access straddles two granules and keeps the historical
		-- per-offset web (the pre-existing overlap gap). Aligned (sub == 0) narrow
		-- traffic that already behaves correctly on the shared web — a load, or a
		-- 1/2-byte store (a modeled web RMW) — keeps its historical form; only the
		-- aligned 4-byte STORE is routed (its zero-extending def was the documented
		-- same-offset gap). Narrow traffic into a NO-granule offset keeps the
		-- historical per-offset web (unchanged behavior).
		local partial = nil
		if wb < 8 and (wb == 1 or wb == 2 or wb == 4) then
			-- Only a single-memory-operand statement routes: a second (heap) memory
			-- operand would need the transform's access-check cascade alongside the
			-- partial-access lowering, and the granule scan only marks single-mem
			-- statements in the first place. (Two STACK mems never reach here — the
			-- escape scan rejects them.)
			local memCount = 0
			for _, o in ops do if o.kind == "mem" then memCount += 1 end end
			-- Write direction: the (dest-first) first operand is the memory operand —
			-- EXCEPT cmp/test (their memory operand is read-only: it feeds the flag
			-- compare) and the one-operand mul/div family (the memory operand is the
			-- multiplicand/divisor). Same positional rule as x86_64_codegen.isStoreInsn.
			local isWrite = ops[1] == mem and b ~= "cmp" and b ~= "test"
				and not (#ops == 1 and (b == "mul" or b == "imul" or b == "div" or b == "idiv"))
			if memCount == 1 then
				local base = off - (off % 8)
				local sub = off - base
				-- Aligned (sub == 0) narrow traffic that ALREADY behaves correctly on
				-- the shared web keeps its historical form: a load reads the low bytes,
				-- and a 1/2-byte store is modeled as a web RMW (partialRegWrite) whose
				-- emitted byte/word store preserves the colored register's upper bits.
				-- Only the aligned 4-byte STORE needs the merge lowering (its
				-- zero-extending def was the documented same-offset gap).
				local historical = (sub == 0 and (not isWrite or wb < 4))
				if slotGranules[base] and sub + wb <= 8 and not historical then
					partial = { sub = sub, w = wb, write = isWrite }
					off = base
				end
			end
		end
		local slot = ctx.slotReg(off, wb * 8)
		local newops = {}
		for _, o in ops do table.insert(newops, (o == mem) and slot or o) end
		emit({ kind = "insn", mnemonic = st.mnemonic, operands = newops, raw = st.raw, line = st.line, annotation = emitAnno, x86_slotPartial = partial })
		return
	end

	-- (a lea of a stack address is handled — and rejected — inside the slot block above:
	-- symbolic-displacement stack operands were rejected above, and every other
	-- stack-based lea hits rejectEscape. The historical in-region lea passthrough
	-- (for the transform's region redirect) is unreachable: regions have no sources
	-- anymore, so the region lookups always miss.)

	-- A recorded `movq %rsp, %reg` saved-rsp carrier: DROP it — sarcasm synthesizes
	-- its own frame, so the parked value has no meaning in the output (it is
	-- phantom): the carrier flows and the recovery are dropped with it, and the
	-- analysis rejects every non-carrier read of the value while the save is
	-- outstanding. Register class and position are unrestricted (every use of a
	-- carrier is dropped or rejected, so no call clobber can be observed). (The
	-- historical exception for a caller-saved save with a frame-escape region
	-- based at rsp+0 — which kept the save alive as a REAL redirected value —
	-- is gone with the region sources.)
	if st.x86_spSave then return end

	-- `movq %rsp, %reg` region passthrough: when a region is based at rsp+0
	-- (static region, or — at a post-prologue "dyn" depth — an anchored one) the
	-- transform redirects this to the allocation pointer (mirrors arm64's
	-- `mov xD, sp` handling). UNREACHABLE today: no region sources remain, so
	-- regionOf always misses and the fall-through escape scan below rejects every
	-- `movq %rsp, %reg`. The static region base is only known relative to the
	-- post-prologue frame base: reject when rsp is perturbed/unknown here (the
	-- anchored case IS the dyn one).
	if b == "mov" and ops[2] and ops[2].rc == "sp" and ops[1] and ops[1].kind == "reg"
		and ops[1].rc == "gpr" then
		local movDyn = not st.x86_prologue and type(d) == "table"
		-- A dynamic-region scope has no static frame-offset region list entry:
		-- `movq %rsp,%reg` there reads region base + bias — a region pointer.
		local reg0 = ctx.regionOf(0, movDyn) or (movDyn and d.dynreg or nil)
		if reg0 then
			if not st.x86_prologue and d ~= nil and d ~= D0 and not movDyn then
				error("sarcasm: reading the stack pointer while it is perturbed or unknown"
					.. " cannot be proven safe: " .. (st.raw or m))
			end
			emit(st); return
		end
	end

	-- Any remaining instruction consuming a stack register as a VALUE (e.g.
	-- `movq %rsp, %rax`, `pushq %rsp`) obtains a live stack address sarcasm cannot track:
	-- reject (arm64 rejects the analogous `mov/add xD, sp, ...`). When rbp is the frame
	-- pointer it IS a stack register (rbp = rsp + frameSize), so reading it as a value —
	-- `movq %rbp, %rax`, arithmetic on it — is the same escape class. (The frame setup /
	-- teardown paths that legitimately name rbp — `movq %rsp, %rbp`, `movq %rbp, %rsp`,
	-- `leave`, teardown `popq %rbp` — all return earlier.) Stack-register memory BASES
	-- were handled above (virtualized or rejected); a stack register hiding in a memory
	-- operand's INDEX slot (`movq (%rdi,%rsp,1), %rax` — which the encoding forbids but
	-- the parser accepts — or `movq (%rdi,%rbp,1), %rax` in an fp frame) escapes the same
	-- way, so scan nested memory registers too.
	for _, o in ops do
		if isStackReg(o, usesRbp) then ctx.rejectEscape(st.raw or m) end
		if o.kind == "mem" and (isStackReg(o.base, usesRbp) or isStackReg(o.index, usesRbp)) then
			ctx.rejectEscape(st.raw or m)
		end
	end

	emit(st)
end

-- The vector-save reservation x86_64 uses when a function's xmm registers must be
-- preserved across an injected runtime call (16 bytes per xmm0..xmm15 slot — the
-- Fil-C runtime is SSE2-only, so xmm16-31 / ymm / zmm / k state is unreachable
-- for it; the expandFpSaves liveness narrows each slot's mov to the live width).
-- The transform forces this reservation for FP-signature CALLSITE functions even
-- when the body touches no vector register (see transform.luau) — the ENTRY
-- signature case is forced directly in analyzeFrame above — matching
-- arm64_frame.VEC_SAVE_BYTES. Consumed by codegen (which also widens its
-- save/restore activation to any forced reservation) and the layout.
x86_64_frame.VEC_SAVE_BYTES = 16 * 16

-- The net rsp depth at the end of the prologue prefix (the frame base) of the
-- function most recently analyzed — the coordinate origin the +8 rule's clone
-- displacement bias is computed against (set by analyzeFrame; single-threaded
-- discipline, like D0).
function x86_64_frame.frameBaseDepth() return D0 end

-- The rbp-relative normalization base of the function most recently analyzed
-- (rbp = rsp + rbpNorm at the post-prologue rsp; 0 without a frame pointer):
-- an rbp-relative displacement d normalizes to frame offset rbpNorm + d. This
-- is D0 - fpDepth with a frame pointer — 0 for the rbp-at-the-bottom layout
-- (bsaes/aesni-xts `leaq -N(%rsp),%rsp; movq %rsp,%rbp`). Set by analyzeFrame
-- (single-threaded discipline, like D0); codegen normalizes rbp-relative
-- region/stack offsets with it (the frame rewrite's exact coordinates).
function x86_64_frame.rbpNormalization() return rbpNorm end

return x86_64_frame
