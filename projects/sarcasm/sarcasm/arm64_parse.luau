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
--   { kind = "reg",  name = "x0", rc = "gpr"|"neon"|"sve"|"sp"|"zr", width = 32|64|..., shift = <shift?> }
--   { kind = "imm",  text = "5381", value = <number?> }
--   { kind = "sym",  text = "foo+16", reloc = <string?> }   -- reloc e.g. "lo12","got","got_lo12"
--   { kind = "mem",  base = <reg>, index = <operand?>, writeback = "pre"|"post"|nil,
--                    shift = <shift?>, text = <original> }
--   { kind = "reglist", regs = {<neon reg>...}, lane = <number?>, text = <original> }
--
-- NEON register operands carry their arrangement/element qualification:
--   v0.16b / v3.2d -> { kind="reg", rc="neon", arrangement = { lanes = 16|nil, elem = "b" } }
--   v0.d[1]        -> { kind="reg", rc="neon", element = { elem = "d", index = 1 } }
--   b0/h0/s0/d0/q0/v0 (bare) -> rc="neon", width in BITS (8/16/32/64/128)
-- SVE registers (z0-31, p0-15, bare or .b/.h/.s/.d-qualified, predicates with /z or /m)
-- parse as rc="sve" so the ISA layer can reject them cleanly.
-- A shift is { op = "lsl"|"lsr"|"asr"|"ror"|"uxtb"|... , amount = <number?>, amountText = <string?> }

local numlabel = require("./numlabel")
local err = require("./err")

local parse = {}

local SHIFT_OPS = {
	lsl = true, lsr = true, asr = true, ror = true, msl = true,
	uxtb = true, uxth = true, uxtw = true, uxtx = true,
	sxtb = true, sxth = true, sxtw = true, sxtx = true,
}

-- Registers ---------------------------------------------------------------

-- Classify a bare register token. Returns a reg operand table, or nil.
-- Pseudoregister number registry (see classifyReg).

local PSEUDO_BASE = 2000
local pseudoNums = {}
local numPseudos = 0

local function classifyReg(tok: string)
	local low = string.lower(tok)
	-- Pseudoregisters: `fil_<ident>`, where <ident> matches [0-9a-zA-Z_]+ and may
	-- start with a digit. The <ident> is case-sensitive (`fil_FOO` and `fil_foo`
	-- are distinct pseudos); only the `fil_` prefix itself matches
	-- case-insensitively, like other register names. A pseudo is a 64-bit GPR with
	-- no fixed physical register;
	-- the register allocator colors it like any other virtual web (spilling across
	-- calls as needed). Each distinct name maps to one register number in a
	-- dedicated band (never colliding with physical GPRs 0-30 or the frame-slot
	-- band); the mapping is allocated in source order, so identical input always
	-- produces identical numbering. NOTE: match on the original `tok` (not the
	-- lowered `low`) so the captured ident keeps its case. NOTE: a bare `fil_<ident>`
	-- in operand position is ALWAYS a pseudo, never a symbol — do not name globals, functions, or
	-- local labels with a `fil_` prefix (a branch/call to one would misparse).
	local ident = string.match(tok, "^[Ff][Ii][Ll]_([0-9a-zA-Z_]+)$")
	if ident then
		-- Canonicalize the prefix to lowercase so `FIL_Foo` and `fil_Foo` share
		-- one register; the ident keeps its case, so `FOO` vs `foo` stay distinct.
		local canon = "fil_" .. ident
		local n = pseudoNums[canon]
		if not n then
			numPseudos += 1
			n = PSEUDO_BASE + numPseudos
			pseudoNums[canon] = n
		end
		return { kind = "reg", name = canon, rc = "gpr", width = 64, num = n, pseudo = ident }
	end
	if string.match(low, "^fil_") or low == "fil" then
		err.here("bad pseudoregister name `" .. tok .. "` (expected `fil_<ident>` with <ident> matching [0-9a-zA-Z_]+)")
	end
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
	-- NEON lane-qualified vector register: v0.16b, v3.4s, v12.2d, v0.1q.
	local vn, lanes, elem = string.match(low, "^v(%d+)%.(%d*)([bhsdq])$")
	if vn and tonumber(vn) <= 31 then
		return { kind = "reg", name = low, rc = "neon", width = 128, num = tonumber(vn),
			arrangement = { lanes = (lanes ~= "" and tonumber(lanes) or nil), elem = elem } }
	end
	-- NEON single-element reference: v0.b[3], v0.d[1], v4.h[0].
	local en, eelem, eidx = string.match(low, "^v(%d+)%.([bhsdq])%[(%d+)%]$")
	if en and tonumber(en) <= 31 then
		return { kind = "reg", name = low, rc = "neon", width = 128, num = tonumber(en),
			element = { elem = eelem, index = tonumber(eidx) } }
	end
	-- SVE scalable vector / predicate registers (parsed so the ISA layer can reject
	-- them cleanly): z0-z31 bare or .b/.h/.s/.d-qualified, p0-p15 bare, qualified,
	-- or with a /z or /m predication suffix.
	local zn = string.match(low, "^z(%d+)$") or string.match(low, "^z(%d+)%.[bhsdq]$")
	if zn and tonumber(zn) <= 31 then
		return { kind = "reg", name = low, rc = "sve", width = 128, num = tonumber(zn) }
	end
	local pn = string.match(low, "^p(%d+)$") or string.match(low, "^p(%d+)%.[bhsdq]$")
		or string.match(low, "^p(%d+)/[zm]$") or string.match(low, "^p(%d+)%.[bhsdq]/[zm]$")
	if pn and tonumber(pn) <= 15 then
		return { kind = "reg", name = low, rc = "sve", width = 32, num = tonumber(pn) }
	end
	local prefix, num = string.match(low, "^([a-z]+)(%d+)$")
	if prefix and num then
		local n = tonumber(num)
		if prefix == "x" and n <= 30 then
			return { kind = "reg", name = low, rc = "gpr", width = 64, num = n }
		elseif prefix == "w" and n <= 30 then
			return { kind = "reg", name = low, rc = "gpr", width = 32, num = n }
		elseif (prefix == "v" or prefix == "q" or prefix == "d" or prefix == "s" or prefix == "h" or prefix == "b") and n <= 31 then
			local bits = (prefix == "b") and 8 or (prefix == "h") and 16
				or (prefix == "s") and 32 or (prefix == "d") and 64 or 128
			return { kind = "reg", name = low, rc = "neon", width = bits, num = n }
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
-- [], () and {} (NEON structure lists) nesting.
local function splitTopLevel(s: string): { string }
	local out = {}
	local depth = 0
	local cur = {}
	local i = 1
	local n = #s
	while i <= n do
		local c = string.sub(s, i, i)
		if c == "[" or c == "(" or c == "{" then
			depth += 1
			table.insert(cur, c)
		elseif c == "]" or c == ")" or c == "}" then
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

-- Parse a NEON/SVE structure-list operand field: `{v0.16b, v1.16b}` (full
-- structures), `{v0.b}[3]` / `{v0.h, v1.h}[1]` (single-element with a lane
-- index). Returns a reglist operand, or nil if malformed.
local function parseRegList(field: string)
	local inner, rest = string.match(field, "^(%b{})%s*(.-)%s*$")
	if not inner then return nil end
	local body = string.sub(inner, 2, -2)
	local regs = {}
	for part in string.gmatch(body .. ",", "(.-),") do
		local r = classifyReg((string.gsub(part, "^%s*(.-)%s*$", "%1")))
		if not r or (r.rc ~= "neon" and r.rc ~= "sve") then return nil end
		table.insert(regs, r)
	end
	if #regs == 0 then return nil end
	local lane = string.match(rest, "^%[(%d+)%]$")
	if rest ~= "" and not lane then return nil end
	return { kind = "reglist", regs = regs, lane = lane and tonumber(lane) or nil, text = field }
end

-- Parse a single (non-memory, non-shift) operand field.
local function parseSimple(field: string)
	-- register?
	local reg = classifyReg(field)
	if reg then
		return reg
	end
	-- NEON/SVE structure list ({v0.16b, v1.16b} / {v0.b}[3]): the only legal
	-- operand starting with '{' in A64 assembly, so a malformed one is an error,
	-- not a symbol (silently treating it as a symbol would hide the access width).
	if string.sub(field, 1, 1) == "{" then
		local rl = parseRegList(field)
		if not rl then
			err.here("malformed register list operand '" .. field .. "'")
		end
		return rl
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

-- Remove a trailing // comment that is not inside a string literal. A `\`
-- inside a string escapes the following character (so `"a \" // x"` keeps its
-- slashes), mirroring stripComments below; `\` has no quoting power outside
-- strings.
local function stripLineComment(s: string): string
	local inStr = false
	local i = 1
	local n = #s
	while i <= n do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr
		elseif inStr and c == "\\" then
			-- escaped character inside the string (e.g. the `"` of `\"`): skip it
			-- whole so it cannot close the string and expose a `//` that is still
			-- inside the string
			i += 1
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
-- here -- on arm64 it introduces an immediate, and `#!` is not an annotation marker
-- either, so `#!` text stays in the code part and produces a normal parse error. The
-- one exception to `//` starting a comment: `//!` is an annotation marker ("a comment
-- that means more", the arm64 sibling of the universal `;!`), so it is emitted
-- verbatim for splitAnnotation to find while the rest of the line keeps being scanned
-- (a later `//` on the same line is still a comment). Block comments still take
-- precedence, so `/* //! load ptr */` is a comment, not an annotation. A comment is
-- replaced by a single space (comments act as token separators in GAS); newlines
-- inside block comments are kept so statement line numbers still match the source.
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
				if string.sub(text, i + 2, i + 2) == "!" then
					-- `//!` is an annotation marker, not a comment: keep it
					-- verbatim so splitAnnotation can find it
					table.insert(out, c)
					table.insert(out, d)
					table.insert(out, "!")
					i += 2
				else
					state = "line"
					table.insert(out, " ")
					i += 1
				end
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
				if e ~= "" then
					table.insert(out, e)
					i += 1
					if e == "\n" then
						-- a backslash-newline continues the string on the next line,
						-- but the per-line scanners below (splitAnnotation, parseLine)
						-- reset at the line boundary, so resync to code here too, like
						-- the unterminated-string case below; carrying string state
						-- across the newline would let the continuation line swallow a
						-- `//!` marker or reach the output with an unbalanced quote
						state = "code"
					end
				end
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

local function trim(s: string): string
	return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

-- Split off an annotation marker (returns code, annotation-or-nil). Markers are
-- `;!` (supported on all architectures) and `//!` (`//` is the comment introducer
-- on arm64, so `//!` reads as "a comment that means more"). The EARLIEST marker
-- outside a string literal wins; the body is the trimmed text after the marker,
-- taken verbatim (stripComments already removed `//` and `/* */` comments). Text
-- inside a double-quoted string never produces an annotation, so `.asciz "a ;! b"`
-- and `.asciz "a //! b"` are ordinary directives, and a `\` inside a string
-- escapes the following character (so `"a \" ;! b"` keeps its `;!`), exactly as
-- in stripComments above; `\` has no quoting power outside strings. `#!` is not
-- a marker here: it stays in the code part and produces a normal parse error.
local function splitAnnotation(s: string): (string, string?)
	local inStr = false
	local i, n = 1, #s
	while i <= n do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr
		elseif inStr and c == "\\" then
			-- escaped character inside the string (e.g. the `"` of `\"`): skip it
			-- whole so it can neither close the string nor be mistaken for a
			-- marker; mirrors stripComments above
			i += 1
		elseif not inStr then
			local d = string.sub(s, i + 1, i + 1)
			if c == ";" and d == "!" then
				return string.sub(s, 1, i - 1), trim(string.sub(s, i + 2))
			elseif c == "/" and d == "/" and string.sub(s, i + 2, i + 2) == "!" then
				return string.sub(s, 1, i - 1), trim(string.sub(s, i + 3))
			end
		end
		i += 1
	end
	return s, nil
end

-- Cut a code-position `#` comment off the front of a line. On arm64 GAS treats
-- a `#` at the START of the code portion as a comment (`# note`, and cpp
-- linemarkers like `# 1 "foo.s"`), while a `#` anywhere else in the code
-- introduces an immediate (`mov x0, #1`) -- so stripComments above must not
-- treat `#` as a comment introducer, and this scan only cuts at the first `#`
-- that sits where a comment could. That is the case exactly when everything
-- before it consists solely of label definitions and whitespace (the same
-- labels `parseLine` below peels, with that peel matching against trimmed
-- text so that an indented label counts as code position too); otherwise the
-- `#` is an immediate introducer and the line is returned unchanged. A later
-- `#` can then never
-- start a comment either, since its prefix only grows. Scanning is
-- string-aware, mirroring stripLineComment above: a `"` toggles a string and a
-- `\` inside a string escapes the next character, so `.asciz "a # b"` keeps
-- its `#`. GAS aarch64 rejects `@` even at code position ("junk at end of
-- line"), so there is no `@` case here. Comments never fabricate annotations:
-- the comment text (including any `//!`/`;!` marker text) is dropped with the
-- `#`, and the labels before it survive. The one exception: `#!` is diagnosed
-- with a targeted error instead of being dropped, because silently losing a
-- would-be annotation would make annotated code assemble as if the annotation
-- was never written.
local function cutCodeHash(s: string): string
	local inStr = false
	local i, n = 1, #s
	while i <= n do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr
		elseif inStr and c == "\\" then
			-- escaped character inside the string (e.g. the `"` of `\"`): skip it
			-- whole so it cannot close the string and expose a `#` that is still
			-- inside the string; mirrors stripLineComment above
			i += 1
		elseif c == "#" then
			local prefix = string.sub(s, 1, i - 1)
			local rest = trim(prefix)
			while true do
				local name, after = string.match(rest, "^([%w_.$][%w_.$@]*):%s*(.*)$")
				if not name then
					break
				end
				rest = trim(after)
			end
			if rest ~= "" then
				-- `#` in the middle of code: it introduces an immediate
				return s
			end
			if string.sub(s, i + 1, i + 1) == "!" then
				err.here("'#!' is not an annotation marker on arm64 (use '//!' or ';!'): " .. s)
			end
			-- comment: keep only the labels before it
			return prefix
		end
		i += 1
	end
	return s
end

-- `.alloca size, alignment, result`: stack allocation without touching sp.
-- Builds an ordinary insn statement (mnemonic "alloca", operands in WRITTEN order:
-- [size, alignment, result]). size/alignment: an immediate (`#N` or bare `N`), a
-- GPR (including a `fil_` pseudo), or a frame-relative spill-slot reference
-- (`[sp, #off]` / `[x29, #off]`); result: a GPR, a pseudo, or a spill slot.
-- A bare symbol is never a valid operand (rejected here, not as a global access).
local function parseDotAlloca(rest, raw)
	-- Split the three operands on top-level commas FIRST (a `[...]` field followed
	-- by a comma is a separate `.alloca` operand, never a post-index address —
	-- parseOperands would otherwise consume `[sp, #8], [sp, #16]` as one
	-- post-indexed memory operand), then parse each field alone.
	local fields = {}
	do
		local depth, cur = 0, {}
		for k = 1, #rest do
			local c = string.sub(rest, k, k)
			if c == "[" then depth += 1; table.insert(cur, c)
			elseif c == "]" then depth -= 1; table.insert(cur, c)
			elseif c == "," and depth == 0 then
				table.insert(fields, (string.gsub(table.concat(cur), "^%s*(.-)%s*$", "%1"))); cur = {}
			else table.insert(cur, c) end
		end
		table.insert(fields, (string.gsub(table.concat(cur), "^%s*(.-)%s*$", "%1")))
	end
	if #fields ~= 3 or fields[1] == "" or fields[2] == "" or fields[3] == "" then
		err.here("`.alloca` takes exactly 3 operands (size, alignment, result): " .. raw)
	end
	local operands = {}
	for k, f in fields do
		local ops = parseOperands(f)
		if #ops ~= 1 then
			err.here("`.alloca` operand did not parse as one operand: `" .. f .. "` (" .. raw .. ")")
		end
		operands[k] = ops[1]
	end
	for _, op in operands do
		if op.kind == "sym" then
			err.here("`.alloca` operand is a symbol, but size/alignment must be a register, spill slot, or immediate"
				.. " and result a register, pseudo-register, or spill slot: " .. raw)
		end
	end
	return {
		kind = "insn",
		mnemonic = "alloca",
		operands = operands,
		raw = raw,
		dotAlloca = true,
	}
end

-- Parse one source line into zero or more statements, appending to `out`.
local function parseLine(rawLine: string, out)
	-- A `#` at the start of the code portion is a comment on arm64 (GAS `# note`,
	-- cpp linemarkers `# 1 "foo.s"`); anywhere else in the code it introduces an
	-- immediate. Cut the comment off before splitAnnotation runs so that comment
	-- text can never fabricate an annotation (e.g. `# use //! load ptr here`
	-- must stay inert); see cutCodeHash above.
	rawLine = cutCodeHash(rawLine)
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
		-- The line starts with something that can neither begin a mnemonic nor a
		-- directive (`#`-initial comments are cut by cutCodeHash above, so this is
		-- junk like a stray `"` or `@`). Fabricating a statement for it would
		-- silently drop the line downstream; raise a normal parse error instead.
		err.here("cannot parse line: " .. trimmed)
	end
	rest = trim(rest)

	if string.sub(mnemonic, 1, 1) == "." then
		if string.lower(mnemonic) == ".alloca" then
			local ainsn = parseDotAlloca(rest, trimmed)
			ainsn.annotation = annotation
			table.insert(out, ainsn)
			return
		else
			table.insert(out, {
				kind = "directive",
				name = mnemonic,
				args = rest,
				raw = trimmed,
				annotation = annotation,
			})
			return
		end
	end

	-- Instruction. Split optional condition suffix off branch mnemonics like b.hs.
	-- A `"` can never appear in a legitimate A64 operand (comments and string
	-- literals are already stripped above), so one here is junk -- for example a
	-- line left malformed by a backslash-newline inside a string literal.
	-- Rejecting it here keeps such junk out of the rendered output, where an
	-- unbalanced quote would swallow the rest of the file in the final GNU-as
	-- pass.
	if string.find(rest, '"', 1, true) then
		err.here("cannot parse line: " .. trimmed)
	end
	local operands = parseOperands(rest)
	table.insert(out, {
		kind = "insn",
		mnemonic = mnemonic,
		operands = operands,
		annotation = annotation,
		raw = trimmed,
	})
end

function parse.parseString(text: string, fileName: string?)
	-- Per-file pseudo-registry reset. pseudoNums/numPseudos live at module scope
	-- (the require cache persists them across files in one process), so
	-- reinitialize them here for deterministic per-file numbering from
	-- PSEUDO_BASE+1 in source order — the same pattern as numlabel's call-local
	-- state. Parsing is synchronous (no yields), so no interleaved parse can
	-- observe a half-built table.
	pseudoNums = {}
	numPseudos = 0
	local out = {}
	-- normalize CRLF
	text = (string.gsub(text, "\r\n", "\n"))
	-- strip C/C++ comments (string-literal aware) before line splitting; the `//!`
	-- annotation marker survives this pass so splitAnnotation can find it
	text = stripComments(text)
	-- publish the input file and current line so err.here (used by the deep
	-- parse helpers, which only receive strings) can locate rejections
	err.beginParse(fileName)
	local lineNo = 0
	for line in (text .. "\n"):gmatch("(.-)\n") do
		lineNo += 1
		err.atParseLine(lineNo)
		local before = #out
		parseLine(line, out)
		-- stamp every statement produced from this source line with its line
		-- number and the input file
		for k = before + 1, #out do
			out[k].line = lineNo
			out[k].file = fileName
		end
	end
	numlabel.resolve(out)
	err.endParse()
	return out
end

return parse
