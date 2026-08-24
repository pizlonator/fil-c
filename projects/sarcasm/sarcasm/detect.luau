--!nonstrict
-- Auto-detect the target architecture (arm64 vs x86_64) and, for x86_64, the assembly
-- syntax (att vs intel) from the input assembly text.

local detect = {}

-- Blank out comment text and string-literal contents before marker matching, so that
-- x86 markers mentioned in a comment or an .asciz payload of an arm64 file (and vice
-- versa) cannot spoof the architecture. Handled: `//` to end of line, `/* ... */`
-- (multi-line), `#` to end of line (the comment character in x86 GAS and in
-- preprocessor/full-line comments of clang arm64 output; stripping a mid-line arm64
-- `#immediate` is harmless -- it can only suppress x86 markers, never fabricate one),
-- and double-quoted strings (with backslash escapes). Newlines are preserved so the
-- result lines up with the input.
local function sanitize(text: string): string
	local out = {}
	local i, n = 1, #text
	local state = "code" -- code | str | line | block
	while i <= n do
		local c = string.sub(text, i, i)
		local d = string.sub(text, i + 1, i + 1)
		if state == "code" then
			if c == '"' then
				state = "str"
			elseif c == "/" and d == "/" then
				state = "line"
				i += 1
			elseif c == "/" and d == "*" then
				state = "block"
				i += 1
			elseif c == "#" then
				state = "line"
			else
				table.insert(out, c)
			end
		elseif state == "str" then
			if c == "\\" then
				i += 1 -- skip the escaped character too (e.g. \" does not end the string)
			elseif c == '"' then
				state = "code"
			elseif c == "\n" then
				state = "code" -- asm strings cannot span lines; resync
				table.insert(out, c)
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
detect.sanitize = sanitize

-- Returns arch ("arm64"|"x86_64"), syntax ("att"|"intel"|nil).
function detect.detect(text)
	text = sanitize(text)
	-- Strong, unambiguous markers first.
	if string.find(text, "%.intel_syntax") then return "x86_64", "intel" end
	if string.find(text, "%.att_syntax") then return "x86_64", "att" end
	if string.find(text, "%.arch%s+armv8") then return "arm64", nil end

	-- AT&T uses % register prefixes; x86 also has endbr64 / RIP-relative.
	if string.find(text, "%%r[a-z0-9]") or string.find(text, "%%e[a-z][a-z]")
		or string.find(text, "%%rip") or string.find(text, "endbr64", 1, true) then
		return "x86_64", "att"
	end

	-- Intel without a .intel_syntax directive: bare 64-bit x86 registers or PTR/[] memory.
	if string.find(text, " PTR ") or string.find(text, "%[") then
		-- brackets alone aren't proof (ARM uses [x0]); require an x86 register token too.
		if string.find(text, "[ \t,%[]r[abcds][xip][ \t,%]%+%-*]")
			or string.find(text, "[ \t,%[]r%d%d?[ \t,%]%+%-*]")
			or string.find(text, " PTR ") then
			return "x86_64", "intel"
		end
	end
	if string.find(text, "[ \t,]r[abcds][xip][ \t,\n]") or string.find(text, "[ \t,]e[abcds][xip][ \t,\n]") then
		return "x86_64", "intel"
	end

	-- Default: ARM64 (x0..x30 / w0.. / sp / xzr).
	return "arm64", nil
end

return detect
