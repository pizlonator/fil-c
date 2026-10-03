--!nonstrict
-- Iterated Register Coalescing (Appel & George) over a generic instruction list.
--
-- The allocator consumes a "machine function" describing instructions in terms of
-- abstract temporaries (temps). It computes liveness over a CFG built from labels and
-- branch instructions, builds the interference graph, and colors temps into the
-- physical GPR set, coalescing moves and spilling when necessary.
--
-- Instruction contract (each entry of func.insns):
--   { defs = {tempId...}, uses = {tempId...},
--     move = { dst = tempId, src = tempId } | nil,   -- reg-reg copy (coalesce candidate)
--     isLabelDef = "name" | nil,                      -- this insn is a label definition
--     targets = { "name"... } | nil,                  -- explicit branch targets
--     fallsThrough = bool }                            -- control can fall to next insn
--
-- func.temps: tempId -> { precolor = physRegName | nil, spillSlot = n | nil, spillCost = num }
-- The caller assigns temp ids (integers). Precolored temps model fixed physical regs.
--
-- Returns: { color = { tempId -> physRegName }, spilled = { tempId... } }
-- The driver re-runs after inserting spill code (handled by the caller) if spilled is
-- non-empty; here we implement the classic actual-spill rewrite inline via a callback.

local prof = require("./prof")
local err = require("./err")

local regalloc = {}

-- This allocator is architecture-agnostic: the allocatable GPR set (the K colors) is
-- supplied by the caller via `opts.allocatable` (arm64_isa.ALLOCATABLE / x86_64_isa.ALLOCATABLE).
-- Register-set knowledge (allocatable / caller- / callee-saved, reg numbers) lives in the
-- per-arch isa modules, not here.

-- Helpers ------------------------------------------------------------------

local function newSet(): { [any]: boolean }
	return {}
end
local function setAdd(s, x) s[x] = true end
local function setHas(s, x) return s[x] == true end

-- Build basic blocks from the instruction list.
-- Returns blocks = { { first=i, last=j, succ={blockIdx...}, insns={i..j} } }, and
-- labelToBlock mapping.
local function buildCFG(insns)
	local n = #insns
	-- leaders: first insn, any label def, any insn after a branch.
	local labelAt = {}          -- label name -> insn index
	for i, ins in insns do
		if ins.isLabelDef then
			labelAt[ins.isLabelDef] = i
		end
	end
	-- Determine block starts.
	local isLeader = {}
	isLeader[1] = true
	for i, ins in insns do
		if ins.isLabelDef then isLeader[i] = true end
		if ins.targets or (ins.fallsThrough == false) then
			if i + 1 <= n then isLeader[i + 1] = true end
		end
	end
	-- Create blocks.
	local blocks = {}
	local blockOfInsn = {}
	local cur
	for i = 1, n do
		if isLeader[i] or i == 1 then
			cur = { first = i, last = i, insns = {} }
			table.insert(blocks, cur)
		end
		cur.last = i
		table.insert(cur.insns, i)
		blockOfInsn[i] = #blocks
	end
	-- Successors.
	local labelToBlock = {}
	for bi, b in blocks do
		local firstIns = insns[b.first]
		-- a block may start with multiple stacked labels; map all leading labels
	end
	for name, idx in labelAt do
		labelToBlock[name] = blockOfInsn[idx]
	end
	for bi, b in blocks do
		b.succ = {}
		local lastIns = insns[b.last]
		local addFall = true
		if lastIns.targets then
			for _, t in lastIns.targets do
				local tb = labelToBlock[t]
				if tb then table.insert(b.succ, tb) end
			end
			addFall = lastIns.fallsThrough ~= false
		end
		if lastIns.fallsThrough == false then addFall = false end
		if addFall and bi + 1 <= #blocks then
			table.insert(b.succ, bi + 1)
		end
	end
	return blocks, labelToBlock, blockOfInsn
end
regalloc.buildCFG = buildCFG

-- Liveness: compute live-out set (of temps) for each instruction.
local function computeLiveness(insns, blocks)
	-- block-level def/use
	for _, b in blocks do
		local use = newSet()
		local def = newSet()
		for _, i in b.insns do
			local ins = insns[i]
			for _, u in (ins.uses or {}) do
				if not def[u] then use[u] = true end
			end
			for _, d in (ins.defs or {}) do
				def[d] = true
			end
		end
		b.useSet = use
		b.defSet = def
		b.liveIn = newSet()
		b.liveOut = newSet()
	end
	-- iterate to fixpoint (backward)
	local changed = true
	while changed do
		changed = false
		for bi = #blocks, 1, -1 do
			local b = blocks[bi]
			local newOut = newSet()
			for _, s in b.succ do
				for t in blocks[s].liveIn do newOut[t] = true end
			end
			-- liveIn = use ∪ (out − def)
			local newIn = newSet()
			for t in b.useSet do newIn[t] = true end
			for t in newOut do
				if not b.defSet[t] then newIn[t] = true end
			end
			-- detect change
			local function differ(a, c)
				for t in a do if not c[t] then return true end end
				for t in c do if not a[t] then return true end end
				return false
			end
			if differ(newIn, b.liveIn) or differ(newOut, b.liveOut) then
				changed = true
				b.liveIn = newIn
				b.liveOut = newOut
			end
		end
	end
	-- per-instruction live-out (backward within block)
	local liveOutOf = {}
	for _, b in blocks do
		local live = newSet()
		for t in b.liveOut do live[t] = true end
		for idx = #b.insns, 1, -1 do
			local i = b.insns[idx]
			local ins = insns[i]
			-- live-out of instruction i = current live
			local lo = newSet()
			for t in live do lo[t] = true end
			liveOutOf[i] = lo
			-- transfer: live = use ∪ (live − def)
			for _, d in (ins.defs or {}) do live[d] = nil end
			for _, u in (ins.uses or {}) do live[u] = true end
		end
	end
	return liveOutOf
end
regalloc.computeLiveness = computeLiveness

-- Per-instruction live-out sets for a generic insn list (defs/uses may be any variable ids,
-- e.g. spill-slot ids). Builds the CFG and runs the backward liveness fixpoint. Returns
-- liveOutOf[i] = set of variables live after instruction i.
function regalloc.liveOut(insns)
	return computeLiveness(insns, buildCFG(insns))
end

-- The core IRC coloring. Returns color map and spill list.
-- `temps` maps tempId -> descriptor. physical precolored temps have .precolor.
function regalloc.color(insns, temps, opts)
	opts = opts or {}
	-- Per-target register set (caller-supplied); K is the color count.
	local ALLOCATABLE = opts.allocatable or error("regalloc.color: opts.allocatable is required")
	-- Reserved colors (opts.reserved = array of register names): colors some OTHER
	-- precolored temp owns exclusively for the whole function — today the implicit
	-- counter web's fixed rcx (loop/loopl/jrcxz/jecxz). A reserved color is removed
	-- from the color pool (no virtual temp is ever ASSIGNED it) and coalescing into
	-- any precolored temp OF that color is blocked (no virtual temp can INHERIT it
	-- by aliasing an entry-unpack or argument-marshal move either — both would let
	-- the counter's `movq $n, %rcx` silently destroy the coalesced value, e.g. a
	-- pointer argument's capability lower that the fast CC delivers in rcx).
	-- Precolored temps themselves keep their fixed color: the counter web must live
	-- in rcx, and argument-marshal physical temps still carry values INTO rcx at
	-- calls (the transform saves/restores the counter around every call).
	local reservedSet = {}
	if opts.reserved then for _, r in opts.reserved do reservedSet[r] = true end end
	local pool = {}
	for _, r in ALLOCATABLE do
		if not reservedSet[r] then table.insert(pool, r) end
	end
	local K = #pool
	local blocks = buildCFG(insns)
	prof.mark("    [ra " .. #insns .. " insns] buildCFG")
	local liveOutOf = computeLiveness(insns, blocks)
	prof.mark("    [ra] computeLiveness")

	-- Interference graph.
	local adj = {}            -- tempId -> set of tempId
	local adjList = {}        -- tempId -> array
	local degree = {}
	local moveList = {}       -- tempId -> set of move-insn indices
	local worklistMoves = {}  -- set of move-insn indices
	local alias = {}          -- coalesced alias
	local nodes = {}          -- all temp ids seen

	local function ensureNode(t)
		if not nodes[t] then
			nodes[t] = true
			adj[t] = newSet()
			adjList[t] = {}
			degree[t] = 0
			moveList[t] = newSet()
		end
	end

	for t in temps do ensureNode(t) end
	for _, ins in insns do
		for _, d in (ins.defs or {}) do ensureNode(d) end
		for _, u in (ins.uses or {}) do ensureNode(u) end
	end

	local function precolored(t) return temps[t] and temps[t].precolor ~= nil end

	local function addEdge(u, v)
		if u == v or adj[u][v] then return end
		adj[u][v] = true
		adj[v][u] = true
		if not precolored(u) then
			table.insert(adjList[u], v)
			degree[u] = degree[u] + 1
		end
		if not precolored(v) then
			table.insert(adjList[v], u)
			degree[v] = degree[v] + 1
		end
	end

	-- Build moves + interference from liveness.
	for i, ins in insns do
		local live = newSet()
		for t in (liveOutOf[i] or {}) do live[t] = true end
		if ins.move then
			-- uses of a move don't interfere with its def — but ONLY for a full-width
			-- move (see buildRaInsns' wide computation): a narrow mov zero-extends, so
			-- its src and dst are genuinely distinct values when the src is live-out.
			if ins.move.wide ~= false then
				for _, u in (ins.uses or {}) do live[u] = nil end
			end
			setAdd(moveList[ins.move.dst], i)
			setAdd(moveList[ins.move.src], i)
			worklistMoves[i] = true
		end
		for _, d in (ins.defs or {}) do live[d] = true end
		for _, d in (ins.defs or {}) do
			for l in live do addEdge(l, d) end
		end
	end
	if prof.enabled then
		local ne, maxdeg, maxlive = 0, 0, 0
		for t in nodes do ne += #adjList[t]; if #adjList[t] > maxdeg then maxdeg = #adjList[t] end end
		for i, ins in insns do local c = 0; for _ in (liveOutOf[i] or {}) do c += 1 end; if c > maxlive then maxlive = c end end
		prof.mark("    [ra] interference edges=" .. ne .. " maxdeg=" .. maxdeg .. " maxlive=" .. maxlive)
	end

	-- Coloring worklists.
	local precoloredSet = newSet()
	local initial = {}
	for t in nodes do
		if precolored(t) then precoloredSet[t] = true
		else table.insert(initial, t) end
	end

	local selectStack = {}
	local onStack = newSet()
	local coalescedNodes = newSet()
	local coloredNodes = newSet()
	local spilledNodes = {}
	local frozenMoves = newSet()
	local activeMoves = newSet()
	local constrainedMoves = newSet()
	local coalescedMoves = newSet()

	local simplifyWorklist = {}
	local freezeWorklist = {}
	local spillWorklist = {}

	local nSimplify, nCoalesce, nFreeze, nSpill, nScan, nDec = 0, 0, 0, 0, 0, 0

	local function inArray(arr, x)
		for idx, v in arr do if v == x then return idx end end
		return nil
	end
	local function removeFrom(arr, x)
		local idx = inArray(arr, x)
		if idx then table.remove(arr, idx) end
	end

	local function nodeMoves(t)
		local out = {}
		for m in moveList[t] do
			if activeMoves[m] or worklistMoves[m] then out[m] = true end
		end
		return out
	end
	local function moveRelated(t)
		return next(nodeMoves(t)) ~= nil
	end

	-- adjacent = adjacent nodes not on select stack / coalesced
	local function adjacent(t)
		local out = {}
		for _, v in adjList[t] do
			if not onStack[v] and not coalescedNodes[v] then out[v] = true end
		end
		return out
	end

	-- build initial worklists
	for _, t in initial do
		if degree[t] >= K then
			table.insert(spillWorklist, t)
		elseif moveRelated(t) then
			table.insert(freezeWorklist, t)
		else
			table.insert(simplifyWorklist, t)
		end
	end

	local function enableMoves(nodesSet)
		for t in nodesSet do
			for m in nodeMoves(t) do
				if activeMoves[m] then
					activeMoves[m] = nil
					worklistMoves[m] = true
				end
			end
		end
	end

	local function decrementDegree(t)
		nDec += 1
		local d = degree[t]
		degree[t] = d - 1
		if d == K and not precolored(t) then
			local s = adjacent(t)
			s[t] = true
			enableMoves(s)
			removeFrom(spillWorklist, t)
			if moveRelated(t) then
				table.insert(freezeWorklist, t)
			else
				table.insert(simplifyWorklist, t)
			end
		end
	end

	local function simplify()
		nSimplify += 1
		local t = table.remove(simplifyWorklist)
		table.insert(selectStack, t)
		onStack[t] = true
		for v in adjacent(t) do
			decrementDegree(v)
		end
	end

	local function getAlias(t)
		while coalescedNodes[t] do t = alias[t] end
		return t
	end

	local function addWorklist(t)
		if not precolored(t) and not moveRelated(t) and degree[t] < K then
			removeFrom(freezeWorklist, t)
			table.insert(simplifyWorklist, t)
		end
	end

	local function ok(t, r)
		-- George criterion helper
		return degree[t] < K or precolored(t) or adj[t][r]
	end

	local function conservative(u, v)
		-- Briggs: count high-degree neighbors of the combined node
		local seen = newSet()
		local cnt = 0
		for w in adjacent(u) do
			if not seen[w] then seen[w] = true; if degree[w] >= K then cnt += 1 end end
		end
		for w in adjacent(v) do
			if not seen[w] then seen[w] = true; if degree[w] >= K then cnt += 1 end end
		end
		return cnt < K
	end

	local function combine(u, v)
		removeFrom(freezeWorklist, v)
		removeFrom(spillWorklist, v)
		coalescedNodes[v] = true
		alias[v] = u
		for m in moveList[v] do moveList[u][m] = true end
		for w in adjacent(v) do
			addEdge(w, u)
			decrementDegree(w)
		end
		if degree[u] >= K and inArray(freezeWorklist, u) then
			removeFrom(freezeWorklist, u)
			table.insert(spillWorklist, u)
		end
	end

	local function coalesce()
		nCoalesce += 1
		local m
		for mm in worklistMoves do m = mm; break end
		worklistMoves[m] = nil
		local mv = insns[m].move
		local x = getAlias(mv.dst)
		local y = getAlias(mv.src)
		local u, v
		if precolored(y) then u, v = y, x else u, v = x, y end
		if u == v then
			coalescedMoves[m] = true
			addWorklist(u)
		elseif precolored(v) or adj[u][v] then
			constrainedMoves[m] = true
			addWorklist(u); addWorklist(v)
		else
			local canCoalesce
			if precolored(u) then
				-- A reserved fixed color is never up for grabs: block the coalesce
				-- (the move stays a real move) so the virtual side cannot inherit it.
				canCoalesce = not reservedSet[temps[u].precolor]
				if canCoalesce then
					for w in adjacent(v) do
						if not ok(w, u) then canCoalesce = false break end
					end
				end
			else
				canCoalesce = conservative(u, v)
			end
			if canCoalesce then
				coalescedMoves[m] = true
				combine(u, v)
				addWorklist(u)
			else
				activeMoves[m] = true
			end
		end
	end

	local function freezeMoves(u)
		for m in nodeMoves(u) do
			activeMoves[m] = nil
			worklistMoves[m] = nil
			frozenMoves[m] = true
			local mv = insns[m].move
			local v = getAlias(mv.src)
			if getAlias(mv.dst) == getAlias(u) then v = getAlias(mv.dst) == u and getAlias(mv.src) or getAlias(mv.src) end
			local other = (getAlias(mv.dst) == getAlias(u)) and getAlias(mv.src) or getAlias(mv.dst)
			if not moveRelated(other) and degree[other] < K and not precolored(other) then
				removeFrom(freezeWorklist, other)
				table.insert(simplifyWorklist, other)
			end
		end
	end

	local function freeze()
		nFreeze += 1
		local u = table.remove(freezeWorklist)
		table.insert(simplifyWorklist, u)
		freezeMoves(u)
	end

	-- Spill-score denominators (max(spillCost, eps)) are loop-invariant: temps'
	-- spillCost is fixed by the caller before coloring and never mutated here.
	-- Cache them so the O(|spillWorklist|) selection scans skip two table lookups
	-- per entry (52M+ scanned entries on huge functions); the score arithmetic
	-- (degree[t] / denom[t], strict-greater scan in array order) is unchanged.
	local spillDen = {}
	local function selectSpill()
		nSpill += 1
		-- pick highest-degree (cheap heuristic) to spill optimistically
		local best, bestIdx, bestScore
		for idx, t in spillWorklist do
			nScan += 1
			local den = spillDen[t]
			if den == nil then
				den = math.max((temps[t] and temps[t].spillCost) or 1, 0.0001)
				spillDen[t] = den
			end
			local score = degree[t] / den
			if not bestScore or score > bestScore then
				bestScore = score; best = t; bestIdx = idx
			end
		end
		table.remove(spillWorklist, bestIdx)
		table.insert(simplifyWorklist, best)
		freezeMoves(best)
	end

	-- main loop
	while true do
		if #simplifyWorklist > 0 then simplify()
		elseif next(worklistMoves) then coalesce()
		elseif #freezeWorklist > 0 then freeze()
		elseif #spillWorklist > 0 then selectSpill()
		else break end
	end
	prof.mark("    [ra] IRC main loop simp=" .. nSimplify .. " coal=" .. nCoalesce .. " frz=" .. nFreeze .. " spill=" .. nSpill .. " scan=" .. nScan .. " dec=" .. nDec)

	-- assign colors
	local color = {}
	for t in precoloredSet do color[t] = temps[t].precolor end

	local actualSpills = {}
	while #selectStack > 0 do
		local t = table.remove(selectStack)
		onStack[t] = nil
		local okColors = {}
		for _, r in pool do okColors[r] = true end
		for _, w in adjList[t] do
			local aw = getAlias(w)
			if coloredNodes[aw] or precoloredSet[aw] then
				if color[aw] then okColors[color[aw]] = nil end
			end
		end
		local chosen
		for _, r in pool do
			if okColors[r] then chosen = r break end
		end
		if not chosen then
			table.insert(actualSpills, t)
		else
			coloredNodes[t] = true
			color[t] = chosen
		end
	end
	for t in coalescedNodes do
		color[t] = color[getAlias(t)]
	end
	prof.mark("    [ra] assign colors")

	-- Soundness backstop (runs on every function, and again after every spill-round
	-- re-run): verify the coloring before handing it to the emitter. If coloring ever
	-- produced an unsound assignment, the emitter would silently render a mis-colored
	-- function — an uncolored temp falls back to rendering at its original input
	-- register, which is exactly the "same web in two different registers at different
	-- points" failure mode — so refuse to emit instead. The checks below mirror the
	-- interference construction above: two webs that are ever simultaneously live must
	-- never share a register, and its cost is the same order as that construction
	-- (insns x live-out).
	--
	-- Intermediate spill rounds re-enter here with actualSpills non-empty: a spilled
	-- temp still occurs (rewriteSpills has not run yet) but has no color, so the
	-- uncolored-occurrence check skips temps that are spilled or coalesced into a
	-- spilled temp. On the final round actualSpills is empty, every coalesced temp's
	-- alias is colored, and the check is complete exactly when it matters.
	local spilledSet = newSet()
	for _, t in actualSpills do spilledSet[t] = true end

	local occurs = newSet()
	for _, ins in insns do
		for _, d in (ins.defs or {}) do occurs[d] = true end
		for _, u in (ins.uses or {}) do occurs[u] = true end
	end

	for t in occurs do
		if color[t] == nil
			and not precoloredSet[t]
			and not spilledSet[t]
			and not coalescedNodes[t] then
			-- The generic IRC instruction list carries no statement back-references
			-- (see the instruction contract above), so this rejection is unlocated.
			err.plain("register allocation left a value without a register (temp "
				.. t .. "); refusing to emit a mis-colored function")
		end
	end

	-- Every def's register must be free of every OTHER temp live across the insn, and
	-- the defs of a single insn must never share a register with each other. An
	-- uncolored temp (spilled, in an intermediate round) never compares equal to a real
	-- color, so it is skipped naturally; the explicit color ~= nil guards make that
	-- obvious.
	for i, ins in insns do
		local defs = ins.defs or {}
		local live = liveOutOf[i]
		if live and #defs > 0 then
			for _, d in defs do
				local cd = color[d]
				if cd ~= nil then
					for t in live do
						if t ~= d and color[t] == cd then
							-- A full-width move's source may share its destination's
							-- color: a same-color move is a no-op the emitter drops
							-- (see the move-exclusion in the interference build).
							if not (ins.move and ins.move.wide ~= false and t == ins.move.src) then
								err.plain("register allocation assigned the same register to two simultaneously-live values (temps "
									.. d .. " and " .. t .. " in " .. cd .. "); refusing to emit a mis-colored function")
							end
						end
					end
				end
			end
			for di = 1, #defs do
				local d = defs[di]
				local cd = color[d]
				if cd ~= nil then
					for dj = di + 1, #defs do
						local d2 = defs[dj]
						if d2 ~= d and color[d2] == cd then
							err.plain("register allocation assigned the same register to two simultaneously-live values (temps "
								.. d .. " and " .. d2 .. " in " .. cd .. "); refusing to emit a mis-colored function")
						end
					end
				end
			end
		end
	end

	prof.mark("    [ra] soundness backstop")
	return { color = color, spilled = actualSpills, blocks = blocks, aliasOf = getAlias }
end

return regalloc
