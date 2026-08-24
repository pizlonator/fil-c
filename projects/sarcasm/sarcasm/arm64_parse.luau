--!nonstrict
-- sarcasm assembly parser for ARM64 GNU-as / clang assembly.
--
-- Produces a flat list of "statements". Each statement is one of:
--   { kind = "blank",     text = <original> }
--   { kind = "label",     name = <string>, annotation = <string?> }
--   { kind = "directive", name = ".text", args = <string>, raw = <string>, annotation = <string?> }
--   { kind = "insn",      mnemonic = "ldr", operands = { <operand>... }, cond = <string?>,
--                         annotation = <string?>, raw = <string> }
--
-- Operands are structured:
--   { kind = "reg",  name = "x0", rc = "gpr"|"neon"|"sp"|"zr", width = 32|64|..., shift = <shift?> }
--   { kind = "imm",  text = "5381", value = <number?> }
--   { kind = "sym",  text = "foo+16", reloc = <string?> }   -- reloc e.g. "lo12","got","got_lo12"
--   { kind = "mem",  base = <reg>, index = <operand?>, writeback = "pre"|"post"|nil,
--                    shift = <shift?>, text = <original> }
-- A shift is { op = "lsl"|"lsr"|"asr"|"ror"|"uxtb"|... , amount = <number?>, amountText = <string?> }

local numlabel = require("./numlabel")

local parse = {}

local SHIFT_OPS = {
	lsl = true, lsr = true, asr = true, ror = true, msl = true,
	uxtb = true, uxth = true, uxtw = true, uxtx = true,
	sxtb = true, sxth = true, sxtw = true, sxtx = true,
}

-- Registers ---------------------------------------------------------------

-- Classify a bare register token. Returns a reg operand table, or nil.
local function classifyReg(tok: string)
	local low = string.lower(tok)
	if low == "sp" then
		return { kind = "reg", name = "sp", rc = "sp", width = 64 }
	end
	if low == "wsp" then
		return { kind = "reg", name = "wsp", rc = "sp", width = 32 }
	end
	if low == "xzr" then
		return { kind = "reg", name = "xzr", rc = "zr", width = 64 }
	end
	if low == "wzr" then
		return { kind = "reg", name = "wzr", rc = "zr", width = 32 }
	end
	-- gcc-style aliases: fp = x29 (frame pointer), lr = x30 (link register).
	if low == "fp" then
		return { kind = "reg", name = "x29", rc = "gpr", width = 64, num = 29 }
	end
	if low == "lr" then
		return { kind = "reg", name = "x30", rc = "gpr", width = 64, num = 30 }
	end
	local prefix, num = string.match(low, "^([a-z]+)(%d+)$")
	if prefix and num then
		local n = tonumber(num)
		if prefix == "x" and n <= 30 then
			return { kind = "reg", name = low, rc = "gpr", width = 64, num = n }
		elseif prefix == "w" and n <= 30 then
			return { kind = "reg", name = low, rc = "gpr", width = 32, num = n }
		elseif (prefix == "v" or prefix == "q" or prefix == "d" or prefix == "s" or prefix == "h" or prefix == "b") and n <= 31 then
			return { kind = "reg", name = low, rc = "neon", width = 128, num = n }
		end
	end
	return nil
end
parse.classifyReg = classifyReg

-- Given a gpr num and desired width, return the canonical name.
function parse.gprName(num: number, width: number): string
	if width == 32 then
		return "w" .. tostring(num)
	end
	return "x" .. tostring(num)
end

-- Operand tokenization ----------------------------------------------------

-- Split an operand string into top-level comma-separated fields, respecting
-- [] and () nesting.
local function splitTopLevel(s: string): { string }
	local out = {}
	local depth = 0
	local cur = {}
	local i = 1
	local n = #s
	while i <= n do
		local c = string.sub(s, i, i)
		if c == "[" or c == "(" then
			depth += 1
			table.insert(cur, c)
		elseif c == "]" or c == ")" then
			depth -= 1
			table.insert(cur, c)
		elseif c == "," and depth == 0 then
			table.insert(out, table.concat(cur))
			cur = {}
		else
			table.insert(cur, c)
		end
		i += 1
	end
	table.insert(out, table.concat(cur))
	-- trim
	for idx, field in out do
		out[idx] = (string.gsub(field, "^%s*(.-)%s*$", "%1"))
	end
	return out
end
parse.splitTopLevel = splitTopLevel

-- Parse a shift/extend field like "lsl 5", "lsl #5", "uxtb", "sxtw".
local function parseShift(field: string)
	local op, rest = string.match(field, "^(%a+)%s*(.*)$")
	if not op or not SHIFT_OPS[string.lower(op)] then
		return nil
	end
	op = string.lower(op)
	rest = (string.gsub(rest, "^%s*(.-)%s*$", "%1"))
	local amount, amountText
	if rest ~= "" then
		amountText = rest
		local numtxt = (string.gsub(rest, "^#", ""))
		amount = tonumber(numtxt)
	end
	return { op = op, amount = amount, amountText = amountText }
end

-- Parse a single (non-memory, non-shift) operand field.
local function parseSimple(field: string)
	-- register?
	local reg = classifyReg(field)
	if reg then
		return reg
	end
	-- immediate: #num, num, 0x..., -num
	local imm = string.match(field, "^#(.+)$")
	if imm then
		return { kind = "imm", text = imm, value = tonumber(imm) }
	end
	-- numeric local label reference: `1f` / `1b` (next/previous `1:` definition)
	local nref, ndir = string.match(field, "^(%d+)([fb])$")
	if nref then
		return { kind = "numref", text = field, num = nref, dir = ndir }
	end
	if string.match(field, "^-?%d") or string.match(field, "^0[xX]") then
		return { kind = "imm", text = field, value = tonumber(field) }
	end
	-- reloc-decorated symbol: :lo12:sym, :got:sym, etc.
	local reloc, sym = string.match(field, "^:(%w+):(.+)$")
	if reloc then
		return { kind = "sym", text = sym, reloc = reloc }
	end
	-- plain symbol / label / expression
	return { kind = "sym", text = field }
end

-- Parse a memory operand starting with '['.
local function parseMem(field: string, postIndex: string?)
	local writeback = nil
	local inner = field
	if string.sub(inner, -1) == "!" then
		writeback = "pre"
		inner = string.sub(inner, 1, -2)
	end
	-- strip [ ]
	inner = string.match(inner, "^%[(.*)%]$") or inner
	inner = (string.gsub(inner, "^%s*(.-)%s*$", "%1"))
	local parts = splitTopLevel(inner)
	local base = parseSimple(parts[1])
	local mem = { kind = "mem", base = base, text = field, writeback = writeback }
	if #parts >= 2 then
		-- second part is index/offset; third (if any) is a shift/extend on the index
		local idx = parts[2]
		local sh = parseShift(idx)
		if sh then
			mem.shift = sh
		else
			mem.index = parseSimple(idx)
		end
		if #parts >= 3 then
			mem.shift = parseShift(parts[3])
		end
	end
	if postIndex then
		mem.writeback = "post"
		local sh = parseShift(postIndex)
		if sh then
			mem.postShift = sh
		else
			mem.index = parseSimple(postIndex)
		end
	end
	return mem
end

-- Parse the full operand list of an instruction.
local function parseOperands(s: string)
	s = (string.gsub(s, "^%s*(.-)%s*$", "%1"))
	if s == "" then
		return {}
	end
	local fields = splitTopLevel(s)
	local operands = {}
	local i = 1
	while i <= #fields do
		local field = fields[i]
		if field == "" then
			i += 1
		elseif string.sub(field, 1, 1) == "[" then
			-- memory operand; check for post-index (a following field that is
			-- not a shift and this bracket has no '!').
			if string.sub(field, -1) ~= "!" and i + 1 <= #fields and not parseShift(fields[i + 1]) then
				table.insert(operands, parseMem(field, fields[i + 1]))
				i += 2
			else
				table.insert(operands, parseMem(field, nil))
				i += 1
			end
		else
			local sh = parseShift(field)
			if sh and #operands > 0 then
				-- attach to previous operand
				operands[#operands].shift = sh
			else
				table.insert(operands, parseSimple(field))
			end
			i += 1
		end
	end
	return operands
end
parse.parseOperands = parseOperands

-- Line splitting ----------------------------------------------------------

-- Remove a trailing // comment that is not inside a string literal.
local function stripLineComment(s: string): string
	local inStr = false
	local i = 1
	local n = #s
	while i <= n do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr
		elseif not inStr and c == "/" and string.sub(s, i + 1, i + 1) == "/" then
			return string.sub(s, 1, i - 1)
		end
		i += 1
	end
	return s
end

-- Remove comments from the whole text before line splitting: `/* ... */` (inline and
-- multi-line) and `//` to end of line, neither of which may appear inside a
-- double-quoted string (`.asciz "a // b"` keeps its slashes). `#` is NOT a comment
-- here -- on arm64 it introduces an immediate. A comment is replaced by a single space
-- (comments act as token separators in GAS); newlines inside block comments are kept so
-- statement line numbers still match the source.
local function stripComments(text: string): string
	local out = {}
	local i, n = 1, #text
	local state = "code" -- code | str | line | block
	while i <= n do
		local c = string.sub(text, i, i)
		local d = string.sub(text, i + 1, i + 1)
		if state == "code" then
			if c == '"' then
				state = "str"
				table.insert(out, c)
			elseif c == "/" and d == "/" then
				state = "line"
				table.insert(out, " ")
				i += 1
			elseif c == "/" and d == "*" then
				state = "block"
				table.insert(out, " ")
				i += 1
			else
				table.insert(out, c)
			end
		elseif state == "str" then
			table.insert(out, c)
			if c == "\\" then
				local e = string.sub(text, i + 1, i + 1)
				if e ~= "" then table.insert(out, e); i += 1 end
			elseif c == '"' then
				state = "code"
			elseif c == "\n" then
				state = "code" -- unterminated string; resync at the line boundary
			end
		elseif state == "line" then
			if c == "\n" then
				state = "code"
				table.insert(out, c)
			end
		else -- block
			if c == "*" and d == "/" then
				state = "code"
				i += 1
			elseif c == "\n" then
				table.insert(out, c)
			end
		end
		i += 1
	end
	return table.concat(out)
end
parse.stripComments = stripComments

-- Split off a ;! annotation (returns code, annotation-or-nil).
local function splitAnnotation(s: string): (string, string?)
	local idx = string.find(s, ";!", 1, true)
	if idx then
		local code = string.sub(s, 1, idx - 1)
		local ann = string.sub(s, idx + 2)
		ann = (string.gsub(ann, "^%s*(.-)%s*$", "%1"))
		return code, ann
	end
	return s, nil
end

local function trim(s: string): string
	return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

-- Parse one source line into zero or more statements, appending to `out`.
local function parseLine(rawLine: string, out)
	local code, annotation = splitAnnotation(rawLine)
	code = stripLineComment(code)
	local trimmed = trim(code)

	if trimmed == "" then
		if annotation then
			-- annotation on an otherwise-blank line: attach to a blank marker
			table.insert(out, { kind = "blank", text = rawLine, annotation = annotation })
		else
			table.insert(out, { kind = "blank", text = rawLine })
		end
		return
	end

	-- Peel labels off the front: NAME:
	while true do
		local name, rest = string.match(trimmed, "^([%w_.$][%w_.$@]*):%s*(.*)$")
		if not name then
			break
		end
		table.insert(out, { kind = "label", name = name })
		trimmed = trim(rest)
		if trimmed == "" then
			break
		end
	end

	-- Attach annotation to the last produced statement for this line (if any),
	-- but if there's remaining code, it will be attached below instead.
	if trimmed == "" then
		if annotation and #out > 0 then
			out[#out].annotation = annotation
		end
		return
	end

	-- Directive or instruction.
	local mnemonic, rest = string.match(trimmed, "^([%.%w_][%w_.]*)%s*(.*)$")
	if not mnemonic then
		table.insert(out, { kind = "directive", name = trimmed, args = "", raw = trimmed, annotation = annotation })
		return
	end
	rest = trim(rest)

	if string.sub(mnemonic, 1, 1) == "." then
		table.insert(out, {
			kind = "directive",
			name = mnemonic,
			args = rest,
			raw = trimmed,
			annotation = annotation,
		})
		return
	end

	-- Instruction. Split optional condition suffix off branch mnemonics like b.hs.
	local operands = parseOperands(rest)
	table.insert(out, {
		kind = "insn",
		mnemonic = mnemonic,
		operands = operands,
		annotation = annotation,
		raw = trimmed,
	})
end

function parse.parseString(text: string)
	local out = {}
	-- normalize CRLF
	text = (string.gsub(text, "\r\n", "\n"))
	-- strip C/C++ comments (string-literal aware) before line splitting
	text = stripComments(text)
	local lineNo = 0
	for line in (text .. "\n"):gmatch("(.-)\n") do
		lineNo += 1
		local before = #out
		parseLine(line, out)
		-- stamp every statement produced from this source line with its line number
		for k = before + 1, #out do out[k].line = lineNo end
	end
	numlabel.resolve(out)
	return out
end

return parse
