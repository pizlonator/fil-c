--!nonstrict
-- Pointer-flow analysis over the lifted IR (register webs = temps).
-- Determines which temps hold pointers and, for each, the temp that carries its
-- `lower` (capability). Lowers propagate unchanged through copies and address
-- arithmetic (GEP), matching GIMSO semantics.
--
-- Seeds (provided by the caller):
--   argPtrTemps  : array of temp ids that are pointer arguments
--   loadPtrDefs  : set of node -> true for `;! load ptr` nodes (their def temp is a ptr)
--   callPtrDefs  : set of node -> true for call nodes whose result is a pointer
--   capSave      : map of node -> var name for `save capability (var)` nodes
--   capRestore   : map of node -> var name for `restore capability (var)` nodes
--
-- Returns { isPtr = {temp->bool}, lowerTemp = {temp->loTemp}, newTemps = {...} }
-- New lower temps are allocated via allocTemp().

local ptrflow = {}

function ptrflow.analyze(lf, seeds, allocTemp, opts)
	opts = opts or {}
	local baseMnemonic = opts.baseMnemonic or function(m) return string.lower(m) end
	local isPtr = {}
	local lowerTemp = {}     -- ptr temp -> its lower temp
	local nodes = lf.nodes

	local function ensureLower(t)
		if not lowerTemp[t] then
			lowerTemp[t] = allocTemp({ isLower = true, forPtr = t })
		end
		return lowerTemp[t]
	end

	-- Seed argument pointers.
	for _, t in (seeds.argPtrTemps or {}) do
		isPtr[t] = true
		ensureLower(t)
	end
	-- Seed load-ptr / call-ptr / `.alloca`-result definitions (their def temp is a
	-- pointer with a fresh lower).
	for _, node in nodes do
		if node.kind == "insn" then
			if (seeds.loadPtrDefs and seeds.loadPtrDefs[node])
				or (seeds.callPtrDefs and seeds.callPtrDefs[node])
				or (seeds.allocaResultDefs and seeds.allocaResultDefs[node]) then
				local d = node.defTemps and node.defTemps[1]
				if d then isPtr[d] = true; ensureLower(d) end
			end
		end
	end

	-- Seeded webs (pointer arguments, pointer loads/returns, alloca results)
	-- are NEVER killed to scalar: a seed defines a genuine pointer, and
	-- later killing ops on the same RMW web merely reuse the register as
	-- scratch. Keeping the seed's capability is the long-standing behavior
	-- everything relies on (it also keeps entry unpacking's root stores and
	-- the `use capability` validations well-formed, which assume seeded
	-- webs always carry lowers). Kills apply only to mov/ALU-derived
	-- scratch webs (the AES_decrypt `mov $key,%rbp` shape), which no seed
	-- roots. Web identities are lift-fixed, so this set is stable for the
	-- whole fixpoint.
	local seedWeb = {}
	for _, t in (seeds.argPtrTemps or {}) do seedWeb[t] = true end
	for _, node in nodes do
		if node.kind == "insn" then
			if (seeds.loadPtrDefs and seeds.loadPtrDefs[node])
				or (seeds.callPtrDefs and seeds.callPtrDefs[node])
				or (seeds.allocaResultDefs and seeds.allocaResultDefs[node]) then
				local d = node.defTemps and node.defTemps[1]
				if d then seedWeb[d] = true end
			end
		end
	end

	-- Webs observed as an address (base/index of a real memory access) are
	-- never killed either: a later killing op on the same RMW web merely
	-- reuses the register as scratch, but the pre-kill range addresses
	-- memory through it — sha1's `sub $in0,$out` feeds `disp($out,$in0)`
	-- stores (guarded by an explicit `#! use capability $out`), and vpaes's
	-- `subq %rdi,%rsi` feeds the `(%rsi,%rdi,1)` loop under the base's
	-- capability — so a null capability there would trap legitimate
	-- accesses. Kills apply only to pure-integer scratch that never
	-- addresses memory (the AES_decrypt select: its scratch touches only
	-- ALU ops and leas). `lea` is address COMPUTATION, not an access, so
	-- lea-operand uses don't block a kill. Web identities are lift-fixed,
	-- so this set is stable for the whole fixpoint.
	local memAddrWeb = {}
	for _, node in nodes do
		if node.kind == "insn" then
			local m = baseMnemonic(node.stmt and node.stmt.mnemonic or node.mnemonic or "")
			if m ~= "lea" and node.stmt and node.stmt.operands and node.opTemp then
				for _, op in node.stmt.operands do
					if op.kind == "mem" then
						if op.base and node.opTemp[op.base] then
							memAddrWeb[node.opTemp[op.base]] = true
						end
						if op.index and node.opTemp[op.index] then
							memAddrWeb[node.opTemp[op.index]] = true
						end
					end
				end
			end
		end
	end

	-- Fixpoint propagation through copies and address arithmetic.
	--
	-- Convergence. isPtr only ever goes false->true, but a temp's lower CAN be reassigned
	-- by a later def of the same web (x86 2-operand RMW instructions and branch joins
	-- both give one web several def nodes). Reassignment is fine when the web's pointer
	-- sources eventually agree (e.g. a `;! load ptr` seed's fresh lower gets replaced by
	-- the propagated source's lower and stays there). A web whose defs draw on genuinely
	-- different pointer origins never settles: the lower flip-flops between the origins'
	-- lowers forever. Each pass is a deterministic function of the (isPtr, lowerTemp)
	-- state, so a repeated global state proves non-convergence.
	--
	-- Dynamic-lower widening (the pointer-SWAP case). A non-converging web is not always
	-- a genuine conflict: a loop-carried pointer swap (keccak1600's `xchgq %rsi,%rdi`
	-- bounce-buffer round, or the equivalent three-move rotation) oscillates because no
	-- STATIC lower is correct for a web whose runtime value alternates between two
	-- origins — but a DYNAMIC lower is: give the web its own fresh lower temp and mirror
	-- every pointer def of the iv web with a lower def from the same source (a "lockstep
	-- copy"), so the (iv, lower) pairing tracks the iv dataflow exactly at runtime. The
	-- lower temp then has one def per iv def (a phi, exactly like the iv web itself) and
	-- the assignment lowerTemp[web] is fixed at creation — the fixpoint can no longer
	-- oscillate on it. Widening is monotone (a web goes dynamic at most once), so the
	-- loop terminates. The copies themselves are emitted by the transform from the
	-- returned dynCopy map; a swap's crossed pair is emitted atomically (via a scratch).
	-- Widening is sound: every capability a dynamic lower can hold entered through a
	-- seed, and every seed roots its lower at definition, so the GC still sees every
	-- live capability; and a stale lower on a scalar-holding def can only trap or pass
	-- for an in-bounds address, never access out of bounds.
	--
	-- Contagion: a pointer COPY whose source is dynamic must snapshot the source's
	-- lower at the copy point (sharing it would observe the source's LATER swaps), so
	-- copies out of dynamic webs widen the destination too. xchg is modeled as the
	-- crossed pair (first operand's def web receives the second operand's pointer-ness
	-- and lower, and vice versa); in a loop the crossed sharing oscillates and the
	-- widening above turns it into the runtime lower swap. A web defined by an xchg is
	-- widened together with its swap partner: the emission exchanges the two lowers,
	-- so both sides must be dynamic (or neither).
	-- The widening machinery is enabled per-target (x86_64; arm64 keeps the
	-- historical clean rejection — no xchg exists there, and its codegen is
	-- unchanged by construction).
	local widen = opts.widenSwaps and true or false
	local dynamic = {}        -- web -> true once widened to a lockstep lower
	local dynSrc = {}         -- node -> { [defWeb] = sourceWeb } for dynamic defs
	local capShot = {}        -- save var -> { web, lo }: the frozen snapshot (phase 2)
	-- Killed webs (sticky scalars). A shift/rotate result is NEVER a pointer:
	-- shifting an address destroys its meaning, so the destination web is
	-- forced scalar even when the shifted value was a pointer (an RMW
	-- `shrq $3,%rbp` shares one web for the pointer input and the integer
	-- output — merely skipping propagation would leave the key's capability
	-- on the integer). Likewise `sub` of two pointers is an integer
	-- difference, not a pointer. The kill is sticky: once a web is killed,
	-- no later merge may re-mark it (markFrom skips killed webs), so the
	-- fixpoint stays monotone and converges. Seeded webs and webs that
	-- address memory are exempt (see `seedWeb`/`memAddrWeb` below): only
	-- mov/ALU-derived pure-integer scratch is killed.
	-- Over-killing is sound: a scalar web carries a null capability, so a
	-- mistaken kill can only trap (bounds check on null), never access out
	-- of bounds.
	local killed = {}
	-- Shift/rotate mnemonics (post-baseMnemonic, plus the rcl/rcr forms
	-- baseMnemonic leaves suffixed) whose register destination is always a
	-- scalar kill of its web.
	local KILL_SHIFT = {
		shl = true, shr = true, sal = true, sar = true,
		rol = true, ror = true, rcl = true, rcr = true,
		shld = true, shrd = true,
		rclb = true, rclw = true, rcll = true, rclq = true,
		rcrb = true, rcrw = true, rcrl = true, rcrq = true,
	}
	-- Pinned webs (save AND restore def webs): in phase 2, no instruction but
	-- a save may change a pinned web's capability. Saves need their own
	-- natural inflow, and restores are explicit user overrides, so every other
	-- node's merge into a pinned web is skipped — later clobbering math can
	-- never overwrite a snapshot, and a restore always wins over its
	-- instruction's own sources. Phase 1 runs the natural fixpoint with no cap
	-- effects at all, so save webs resolve exactly as without annotations.
	-- (Sound either way: a protected capability can only trap — its bounds
	-- check still guards every access — never access out of bounds.)
	local pinned = {}
	do
		local function pinMap(m)
			if not m then return end
			for pnode in m do
				if pnode.kind == "insn" then
					local pd = pnode.defTemps and pnode.defTemps[1]
					if pd then pinned[pd] = true end
				end
			end
		end
		pinMap(seeds.capSave)
		pinMap(seeds.capRestore)
	end
	-- Phase 2 runs only when restores exist (otherwise the phase-1 state is
	-- already the answer, and lone saves are validated against it).
	local phase2 = false

	-- The memory-address exclusion shared by the propagation rule below and
	-- the save snapshot below: a memory operand's base/index registers are
	-- consumed as an ADDRESS, not as a value source (`lea` excepted). A
	-- memory-source instruction is a LOAD, and the loaded value's pointer-ness
	-- comes from a `;! load ptr` seed, never the address register. On x86 a
	-- load is `mov reg, [mem]`, so without the hasMemSrc guard ptrflow would
	-- treat it as `reg = base` and flip-flop the lower forever. The same
	-- applies to memory-source arithmetic like `addq (%rsi), %rax` (which adds
	-- the loaded SCALAR to %rax): the address registers are excluded from the
	-- pointer-source candidates, otherwise each such def would re-derive %rax's
	-- lower from its own address register and flip-flop across the defs of the
	-- web. (`lea` is the exception: its memory-shaped operand IS the computed
	-- value, so its base/index remain legitimate pointer sources.)
	local function capAddrInfo(nd, m)
		local memSrc, aus = false, nil
		local sops = nd.stmt and nd.stmt.operands
		if sops then
			for _, op in sops do
				if op.kind == "mem" then
					memSrc = true
					if m ~= "lea" and nd.opTemp then
						if op.base and nd.opTemp[op.base] then
							if not aus then aus = {} end
							aus[nd.opTemp[op.base]] = true
						end
						if op.index and nd.opTemp[op.index] then
							if not aus then aus = {} end
							aus[nd.opTemp[op.index]] = true
						end
					end
				end
			end
		end
		return memSrc, aus
	end

	-- The address-arithmetic source selection shared by the propagation rule
	-- below and the save snapshot below: which pointer source a
	-- pointer-combined-with-integers result draws its lower from (including
	-- the lea-with-pointer-base preference). A `sub` whose value sources
	-- are all pointers (intCount == 0) is an integer difference, not a
	-- pointer: the propagation rule kills its destination web to a sticky
	-- scalar instead of drawing any lower (and the save snapshot falls back
	-- to the lower-less def web, which the transform rejects loudly).
	-- Returns (ptrSrc, intCount).
	--
	-- Only genuine address arithmetic preserves capabilities: add, sub, lea,
	-- and, or (arm64 orr). Shifts (shl/shr/sal/sar/lsl/lsr/asr and the
	-- rotates) NEVER do: shifting a pointer destroys its meaning as an
	-- address, so a shift result is killed to a sticky scalar — it must not
	-- inherit a capability from any operand, notably not from a count
	-- register (`shlq %cl,%rdx` must not make %rdx carry %rcx's
	-- capability). The caller gates which mnemonics reach this
	-- function (shifts never do — they kill unconditionally); this helper
	-- itself just scans the uses.
	--
	-- The lea preference covers aes's cache-alias trick (`leaq 768(%rsp),%rbp;
	-- subq %r14,%rbp; andq $0x300,%rbp; leaq (%r14,%rbp,1),%r14` — the final
	-- address derives from the T-table base, while the index is scalar math
	-- that kept its seed's lower through the RMW web union): the computed
	-- address derives from the base, so the base's lower is the right one.
	-- Prefer it. (x86_64-gated via widen: arm64's behavior is unchanged.)
	local function capAluSrc(nd, m, us, addrUse)
		local ptrSrc, intCount = nil, 0
		for _, u in us do
			if addrUse and addrUse[u] then
				-- consumed as a memory address, not a value source
			elseif isPtr[u] then ptrSrc = u else intCount += 1 end
		end
		local sops = nd.stmt and nd.stmt.operands
		if m == "lea" and widen and sops and nd.opTemp then
			for _, op in sops do
				if op.kind == "mem" and op.base and op.base ~= op.index then
					local bt = nd.opTemp[op.base]
					if bt and isPtr[bt] then ptrSrc = bt end
				end
			end
		end
		return ptrSrc, intCount
	end

	-- The source web whose capability a save captures: the value source the
	-- propagation rule would draw from, read at phase-1 convergence — so later
	-- clobbering math on the same web cannot pollute the snapshot. Falls back
	-- to the save's own def web (RMW-sticky ops keep the web's own capability,
	-- as do seeded address materializations like global leas; loads and other
	-- scalar producers yield a lower-less web, which the transform rejects
	-- loudly).
	local function capSaveSource(snode)
		local m = baseMnemonic(snode.stmt and snode.stmt.mnemonic or snode.mnemonic or "")
		local us = snode.useTemps or {}
		local d = snode.defTemps and snode.defTemps[1]
		local hasMemSrc, addrUse = capAddrInfo(snode, m)
		local src = nil
		if m == "mov" then
			if #us == 1 and not hasMemSrc then src = us[1] end
		elseif m == "cmov" then
			for _, u in us do
				if u ~= d and not (addrUse and addrUse[u]) then src = u break end
			end
		elseif m == "add" or m == "sub" or m == "orr" or m == "lea"
			or m == "and" or m == "or" then
			local ptrSrc, intCount = capAluSrc(snode, m, us, addrUse)
			if ptrSrc and not (m == "sub" and intCount == 0) then src = ptrSrc end
		end
		if src and isPtr[src] then return src end
		return d
	end
	local isXchg = function(m) return string.match(m, "^xchg[bwlq]?$") ~= nil end

	local dynamicize
	function dynamicize(d)
		if dynamic[d] or not widen then return dynamic[d] and true or false end
		dynamic[d] = true
		isPtr[d] = true
		lowerTemp[d] = allocTemp({ isLower = true, forPtr = d, dynamicLower = true })
		-- Widen the swap partner too (see above): the xchg emission swaps the lowers.
		for _, node in nodes do
			if node.kind == "insn" then
				local m = baseMnemonic(node.stmt and node.stmt.mnemonic or node.mnemonic or "")
				if isXchg(m) then
					local defs = node.defTemps or {}
					if defs[1] == d and defs[2] and isPtr[defs[2]] then dynamicize(defs[2]) end
					if defs[2] == d and defs[1] and isPtr[defs[1]] then dynamicize(defs[1]) end
				end
			end
		end
		return true
	end

	-- Force a web scalar (see the `killed` set above): drops any capability
	-- it carries and pins it scalar so later merges cannot re-mark it. A
	-- no-op on dynamic (lockstep) webs, whose lowers are maintained
	-- per-def by the widening machinery instead; on seeded webs (see
	-- `seedWeb` above), which keep their capability unconditionally; and on
	-- webs that address memory (see `memAddrWeb` above), whose pre-kill
	-- range holds legitimate accesses. Returns true when the kill changed
	-- the analysis state.
	local function killWeb(d)
		if not d or dynamic[d] or killed[d] or seedWeb[d] or memAddrWeb[d] then
			return false
		end
		killed[d] = true
		local changedNow = false
		if isPtr[d] then isPtr[d] = nil; changedNow = true end
		if lowerTemp[d] ~= nil then lowerTemp[d] = nil; changedNow = true end
		return changedNow
	end

	local seenStates = {}
	local changed = true
	while changed do
		changed = false
		local reassigned = {}
		for _, node in nodes do
			local seeded = (seeds.loadPtrDefs and seeds.loadPtrDefs[node])
				or (seeds.allocaResultDefs and seeds.allocaResultDefs[node])
			if node.kind == "insn" and not seeded then
				local m = baseMnemonic(node.stmt and node.stmt.mnemonic or node.mnemonic or "")
				local us = node.useTemps or {}
				local function markFrom(d, srcPtr)
					if not d or not isPtr[srcPtr] then return end
					if killed[d] then
						-- A killed (integer) web never regains a capability:
						-- an upstream pointer merge into the same RMW web
						-- (e.g. the `mov key,%rbp` before a killing `sub`)
						-- must not resurrect the capability on the integer.
						return
					end
					if dynamic[srcPtr] and not dynamic[d] then
						-- snapshot contagion (see above)
						dynamicize(d)
						changed = true
					end
					if dynamic[d] then
						-- the lower is fixed; record the def's source for the lockstep
						-- copy the transform emits at this node.
						dynSrc[node] = dynSrc[node] or {}
						dynSrc[node][d] = srcPtr
						return
					end
					if phase2 and pinned[d] and not (seeds.capSave and seeds.capSave[node]) then
						-- A pinned (saved/restored) web keeps its capability:
						-- only a save's own natural inflow may set it; every
						-- other node's merge is skipped, so later clobbering
						-- math can never overwrite a snapshot and a restore
						-- always wins over its instruction's own sources. This
						-- is sound either way: a protected capability can only
						-- trap (its bounds check still guards every access),
						-- never access out of bounds.
						return
					end
					if not isPtr[d] then isPtr[d] = true; changed = true end
					local lo = lowerTemp[srcPtr]
					if lo and lowerTemp[d] ~= lo then
						if lowerTemp[d] ~= nil and widen then
							-- d already carries a DIFFERENT lower: this def merges a
							-- second, distinct pointer origin into the web, so no single
							-- static lower is correct for it (a loop-carried pointer
							-- swap or a conflicting join — rsaz_1024_sqr_avx2's
							-- `movq %rdi,%rsi` out/inp swap, whose web is defined by the
							-- entry arg `a` AND the loop-back `out`). Widen to a dynamic
							-- lower that tracks each def's source in lockstep, rather
							-- than overwriting and silently dropping one origin's
							-- capability (which then mis-keys a later pointer arg).
							if dynamicize(d) then
								dynSrc[node] = dynSrc[node] or {}
								dynSrc[node][d] = srcPtr
							end
						elseif lowerTemp[d] ~= nil then
							reassigned[d] = true
							lowerTemp[d] = lo
						else
							lowerTemp[d] = lo
						end
						changed = true
					end
				end
				-- A memory operand's base/index registers are consumed as an ADDRESS, not
				-- as a value source (see capAddrInfo above for the full rationale).
				local hasMemSrc, addrUse = capAddrInfo(node, m)
				if isXchg(m) then
					-- register-register xchg (memory forms are rejected by classify):
					-- the crossed pair — each def web receives the OTHER operand's value.
					local defs = node.defTemps or {}
					if #defs == 2 and #us == 2 then
						-- A LOOP-CARRIED swap (each def web coincides with its use web —
						-- the def site reaches back into the web the xchg reads): the
						-- web's runtime value alternates between the two origins, so no
						-- static lower is correct (crossed sharing converges to BOTH
						-- webs sharing one origin's lower — false traps half the time).
						-- Force the dynamic lower swap. (A straight-line xchg has fresh
						-- def webs: crossed sharing is exact there, and a swap of two
						-- same-origin pointers shares one lower already.)
						local loopCarried = (defs[1] == us[1] or defs[1] == us[2])
							and (defs[2] == us[1] or defs[2] == us[2])
						if widen and loopCarried and isPtr[us[1]] and isPtr[us[2]]
							and lowerTemp[us[1]] ~= lowerTemp[us[2]]
							and not (dynamic[defs[1]] and dynamic[defs[2]]) then
							dynamicize(defs[1])
							dynamicize(defs[2])
							changed = true
						end
						markFrom(defs[1], us[2])
						markFrom(defs[2], us[1])
					end
				else
					local d = node.defTemps and node.defTemps[1]
					if d then
						if m == "mov" and #us == 1 and not hasMemSrc then
							markFrom(d, us[1])
						elseif m == "cmov" then
							-- x86 cmov: a CONDITIONAL merge of the register source into
							-- the destination web (the rmw union makes the def web the
							-- destination's use web): d = cond ? src : d. A memory
							-- source is a load — the loaded value is a scalar here
							-- (pointer loads carry `;! load ptr`, never a cmov), and
							-- memory address registers are excluded from value sources
							-- as usual. Pointer-ness merges in from the source: a web
							-- not yet a pointer simply shares the source's lower (the
							-- not-taken path then carries a stale lower on a scalar —
							-- the documented sound case, like a join). A web that is
							-- ALREADY a pointer with the SAME lower needs nothing.
							-- With DIFFERENT lowers no static lower is correct (the
							-- hardware picks per execution): widen to a dynamic
							-- (lockstep) lower and maintain it at this node with a
							-- CONDITIONAL lower copy — a cmov on the lower temps with
							-- the same condition, emitted by the transform right after
							-- the iv cmov (dynCopy carries `cmov = true`).
							local src = nil
							for _, u in us do
								if u ~= d and not (addrUse and addrUse[u]) then src = u break end
							end
							if src and isPtr[src] then
								if dynamic[d] then
									dynSrc[node] = dynSrc[node] or {}
									dynSrc[node][d] = src
								elseif isPtr[d] and lowerTemp[d] ~= lowerTemp[src] then
									if dynamicize(d) then
										dynSrc[node] = dynSrc[node] or {}
										dynSrc[node][d] = src
										changed = true
									else
										-- widening disabled on this target: treat as a
										-- plain conflicting merge — the cycle detector
										-- reports the non-convergence cleanly.
										markFrom(d, src)
									end
								elseif not isPtr[d] then
									markFrom(d, src)
								end
							end
						elseif m == "add" or m == "sub" or m == "orr" or m == "lea"
							or m == "and" or m == "or" then
							-- address arithmetic / alignment: a pointer combined with integers
							-- keeps the pointer's lower (bounds checks still guard any misuse).
							-- (Source selection lives in capAluSrc above, shared with
							-- the save snapshot.)
							--
							-- Explicit capability directive stashed on the node by the
							-- transform (`#! use capability %reg` on any of these ops;
							-- `#! new capability %reg` is a deprecated alias): the
							-- result draws from the NAMED source, bypassing the scan
							-- and (for sub) the ptr-ptr guard. The annotation is an
							-- explicit, validated, bounds-checked user choice for the
							-- AMBIGUOUS case where several inputs carry capabilities;
							-- a wrong choice can only trap (its bounds check still
							-- guards every access), never access out of bounds.
							--
							-- `sub` of two pointers is an integer difference, not a
							-- pointer: when every value source carries a capability
							-- (and at least two do — a single-source `sub` off a
							-- memory operand subtracts a loaded scalar and stays a
							-- pointer), the destination web is killed to a sticky
							-- scalar instead of keeping any capability. The RMW web
							-- is shared with the pointer input, so merely skipping
							-- propagation would leave the input's capability on the
							-- integer (the AES_decrypt `sub $sbox,%rbp` poison).
							local ov = node.useCapTemp or node.newCapTemp
							if ov then
								markFrom(d, ov)
							elseif m == "sub" then
								local ptrSrc, intCount = capAluSrc(node, m, us, addrUse)
								if ptrSrc and intCount == 0 then
									local nptr = 0
									for _, u in us do
										if not (addrUse and addrUse[u]) and isPtr[u] then
											nptr += 1
										end
									end
									if nptr >= 2 then
										if killWeb(d) then changed = true end
									end
								elseif ptrSrc then
									markFrom(d, ptrSrc)
								end
							else
								local ptrSrc, intCount = capAluSrc(node, m, us, addrUse)
								if ptrSrc then
									markFrom(d, ptrSrc)
								end
							end
						elseif KILL_SHIFT[m] then
							-- Shifts/rotates never propagate capabilities: the
							-- result is an integer scratch even when the shifted
							-- value was a pointer (and the count register's
							-- capability never transfers to the destination).
							-- The RMW destination shares its web with the input,
							-- so the web must be actively killed — skipping it
							-- would leave the input's capability on the integer
							-- (the AES_decrypt `shr $3,%rbp` poison).
							if killWeb(d) then changed = true end
						end
					end
				end
			end
			-- Capability restore (`restore capability (v)`, threaded in as
			-- seeds.capRestore node -> var). Runs only in phase 2 (phase 1 ran
			-- the natural fixpoint with no cap effects) and outside the seeded
			-- guard above: a restore node is an ordinary value-producing
			-- instruction whose propagation is unaffected. The shared lower is
			-- the phase-1 snapshot frozen at convergence, so clobbering math
			-- on the same web can never pollute what the restore re-attaches.
			if phase2 and node.kind == "insn" then
				local rv = seeds.capRestore and seeds.capRestore[node]
				if rv then
					local rd = node.defTemps and node.defTemps[1]
					local shot = capShot[rv]
					local slo = shot and shot.lo or nil
					local sweb = shot and shot.web or nil
					-- A dynamic (lockstep) lower belongs to exactly one web
					-- and cannot be shared: the transform rejects such
					-- saves/restores loudly in its post-pass validation.
					if rd and slo and sweb and not dynamic[sweb] and not dynamic[rd] then
						if not isPtr[rd] then isPtr[rd] = true; changed = true end
						if lowerTemp[rd] ~= slo then lowerTemp[rd] = slo; changed = true end
					end
				end
			end
		end
		if changed then
			-- Cycle detection: a repeated (isPtr, lowerTemp) state after a changing pass
			-- means the deterministic sharing iteration would loop forever (see above);
			-- widen the webs it reassigned to dynamic lowers and keep going.
			local parts = {}
			for t, lo in lowerTemp do table.insert(parts, tostring(t) .. "=" .. tostring(lo)) end
			for t, v in isPtr do if v then table.insert(parts, tostring(t) .. "=ptr") end end
			for t, v in dynamic do if v then table.insert(parts, tostring(t) .. "=dyn") end end
			for t in killed do table.insert(parts, tostring(t) .. "=killed") end
			table.sort(parts)
			local key = table.concat(parts, ";")
			if seenStates[key] then
				-- Widen in sorted order: the fresh lower temps are allocated in this
				-- order, and hash iteration over the reassigned set would make the
				-- temp ids (and every later temp id, spill layout, and coloring
				-- tie-break) reorder per process — nondeterministic output.
				local widened = {}
				for d in reassigned do table.insert(widened, d) end
				table.sort(widened)
				local any = false
				for _, d in widened do
					if dynamicize(d) then any = true end
				end
				if not any then
					-- Genuine conflict (or widening disabled on this target): one web
					-- has a single runtime lower, so it cannot hold pointers from two
					-- origins — reject cleanly instead of spinning.
					error("sarcasm: pointer-flow analysis does not converge: a register web "
						.. "has conflicting pointer sources (one temp cannot carry two capabilities)")
				end
				seenStates = {}
			else
				seenStates[key] = true
			end
		else
			-- Converged. After phase 1, freeze each save's snapshot (the source
			-- capability at convergence) and — when restores exist — run phase 2,
			-- which pins restore webs and threads the frozen snapshots. Lone
			-- saves need no phase 2: the phase-1 state is already the answer.
			if not phase2 and seeds.capSave and next(seeds.capSave) ~= nil then
				for snode, var in pairs(seeds.capSave) do
					if snode.kind == "insn" then
						local sw = capSaveSource(snode)
						capShot[var] = { web = sw, lo = sw and lowerTemp[sw] or nil }
					end
				end
				if seeds.capRestore and next(seeds.capRestore) ~= nil then
					phase2 = true
					seenStates = {}
					changed = true
				end
			end
		end
	end

	-- The lockstep emission map for the transform, in statement order (deterministic):
	-- mov/lea-family defs of a dynamic web get a lower copy from the def's source;
	-- a cmov def gets a CONDITIONAL lower copy (cmov = true — the iv merge is
	-- conditional, so the lower merge must be too); an xchg gets the crossed pair
	-- (emitted atomically — the sources alias the destinations in a loop-carried
	-- swap).
	local dynCopy = {}
	for _, node in nodes do
		local dm = dynSrc[node]
		if dm then
			local m = baseMnemonic(node.stmt and node.stmt.mnemonic or node.mnemonic or "")
			if isXchg(m) then
				local defs = node.defTemps or {}
				local c1 = dm[defs[1]] and { dst = lowerTemp[defs[1]], src = lowerTemp[dm[defs[1]]] } or nil
				local c2 = dm[defs[2]] and { dst = lowerTemp[defs[2]], src = lowerTemp[dm[defs[2]]] } or nil
				if c1 and c1.dst == c1.src then c1 = nil end
				if c2 and c2.dst == c2.src then c2 = nil end
				if (c1 and not c1.src) or (c2 and not c2.src) then
					-- one-sided swap with a lower-less (non-pointer) partner: the dynamic
					-- side's static lower stays correct for its origin (the swap hands
					-- the scalar side a null capability — pointer uses of it trap).
					if c1 and not c1.src then c1 = nil end
					if c2 and not c2.src then c2 = nil end
				end
				local copies = {}
				if c1 then table.insert(copies, c1) end
				if c2 then table.insert(copies, c2) end
				if #copies > 0 then dynCopy[node] = { xchg = true, copies = copies } end
			else
				local copies = {}
				for _, d in (node.defTemps or {}) do
					local srcPtr = dm[d]
					local dstLo = d and lowerTemp[d]
					local srcLo = srcPtr and lowerTemp[srcPtr]
					if dstLo and srcLo and dstLo ~= srcLo then
						table.insert(copies, { dst = dstLo, src = srcLo })
					end
				end
				-- A cmov's lockstep copy must be CONDITIONAL (the iv merge fires
				-- only when the condition holds): `cmov = true` tells the
				-- transform to emit it as a cmov with the node's own condition,
				-- immediately after the iv cmov (the flags still hold it).
				if #copies > 0 then dynCopy[node] = { copies = copies, cmov = (m == "cmov") or nil } end
			end
		end
	end

	return { isPtr = isPtr, lowerTemp = lowerTemp, dynCopy = dynCopy, dynamic = dynamic,
		capShot = capShot }
end

return ptrflow
