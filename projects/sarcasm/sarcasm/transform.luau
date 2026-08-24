--!nonstrict
-- The GIMSO transform (architecture-independent): rewrite a lifted yolo function into a
-- memory-safe Fil-C fast-entrypoint body (IR nodes), plus metadata for glue/prologue/
-- epilogue emission. The walk and every invisicap sequence (access checks, pointer loads/
-- stores with the FUGC store barrier, calls, pollchecks, allocations) live here ONCE; the
-- actual instructions are produced through a per-arch codegen `cg` (arm64_codegen /
-- x86_64_codegen), so the safety-critical logic is single-source across ISAs.
--
-- transformFunction(name, sigDesc, body, backend) where backend = { cg = <codegen module>,
-- frame = <frame module> }.

local lift = require("./lift")
local ptrflow = require("./ptrflow")
local sig = require("./sig")
local build = require("./build")
local framePass = require("./frame")

local transform = {}

-- `;! alloca ...` annotation parser (architecture-independent).
local function parseAlloca(ann)
	if not ann then return nil end
	local n = string.match(ann, "^alloca%s+size%s*%(%s*(.-)%s*%)$")
	if n then return { kind = "size", name = n } end
	local rn = string.match(ann, "^alloca%s+result%s*%(%s*(.-)%s*%)$")
	if rn then return { kind = "result", name = rn } end
	local sz = string.match(ann, "^alloca%s+result%s+size=(%w+)$")
	if sz then
		local v = tonumber(sz)
		if not v then error("sarcasm: bad alloca size literal: " .. sz) end
		return { kind = "resultSize", size = v }
	end
	if string.find(ann, "alloca", 1, true) then
		error("sarcasm: unrecognized alloca annotation: " .. ann)
	end
	return nil
end

-- The register operand of a store that is not the memory operand (the stored value).
local function storeValueOperand(stmt, mem)
	for _, op in stmt.operands do
		if op.kind == "reg" and op ~= mem then return op end
	end
	return nil
end

-- Directives that EMIT data (or define symbols/macros as values). sarcasm compiles
-- functions only: such content — whether inside a function body (literal pools, jump
-- tables) or between functions (data sections) — cannot be given a capability
-- automatically, so it is rejected rather than silently dropped. Shared with the
-- driver-level top-level scan in sarcasm.luau.
local DATA_DIRECTIVES = {}
for _, d in {
	".word", ".xword", ".quad", ".long", ".int", ".short", ".hword",
	".2byte", ".4byte", ".8byte", ".byte", ".ascii", ".asciz", ".string",
	".zero", ".space", ".fill", ".float", ".single", ".double", ".inst",
	".dc", ".dc.b", ".dc.l", ".dc.s", ".dc.w", ".dc.x", ".dcb", ".dcb.b",
	".dcb.l", ".dcb.s", ".dcb.w", ".dcb.x", ".ds", ".skip",
	".equ", ".set", ".macro", ".comm", ".common", ".lcomm",
} do DATA_DIRECTIVES[d] = true end
transform.DATA_DIRECTIVES = DATA_DIRECTIVES

-- Validate a function's RAW body before the frame pass runs (the frame pass rewrites
-- stack accesses, stripping annotations from the statements it virtualizes, so
-- annotation validation must see the original statements):
--  * unannotated `bl`/`call` and any `blr`/`br`/indirect call or jump are rejected
--    (raw calls are not marshalled and indirect targets are unprovable);
--  * `b`/cond-branch to a non-local target is a tail call — not yet supported;
--  * every `;!` annotation must be a recognized form (alloca family, load/store ptr
--    with a valid instruction shape, or a callsite signature on a call);
--  * a `;!` annotation on a mid-body label is rejected (only the function-entry label
--    carries the signature — any other label annotation would be silently dropped);
--  * data-emitting directives inside the body are rejected;
--  * arch-specific forged-address instructions (adrp/adr/ldr-literal) are rejected.
local function validateBody(body, cgm)
	local localLabels = {}
	for _, st in body do
		if st.kind == "label" then localLabels[st.name] = true end
	end
	for _, st in body do
		if st.kind == "blank" then
			-- an annotation all alone on its line is a typo (the annotation belongs on
			-- the instruction's own line) — silently dropping it could skip a barrier.
			if st.annotation then
				error("sarcasm: ;! annotation '" .. st.annotation
					.. "' on an otherwise-empty line (annotations go on the instruction's own line)")
			end
		elseif st.kind == "label" then
			-- The function-entry label (body[1]) carries the ;! signature. An annotation
			-- on any other label would be silently ignored (annotations are consumed from
			-- instructions only), so reject it like the blank-line case.
			if st.annotation and st ~= body[1] then
				error("sarcasm: ;! annotation '" .. st.annotation .. "' on a label ('" .. st.name
					.. "') inside a function body is not supported (only the function-entry label"
					.. " carries the ;! signature; instruction annotations go on the instruction's"
					.. " own line)")
			end
		elseif st.kind == "directive" then
			if DATA_DIRECTIVES[st.name] then
				error("sarcasm: data in a function body is not supported (literal pools,"
					.. " jump tables and constants cannot be made memory-safe automatically): directive '"
					.. st.name .. "'")
			end
			if st.annotation then
				error("sarcasm: ;! annotation '" .. st.annotation .. "' on a directive ('"
					.. st.name .. "') is not supported")
			end
		elseif st.kind == "insn" then
			local c = cgm.classify(st)
			local ann = st.annotation
			local raw = st.raw or st.mnemonic
			if c.control == "call" then
				if not ann then
					local callee = st.operands and st.operands[1] and st.operands[1].text or "?"
					error("sarcasm: call to '" .. callee .. "' has no ;! signature annotation"
						.. " (annotate the callsite, e.g. `" .. string.lower(st.mnemonic) .. " "
						.. callee .. " ;! int(ptr)`): " .. raw)
				end
			elseif c.control == "indcall" then
				error("sarcasm: indirect call through a raw register cannot be made memory-safe"
					.. " (indirect calls must go through function-pointer values): " .. raw)
			elseif c.control == "indirect" then
				error("sarcasm: indirect branch through a raw register cannot be made memory-safe"
					.. " (computed goto / switch jump tables are not supported): " .. raw)
			elseif (c.control == "branch" or c.control == "condbranch") and c.targets then
				for _, t in c.targets do
					if not localLabels[t] then
						error("sarcasm: branch to '" .. t .. "' leaves the function"
							.. " (a tail call); tail calls are not yet supported: " .. raw)
					end
				end
			end
			if ann then
				if parseAlloca(ann) then
					-- alloca family: validated/consumed by the transform below
				elseif ann == "load ptr" then
					cgm.validatePtrAccess(st, "load")
				elseif ann == "store ptr" then
					cgm.validatePtrAccess(st, "store")
				elseif c.control == "call" then
					-- callsite signature: parse now to reject malformed sigs early
					-- (the transform re-parses via callSigOf when marshalling).
					sig.parseSignature(ann)
				elseif string.find(ann, "%(") then
					error("sarcasm: signature annotation '" .. ann
						.. "' on a non-call instruction is not supported: " .. raw)
				elseif ann == "" then
					error("sarcasm: empty ;! annotation: " .. raw)
				else
					error("sarcasm: unrecognized ;! annotation '" .. ann
						.. "' (expected 'load ptr', 'store ptr', 'alloca ...', or a callsite"
						.. " signature like 'int(ptr)'): " .. raw)
				end
			end
			if cgm.checkForgedAddress then cgm.checkForgedAddress(st) end
		end
	end
end

function transform.transformFunction(name, sigDesc, body, backend)
	local cgm = backend.cg          -- codegen module (static config + make)
	local framePolicy = backend.frame  -- per-arch frame policy (analyzeFrame + rewrite)
	local cc = cgm.cc

	local returnRegs = sigDesc.isVoid and {} or {0}
	local funcLine = (body[1] and body[1].line) or 0
	local curLine = funcLine

	-- Fixed-size stack allocations establish sp-relative "regions" [base, base+size); gcc -O0
	-- re-derives the buffer address per use, so we redirect such sp math into the allocation.
	local allocaRegions = {}
	for _, st in body do
		if st.kind == "insn" then
			local al = parseAlloca(st.annotation)
			if al and al.kind == "resultSize" then
				local base = cgm.allocaRegionBase(st)
				if base then table.insert(allocaRegions, { base = base, size = al.size, stmt = st }) end
			end
		end
	end
	local function regionOf(spOff)
		for _, r in allocaRegions do
			if spOff >= r.base and spOff < r.base + r.size then return r end
		end
		return nil
	end

	-- Reject unannotated/indirect calls, tail calls, unknown/misplaced annotations,
	-- in-body data, and forged addresses BEFORE any rewriting (see validateBody).
	validateBody(body, cgm)

	body = framePass.preprocess(body, allocaRegions, framePolicy)

	-- per-call signature info
	local callSigCache = {}
	local function callSigOf(stmt)
		local ann = stmt.annotation
		if not ann then return nil end
		if callSigCache[stmt] == nil then
			local cs = sig.parseSignature(ann)
			-- Per-arch callsite arity validation (the argument marshal has a finite
			-- number of CC registers). Raises a clean sarcasm: error when over limit.
			if cgm.checkCallsiteSig then
				local callee = stmt.operands and stmt.operands[1] and stmt.operands[1].text or "?"
				cgm.checkCallsiteSig(cs, callee)
			end
			callSigCache[stmt] = cs
		end
		return callSigCache[stmt]
	end
	local function callInfo(stmt)
		local cs = callSigOf(stmt)
		if not cs then return nil end
		local argRegs = {}
		for k = 1, cs.argWords do table.insert(argRegs, cc.yoloArg(k)) end
		return { argRegs = argRegs, resultReg = cs.isVoid and nil or 0 }
	end

	local lf = lift.liftFunction(body, { classify = cgm.classify, returnRegs = returnRegs, callInfo = callInfo })

	local temps = lf.temps
	local nextTemp = lf.nextTemp
	local function allocTemp(desc)
		nextTemp += 1
		temps[nextTemp] = desc or {}
		return nextTemp
	end

	-- Precolored physical-register temps: ONE node per machine register (as IRC requires).
	local pregTemps = {}
	local function pColored(num)
		if not pregTemps[num] then
			pregTemps[num] = allocTemp({ precolor = cgm.precolorName(num), phys = num })
		end
		return pregTemps[num]
	end

	-- Seeds for pointer flow: pointer args occupy their yolo argument registers.
	local argPtrTemps = {}
	local argInfo = {}
	for k, ty in sigDesc.argTypes do
		local yreg = cc.yoloArg(k)
		local et = lf.entryTemp[yreg]
		argInfo[k] = { isPtr = (ty == 7), yoloReg = yreg, entryTemp = et }
		if ty == 7 and et then table.insert(argPtrTemps, et) end
	end

	-- classify nodes: load-ptr / store-ptr / call / alloca.
	local loadPtrDefs, storePtrNodes, callNodes, allocaNodes, allocaResultDefs = {}, {}, {}, {}, {}
	local callPtrDefs = {}
	local sizeNames = {}
	for _, node in lf.nodes do
		if node.kind == "insn" then
			local ann = node.stmt.annotation
			local al = parseAlloca(ann)
			if al then
				allocaNodes[node] = al
				if al.kind == "size" then
					if sizeNames[al.name] then error("sarcasm: duplicate `alloca size (" .. al.name .. ")`") end
					sizeNames[al.name] = true
				else
					allocaResultDefs[node] = true
				end
			elseif node.c.control == "call" and ann then
				callNodes[node] = ann
				local cs = callSigOf(node.stmt)
				if cs and cs.retIsPtr then callPtrDefs[node] = true end
			elseif ann == "load ptr" then loadPtrDefs[node] = true
			elseif ann == "store ptr" then storePtrNodes[node] = true end
		end
	end

	local pf = ptrflow.analyze(lf,
		{ argPtrTemps = argPtrTemps, loadPtrDefs = loadPtrDefs, allocaResultDefs = allocaResultDefs,
			callPtrDefs = callPtrDefs },
		allocTemp, { baseMnemonic = cgm.baseMnemonic })

	-- Link each fixed-size alloca-result node to its region; give the region a stable pointer
	-- temp for redirecting sp-relative math and a shared lower (the result's seeded lower).
	for node, al in allocaNodes do
		if al.kind == "resultSize" then
			for _, r in allocaRegions do
				if r.stmt == node.stmt then al.region = r; break end
			end
			local r = al.region
			if r and not r.ptrTemp then
				r.ptrTemp = allocTemp({ role = "regionptr" })
				local resultIv = node.defTemps and node.defTemps[1]
				r.loTemp = resultIv and pf.lowerTemp[resultIv]
				pf.isPtr[r.ptrTemp] = true
				if r.loTemp then pf.lowerTemp[r.ptrTemp] = r.loTemp end
			end
		end
	end

	-- loop headers (targets of back edges) get a pollcheck
	local loopHeaderLabels = {}
	for _, node in lf.nodes do
		if node.kind == "insn" and node.c.targets and #node.c.targets > 0 then
			for _, t in node.c.targets do
				local ti = lf.labelToFlow[t]
				if ti and ti <= node.fi then loopHeaderLabels[t] = true end
			end
		end
	end

	local out = {}
	local function emit(n) table.insert(out, n) end
	local vMyth = allocTemp({ role = "myth" })
	temps[vMyth].spillCost = 1e9
	local originSym = ".Lfilc_origin_" .. name

	emit({ kind = "prologue", name = name })

	local cg = cgm.make({ emit = emit, allocTemp = allocTemp, pColored = pColored, pf = pf, temps = temps })

	-- Condition-flag (NZCV/RFLAGS) liveness at instrumentation points. The instrumented
	-- sequences (access checks, invisicap loads/stores, pollchecks, allocations) write
	-- the condition flags (cmp/tst/test); gcc -O2 routinely leaves flags live across a
	-- memory instruction (`cmp; ldr; csel`), so an inserted flag write would corrupt
	-- the program's pending condition. Scan forward from the insertion point: if the
	-- next flag-relevant instruction is a USE (csel/b.cond/cmov/jcc/...) the flags are
	-- live and the sequence must be bracketed by a save/restore; if it is a DEF
	-- (cmp/subs/add) the incoming flags are dead; calls clobber the flags per the ABI
	-- (no valid code reads them across a call); at a basic-block end (label, branch,
	-- return, or the end of the function) conservatively assume live-out — a flag
	-- consumer in a successor block (e.g. the b.cond terminating THIS block) counts.
	local nodeIndex = {}
	for i, nd in lf.nodes do nodeIndex[nd] = i end
	local flagLiveCache = {}
	local function flagsLiveFrom(i)
		if flagLiveCache[i] ~= nil then return flagLiveCache[i] end
		local res = true   -- conservative default (end of function = live-out)
		local j = i
		while j <= #lf.nodes do
			local nd = lf.nodes[j]
			if nd.kind == "insn" then
				local ctrl = nd.c and nd.c.control or "none"
				if cgm.flagUse(nd.stmt) then res = true; break
				elseif cgm.flagDef(nd.stmt) then res = false; break
				elseif ctrl == "call" or ctrl == "indcall" then res = false; break
				elseif ctrl ~= "none" then res = true; break
				end
			elseif nd.kind == "label" then
				res = true; break
			end
			j += 1
		end
		flagLiveCache[i] = res
		return res
	end
	-- Run f() emitting an instrumented sequence, bracketed by a flag save/restore iff
	-- the program's flags are live at the sequence's insertion point.
	local function withFlagSave(live, f)
		if not live then f(); return end
		local t = cg.saveFlags()
		f()
		cg.restoreFlags(t)
	end

	-- GC root slots
	local rootTemps, loToSlot = {}, {}
	local function rootSlotFor(loT)
		if loToSlot[loT] == nil then
			table.insert(rootTemps, loT)
			loToSlot[loT] = #rootTemps - 1
			if temps[loT] then temps[loT].spillCost = 1e9 end
		end
		return loToSlot[loT]
	end
	local function emitRootStore(loT) emit({ kind = "rootstore", temp = loT, index = rootSlotFor(loT), useTemps = { loT } }) end

	-- entry CC unpacking: myth <- myth reg; params from CC iv/lo regs into their yolo webs.
	cg.move(vMyth, pColored(cc.myth), true)
	for k, ty in sigDesc.argTypes do
		local info = argInfo[k]
		if info.entryTemp then
			cg.move(info.entryTemp, pColored(cc.argIv(sigDesc.argTypes, k)), true)
			if info.isPtr then
				local loT = pf.lowerTemp[info.entryTemp]
				cg.move(loT, pColored(cc.argLo(sigDesc.argTypes, k)), true)
				emitRootStore(loT)
			end
		end
	end

	-- access-fail stubs (bookkeeping here; the stub body is arch-specific via cg.failStub).
	local accessOrigins, failStubs, failCounter = {}, {}, 0
	local function failLabelFor(ivTemp, loTemp, size, align, isWrite)
		failCounter += 1
		local aco = ".Lfilc_aco_" .. name .. "_" .. failCounter
		local lbl = ".Lfail_" .. name .. "_" .. failCounter
		table.insert(accessOrigins, { sym = aco, size = size, align = align, write = isWrite and true or false, line = curLine })
		cg.failStub(failStubs, lbl, ivTemp, loTemp, aco)
		return lbl
	end

	-- access check for a precomputed effective address (capLo==nil => null cap, always traps).
	local function emitAccessCheck(effTemp, capLo, size, align, isWrite)
		if capLo == nil then
			cg.jump(failLabelFor(effTemp, nil, size, align, isWrite))
			return
		end
		local lbl = failLabelFor(effTemp, capLo, size, align, isWrite)
		cg.branchZero(capLo, lbl)                 -- null capability
		cg.cmpBranch(effTemp, capLo, "ult", lbl)  -- eff < lower
		local upper = allocTemp({ role = "scratch" })
		cg.load(upper, capLo, -16, 8)             -- upper = [lower-16]
		if size > 1 then
			local hi = allocTemp({ role = "scratch" })
			cg.addImm(hi, effTemp, size)          -- hi = eff + size
			cg.cmpBranch(hi, upper, "ugt", lbl)   -- eff+size > upper
		else
			cg.cmpBranch(effTemp, upper, "uge", lbl)  -- eff >= upper
		end
		if align > 1 then cg.testMaskBranch(effTemp, align - 1, "ne", lbl) end
	end

	-- invisicap pointer load: load (dstIv, dstLo) from the address of `mem`.
	local ptrldCounter = 0
	local function emitPtrLoad(node, mem, dstIv, dstLo)
		ptrldCounter += 1
		local tag = name .. "_" .. ptrldCounter
		local effTemp, baseLo = cg.computeAddr(node, mem)
		emitAccessCheck(effTemp, baseLo, 8, 8)
		if baseLo == nil then cg.moveZero(dstLo); cg.moveZero(dstIv); return end
		local offInObj = allocTemp({ role = "offobj" })
		cg.subReg(offInObj, effTemp, baseLo)                       -- offInObj = eff - lower
		local aux = allocTemp({ role = "aux" })
		cg.load(aux, baseLo, -8, 8); cg.maskLow48(aux)             -- aux = [lower-8] & 0xffffffffffff
		local nullL, haveL, direct = ".Lnulllo_" .. tag, ".Lhavelo_" .. tag, ".Ldirlo_" .. tag
		cg.branchZero(aux, nullL)
		local auxE = allocTemp({ role = "auxentry" })
		cg.loadIndexed(auxE, aux, offInObj, 1, 8)                  -- auxE = [aux + offInObj]
		cg.testMaskBranch(auxE, 1, "eq", direct)                   -- bit0 clear -> direct
		local boxp = allocTemp({ role = "box" })
		cg.move(boxp, auxE); cg.andImmInPlace(boxp, -2); cg.load(dstLo, boxp, 8, 8)  -- box: lower=[(auxE&~1)+8]
		cg.jump(haveL)
		cg.label(direct); cg.move(dstLo, auxE, true); cg.jump(haveL)
		cg.label(nullL); cg.moveZero(dstLo)
		cg.label(haveL); cg.load(dstIv, effTemp, 0, 8)             -- dstIv = [eff]
	end

	-- invisicap pointer store (aux allocation + readonly reject + FUGC store barrier).
	local ptrstCounter = 0
	local function emitPtrStore(node, mem, valIv, valLo)
		ptrstCounter += 1
		local tag = name .. "_st_" .. ptrstCounter
		local effTemp, baseLo = cg.computeAddr(node, mem)
		if baseLo == nil then emitAccessCheck(effTemp, nil, 8, 8, true); return end
		if valLo == nil then valLo = allocTemp({ role = "nulllo" }); cg.moveZero(valLo) end
		local lbl = failLabelFor(effTemp, baseLo, 8, 8, true)
		cg.branchZero(baseLo, lbl)
		cg.testMaskBranch(effTemp, 7, "ne", lbl)                   -- alignment
		cg.cmpBranch(effTemp, baseLo, "ult", lbl)                  -- lower bound
		local auxword = allocTemp({ role = "auxword" })
		cg.load(auxword, baseLo, -8, 8)
		cg.testMaskBranch(auxword, "0x6000000000000", "ne", lbl)   -- read-only/special reject
		local upper = allocTemp({ role = "scratch" })
		cg.load(upper, baseLo, -16, 8)
		local hi = allocTemp({ role = "scratch" })
		cg.addImm(hi, effTemp, 8); cg.cmpBranch(hi, upper, "ugt", lbl)  -- upper bound
		local offInObj = allocTemp({ role = "offobj" })
		cg.subReg(offInObj, effTemp, baseLo)
		local aux = allocTemp({ role = "aux" })
		cg.move(aux, auxword); cg.maskLow48(aux)
		local needAux, barrier, doBarrier, storeLbl, doneLbl =
			".Lneedaux_" .. tag, ".Lbarrier_" .. tag, ".Ldobar_" .. tag, ".Lstore_" .. tag, ".Ldone_" .. tag
		cg.branchZero(aux, needAux)
		cg.label(barrier)
		cg.markingStateNonzeroBranch(doBarrier)
		cg.label(storeLbl)
		cg.storeIndexed(valLo, aux, offInObj, 1)
		cg.store(valIv, effTemp, 0)
		cg.jump(doneLbl)
		-- need-aux: filc_object_ensure_aux_ptr_outline(myth, lower-16) -> aux
		cg.label(needAux)
		local header = allocTemp({ role = "scratch" })
		cg.subImm(header, baseLo, 16)
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), header)
		cg.call("filc_object_ensure_aux_ptr_outline", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.move(aux, pColored(cc.rtRet))
		cg.jump(barrier)
		-- do-barrier: filc_store_barrier_for_lower_slow(myth, lower)
		cg.label(doBarrier)
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.move(pColored(cc.rtArg(2)), valLo)
		cg.call("filc_store_barrier_for_lower_slow", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.jump(storeLbl)
		cg.label(doneLbl)
	end

	-- Pre/post-index writeback for annotated pointer accesses. The invisicap sequence
	-- replaces the load/store wholesale, so the base register's `base += index` side
	-- effect must be re-applied explicitly (e.g. `ldr x0, [x0], #8 ;! load ptr` walking
	-- an array). Only the intval temp advances; the lower (capability) is unchanged —
	-- it is the same object. The effective address used for the access is already
	-- correct (old base for post-index, base+index for pre-index via computeAddr), so
	-- a single add/sub after the sequence covers both forms.
	local function emitWriteback(node, mem)
		if not mem or not mem.writeback then return end
		local baseT = node.opTemp[mem.base]
		if not baseT then
			error("sarcasm: writeback base of an annotated pointer access has no usable value: "
				.. (node.stmt and node.stmt.raw or "?"))
		end
		local amt = (mem.index and mem.index.kind == "imm")
			and (mem.index.value or tonumber(mem.index.text)) or nil
		if not amt then
			error("sarcasm: annotated pointer access with a non-immediate writeback index"
				.. " is not supported: " .. (node.stmt and node.stmt.raw or "?"))
		end
		if amt > 0 then cg.addImm(baseT, baseT, amt)
		elseif amt < 0 then cg.subImm(baseT, baseT, -amt) end
	end

	-- pollcheck: myth flags at [myth+8], bits 0xe.
	local pollCounter = 0
	local function emitPollcheck()
		pollCounter += 1
		local okLbl = ".Lpollok_" .. name .. "_" .. pollCounter
		local flags = allocTemp({ role = "scratch" })
		cg.load(flags, vMyth, 8, 1)
		cg.testMaskBranch(flags, 0xe, "eq", okLbl, 32)
		cg.move(pColored(cc.rtArg(1)), vMyth)
		cg.leaGlobal(cc.rtArg(2), originSym)
		cg.call("filc_pollcheck_slow", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		cg.label(okLbl)
	end

	local excLabel = nil
	local function ensureExcLabel() if not excLabel then excLabel = ".Lexc_" .. name end return excLabel end
	local calledExterns = {}

	-- move the web's capability (its lower, or null) into a physical register.
	local function emitCapMove(destReg, web)
		local pDest = pColored(destReg)
		local loT = web and pf.lowerTemp[web]
		if loT then cg.move(pDest, loT) else cg.moveZero(pDest) end
		return pDest
	end

	local function emitCall(node)
		local cs = callSigOf(node.stmt)
		local callee = node.stmt.operands[1].text
		calledExterns[callee] = { name = callee, sig = cs.signature, sigDesc = cs,
			sigName = "pizlonatedFI" .. cs.signature .. "_" .. callee }
		local argTemps = {}
		for _, r in node.c.uses do if r.argReg then argTemps[r.num] = node.opTemp[r] end end
		cg.move(pColored(cc.myth), vMyth)
		local callUses = { pColored(cc.myth) }
		for k = 1, cs.argWords do
			local aTemp = argTemps[cc.yoloArg(k)]
			cg.move(pColored(cc.argIv(cs.argTypes, k)), aTemp)
			table.insert(callUses, pColored(cc.argIv(cs.argTypes, k)))
			if cs.argTypes[k] == 7 then table.insert(callUses, emitCapMove(cc.argLo(cs.argTypes, k), aTemp)) end
		end
		cg.call("pizlonatedFI" .. cs.signature .. "_" .. callee, callUses, cg.callerSavedClobbers())
		cg.exceptionBranch(ensureExcLabel())
		if not cs.isVoid then
			cg.move(node.defTemps[1], pColored(cc.retIv))
			if cs.retIsPtr then
				-- a pointer call result is (iv, lo); define its lower from retLo and root it
				-- before any later safepoint can run.
				local loT = pf.lowerTemp[node.defTemps[1]]
				if loT then
					cg.move(loT, pColored(cc.retLo))
					emitRootStore(loT)
				end
			end
		end
	end

	-- Stack allocations. `;! alloca size (x)` captures the byte size; `;! alloca result (x)` /
	-- `;! alloca result size=N` becomes filc_allocate(myth, size); payload = allocation+16.
	local allocaSizes = {}
	local function emitAlloca(node, al)
		if al.kind == "size" then
			local sizeOp = cgm.allocaSizeOperand(node.stmt)
			local sizeTemp = allocTemp({ role = "allocasize" })
			if sizeOp and sizeOp.kind == "reg" then cg.move(sizeTemp, node.opTemp[sizeOp], true)
			elseif sizeOp and sizeOp.kind == "imm" then cg.moveImm(sizeTemp, sizeOp.value or tonumber(sizeOp.text))
			else error("sarcasm: `alloca size` instruction has no size operand: " .. node.stmt.raw) end
			allocaSizes[al.name] = sizeTemp
			return
		end
		cg.move(pColored(cc.rtArg(1)), vMyth)
		if al.kind == "result" then
			local sizeTemp = allocaSizes[al.name]
			if not sizeTemp then
				error("sarcasm: `alloca result (" .. al.name .. ")` has no preceding `alloca size (" .. al.name .. ")`")
			end
			cg.move(pColored(cc.rtArg(2)), sizeTemp)
		else
			cg.moveImm(pColored(cc.rtArg(2)), al.size)
		end
		cg.call("filc_allocate", { pColored(cc.rtArg(1)), pColored(cc.rtArg(2)) }, cg.callerSavedClobbers())
		local resultIv = node.defTemps and node.defTemps[1]
		if not resultIv then error("sarcasm: `alloca result` instruction has no destination register: " .. node.stmt.raw) end
		local ret = pColored(cc.rtRet)  -- allocation base; payload = base + 16
		if al.region then
			local r = al.region
			cg.effAddr(resultIv, ret, 16)
			if r.loTemp then cg.effAddr(r.loTemp, ret, 16) end
			cg.effAddr(r.ptrTemp, ret, 16)
			if r.loTemp then emitRootStore(r.loTemp) end
		else
			local resultLo = pf.lowerTemp[resultIv]
			cg.effAddr(resultIv, ret, 16)
			cg.effAddr(resultLo, ret, 16)
			emitRootStore(resultLo)
		end
	end

	-- Redirect an sp-relative address computation that lands inside a fixed alloca region to
	-- the allocation pointer (gcc -O0). Arch-specific; returns true if it replaced the node.
	local function emitRegionRedirect(node)
		if #allocaRegions == 0 or not cg.regionRedirect then return false end
		return cg.regionRedirect(node, regionOf)
	end

	-- Walk the lifted body.
	for _, node in lf.nodes do
		if node.kind == "label" then
			if node.name ~= name and not string.match(node.name, "^%.LF[BE]") then
				emit(node)
				if loopHeaderLabels[node.name] then
					withFlagSave(flagsLiveFrom(nodeIndex[node] + 1), emitPollcheck)
				end
			end
		elseif node.kind == "other" then
			-- drop directives
		elseif node.kind == "insn" then
			local c = node.c
			curLine = (node.stmt and node.stmt.line) or curLine
			if allocaNodes[node] then
				withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
					emitAlloca(node, allocaNodes[node])
				end)
			elseif c.control == "return" then
				if not sigDesc.isVoid then
					local retTemp
					for _, r in c.uses do if r.retReg then retTemp = node.opTemp[r] end end
					if retTemp then cg.move(pColored(cc.retIv), retTemp) end
					if sigDesc.retIsPtr then emitCapMove(cc.retLo, retTemp) end
				end
				cg.clearRetFlag()
				emit({ kind = "epilogue", name = name, useTemps = { vMyth } })
				emit(build.insn("ret", {}, { control = "return", fallsThrough = false }))
			elseif c.control == "call" and callNodes[node] then
				emitCall(node)
			elseif emitRegionRedirect(node) then
				-- sp-relative address computation redirected into an alloca region
			else
				local mem = cgm.memOperand(node.stmt)
				if cg.lowerSpecial(node, mem) then
					-- arch-specific passthrough lowering (e.g. x86 cltq -> movslq, lea passthrough)
				elseif loadPtrDefs[node] then
					local dstIv = node.defTemps[1]
					local dstLo = pf.lowerTemp[dstIv]
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						emitPtrLoad(node, mem, dstIv, dstLo)
						emitWriteback(node, mem)
					end)
					emitRootStore(dstLo)
				elseif storePtrNodes[node] then
					local valOp = storeValueOperand(node.stmt, mem)
					local valIv = valOp and node.opTemp[valOp] or nil
					local valLo = valIv and pf.lowerTemp[valIv] or nil
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						emitPtrStore(node, mem, valIv, valLo)
						emitWriteback(node, mem)
					end)
				elseif mem then
					cg.checkMemInsn(node, mem)
					local effTemp, capLo = cg.computeAddr(node, mem)
					local size, align = cgm.accessSizeAlign(node.stmt)
					local isWrite = cgm.isStoreInsn(node.stmt)
					withFlagSave(flagsLiveFrom(nodeIndex[node]), function()
						emitAccessCheck(effTemp, capLo, size, align, isWrite)
					end)
					emit(node)
				else
					emit(node)
				end
			end
		else
			emit(node)
		end
	end

	if excLabel then
		emit(build.label(excLabel))
		emit({ kind = "epilogue", name = name, useTemps = { vMyth } })
		emit(build.insn("ret", {}, { control = "return", fallsThrough = false }))
	end
	for _, s in failStubs do emit(s) end

	local externsList = {}
	for _, e in calledExterns do table.insert(externsList, e) end

	return {
		name = name, sigDesc = sigDesc, fipNodes = out, temps = temps, vMyth = vMyth,
		originSym = originSym, numRoots = #rootTemps, funcLine = funcLine, nextTemp = nextTemp,
		rootTemps = rootTemps, accessOrigins = accessOrigins, calledExterns = externsList,
		lf = lf, pf = pf,
	}
end

return transform
