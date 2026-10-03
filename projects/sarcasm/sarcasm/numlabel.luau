--!nonstrict

-- GNU-as-style numeric local labels. A label `1:` (any non-negative integer) may be
-- defined any number of times; an operand `1f` refers to the NEXT `1:` definition and
-- `1b` to the most recent PREVIOUS one. These are ubiquitous in handwritten assembly.
--
-- The per-arch operand parsers produce
--   { kind = "numref", text = "1f", num = "1", dir = "f"|"b" }
-- for such references (a memory operand's displacement symbol may also be one, e.g.
-- `1f(%rip)`), and the line parsers already peel `1:` off as an ordinary label whose
-- name is all digits.
--
-- This pass renames every numeric label definition to a unique private symbol and
-- rewrites each numref to a plain symbol reference, so the rest of the pipeline only
-- ever sees ordinary named labels. It runs at the end of parseString and is a no-op
-- for inputs without numeric labels. A reference with no matching definition in the
-- requested direction is a clean compile-time error.

local err = require("./err")

local numlabel = {}

local function isNumericName(name)
	return string.match(name, "^%d+$") ~= nil
end

-- Find the definition for a reference at statement index i (defs are in order).
local function resolveRef(defs, i, num, dir)
	if dir == "f" then
		for _, d in defs do
			if d.pos > i and d.num == num then return d end
		end
	else
		for k = #defs, 1, -1 do
			local d = defs[k]
			if d.pos < i and d.num == num then return d end
		end
	end
	return nil
end

-- Matches and rewrites a numeric-label reference in operand `op` (either a numref
-- operand or a memory operand whose displacement symbol is one). `st` is the
-- statement containing the reference (used to locate the error). Returns nothing;
-- errors on an unresolvable reference.
local function rewriteOperand(defs, i, op, st)
	local refText, refNum, refDir
	if op.kind == "numref" then
		refText, refNum, refDir = op.text, op.num, op.dir
	elseif op.kind == "mem" and op.dispSym then
		local n, d = string.match(op.dispSym, "^(%d+)([fb])$")
		if n then refText, refNum, refDir = op.dispSym, n, d end
	end
	if not refText then return end
	local d = resolveRef(defs, i, refNum, refDir)
	if not d then
		err.at(st, "unresolved numeric label reference '" .. refText .. "'")
	end
	if op.kind == "numref" then
		op.kind = "sym"
		op.text = d.uniq
		op.num = nil
		op.dir = nil
	else
		op.dispSym = d.uniq
	end
end

function numlabel.resolve(out)
	-- cheap gate: nothing to do unless a numeric label or reference exists
	local hasNum = false
	for _, st in out do
		if st.kind == "label" and isNumericName(st.name) then hasNum = true; break end
		if st.kind == "insn" then
			for _, op in (st.operands or {}) do
				if op.kind == "numref" then hasNum = true; break end
				if op.kind == "mem" and op.dispSym and string.match(op.dispSym, "^%d+[fb]$") then
					hasNum = true; break
				end
			end
			if hasNum then break end
		end
	end
	if not hasNum then return end

	-- rename every numeric label definition to a unique private symbol
	local defs = {}
	local counter = 0
	for i, st in out do
		if st.kind == "label" and isNumericName(st.name) then
			counter += 1
			local uniq = ".Lsarcasm_num" .. tostring(counter)
			table.insert(defs, { pos = i, num = st.name, uniq = uniq })
			st.name = uniq
		end
	end

	-- rewrite every reference
	for i, st in out do
		if st.kind == "insn" then
			for _, op in (st.operands or {}) do
				rewriteOperand(defs, i, op, st)
			end
		end
	end
end

return numlabel
