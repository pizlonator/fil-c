--!nonstrict
-- x86_64 assembly parser (both AT&T and Intel syntax) producing the common operand
-- model used by the rest of sarcasm. Registers are canonicalized to an encoding number
-- (rax=0..r15=15) plus a width in bits.
--
-- Operand model (shared with the ARM64 parser):
--   { kind="reg",  num, width, name, rc="gpr"|"sp"|"rip", indirect=<bool?> }
--   { kind="imm",  value, text }
--   { kind="sym",  text, reloc=<@PLT/@GOTPCREL/nil> }
--   { kind="mem",  base=<reg?>, index=<reg?>, scale=<1|2|4|8>, dispVal=<num?>,
--                  dispSym=<str?>, size=<bytes?>, indirect=<bool?> }

local numlabel = require("./numlabel")
local err = require("./err")

local x86parse = {}

-- name -> { num, width } ---------------------------------------------------
local REG = {}
do
	local q = { "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
		"r8","r9","r10","r11","r12","r13","r14","r15" }
	local d = { "eax","ecx","edx","ebx","esp","ebp","esi","edi",
		"r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d" }
	local w = { "ax","cx","dx","bx","sp","bp","si","di",
		"r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w" }
	local b = { "al","cl","dl","bl","spl","bpl","sil","dil",
		"r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b" }
	for i = 1, 16 do
		REG[q[i]] = { num = i - 1, width = 64 }
		REG[d[i]] = { num = i - 1, width = 32 }
		REG[w[i]] = { num = i - 1, width = 16 }
		REG[b[i]] = { num = i - 1, width = 8 }
	end
	-- The high-byte aliases ah/ch/dh/bh (the bytes 8-15 of rax/rcx/rdx/rbx — a
	-- separate x86 encoding from the low-byte al/cl/dl/bl). They parse to their
	-- register's web with the name preserved, so x86_64_isa.classify can model
	-- them exactly (classifyHighByte pins the enclosing web to the spelled
	-- physical register) instead of their masquerading as bare symbols and dying
	-- with the misleading "memory access with a symbolic address" error.
	-- parseOnly keeps them out of the canonical-name fill (NAMES), so
	-- regName/canonical behavior stays byte-identical and the canonical names
	-- for (width=8, num) are the low-byte al/cl/dl/bl — these aliases can never
	-- become canonical (the renderer names a pinned high-byte operand via its
	-- own highByte tag instead, see emitPinned).
	REG["ah"] = { num = 0, width = 8, parseOnly = true }
	REG["ch"] = { num = 1, width = 8, parseOnly = true }
	REG["dh"] = { num = 2, width = 8, parseOnly = true }
	REG["bh"] = { num = 3, width = 8, parseOnly = true }
end

-- Segment registers (encoding numbers, per the SDM Sreg encoding): recognized as
-- their OWN register class so they stop masquerading as bare symbols. This is
-- load-bearing in BOTH directions: a move INTO one (`movw %ax, %fs`) is rejected
-- by fp.unsafeReject (a segment-register write cannot be proven safe), while a
-- move FROM one (`movl %fs, %eax`) passes through as an ordinary (unknown-value)
-- definition of the destination web. The segment-QUALIFIED memory spelling
-- (`%fs:0x28`) is unaffected: classifyReg is called with the exact token
-- "fs:0x28", which is not in SEG, so that operand still parses as a symbolic
-- displacement and keeps its existing rejection. These names deliberately do NOT
-- go into REG: the canonical-names fill (NAMES) is keyed by (width, num) and a
-- 16-bit "fs" at num 4 would collide with %sp's slot (and make regName
-- order-dependent).
local SEG = { es = 0, cs = 1, ss = 2, ds = 3, fs = 4, gs = 5 }

-- Pseudoregister number registry (see classifyReg).

local PSEUDO_BASE = 2000
local pseudoNums = {}
local numPseudos = 0

local function classifyReg(name)
	local low = string.lower(name)
	-- Pseudoregisters: `%fil_<ident>` (in Intel-syntax files the `%` prefix is
	-- still required, so a bare `fil_<ident>` keeps its plain-symbol reading and
	-- can never shadow a same-named global). <ident> matches [0-9a-zA-Z_]+ and may
	-- start with a digit. The <ident> is case-sensitive (`%fil_FOO` and `%fil_foo`
	-- are distinct pseudos); only the `fil_` prefix itself matches
	-- case-insensitively, like other register names. A pseudo is a 64-bit GPR:
	-- it has no fixed physical
	-- register and the register allocator colors it like any other virtual web
	-- (spilling across calls as needed). Each distinct name maps to one register
	-- number in a dedicated band (never colliding with physical GPRs 0-15, the
	-- yolo-argument band, or the frame-slot band); the mapping is allocated in
	-- source order, so identical input always produces identical numbering.
	-- NOTE: match on the original `name` (not the lowered `low`) so the captured
	-- ident keeps its case.
	local stripped = string.match(name, "^%%?[Ff][Ii][Ll]_(.*)$")
	if stripped ~= nil then
		if not string.match(stripped, "^[0-9a-zA-Z_]+$") then
			err.here("bad pseudoregister name `" .. name .. "` (expected `%fil_<ident>` with <ident> matching [0-9a-zA-Z_]+)")
		end
		-- Canonicalize the Intel `%`-spelling (and any prefix-case spelling) to
		-- the bare lowercase-prefix name so all spellings share one register (a
		-- file uses one syntax, but the rendering and messages stay clean). The
		-- ident itself keeps its case, so `FOO` vs `foo` stay distinct.
		local canon = "fil_" .. stripped
		local n = pseudoNums[canon]
		if not n then
			numPseudos += 1
			n = PSEUDO_BASE + numPseudos
			pseudoNums[canon] = n
		end
		return { kind = "reg", rc = "gpr", num = n, width = 64, name = canon, pseudo = stripped }
	end
	if low == "rip" then return { kind = "reg", rc = "rip", name = "rip", width = 64 } end
	local segNum = SEG[low]
	if segNum then
		return { kind = "reg", rc = "seg", num = segNum, width = 16, name = low }
	end
	local r = REG[low]
	if not r then
		-- Vector/x87 register names (xmm/ymm/zmm/mm/st) get rc="xmm" so downstream (the
		-- frame policy) can REJECT them cleanly; left unclassified they would masquerade
		-- as symbols and pass through into garbage output or cryptic assembler errors.
		local vec, num = string.match(low, "^([xyz]?mm)(%d+)$")
		if vec then
			local w = (vec == "xmm") and 128 or (vec == "ymm") and 256 or (vec == "zmm") and 512 or 64
			return { kind = "reg", rc = "xmm", num = tonumber(num), width = w, name = low }
		end
		-- AVX512 opmask registers k0-k7 (rc="xmm" class so they pass through unmodeled,
		-- like vector registers; they never participate in GPR webs).
		local kn = string.match(low, "^k([0-7])$")
		if kn then
			return { kind = "reg", rc = "xmm", num = tonumber(kn), width = 64, name = low }
		end
		-- x87 FPU stack registers: st / st(N) / stN (AT&T %st/%st(N); Intel st/st(N)/stN).
		local stn = string.match(low, "^st%((%d+)%)$") or string.match(low, "^st(%d+)$")
		if low == "st" or stn then
			return { kind = "reg", rc = "xmm", num = tonumber(stn) or 0, width = 80, name = low }
		end
		return nil
	end
	local rc = (r.num == 4) and "sp" or "gpr"  -- rsp is the stack pointer
	return { kind = "reg", rc = rc, num = r.num, width = r.width, name = low }
end
x86parse.classifyReg = classifyReg

-- canonical name for (num, width)
local NAMES = { [64] = {}, [32] = {}, [16] = {}, [8] = {} }
do
	-- DETERMINISM: REG is string-keyed, and a VM's string-keyed hash-table iteration
	-- order is an implementation detail not derived from the input, so sort the names
	-- rather than rely on it (this also stays deterministic if a future Luau seeds
	-- string hashes). "ah" and "al" both map to (width=8, num=0); skip the parse-only
	-- "ah" alias during the fill so the canonical low-byte name wins (matching the
	-- standard encoding, where al/cl/dl/bl are nums 0-3).
	local regNames = {}
	for name in REG do table.insert(regNames, name) end
	table.sort(regNames)
	for _, name in regNames do
		local r = REG[name]
		if not r.parseOnly and not NAMES[r.width][r.num] then NAMES[r.width][r.num] = name end
	end
	-- prefer the standard names
	local pref64 = { [0]="rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15" }
	for n, nm in pref64 do NAMES[64][n] = nm end
end
function x86parse.regName(num, width)
	return NAMES[width] and NAMES[width][num] or ("r" .. num)
end

-- PTR size specifier -> bytes
local PTRSIZE = { byte = 1, word = 2, dword = 4, qword = 8, xmmword = 16,
	ymmword = 32, zmmword = 64, tbyte = 10, oword = 16 }

local function trim(s) return (string.gsub(s, "^%s*(.-)%s*$", "%1")) end

-- Split an operand string into top-level comma fields, respecting () and [].
local function splitOperands(s)
	local out, depth, cur = {}, 0, {}
	for i = 1, #s do
		local c = string.sub(s, i, i)
		if c == "(" or c == "[" then depth += 1; table.insert(cur, c)
		elseif c == ")" or c == "]" then depth -= 1; table.insert(cur, c)
		elseif c == "," and depth == 0 then table.insert(out, trim(table.concat(cur))); cur = {}
		else table.insert(cur, c) end
	end
	local last = trim(table.concat(cur))
	if last ~= "" then table.insert(out, last) end
	return out
end

-- Parse a symbol reference possibly decorated with @PLT/@GOTPCREL and +disp.
local function parseSym(text)
	local reloc
	local at = string.find(text, "@", 1, true)
	if at then reloc = string.sub(text, at + 1); text = string.sub(text, 1, at - 1) end
	return { kind = "sym", text = text, reloc = reloc }
end

-- Decimal text for a folded constant. tostring renders large integral doubles in
-- exponent notation ("4.6116860184274e+18"), which `as` cannot read back as an
-- integer literal; %.0f always prints the full decimal expansion.
local function intText(v)
	return string.format("%.0f", v)
end

-- Constant-expression evaluation for immediates and displacements ------------------
--
-- gas folds integer constant expressions at assembly time, and real-world inputs
-- rely on it (OpenSSL perlasm emits `subq $64+32,%rsp`, `movl 512-128(%rcx), %eax`,
-- `movq $1<<4, %rax`, `K256+512(%rip)`). The operand model keeps an immediate as
-- (text, value) and a displacement as (dispSym, dispVal), so the parser must fold
-- such expressions itself: parseDispExpr(text) evaluates an integer constant
-- expression and returns { sym = <name?>, const = <number> } — a pure constant has
-- no sym, and a symbolic displacement is the bare symbol plus a folded constant
-- offset. Supported syntax: decimal and 0x-hex literals, the binary operators
-- + - * / % << >> (integer semantics: / truncates toward zero like C/gas, >> is an
-- arithmetic shift), unary - and +, and parentheses. A symbol may only stand alone
-- or be added to/subtracted from constants: sym*2, sym-sym, -sym and friends are
-- not representable as one displacement and are rejected, as is anything
-- malformed — a misparse must NEVER silently become a 0 immediate/displacement
-- (which is what tonumber's nil used to turn it into). `what` ("immediate" /
-- "displacement") is for error messages.
local function parseDispExpr(text, what)
	-- A GNU-as numeric local label reference (`1f`/`2b`) is a SYMBOL as far as the
	-- operand model is concerned (numlabel.luau rewrites it after parsing).
	if string.match(text, "^%d+[fb]$") then return { sym = text, const = 0 } end
	-- Fast path: a plain literal keeps the exact tonumber semantics it always had,
	-- so every non-expression input behaves byte-identically to before.
	local plain = tonumber(text)
	if plain then return { const = plain } end

	-- Tokenize into operator strings plus { num = n } / { sym = name } tables.
	local tokens = {}
	local i, n = 1, #text
	while i <= n do
		local c = string.sub(text, i, i)
		if string.match(c, "%s") then
			i += 1
		elseif string.match(c, "[%a_.$]") then
			local name = string.match(text, "^[%a_.$][%w_.$]*", i)
			table.insert(tokens, { sym = name })
			i += #name
		elseif string.match(c, "%d") then
			local hex = string.match(text, "^0[xX][0-9a-fA-F]+", i)
			if hex then
				table.insert(tokens, { num = tonumber(hex) })
				i += #hex
			else
				local digits = string.match(text, "^%d+", i)
				-- digits directly followed by f/b are a numeric local label
				-- reference (`1f`/`2b`), which gas accepts anywhere a symbol goes.
				local fb = string.sub(text, i + #digits, i + #digits)
				if fb == "f" or fb == "b" then
					table.insert(tokens, { sym = digits .. fb })
					i += #digits + 1
				else
					table.insert(tokens, { num = tonumber(digits) })
					i += #digits
				end
			end
		elseif c == "<" or c == ">" then
			if string.sub(text, i + 1, i + 1) ~= c then
				err.here("unsupported operator '" .. c .. "' in " .. what
					.. " expression: " .. text)
			end
			table.insert(tokens, c .. c)
			i += 2
		elseif c == "+" or c == "-" or c == "*" or c == "/" or c == "%"
			or c == "(" or c == ")" or c == "~" then
			table.insert(tokens, c)
			i += 1
		else
			err.here("unexpected character '" .. c .. "' in " .. what
				.. " expression: " .. text)
		end
	end
	if #tokens == 0 then
		err.here("empty " .. what .. " expression")
	end

	-- Recursive descent. Every value is { const = n } or { sym = name, const = n }:
	-- a displacement is affine, so at most ONE symbol may appear, and only under
	-- +/- with constants.
	local pos = 0
	local function peek() return tokens[pos + 1] end
	local function take() pos += 1; return tokens[pos] end

	local parseExpr

	local function constOf(v, opname)
		if v.sym then
			err.here("symbol '" .. v.sym .. "' cannot appear in a '" .. opname
				.. "' " .. what .. " expression: " .. text)
		end
		return v.const
	end

	local function parseFactor()
		local t = take()
		if t == nil then
			err.here("missing operand in " .. what .. " expression: " .. text)
		end
		if t == "(" then
			local v = parseExpr()
			if take() ~= ")" then
				err.here("missing ')' in " .. what .. " expression: " .. text)
			end
			return v
		end
		if t == "-" then return { const = -constOf(parseFactor(), "-") } end
		if t == "+" then return { const = constOf(parseFactor(), "+") } end
		-- unary bitwise NOT (gas: ~x = -x - 1, exact in integer range)
		if t == "~" then return { const = -1 - constOf(parseFactor(), "~") } end
		if type(t) == "table" then return { sym = t.sym, const = t.num or 0 } end
		err.here("unexpected '" .. tostring(t) .. "' in " .. what .. " expression: " .. text)
	end

	local function parseTerm()
		local v = parseFactor()
		while true do
			local op = peek()
			if op ~= "*" and op ~= "/" and op ~= "%" and op ~= "<<" and op ~= ">>" then return v end
			take()
			local lhs = constOf(v, op)
			local rhs = constOf(parseFactor(), op)
			if op == "*" then v = { const = lhs * rhs }
			elseif op == "<<" or op == ">>" then
				if rhs < 0 then
					err.here("negative shift count in " .. what .. " expression: " .. text)
				end
				if op == "<<" then v = { const = lhs * 2^rhs }
				else v = { const = math.floor(lhs / 2^rhs) } end
			elseif rhs == 0 then
				err.here("division by zero in " .. what .. " expression: " .. text)
			else
				local q = lhs / rhs
				local qt = q >= 0 and math.floor(q) or math.ceil(q)
				if op == "/" then v = { const = qt } else v = { const = lhs - qt * rhs } end
			end
		end
	end

	parseExpr = function()
		local v = parseTerm()
		while true do
			local op = peek()
			if op ~= "+" and op ~= "-" then return v end
			take()
			local rhs = parseTerm()
			if op == "+" then
				if v.sym and rhs.sym then
					err.here("more than one symbol in " .. what .. " expression ('"
						.. v.sym .. "', '" .. rhs.sym .. "'): " .. text)
				end
				v = { sym = v.sym or rhs.sym, const = v.const + rhs.const }
			else
				if rhs.sym then
					err.here("subtracting symbol '" .. rhs.sym .. "' in a " .. what
						.. " expression is not a displacement: " .. text)
				end
				v = { sym = v.sym, const = v.const - rhs.const }
			end
		end
	end

	local result = parseExpr()
	if pos ~= #tokens then
		err.here("unexpected trailing text in " .. what .. " expression: " .. text)
	end
	-- folded: real evaluation happened (the fast paths above leave it nil), so a
	-- caller that re-renders the text (the $-immediate path) canonicalizes it.
	result.folded = true
	return result
end
-- Exported for the driver's data-object collector (sarcasm.luau), which folds the
-- constant expressions of data directives (`.zero 64+32`, `.long 1<<4`, ...).
x86parse.parseDispExpr = parseDispExpr

-- AVX512 decorator handling -------------------------------------------------
-- Strip trailing {...} groups off an operand field (`%zmm0{%k1}{z}`, `(%rdi){1to16}`,
-- `[rdi]{k3}`). Returns the stripped field plus the decorator strings in left-to-right
-- order.
local function stripDecos(field)
	local decos = {}
	while true do
		local d = string.match(field, "(%b{})$")
		if not d then break end
		table.insert(decos, 1, d)
		field = string.sub(field, 1, #field - #d)
	end
	return field, decos
end

-- Attach decorators to a parsed operand: `{1toN}` on a memory operand records the
-- embedded-broadcast factor in op.bcastN; the raw text of every decorator is kept in
-- op.deco for re-rendering and validation. A mask group written without `%` (Intel
-- syntax `{k1}`) is normalized to the AT&T spelling `{%k1}` (output is always AT&T).
local function attachDecos(op, decos)
	if #decos == 0 then return op end
	local parts = {}
	for _, d in decos do
		local kn = string.match(d, "^{k([0-7])}$")
		if kn then d = "{%k" .. kn .. "}" end
		if op.kind == "mem" then
			local n = string.match(d, "^{1to(%d+)}$")
			if n then op.bcastN = tonumber(n) end
		end
		table.insert(parts, d)
	end
	op.deco = table.concat(parts)
	return op
end

-- AT&T operand ------------------------------------------------------------
local function parseAttOperandRaw(field)
	local indirect = false
	if string.sub(field, 1, 1) == "*" then indirect = true; field = string.sub(field, 2) end
	if string.sub(field, 1, 1) == "$" then
		local t = string.sub(field, 2)
		-- Fold a constant expression (`$64+32`, `$1<<4`): the value feeds frame-size
		-- and bounds analysis, and when real evaluation happened the folded TEXT
		-- replaces the operand text so every downstream consumer (and the
		-- re-rendered output) sees the constant. A symbolic immediate (`$sym`,
		-- `$sym+8`) keeps its verbatim text — `as` resolves the relocation — as
		-- does an @RELOC decoration, which is never an expression. A malformed
		-- expression is a clean compile error here, never a silent 0.
		if not string.find(t, "@", 1, true) then
			local e = parseDispExpr(t, "immediate")
			if not e.sym then
				return { kind = "imm", text = e.folded and intText(e.const) or t, value = e.const }
			end
		end
		return { kind = "imm", text = t, value = tonumber(t) }
	end
	-- A `%reg` operand is register-shaped unless the parens make it memory — EXCEPT the
	-- x87 stack register %st(N), whose parens are part of the register NAME; classifying
	-- it here keeps it out of the memory parse below (where it would become a bogus
	-- "%st"-displacement operand and crash or corrupt the renderer).
	if string.sub(field, 1, 1) == "%"
		and (not string.find(field, "(", 1, true) or string.match(field, "^%%st%(%d+%)$")) then
		local r = classifyReg(string.sub(field, 2))
		if r then r.indirect = indirect; return r end
	end
	-- memory: disp(base, index, scale)  [disp and any component optional]
	local pre, inside = string.match(field, "^(.-)%((.*)%)$")
	if inside ~= nil then
		-- gas concatenates base/index expressions: `(4096)(%rbx)` means
		-- `4096(%rbx)`. Rewrite any `(...)(` concatenation into the equivalent
		-- single group so the standard parse below covers it.
		while true do
			local gsubbed = string.gsub(field, "%(([^()]*)%)%(", "%1(")
			if gsubbed == field then break end
			field = gsubbed
		end
		pre, inside = string.match(field, "^(.-)%((.*)%)$")
		local mem = { kind = "mem", scale = 1, indirect = indirect }
		pre = trim(pre)
		if pre ~= "" then
			-- The displacement may be a constant expression (`512-128(%rcx)`, or
			-- `(64+32)(%rsp)` after the concatenation rewrite above) or a symbol
			-- plus/minus a folded constant (`K256+512(%rip)`): split any @reloc
			-- decoration first, then fold. A numeric local label reference (`1f`)
			-- stays a symbol for numlabel.luau. dispVal stays nil for a bare
			-- symbol (the exact shape downstream expects) and is set whenever a
			-- nonzero constant rides alongside one (the renderer re-joins them).
			local ps = parseSym(pre)
			if ps.reloc then
				-- @-decorated: a relocation attaches to a symbol, never to a
				-- folded constant — keep the verbatim symbol pass-through.
				mem.dispSym = ps.text
				mem.dispReloc = ps.reloc
			else
				local e = parseDispExpr(ps.text, "displacement")
				mem.dispSym = e.sym
				if not e.sym or e.const ~= 0 then mem.dispVal = e.const end
			end
		end
		local parts = {}
		for p in (inside .. ","):gmatch("(.-),") do table.insert(parts, trim(p)) end
		if parts[1] and parts[1] ~= "" then
			-- A NUMBER in the base position is a displacement (gas: `(4096)` ==
			-- `4096` — an absolute address), never a register: keeping it as the
			-- displacement lets classify reject the absolute access precisely
			-- instead of silently dropping the displacement. The same goes for a
			-- constant EXPRESSION there (`(64+32)` == `96`) — anything starting
			-- with a digit/sign/paren is expression-shaped; other junk keeps the
			-- old silent-drop behavior.
			mem.base = classifyReg(string.sub(parts[1], 2))
			if not mem.base then
				if tonumber(parts[1]) then
					mem.dispVal = tonumber(parts[1])
				elseif string.match(parts[1], "^[%d%+%-%(]") then
					local e = parseDispExpr(parts[1], "displacement")
					mem.dispSym = e.sym
					if not e.sym or e.const ~= 0 then mem.dispVal = e.const end
				end
			end
		end
		if parts[2] and parts[2] ~= "" then mem.index = classifyReg(string.sub(parts[2], 2)) end
		if parts[3] and parts[3] ~= "" then mem.scale = tonumber(parts[3]) or 1 end
		return mem
	end
	-- numeric local label reference: `1f` / `1b` (next/previous `1:` definition)
	local nref, ndir = string.match(field, "^(%d+)([fb])$")
	if nref then
		return { kind = "numref", text = field, num = nref, dir = ndir, indirect = indirect }
	end
	-- bare symbol / label / number
	if string.match(field, "^-?%d") or string.match(field, "^0[xX]") then
		-- A bare (non-$-prefixed) numeric literal in AT&T syntax is NOT an
		-- immediate: gas reads it as an absolute (moffs) MEMORY operand
		-- (`movq 0x600000, %rax` LOADS from that address). Tag it so
		-- x86_64_isa.classify can reject the absolute access cleanly instead
		-- of silently rendering it as an immediate move. (Intel syntax is
		-- different: a bare number there IS an immediate, so the Intel parser
		-- never sets this tag.) The `*` indirect marker is retained too, so
		-- `jmp *0x600000` / `call *0x600000` (an indirect branch THROUGH that
		-- absolute address) classify as the same un-checkable absolute read
		-- rather than falling through the call/jmp moffs exemption.
		return { kind = "imm", text = field, value = tonumber(field), absAddr = true,
			indirect = indirect }
	end
	local s = parseSym(field); s.indirect = indirect; return s
end

local function parseAttOperand(field)
	local f, decos = stripDecos(field)
	return attachDecos(parseAttOperandRaw(f), decos)
end

-- Intel operand -----------------------------------------------------------
local function parseIntelOperandRaw(field)
	-- An AT&T-style memory operand (disp(base,index,scale)) is never a valid
	-- Intel-syntax operand, but without an explicit check it falls through to
	-- parseSym below and RE-RENDERS verbatim as valid AT&T memory syntax — with
	-- Intel dest-first operand order applied and NO capability/bounds check (a
	-- total check-bypass: `movq (%rdi), %rsi` in Intel mode emitted an unchecked
	-- 8-byte store). Reject any field containing parens — covering the parseSym
	-- fallthrough, the numeric-prefix immediate path (`8(%rdi)`), and
	-- whitespace-dodged forms (`( %rdi )`). The ONLY legitimate parens in an
	-- Intel operand are the x87 stack-register name st(N).
	if string.find(field, "[()]") and not string.match(string.lower(field), "^st%(%d+%)$") then
		err.here("AT&T-style operand in Intel-syntax input"
			.. " (use [bracket] memory operands): " .. field)
	end
	local indirect = false
	local size = nil
	-- size specifier: BYTE PTR / WORD PTR / ... — the PTR keyword itself is
	-- matched in any case (real assemblers accept `qword ptr`, `QWORD PTR`,
	-- `Qword Ptr`, ... alike); the size word is lowercased for PTRSIZE below.
	local sz, rest = string.match(field, "^(%a+)%s+[Pp][Tt][Rr]%s+(.*)$")
	if sz and PTRSIZE[string.lower(sz)] then size = PTRSIZE[string.lower(sz)]; field = trim(rest) end
	-- memory: optional leading disp then [ ... ]  (GAS Intel: `8[rbx]`, `[rbx+8]`, `[rip+sym]`)
	local pre, inside = string.match(field, "^(.-)%[(.*)%]$")
	if inside ~= nil then
		local mem = { kind = "mem", scale = 1, size = size }
		pre = trim(pre)
		if pre ~= "" then
			-- A leading displacement may be a constant expression (`64+32[rbx]`),
			-- folded like the AT&T displacement above. An @-decorated symbol is
			-- not an expression: keep it verbatim.
			if not string.find(pre, "@", 1, true) then
				local e = parseDispExpr(pre, "displacement")
				mem.dispSym = e.sym
				if not e.sym or e.const ~= 0 then mem.dispVal = e.const end
			else
				mem.dispSym = pre
			end
		end
		-- inside: base [+ index*scale] [+/- disp]  (also `rip + sym`)
		inside = (string.gsub(inside, "%s+", ""))
		-- split on + / - keeping sign
		local terms = {}
		local i, n = 1, #inside
		local sign = 1
		local tok = {}
		local function flush()
			local t = table.concat(tok); tok = {}
			if t ~= "" then table.insert(terms, { sign = sign, tok = t }) end
		end
		while i <= n do
			local c = string.sub(inside, i, i)
			if c == "+" then flush(); sign = 1
			elseif c == "-" then flush(); sign = -1
			else table.insert(tok, c) end
			i += 1
		end
		flush()
		for _, term in terms do
			local t = term.tok
			if string.find(t, "*", 1, true) then
				local rn, sc = string.match(t, "^(.-)%*(%d+)$")
				local ir = rn and classifyReg(rn)
				if ir then
					mem.index = ir; mem.scale = tonumber(sc) or 1
				else
					-- Not reg*scale: a constant-product term (`[rbx+8*4]` = +32),
					-- folded into the displacement. Anything else (sym*K, junk)
					-- is a clean parse error instead of a silently dropped term.
					local e = parseDispExpr(t, "displacement")
					mem.dispVal = (mem.dispVal or 0) + term.sign * e.const
				end
			else
				local r = classifyReg(t)
				if r then
					if r.rc == "rip" then mem.base = r
					elseif not mem.base then mem.base = r else mem.index = r end
				elseif string.find(t, "@", 1, true) then
					-- @-decorated symbol (`[rip+sym@GOTPCREL]`): not an expression,
					-- keep it verbatim (the AT&T renderer re-attaches the reloc).
					mem.dispSym = t
				else
					-- A constant-expression term (`[rbx+1<<4]`, `[rip+K256]`):
					-- fold the constant into the displacement, keep any symbol
					-- bare. A NEGATED symbol (`[rip-sym]`) is not a representable
					-- displacement (gas rejects it too), so error rather than
					-- silently dropping the sign.
					local e = parseDispExpr(t, "displacement")
					if e.sym then
						if term.sign < 0 then
							err.here("subtracting symbol '" .. e.sym
								.. "' in a displacement expression is not a displacement: " .. field)
						end
						if mem.dispSym then
							err.here("more than one displacement symbol in memory operand ('"
								.. mem.dispSym .. "', '" .. e.sym .. "'): " .. field)
						end
						mem.dispSym = e.sym
						-- a symbol term carries no constant of its own (the +/- term
						-- split guarantees that), so dispVal keeps its exact old shape
						if e.const ~= 0 then mem.dispVal = (mem.dispVal or 0) + term.sign * e.const end
					else
						mem.dispVal = (mem.dispVal or 0) + term.sign * e.const
					end
				end
			end
		end
		return mem
	end
	local r = classifyReg(field)
	if r then r.indirect = indirect; return r end
	-- numeric local label reference: `1f` / `1b` (next/previous `1:` definition)
	local nref, ndir = string.match(field, "^(%d+)([fb])$")
	if nref then
		return { kind = "numref", text = field, num = nref, dir = ndir, indirect = indirect }
	end
	if string.match(field, "^-?%d") or string.match(field, "^0[xX]") then
		-- A digit-led field is an immediate (unlike AT&T, where a bare number is
		-- an absolute memory operand): fold a constant expression (`sub rsp,
		-- 64+32`, `mov eax, 1<<4`) exactly like the AT&T $-immediate path — a
		-- malformed expression errors cleanly instead of silently becoming a
		-- nil-valued (zero for frame purposes) immediate. A symbol caught by the
		-- leading-digit heuristic (`8+sym`) or an @ decoration keeps the
		-- verbatim pass-through.
		if not string.find(field, "@", 1, true) then
			local e = parseDispExpr(field, "immediate")
			if not e.sym then
				return { kind = "imm", text = e.folded and intText(e.const) or field, value = e.const }
			end
		end
		return { kind = "imm", text = field, value = tonumber(field) }
	end
	return parseSym(field)
end

local function parseIntelOperand(field)
	local f, decos = stripDecos(field)
	return attachDecos(parseIntelOperandRaw(f), decos)
end

-- Strip an AT&T size suffix (movq->mov, addl->add) when the base mnemonic exists in both.
-- We keep the full mnemonic but also record a `.opsize` (bytes) when a suffix is present.
local ATT_SUFFIX = { b = 1, w = 2, l = 4, q = 8 }
local function attMnemonic(m)
	-- movzbl/movsbl etc. keep as-is (semantic); only strip a trailing size on plain ops
	local base, suf = string.match(m, "^(.-)([bwlq])$")
	if base and ATT_SUFFIX[suf] and #base >= 2 then
		return base, ATT_SUFFIX[suf]
	end
	return m, nil
end

-- Comments and annotation markers.
--
-- `#` starts a comment that runs to end of line (never inside strings). But
-- `#!` is an ANNOTATION MARKER, like the universal `;!`: on x86_64 GAS, `#` is
-- the comment introducer, so `#!` reads as "a comment that means more". The
-- EARLIEST marker outside a string literal splits the line into code +
-- annotation body (see splitAnnotation below — comment text must not fabricate
-- annotations).
--
-- stripComment truncates `#`-to-EOL (string-aware). It is applied to the text
-- after an annotation marker so a trailing comment is stripped from the body
-- exactly as it was when the whole line was comment-stripped before the split:
-- `movq %rax, (%rbx) ;! store ptr # note` has body `store ptr`. Like the other
-- string-aware scans, a `\` inside a string escapes the following character, so
-- the `#` in `;! store "a \" # b"` is still inside the string; `\` has no
-- quoting power outside strings.
local function stripComment(s)
	local inStr = false
	local i = 1
	while i <= #s do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr
		elseif inStr and c == "\\" then
			-- escaped character inside the string (e.g. the `"` of `\"`): skip it
			-- whole so it can neither close the string nor start structure; this
			-- mirrors arm64 stripComments and detect.luau's sanitize
			i += 1
		elseif not inStr and c == "#" then
			return string.sub(s, 1, i - 1)
		end
		i += 1
	end
	return s
end

-- Split the EARLIEST `;!` or `#!` annotation marker off a line, string-aware
-- (returns code, annotation-or-nil). Text inside a double-quoted string never
-- produces an annotation (`.asciz "a ;! b"` is an ordinary directive), and a
-- `\` inside a string escapes the following character, so `"a \" ;! b"` keeps
-- its `;!` (the string is still open); `\` has no quoting power outside
-- strings. A `#` NOT immediately followed by `!` starts a comment: the rest
-- of the line is dropped and no annotation is fabricated, even if the comment
-- text contains
-- `;!` or `#!` (e.g. `# use ;! load ptr here`). The annotation body is the
-- text after the marker with any trailing `#` comment stripped and then
-- trimmed (`movq %rax, (%rbx) #! store ptr # note` has body `store ptr`).
local function splitAnnotation(s)
	local inStr = false
	local i = 1
	while i <= #s do
		local c = string.sub(s, i, i)
		if c == '"' then
			inStr = not inStr
		elseif inStr and c == "\\" then
			-- escaped character inside the string (e.g. the `"` of `\"`): skip it
			-- whole so it can neither close the string nor be mistaken for a
			-- marker; mirrors arm64 stripComments and detect.luau's sanitize
			i += 1
		elseif not inStr then
			local d = string.sub(s, i + 1, i + 1)
			if (c == ";" or c == "#") and d == "!" then
				-- earliest annotation marker: `;!` or `#!`
				return string.sub(s, 1, i - 1), trim(stripComment(string.sub(s, i + 2)))
			elseif c == "#" then
				-- plain `#` comment: drop the rest of the line, no annotation
				return string.sub(s, 1, i - 1), nil
			end
		end
		i += 1
	end
	return s, nil
end

-- Parse one instruction line (mnemonic plus operand text; labels and annotations
-- already peeled) into an insn statement. parseString uses this for spelled
-- instructions; the .byte decoder (decodeByteInsns below) uses it for the
-- canonical AT&T spellings its byte sequences stand for, so a decoded
-- instruction is indistinguishable from a spelled one in every downstream pass
-- (validation, frame analysis, lift, codegen, rendering).
local function parseInsnText(t, syntax)
	-- gas ENCODING-HINT pseudo-prefixes ({vex}/{vex2}/{vex3}/{evex}/{np}):
	-- OpenSSL perlasm emits `{vex} vpmadd52luq ...` to force a VEX (rather than
	-- EVEX) encoding of an instruction that has both. The hint changes only the
	-- encoding, never the semantics — sarcasm models the instruction — so strip
	-- it and let the renderer emit the plain spelling (gas picks an encoding
	-- with identical semantics). An unrecognized {...} prefix stays a parse
	-- error.
	while true do
		local pfx, restP = string.match(t, "^(%b{})%s*(.*)$")
		if not pfx then break end
		local inner = string.lower(string.sub(pfx, 2, -2))
		if inner ~= "vex" and inner ~= "vex2" and inner ~= "vex3"
			and inner ~= "evex" and inner ~= "np" then
			err.here("unsupported encoding-hint prefix '" .. pfx
				.. "' (only {vex}/{vex2}/{vex3}/{evex}/{np} are recognized): " .. t)
		end
		t = restP
	end
	local mnem, rest = string.match(t, "^([%.%w_][%w_.]*)%s*(.*)$")
	if not mnem then
		-- The line starts with something that can neither begin a mnemonic nor a
		-- directive (e.g. `//!` annotation text, which is NOT a marker on x86_64
		-- and so stays in the code part). Raise a normal parse error rather than
		-- choking on string.sub(nil, ...) below (a latent crash for such lines
		-- before markers existed).
		err.here("cannot parse line: " .. t)
	end
	rest = trim(rest or "")
	-- `lock` is an instruction PREFIX, not a mnemonic: gas emits it as a leading
	-- token on the same line (`lock xaddq %rsi, (%rdi)`, also
	-- `lock;`-separated). Re-split the remainder into the real mnemonic +
	-- operands and tag the instruction; WHICH instructions may carry the prefix
	-- is validated in x86_64_isa.classify. xacquire/xrelease stay
	-- reject-listed "mnemonics" (fp.unsafeReject).
	local lockPrefix = false
	if string.lower(mnem) == "lock" then
		local r2 = (string.gsub(rest, "^%s*;%s*", ""))
		local m2, rest2 = string.match(r2, "^([%.%w_][%w_.]*)%s*(.*)$")
		if not m2 then
			err.here("`lock` prefix with no following instruction: " .. t)
		end
		mnem, rest = m2, trim(rest2 or "")
		lockPrefix = true
	end

	-- `rep`/`repe`/`repne`/`repz`/`repnz` are instruction PREFIXES, not
	-- mnemonics: gas emits them as a leading token on the same line
	-- (`rep movsq`, also `rep;`-separated). Re-split the remainder into the
	-- real mnemonic + operands and tag the instruction; WHICH instructions
	-- may carry the prefix is validated in x86_64_isa.classify (rep movs* /
	-- rep stos* lower to checked copies/fills, `rep ret` keeps its hint).
	local repPrefix = nil
	do
		local low = string.lower(mnem)
		if low == "rep" or low == "repe" or low == "repne"
			or low == "repz" or low == "repnz" then
			local r2 = (string.gsub(rest, "^%s*;%s*", ""))
			local m2, rest2 = string.match(r2, "^([%.%w_][%w_.]*)%s*(.*)$")
			if not m2 then
				err.here("`" .. low .. "` prefix with no following instruction: " .. t)
			end
			mnem, rest = m2, trim(rest2 or "")
			repPrefix = low
		end
	end
	local opsize
	if syntax == "att" then local _; _, opsize = attMnemonic(mnem) end
	local ops = {}
	for _, f in splitOperands(rest) do
		if f ~= "" then
			table.insert(ops, syntax == "att" and parseAttOperand(f) or parseIntelOperand(f))
		end
	end
	-- AT&T lists operands source-first; normalize to dest-first (like Intel/ARM)
	if syntax == "att" and #ops >= 2 then
		local rev = {}
		for i = #ops, 1, -1 do table.insert(rev, ops[i]) end
		ops = rev
	end
	return { kind = "insn", mnemonic = string.lower(mnem), operands = ops,
		opsize = opsize, raw = t, syntax = syntax, lockPrefix = lockPrefix or nil,
		repPrefix = repPrefix }
end

-- `.alloca size, alignment, result`: stack allocation without touching %rsp.
-- Parses the three comma-separated operands with the file's syntax parsers and
-- builds an ordinary insn statement (mnemonic "alloca", operands in WRITTEN order:
-- [size, alignment, result] — unlike AT&T instructions, nothing is reversed).
-- size/alignment: an immediate (AT&T `$N` or bare `N`), a GPR (including a
-- `%fil_` pseudo), or a frame-relative spill-slot reference; result: a GPR, a
-- pseudo, or a spill slot. A bare AT&T number parses as an absolute-address
-- immediate elsewhere — here it is a size, so the absAddr tag is cleared; a
-- bare symbol is never a valid operand (rejected here, not as a global access).
local function parseDotAlloca(rest, syntax, t)
	local fields = splitOperands(rest)
	if #fields ~= 3 then
		err.here("`.alloca` takes exactly 3 operands (size, alignment, result): " .. t)
	end
	local ops = {}
	for k, f in fields do
		if f == "" then
			err.here("`.alloca` takes exactly 3 operands (size, alignment, result): " .. t)
		end
		local op = syntax == "att" and parseAttOperand(f) or parseIntelOperand(f)
		if op.kind == "imm" then op.absAddr = nil end
		if op.kind == "sym" or op.kind == "numref" then
			err.here("`.alloca` operand is a symbol, but size/alignment must be a register, spill slot, or immediate"
				.. " and result a register, pseudo-register, or spill slot: " .. t)
		end
		ops[k] = op
	end
	return { kind = "insn", mnemonic = "alloca", operands = ops, raw = t, syntax = syntax, dotAlloca = true }
end

function x86parse.parseString(text, syntax, fileName)
	err.beginParse(fileName)
	-- Per-file pseudo-registry reset. pseudoNums/numPseudos live at module scope
	-- (the require cache persists them across files in one process), so
	-- reinitialize them here for deterministic per-file numbering from
	-- PSEUDO_BASE+1 in source order — the same pattern as numlabel's call-local
	-- state. Parsing is synchronous (no yields), so no interleaved parse can
	-- observe a half-built table.
	pseudoNums = {}
	numPseudos = 0
	local out = {}
	text = (string.gsub(text, "\r\n", "\n"))
	local lineNo = 0
	for raw in (text .. "\n"):gmatch("(.-)\n") do
		lineNo += 1
		err.atParseLine(lineNo)
		-- Split the annotation marker (`;!` everywhere, or `#!` — on x86_64 `#` is
		-- the comment introducer, so `#!` is "a comment that means more") off the
		-- line, string-aware. Comment text must never fabricate an annotation: a
		-- line like `# use ;! load ptr here` would otherwise attach a spurious
		-- annotation to an otherwise-empty statement, which the shared body
		-- validation rejects ("annotation on an otherwise-empty line"); the arm64
		-- parser keeps `//`/`//!`/`/* */` comments out of annotations the same
		-- way. A `#` after the annotation still ends a trailing comment inside
		-- the body.
		local code, annotation = splitAnnotation(raw)
		local t = trim(code)
		local before = #out
		if t == "" then
			table.insert(out, { kind = "blank", text = raw, annotation = annotation })
		else
			-- peel labels
			while true do
				local name, rest = string.match(t, "^([%w_.$][%w_.$@]*):%s*(.*)$")
				if not name then break end
				table.insert(out, { kind = "label", name = name })
				t = trim(rest)
				if t == "" then break end
			end
			if t ~= "" then
				-- A leading {..} group is a gas encoding-hint pseudo-prefix
				-- ({vex} & co — see parseInsnText): the mnemonic follows it.
				local mnem, rest = string.match(t, "^([%.%w_][%w_.]*)%s*(.*)$")
				if not mnem and string.sub(t, 1, 1) == "{" then
					mnem, rest = string.match(t, "^%b{}%s*([%.%w_][%w_.]*)%s*(.*)$")
				end
				if not mnem then
					-- The line starts with something that can neither begin a
					-- mnemonic nor a directive (e.g. `//!` annotation text, which is
					-- NOT a marker on x86_64 and so stays in the code part). Raise a
					-- normal parse error rather than choking on string.sub(nil, ...)
					-- below (a latent crash for such lines before markers existed).
					err.here("cannot parse line: " .. t)
				end
				rest = trim(rest or "")
					if string.sub(mnem, 1, 1) == "." then
						if string.lower(mnem) == ".alloca" then
							table.insert(out, parseDotAlloca(rest, syntax, t))
						else
							table.insert(out, { kind = "directive", name = mnem, args = rest, raw = t })
						end
				else
					table.insert(out, parseInsnText(t, syntax))
				end
			end
		end
		if annotation and #out > before then out[#out].annotation = annotation end
		for k = before + 1, #out do
			out[k].line = out[k].line or lineNo
			out[k].file = out[k].file or fileName
		end
	end
	err.endParse()
	numlabel.resolve(out)
	return out
end

-- .byte instruction decoding (OpenSSL perlasm legacy-gas workarounds) -----------
--
-- OpenSSL's perlasm emits many instructions as raw `.byte` sequences inside
-- function bodies (legacy-gas workarounds): every function ends with
-- `.byte 0xf3,0xc3` (rep ret), CET entry points are `.byte 243,15,30,250`
-- (endbr64), locked RMWs are a `.byte 0xf0` line followed by the spelled
-- instruction, prologue stack adjustments are `.byte 0x48,0x83,0xEC,0x08`, and
-- so on. Data in a function body is rejected (transform's validateBody), so
-- such inputs would not compile at all. decodeByteInsns rewrites a function
-- body's RUNS of adjacent `.byte` statements into the instructions those byte
-- sequences encode — at parse time, by re-parsing the canonical AT&T spelling
-- through parseInsnText, so the result is indistinguishable from a spelled
-- instruction for every downstream pass (a decoded `ret` IS a `ret` to the
-- frame pass and the fall-off reachability check; a decoded `lock cmpxchgl`
-- carries the same lockPrefix tag a spelled one does, with identical
-- validation). Sequences that do not match the table stay `.byte` directives,
-- so genuine data keeps hitting the exact same "data in a function body"
-- compile error as before (a clean sarcasm error, never a crash).
--
-- The table is data-driven: an ordered list of { bytes = {..}, text = ".."},
-- plus the special `lock` marker (0xf0), which JOINS the following instruction
-- — the next bytes of the same run, or the spelled instruction on the next
-- line — exactly like the spelled `lock` prefix (x86_64_isa.lockAllows decides
-- whether the joined instruction may carry it, so a byte-encoded `lock ret`
-- rejects precisely like the spelled form). Matching is greedy longest-first
-- at each buffer position; no table entry is a prefix of another today, but
-- the longest-match rule keeps future entries order-independent.
local BYTE_DECODE = {
	-- rep ret: the 0xf3 is an AMD branch-prediction hint; treat exactly like ret.
	{ bytes = { 0xf3, 0xc3 }, text = "ret" },
	-- CET indirect-branch landing pad (modeled as a nop by x86_64_isa).
	{ bytes = { 0xf3, 0x0f, 0x1e, 0xfa }, text = "endbr64" },
	{ bytes = { 0x0f, 0x0b }, text = "ud2" },
	-- the 3-byte padding nop; its (%rax) operand is an encoding hint, not an access.
	{ bytes = { 0x0f, 0x1f, 0x00 }, text = "nopl (%rax)" },
	-- 0xf0 lock prefix: joins the next instruction (see the long comment above).
	{ bytes = { 0xf0 }, lock = true },
	{ bytes = { 0x48, 0x98 }, text = "cdqe" },          -- aka cltq
	{ bytes = { 0x0f, 0x01, 0xd0 }, text = "xgetbv" },
	{ bytes = { 0x49, 0x0f, 0xc7, 0xf2 }, text = "rdrand %r10" },  -- REX.WB 0f c7 /6, r10
	{ bytes = { 0x49, 0x0f, 0xc7, 0xfa }, text = "rdseed %r10" },  -- REX.WB 0f c7 /7, r10
	-- frame-relevant prologue forms (must be real sub/lea to the frame pass):
	{ bytes = { 0x48, 0x83, 0xec, 0x08 }, text = "subq $8,%rsp" },
	{ bytes = { 0x48, 0x83, 0xec, 0x28 }, text = "subq $40,%rsp" },
	{ bytes = { 0x48, 0x81, 0xec, 0x08, 0x01, 0x00, 0x00 }, text = "subq $0x108,%rsp" },
	{ bytes = { 0x4c, 0x8d, 0x14, 0x24 }, text = "leaq (%rsp),%r10" },
	-- single-byte filler prefixes gas emits as standalone `.byte` lines for
	-- alignment padding: each lone byte is its own nop. (0x64/0x65 fs/gs are
	-- deliberately NOT here: a segment override changes the next instruction's
	-- addressing, so it may never be dropped.)
	{ bytes = { 0x66 }, text = "nop" },
	{ bytes = { 0x67 }, text = "nop" },
	{ bytes = { 0x3e }, text = "nop" },
	{ bytes = { 0x2e }, text = "nop" },
	-- 0x90 (xchg %eax,%eax) is the one-byte nop. (The pattern decoder also
	-- catches it inside prefixed multi-byte nop forms.)
	{ bytes = { 0x90 }, text = "nop" },
}

-- Pattern-based data decoding (a comprehensive x86_64 disassembler) -----------
--
-- The fixed BYTE_DECODE table covers whole instructions with no operand
-- variation. Everything else — data-directive runs encoding arbitrary x86_64
-- instructions (the OpenSSL perlasm legacy-gas workarounds for AES/SHA/movbe/
-- mulx/vmovdqu, the always-.byte SHA512-NI forms, `.long 0x9066A4F3` = a rep
-- plus its 2-byte nop pad, and hand-written encodings of any modeled integer or
-- SSE instruction) — is decoded by the ModRM/SIB/displacement-pattern decoder
-- below: a small but comprehensive x86_64 disassembler covering the legacy
-- one-byte map, the 0F two-byte map, the 0F38/0F3A three-byte maps, and the
-- VEX/EVEX shapes sarcasm models. It reconstructs the canonical AT&T spelling,
-- which is re-parsed through parseInsnText exactly like a table entry (so a
-- decoded instruction is indistinguishable from a spelled one downstream).
-- Anything the decoder cannot fully consume stays data and hits the usual
-- in-body-data rejection.
--
-- SOUNDNESS RULES (these keep the decode a faithful renaming of the bytes):
--  * prefixes are never dropped where they change semantics — fs/gs (0x64/0x65)
--    make the run undecodable, and the address-size prefix (0x67) only decodes
--    as the standalone alignment-padding nop gas emits (a 0x67 IN FRONT of an
--    instruction truncates that instruction's effective addresses, so it may
--    never be folded away);
--  * a lock byte is a run-level marker that joins the next instruction (the
--    BYTE_DECODE table entry above — it never silently disappears);
--  * branches decode only when the target is an instruction boundary INSIDE the
--    same run (a synthetic local label is planted there); a branch out of the
--    run or into the middle of an instruction leaves the run data;
--  * immediates wider than 4 bytes are emitted as exact hex text (double
--    arithmetic is inexact above 2^53, and a decoded constant must be exact);
--  * every byte of the run must be consumed by exactly one successfully decoded
--    instruction whose text re-parses cleanly (checked by the caller, which
--    re-parses under pcall) — a partially decodable run stays data entirely.

-- GPR name for (num, width) — the parser's canonical names.
local function gprName(num, width) return x86parse.regName(num, width) end

-- Sign-extend a displacement of `bits` (8 or 32).
local function sx(v, bits)
	if bits == 8 and v >= 128 then return v - 256 end
	if bits == 32 and v >= 2147483648 then return v - 4294967296 end
	return v
end

-- Sign-extend an 8-bit immediate.
local function sx8(v)
	if v >= 128 then return v - 256 end
	return v
end

-- Sign-extend a 32-bit immediate.
local function sx32(v)
	if v >= 2147483648 then return v - 4294967296 end
	return v
end

-- Byte-width register name: without a REX prefix the reg/rm fields 4-7 name the
-- HIGH bytes (%ah/%ch/%dh/%bh — modeled exactly by x86_64_isa's high-byte
-- classification); with REX they are the low bytes spl/bpl/sil/dil.
local HIGH_BYTE_NAME = { [4] = "ah", [5] = "ch", [6] = "dh", [7] = "bh" }
local function byteRegName(num, rex)
	if rex == 0 and HIGH_BYTE_NAME[num] then return HIGH_BYTE_NAME[num] end
	return gprName(num, 8)
end

-- AT&T size suffix by operand width in bytes.
local SUFFIX = { [1] = "b", [2] = "w", [4] = "l", [8] = "q" }

-- Group-1 (ALU) and group-2 (shift/rotate) ModRM reg-field suboperations.
local GRP1_SUBOP = { [0] = "add", [1] = "or", [2] = "adc", [3] = "sbb",
	[4] = "and", [5] = "sub", [6] = "xor", [7] = "cmp" }
local GRP2_SUBOP = { [0] = "rol", [1] = "ror", [2] = "rcl", [3] = "rcr",
	[4] = "shl", [5] = "shr", [6] = "sal", [7] = "sar" }

-- Condition code names (jcc/setcc/cmovcc), indexed by the low opcode nibble.
local CONDS = { [0] = "o", "no", "b", "ae", "e", "ne", "be", "a",
	"s", "ns", "p", "np", "l", "ge", "le", "g" }

-- Decode a ModRM/SIB/disp sequence at b[p] into the rm operand text (register
-- or memory) and the position past it. regExt/rmExt/indexExt are the extension
-- bits (0 or 8) from a REX/VEX/EVEX prefix; rmNumFn names registers for the
-- rm field (used in mod=11). Returns nil when the form cannot be expressed as
-- one operand (RIP-relative or baseless absolute-addressing forms — a data run
-- has no symbol to name them with).
local function decodeModRM(b, p, regExt, rmExt, indexExt, rmNumFn)
	local modrm = b[p]
	if not modrm then return nil end
	local mod = math.floor(modrm / 64) % 4
	local rm = modrm % 8
	p += 1
	if mod == 3 then
		return rmNumFn(rm + rmExt), p
	end
	-- memory operand
	local base, index, scale = nil, nil, nil
	if rm == 4 then
		-- SIB
		local sib = b[p]
		if not sib then return nil end
		p += 1
		scale = 2 ^ (math.floor(sib / 64) % 4)
		local idx = (math.floor(sib / 8) % 8) + indexExt
		if idx ~= 4 then index = idx end   -- 4 = no index (rsp encoding)
		local baseField = (sib % 8) + rmExt
		if (sib % 8) == 5 and mod == 0 then
			return nil   -- disp32 with no base: absolute — cannot express
		end
		base = baseField
	elseif rm == 5 and mod == 0 then
		return nil   -- RIP-relative disp32: no symbol — cannot express
	else
		base = rm + rmExt
	end
	local disp = 0
	if mod == 1 then
		if not b[p] then return nil end
		disp = sx(b[p], 8); p += 1
	elseif mod == 2 then
		if not (b[p] and b[p + 1] and b[p + 2] and b[p + 3]) then return nil end
		disp = sx(b[p] + b[p + 1] * 256 + b[p + 2] * 65536 + b[p + 3] * 16777216, 32)
		p += 4
	end
	local text = tostring(disp) .. "(%" .. gprName(base, 64)
	if index then
		text = text .. ",%" .. gprName(index, 64) .. "," .. tostring(scale)
	end
	return text .. ")", p
end

-- The ModRM reg-field number (plus the REX.R extension) at byte position p.
local function regNumAt(b, p, rexR)
	return (math.floor((b[p] or 0) / 8) % 8) + rexR
end

-- Decode one LEGACY-map (one-byte opcode) instruction. b[p] is the opcode byte
-- (prefixes already scanned into pf). Returns the AT&T text and the total length
-- from `pos`, or (text, len, rel) for a decodable branch (rel = the displacement;
-- the caller synthesizes the target label), or nil when the bytes are not a
-- modeled instruction.
local function decodeLegacy(b, pos, pf, p)
	local rex, osz, f2, f3 = pf.rex, pf.osz, pf.f2, pf.f3
	local w = (pf.rexW and 8) or (osz and 2) or 4
	local suf = SUFFIX[w]
	local function R(num)
		if w == 1 then return "%" .. byteRegName(num, rex) end
		return "%" .. gprName(num, w * 8)
	end
	local function R32(num) return "%" .. gprName(num, 32) end
	local function rmFn(width)
		return function(n)
			if width == 1 then return "%" .. byteRegName(n, rex) end
			return "%" .. gprName(n, width * 8)
		end
	end
	-- rm operand at `width` bytes (register or memory), starting at the ModRM byte.
	local function RM(width)
		return decodeModRM(b, p + 1, pf.rexR, pf.rexB, pf.rexX, rmFn(width))
	end
	-- Two-operand r/m <-> reg instruction. dir "mr" = Intel `op r/m, r`
	-- (AT&T lists the register first); dir "rm" = Intel `op r, r/m`.
	local function alu(mnem, dir, width)
		local rmT, np = RM(width)
		if not rmT then return nil end
		local regT
		if width == 1 then
			regT = "%" .. byteRegName(regNumAt(b, p + 1, pf.rexR), rex)
		else
			regT = "%" .. gprName(regNumAt(b, p + 1, pf.rexR), width * 8)
		end
		local m = mnem .. SUFFIX[width]
		if dir == "mr" then return m .. " " .. regT .. "," .. rmT, np - pos
		else return m .. " " .. rmT .. "," .. regT, np - pos end
	end
	-- Immediate readers at byte position q; each returns (value, nextPos) or nil.
	local function imm8u(q)
		if not b[q] then return nil end
		return b[q], q + 1
	end
	local function imm8s(q)
		if not b[q] then return nil end
		return sx8(b[q]), q + 1
	end
	local function imm16(q)
		if not (b[q] and b[q + 1]) then return nil end
		return b[q] + b[q + 1] * 256, q + 2
	end
	local function imm32u(q)
		if not (b[q] and b[q + 1] and b[q + 2] and b[q + 3]) then return nil end
		return b[q] + b[q + 1] * 256 + b[q + 2] * 65536 + b[q + 3] * 16777216, q + 4
	end
	local function imm32s(q)
		local v = imm32u(q)
		if not v then return nil end
		return sx32(v), q + 4
	end
	-- A 64-bit immediate as exact hex text (double arithmetic is inexact past
	-- 2^53; the byte values are exact).
	local function imm64hex(q)
		local parts = {}
		for i = 7, 0, -1 do
			local c = b[q + i]
			if not c then return nil end
			table.insert(parts, string.format("%02x", c))
		end
		return "0x" .. table.concat(parts), q + 8
	end
	-- The width-appropriate Iz immediate for the current operand size (a 64-bit
	-- operand takes an imm32 that the hardware SIGN-extends).
	local function immIz(q)
		if w == 2 then return imm16(q) end
		if w == 8 then return imm32s(q) end
		return imm32u(q)
	end
	-- A string instruction (the AT&T dword spellings are movsl/stosl — the
	-- movsd/stosd spellings collide with the FP instructions).
	local function strInsn(text, hasRep)
		if f2 then return nil end   -- repne movs/stos is not a valid encoding
		if f3 and not hasRep then return nil end
		if rex ~= 0 and not pf.rexW then return nil end   -- REX without W: stray
		if hasRep and f3 then return "rep " .. text end
		return text
	end
	local c = b[p]

	if c <= 0x3D and c % 8 <= 5 then
		-- The eight ALU groups: add/or/adc/sbb/and/sub/xor/cmp, six forms each.
		local mnem = GRP1_SUBOP[math.floor(c / 8)]
		local form = c % 8
		if form == 0 then return alu(mnem, "mr", 1) end
		if form == 1 then return alu(mnem, "mr", w) end
		if form == 2 then return alu(mnem, "rm", 1) end
		if form == 3 then return alu(mnem, "rm", w) end
		local v, q = imm8u(p + 1)
		if not v then return nil end
		if form == 4 then return mnem .. "b $" .. v .. ",%al", q - pos end
		local vv = immIz(p + 1)
		if not vv then return nil end
		return mnem .. suf .. " $" .. vv .. "," .. R(0), p + 1 + (w == 2 and 2 or 4) - pos
	elseif c >= 0x50 and c <= 0x57 then
		-- push r64 (r16 with 0x66); the default operand size is 64 bits
		local pw = osz and 2 or 8
		return (osz and "pushw " or "pushq ") .. "%" .. gprName((c - 0x50) + pf.rexB, pw * 8), p + 1 - pos
	elseif c >= 0x58 and c <= 0x5F then
		local pw = osz and 2 or 8
		return (osz and "popw " or "popq ") .. "%" .. gprName((c - 0x58) + pf.rexB, pw * 8), p + 1 - pos
	elseif c == 0x63 then
		-- movsxd (only the 64-bit-destination form is spelled movslq)
		if not pf.rexW then return nil end
		local rmT, np = RM(4)
		if not rmT then return nil end
		return "movslq " .. rmT .. ",%" .. gprName(regNumAt(b, p + 1, pf.rexR), 64), np - pos
	elseif c == 0x68 or c == 0x6A then
		-- push imm (imm32 sign-extended; imm16 with 0x66; imm8 sign-extended)
		local v, q
		if c == 0x68 then v, q = (osz and imm16 or imm32s)(p + 1)
		else v, q = imm8s(p + 1) end
		if not v then return nil end
		return (osz and "pushw $" or "pushq $") .. v, q - pos
	elseif c == 0x69 or c == 0x6B then
		-- imul r, r/m, imm (the immediate follows the full ModRM/SIB/disp)
		local rmT, np = RM(w)
		if not rmT then return nil end
		local v, q
		if c == 0x69 then v, q = (osz and imm16 or (w == 8 and imm32s or imm32u))(np)
		else v, q = imm8s(np) end
		if not v then return nil end
		return "imul" .. suf .. " $" .. v .. "," .. rmT .. "," .. R(regNumAt(b, p + 1, pf.rexR)), q - pos
	elseif c >= 0x70 and c <= 0x7F then
		local v = b[p + 1]
		if not v then return nil end
		return "j" .. CONDS[c - 0x70], p + 2 - pos, sx8(v)
	elseif c == 0x80 then
		local mnem = GRP1_SUBOP[regNumAt(b, p + 1, 0)]
		if not mnem then return nil end
		local rmT, np = RM(1)
		if not rmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. "b $" .. v .. "," .. rmT, q - pos
	elseif c == 0x81 then
		local mnem = GRP1_SUBOP[regNumAt(b, p + 1, 0)]
		if not mnem then return nil end
		local rmT, np = RM(w)
		if not rmT then return nil end
		local v, q = immIz(np)
		if not v then return nil end
		return mnem .. suf .. " $" .. v .. "," .. rmT, q - pos
	elseif c == 0x83 then
		local mnem = GRP1_SUBOP[regNumAt(b, p + 1, 0)]
		if not mnem then return nil end
		local rmT, np = RM(w)
		if not rmT then return nil end
		local v, q = imm8s(np)
		if not v then return nil end
		return mnem .. suf .. " $" .. v .. "," .. rmT, q - pos
	elseif c == 0x84 or c == 0x85 then
		return alu("test", "mr", (c == 0x84) and 1 or w)
	elseif c == 0x86 or c == 0x87 then
		return alu("xchg", "mr", (c == 0x86) and 1 or w)
	elseif c >= 0x88 and c <= 0x8B then
		local form = c - 0x88
		if form == 0 then return alu("mov", "mr", 1) end
		if form == 1 then return alu("mov", "mr", w) end
		if form == 2 then return alu("mov", "rm", 1) end
		return alu("mov", "rm", w)
	elseif c == 0x8D then
		-- lea: the rm operand MUST be memory
		local modrm = b[p + 1]
		if not modrm or math.floor(modrm / 64) % 4 == 3 then return nil end
		local rmT, np = RM(w)
		if not rmT then return nil end
		return "lea" .. suf .. " " .. rmT .. ",%" .. gprName(regNumAt(b, p + 1, pf.rexR), w * 8), np - pos
	elseif c == 0x8F then
		-- pop r/m (only the /0 form; /1-7 are the XOP prefixes). The default
		-- operand size is 64 bits (0x66 selects 16).
		if regNumAt(b, p + 1, 0) ~= 0 then return nil end
		local pw = osz and 2 or 8
		local rmT, np = RM(pw)
		if not rmT then return nil end
		return "pop" .. SUFFIX[pw] .. " " .. rmT, np - pos
	elseif c == 0x90 then
		-- nop / pause (f3) / xchg eAX,r with REX
		if f3 then
			if rex ~= 0 or osz then return nil end
			return "rep nop", p + 1 - pos
		end
		if rex == 0 then return "nop", p + 1 - pos end
		-- 48 90 = xchg rax,rax (REX.W) / 40+r 90 = xchg eax,r (register form)
		local rw = (pf.rexW and 8) or 4
		local rs = SUFFIX[rw]
		return "xchg" .. rs .. " %" .. gprName(0, rw * 8) .. ",%"
			.. gprName(rex % 2, rw * 8), p + 1 - pos
	elseif c >= 0x91 and c <= 0x97 then
		local rw = (pf.rexW and 8) or (osz and 2) or 4
		return "xchg" .. SUFFIX[rw] .. " %" .. gprName(0, rw * 8) .. ",%"
			.. gprName((c - 0x90) + pf.rexB, rw * 8), p + 1 - pos
	elseif c == 0x98 or c == 0x99 then
		if rex == 0 and not f2 and not f3 then
			if c == 0x98 then return (osz and "cbtw" or "cwtl"), p + 1 - pos end
			return (osz and "cwd" or "cltd"), p + 1 - pos
		end
		if pf.rexW and not osz and not f2 and not f3 then
			if c == 0x98 then return "cdqe", p + 1 - pos end
			return "cqo", p + 1 - pos
		end
		return nil
	elseif c == 0x9E or c == 0x9F then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return (c == 0x9E) and "sahf" or "lahf", p + 1 - pos
	elseif c == 0xA4 or c == 0xA5 then
		local mnem = (c == 0xA4) and "movsb" or (osz and "movsw" or (pf.rexW and "movsq" or "movsl"))
		return strInsn(mnem, true), p + 1 - pos
	elseif c == 0xA8 then
		local v, q = imm8u(p + 1)
		if not v then return nil end
		return "testb $" .. v .. ",%al", q - pos
	elseif c == 0xA9 then
		local v, q = immIz(p + 1)
		if not v then return nil end
		return "test" .. suf .. " $" .. v .. "," .. R(0), q - pos
	elseif c == 0xAA or c == 0xAB then
		local mnem = (c == 0xAA) and "stosb" or (osz and "stosw" or (pf.rexW and "stosq" or "stosl"))
		return strInsn(mnem, true), p + 1 - pos
	elseif c >= 0xB0 and c <= 0xB7 then
		local v, q = imm8u(p + 1)
		if not v then return nil end
		return "movb $" .. v .. ",%" .. byteRegName((c - 0xB0) + pf.rexB, rex), q - pos
	elseif c >= 0xB8 and c <= 0xBF then
		local reg = (c - 0xB8) + pf.rexB
		if w == 8 then
			local v, q = imm64hex(p + 1)
			if not v then return nil end
			return "movabsq $" .. v .. ",%" .. gprName(reg, 64), q - pos
		end
		local v, q = immIz(p + 1)
		if not v then return nil end
		return "mov" .. suf .. " $" .. v .. "," .. R(reg), q - pos
	elseif c == 0xC0 or c == 0xC1 then
		local mnem = GRP2_SUBOP[regNumAt(b, p + 1, 0)]
		if not mnem then return nil end
		local width = (c == 0xC0) and 1 or w
		local rmT, np = RM(width)
		if not rmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. SUFFIX[width] .. " $" .. v .. "," .. rmT, q - pos
	elseif c == 0xC3 then
		return "ret", p + 1 - pos
	elseif c == 0xC6 or c == 0xC7 then
		-- mov imm to r/m (only the /0 form; the immediate follows the ModRM)
		if regNumAt(b, p + 1, 0) ~= 0 then return nil end
		local width = (c == 0xC6) and 1 or w
		local rmT, np = RM(width)
		if not rmT then return nil end
		local v, q
		if width == 1 then v, q = imm8u(np) else v, q = immIz(np) end
		if not v then return nil end
		return "mov" .. SUFFIX[width] .. " $" .. v .. "," .. rmT, q - pos
	elseif c == 0xC9 then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return "leave", p + 1 - pos
	elseif c == 0xD0 or c == 0xD1 or c == 0xD2 or c == 0xD3 then
		local mnem = GRP2_SUBOP[regNumAt(b, p + 1, 0)]
		if not mnem then return nil end
		local width = (c == 0xD0 or c == 0xD2) and 1 or w
		local rmT, np = RM(width)
		if not rmT then return nil end
		local count = (c <= 0xD1) and "$1" or "%cl"
		return mnem .. SUFFIX[width] .. " " .. count .. "," .. rmT, np - pos
	elseif c == 0xE2 or c == 0xE3 then
		-- loop / jrcxz (jecxz with 0x66) rel8 branches
		local v = b[p + 1]
		if not v then return nil end
		if c == 0xE2 then
			if f2 or f3 or rex ~= 0 then return nil end
			return (osz and "loopw" or "loop"), p + 2 - pos, sx8(v)
		end
		if f2 or f3 or rex ~= 0 then return nil end
		return (osz and "jecxz" or "jrcxz"), p + 2 - pos, sx8(v)
	elseif c == 0xE9 then
		local v = imm32s(p + 1)
		if not v then return nil end
		return "jmp", p + 5 - pos, v
	elseif c == 0xEB then
		local v = b[p + 1]
		if not v then return nil end
		return "jmp", p + 2 - pos, sx8(v)
	elseif c == 0xCC then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return "int3", p + 1 - pos
	elseif c == 0xF5 then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return "cmc", p + 1 - pos
	elseif c == 0xF6 or c == 0xF7 then
		local width = (c == 0xF6) and 1 or w
		local fs = SUFFIX[width]
		local subop = regNumAt(b, p + 1, 0)
		local rmT, np = RM(width)
		if not rmT then return nil end
		if subop <= 1 then
			-- test imm (the immediate follows the full ModRM/SIB/disp)
			local v, q
			if width == 1 then v, q = imm8u(np)
			elseif w == 2 then v, q = imm16(np)
			elseif w == 8 then v, q = imm32s(np)
			else v, q = imm32u(np) end
			if not v then return nil end
			return "test" .. fs .. " $" .. v .. "," .. rmT, q - pos
		end
		if subop == 2 then return "not" .. fs .. " " .. rmT, np - pos end
		if subop == 3 then return "neg" .. fs .. " " .. rmT, np - pos end
		if subop == 4 then return "mul" .. fs .. " " .. rmT, np - pos end
		if subop == 5 then return "imul" .. fs .. " " .. rmT, np - pos end
		if subop == 6 then return "div" .. fs .. " " .. rmT, np - pos end
		if subop == 7 then return "idiv" .. fs .. " " .. rmT, np - pos end
		return nil
	elseif c == 0xF8 or c == 0xF9 then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return (c == 0xF8) and "clc" or "stc", p + 1 - pos
	elseif c == 0xFC or c == 0xFD then
		if rex ~= 0 or f2 or f3 then return nil end
		return (c == 0xFC) and "cld" or "std", p + 1 - pos
	elseif c == 0xFE then
		local subop = regNumAt(b, p + 1, 0)
		if subop > 1 then return nil end
		local rmT, np = RM(1)
		if not rmT then return nil end
		return ((subop == 0) and "incb " or "decb ") .. rmT, np - pos
	elseif c == 0xFF then
		local subop = regNumAt(b, p + 1, pf.rexR)
		local rmT, np = RM(w)
		if not rmT then return nil end
		-- /2 call, /3, /4 jmp, /5 far forms and /7 are not modeled (call/jmp
		-- targets cannot be expressed; leave them data)
		if subop == 0 then return "inc" .. suf .. " " .. rmT, np - pos end
		if subop == 1 then return "dec" .. suf .. " " .. rmT, np - pos end
		if subop == 6 then return (osz and "pushw " or "pushq ") .. rmT, np - pos end
		return nil
	end
	return nil
end

-- SSE two-operand opcode table: opcode -> mnemonic by prefix (np/66/F2/F3) and
-- operand direction. dir 1 = AT&T lists the rm operand first (loads and every
-- two-operand ALU form: `addps (%rax),%xmm0`); dir 2 = the register is the
-- SOURCE and r/m the destination (the mov* store forms).
-- A nil mnemonic means the prefix/op combination does not exist (NP forms of
-- the SSE2 integer map are MMX and are not decoded).
local SSE2OP = {
	[0x10] = { np = "movups", p66 = "movupd", f2 = "movsd", f3 = "movss", dir = 1 },
	[0x11] = { np = "movups", p66 = "movupd", f2 = "movsd", f3 = "movss", dir = 2 },
	[0x12] = { np = "movlps", p66 = "movlpd", f2 = "movddup", f3 = "movsldup", dir = 1 },
	[0x13] = { np = "movlps", p66 = "movlpd", dir = 2 },
	[0x14] = { np = "unpcklps", p66 = "unpcklpd", dir = 1 },
	[0x15] = { np = "unpckhps", p66 = "unpckhpd", dir = 1 },
	[0x16] = { np = "movhps", p66 = "movhpd", f3 = "movshdup", dir = 1 },
	[0x17] = { np = "movhps", p66 = "movhpd", dir = 2 },
	[0x28] = { np = "movaps", p66 = "movapd", dir = 1 },
	[0x29] = { np = "movaps", p66 = "movapd", dir = 2 },
	[0x2A] = { f2 = "cvtsi2sd", f3 = "cvtsi2ss", dir = 1, gprRm = true },
	[0x2B] = { np = "movntps", p66 = "movntpd", dir = 2 },
	[0x2C] = { f2 = "cvttsd2si", f3 = "cvttss2si", dir = 3 },
	[0x2D] = { f2 = "cvtsd2si", f3 = "cvtss2si", dir = 3 },
	[0x2E] = { np = "ucomiss", p66 = "ucomisd", dir = 1 },
	[0x2F] = { np = "comiss", p66 = "comisd", dir = 1 },
	[0x51] = { np = "sqrtps", p66 = "sqrtpd", f2 = "sqrtsd", f3 = "sqrtss", dir = 1 },
	[0x52] = { np = "rsqrtps", f3 = "rsqrtss", dir = 1 },
	[0x53] = { np = "rcpps", f3 = "rcpss", dir = 1 },
	[0x54] = { np = "andps", p66 = "andpd", dir = 1 },
	[0x55] = { np = "andnps", p66 = "andnpd", dir = 1 },
	[0x56] = { np = "orps", p66 = "orpd", dir = 1 },
	[0x57] = { np = "xorps", p66 = "xorpd", dir = 1 },
	[0x58] = { np = "addps", p66 = "addpd", f2 = "addsd", f3 = "addss", dir = 1 },
	[0x59] = { np = "mulps", p66 = "mulpd", f2 = "mulsd", f3 = "mulss", dir = 1 },
	[0x5A] = { np = "cvtps2pd", p66 = "cvtpd2ps", f2 = "cvtsd2ss", f3 = "cvtss2sd", dir = 1 },
	[0x5B] = { np = "cvtdq2ps", p66 = "cvtps2dq", f3 = "cvttps2dq", dir = 1 },
	[0x5C] = { np = "subps", p66 = "subpd", f2 = "subsd", f3 = "subss", dir = 1 },
	[0x5D] = { np = "minps", p66 = "minpd", f2 = "minsd", f3 = "minss", dir = 1 },
	[0x5E] = { np = "divps", p66 = "divpd", f2 = "divsd", f3 = "divss", dir = 1 },
	[0x5F] = { np = "maxps", p66 = "maxpd", f2 = "maxsd", f3 = "maxss", dir = 1 },
	[0xC2] = { np = "cmpps", p66 = "cmppd", f2 = "cmpsd", f3 = "cmpss", dir = 1, imm = true },
	[0xC6] = { np = "shufps", p66 = "shufpd", dir = 1, imm = true },
}

-- Packed-integer SSE (SSE2/SSSE3/SSE4.1) two-operand forms: the 66 prefix
-- selects the xmm form, the no-prefix form is MMX (not decoded).
local PSSE2OP = {
	[0x60] = "punpcklbw", [0x61] = "punpcklwd", [0x62] = "punpckldq",
	[0x63] = "packsswb", [0x64] = "pcmpgtb", [0x65] = "pcmpgtw",
	[0x66] = "pcmpgtd", [0x67] = "packuswb", [0x68] = "punpckhbw",
	[0x69] = "punpckhwd", [0x6A] = "punpckhdq", [0x6B] = "packssdw",
	[0x6C] = "punpcklqdq", [0x6D] = "punpckhqdq",
	[0x74] = "pcmpeqb", [0x75] = "pcmpeqw", [0x76] = "pcmpeqd",
	[0xD1] = "psrlw", [0xD2] = "psrld", [0xD3] = "psrlq",
	[0xD4] = "paddq", [0xD5] = "pmullw",
	[0xD8] = "psubusb", [0xD9] = "psubusw", [0xDA] = "pminub", [0xDB] = "pand",
	[0xDC] = "paddusb", [0xDD] = "paddusw", [0xDE] = "pmaxub", [0xDF] = "pandn",
	[0xE0] = "pavgb", [0xE1] = "psraw", [0xE2] = "psrad", [0xE3] = "pavgw",
	[0xE4] = "pmulhuw", [0xE5] = "pmulhw",
	[0xE8] = "psubsb", [0xE9] = "psubsw", [0xEA] = "pminsw",
	[0xEB] = "por", [0xEC] = "paddsb", [0xED] = "paddsw",
	[0xEE] = "pmaxsw", [0xEF] = "pxor",
	[0xF1] = "psllw", [0xF2] = "pslld", [0xF3] = "psllq",
	[0xF4] = "pmuludq", [0xF5] = "pmaddwd", [0xF6] = "psadbw",
	[0xF8] = "psubb", [0xF9] = "psubw", [0xFA] = "psubd", [0xFB] = "psubq",
	[0xFC] = "paddb", [0xFD] = "paddw", [0xFE] = "paddd",
}

-- SSSE3/SSE4.1 packed-integer forms with an xmm-or-mem second operand
-- (0F 38 xx, 66 prefix; the no-66 forms are MMX).
local PSSE38OP = {
	[0x00] = "pshufb", [0x01] = "phaddw", [0x02] = "phaddd",
	[0x03] = "phaddsw", [0x04] = "pmaddubsw", [0x05] = "phsubw",
	[0x06] = "phsubd", [0x07] = "phsubsw", [0x08] = "psignb",
	[0x09] = "psignw", [0x0A] = "psignd", [0x0B] = "pmulhrsw",
	[0x1C] = "pabsb", [0x1D] = "pabsw", [0x1E] = "pabsd",
	[0x28] = "pmuldq", [0x2B] = "packusdw", [0x37] = "pcmpgtq",
	[0x40] = "pmulld", [0x41] = "phminposuw",
}

-- Decode one 0F-map instruction. b[p] is 0x0F, b[p+1] the second opcode byte.
-- (decode0F38/decode0F3A are defined below decode0F and forward-declared here)
local decode0F38, decode0F3A
local function decode0F(b, pos, pf, p)
	local rex, osz, f2, f3 = pf.rex, pf.osz, pf.f2, pf.f3
	local w = (pf.rexW and 8) or (osz and 2) or 4
	local suf = SUFFIX[w]
	local c = b[p + 1]
	if c == nil then return nil end
	local function R(num)
		if w == 1 then return "%" .. byteRegName(num, rex) end
		return "%" .. gprName(num, w * 8)
	end
	local function rmFn(width)
		return function(n)
			if width == 1 then return "%" .. byteRegName(n, rex) end
			return "%" .. gprName(n, width * 8)
		end
	end
	local function XM()
		return decodeModRM(b, p + 2, pf.rexR, pf.rexB, pf.rexX, function(n) return "%xmm" .. n end)
	end
	local function RM(width)
		return decodeModRM(b, p + 2, pf.rexR, pf.rexB, pf.rexX, rmFn(width))
	end
	local function imm8u(q)
		if not b[q] then return nil end
		return b[q], q + 1
	end
	local function imm32s(q)
		if not (b[q] and b[q + 1] and b[q + 2] and b[q + 3]) then return nil end
		local v = b[q] + b[q + 1] * 256 + b[q + 2] * 65536 + b[q + 3] * 16777216
		return sx32(v), q + 4
	end
	local function R32(num) return "%" .. gprName(num, 32) end
	local function xmmRegT()
		return "%xmm" .. regNumAt(b, p + 2, pf.rexR)
	end
	local function gprRegT(width)
		if width == 1 then
			return "%" .. byteRegName(regNumAt(b, p + 2, pf.rexR), rex)
		end
		return "%" .. gprName(regNumAt(b, p + 2, pf.rexR), width * 8)
	end
	-- A 2-op SSE instruction with an optional leading imm8 (cmpps/pshufd shapes).
	local function sse2(mnem, dir, withImm)
		local xmT, np
		if dir == 3 then
			-- cvtt[sd]2si: GPR destination (the register operand is last); the
			-- AT&T spelling takes a dest-width suffix (cvttsd2sil/cvttsd2siq)
			xmT, np = XM()
			if not xmT then return nil end
			return mnem .. ((w == 8) and "q" or "l") .. " " .. xmT .. ","
				.. gprRegT(w), np - pos
		end
		xmT, np = XM()
		if not xmT then return nil end
		local regT = xmmRegT()
		if withImm then
			local v, q = imm8u(np)
			if not v then return nil end
			np = q
			if dir == 1 then return mnem .. " $" .. v .. "," .. xmT .. "," .. regT, np - pos
			else return mnem .. " $" .. v .. "," .. regT .. "," .. xmT, np - pos end
		end
		if dir == 2 then return mnem .. " " .. regT .. "," .. xmT, np - pos end
		return mnem .. " " .. xmT .. "," .. regT, np - pos
	end
	local function psse2(mnem, withImm)
		-- packed-integer form: requires the 66 prefix (the NP form is MMX, and
		-- the F2/F3 prefixes have no packed-integer meaning on these opcodes)
		if not osz then return nil end
		if f2 or f3 then return nil end
		local xmT, np = XM()
		if not xmT then return nil end
		local regT = xmmRegT()
		if withImm then
			local v, q = imm8u(np)
			if not v then return nil end
			np = q
			return mnem .. " $" .. v .. "," .. xmT .. "," .. regT, np - pos
		end
		return mnem .. " " .. xmT .. "," .. regT, np - pos
	end

	if c == 0x38 then return decode0F38(b, pos, pf, p) end
	if c == 0x3A then return decode0F3A(b, pos, pf, p) end

	-- no-operand instructions
	if c == 0x05 then return "syscall", p + 2 - pos end
	if c == 0x07 then return "sysret", p + 2 - pos end
	if c == 0x0B then return "ud2", p + 2 - pos end
	if c == 0x30 then return "wrmsr", p + 2 - pos end
	if c == 0x31 then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return "rdtsc", p + 2 - pos
	end
	if c == 0x32 then return "rdmsr", p + 2 - pos end
	if c == 0x33 then return "rdpmc", p + 2 - pos end
	if c == 0x34 then return "sysenter", p + 2 - pos end
	if c == 0x35 then return "sysexit", p + 2 - pos end
	if c == 0xA2 then
		if rex ~= 0 or osz or f2 or f3 then return nil end
		return "cpuid", p + 2 - pos end
	if c == 0x00 or c == 0x01 then
		-- 0F 00: sldt/str/verr/verw; 0F 01: sgdt/sidt/smsw and the register-
		-- encoded system instructions. lgdt/lidt/lldt/ltr/lmsw/invlpg stay data
		-- (privileged/unsupported).
		local modrm = b[p + 2]
		if not modrm then return nil end
		if math.floor(modrm / 64) % 4 == 3 then
			-- register-encoded forms (0F 01 only; no prefixes on any of them)
			if c == 0x01 and rex == 0 and not osz and not f2 and not f3 then
				local map = { [0xC8] = "monitor", [0xC9] = "mwait",
					[0xD0] = "xgetbv", [0xD1] = "xsetbv",
					[0xEE] = "rdpkru", [0xEF] = "wrpkru",
					[0xF8] = "swapgs", [0xF9] = "rdtscp" }
				local t = map[modrm]
				if t then return t, p + 3 - pos end
			end
			-- 0F 00 register forms: sldt/str (and lldt/ltr, which stay data)
			if c == 0x00 and rex == 0 and not osz and not f2 and not f3 then
				local rm = modrm % 8
				if rm == 0 then return "sldt %" .. gprName((modrm % 8), 16), p + 3 - pos end
				if rm == 1 then return "str %" .. gprName((modrm % 8), 16), p + 3 - pos end
			end
			return nil
		end
		local subop = regNumAt(b, p + 2, 0)
		local width = (osz and 2) or 4
		local mnem = nil
		if c == 0x01 then
			if subop == 0 then mnem, width = "sgdt", 8
			elseif subop == 1 then mnem, width = "sidt", 8
			elseif subop == 4 then mnem = "smsw" end
		else
			if subop == 0 then mnem = "sldt"
			elseif subop == 1 then mnem = "str"
			elseif subop == 4 then mnem = "verr"
			elseif subop == 5 then mnem = "verw" end
		end
		if not mnem then return nil end
		local rmT, np = RM(width)
		if not rmT then return nil end
		return mnem .. " " .. rmT, np - pos
	end

	-- hint nops and prefetches (0F 18-1F)
	if c == 0x0D then
		-- 0F 0D /1 = prefetchw (AMD); other /subops stay data
		if regNumAt(b, p + 2, 0) ~= 1 then return nil end
		local rmT, np = XM()
		if not rmT then return nil end
		return "prefetchw " .. rmT, np - pos
	end

	if c >= 0x18 and c <= 0x1F then
		if c == 0x1F then
			if regNumAt(b, p + 2, 0) ~= 0 then return nil end
			local rmT, np = XM()
			if not rmT then return nil end
			return "nopl " .. rmT, np - pos
		end
		if c == 0x18 then
			local subop = regNumAt(b, p + 2, 0)
			local mnems = { [0] = "prefetchnta", [1] = "prefetcht0", [2] = "prefetcht1", [3] = "prefetcht2" }
			if mnems[subop] then
				local rmT, np = XM()
				if not rmT then return nil end
				return mnems[subop] .. " " .. rmT, np - pos
			end
			return nil
		end
		-- 0F 19-1E are hint nops with any ModRM form
		local _, np = XM()
		if not np then return nil end
		return "nop", np - pos
	end

	-- cmovcc (0F 40-4F)
	if c >= 0x40 and c <= 0x4F then
		local rmT, np = RM(w)
		if not rmT then return nil end
		return "cmov" .. CONDS[c - 0x40] .. suf .. " " .. rmT .. "," .. R(regNumAt(b, p + 2, pf.rexR)), np - pos
	end

	-- jcc near (0F 80-8F)
	if c >= 0x80 and c <= 0x8F then
		local v = imm32s(p + 2)
		if not v then return nil end
		return "j" .. CONDS[c - 0x80], p + 6 - pos, v
	end

	-- setcc (0F 90-9F)
	if c >= 0x90 and c <= 0x9F then
		local rmT, np = RM(1)
		if not rmT then return nil end
		return "set" .. CONDS[c - 0x90] .. " " .. rmT, np - pos
	end

	-- bt/bts/btr/btc r/m, r (0F A3/AB/B3/BB)
	if c == 0xA3 or c == 0xAB or c == 0xB3 or c == 0xBB then
		local mnem = ({ [0xA3] = "bt", [0xAB] = "bts", [0xB3] = "btr", [0xBB] = "btc" })[c]
		local rmT, np = RM(w)
		if not rmT then return nil end
		return mnem .. suf .. " " .. R(regNumAt(b, p + 2, pf.rexR)) .. "," .. rmT, np - pos
	end

	-- shld/shrd (0F A4/A5/AC/AD): count, src reg, dst r/m
	if c == 0xA4 or c == 0xA5 or c == 0xAC or c == 0xAD then
		local mnem = (c == 0xA4 or c == 0xA5) and "shld" or "shrd"
		local rmT, np = RM(w)
		if not rmT then return nil end
		local srcT = "%" .. gprName(regNumAt(b, p + 2, pf.rexR), w * 8)
		local count = (c == 0xA4 or c == 0xAC)
			and ("$" .. (b[np] or "MISSING"))
			or "%cl"
		if c == 0xA4 or c == 0xAC then
			if not b[np] then return nil end
			np += 1
		end
		return mnem .. suf .. " " .. count .. "," .. srcT .. "," .. rmT, np - pos
	end

	-- imul r, r/m (0F AF)
	if c == 0xAF then
		local rmT, np = RM(w)
		if not rmT then return nil end
		return "imul" .. suf .. " " .. rmT .. "," .. R(regNumAt(b, p + 2, pf.rexR)), np - pos
	end

	-- cmpxchg r/m, r (0F B0/B1)
	if c == 0xB0 or c == 0xB1 then
		local width = (c == 0xB0) and 1 or w
		local rmT, np = RM(width)
		if not rmT then return nil end
		local regT
		if width == 1 then
			regT = "%" .. byteRegName(regNumAt(b, p + 2, pf.rexR), rex)
		else
			regT = "%" .. gprName(regNumAt(b, p + 2, pf.rexR), width * 8)
		end
		return "cmpxchg" .. SUFFIX[width] .. " " .. regT .. "," .. rmT, np - pos
	end

	-- movzx/movsx (0F B6/B7/BE/BF): the AT&T mnemonic encodes both widths
	if c == 0xB6 or c == 0xB7 or c == 0xBE or c == 0xBF then
		local mnems
		if c == 0xB6 then
			mnems = { [2] = "movzbw", [4] = "movzbl", [8] = "movzbq" }
		elseif c == 0xB7 then
			mnems = { [4] = "movzwl", [8] = "movzwq" }
		elseif c == 0xBE then
			mnems = { [2] = "movsbw", [4] = "movsbl", [8] = "movsbq" }
		else
			mnems = { [4] = "movswl", [8] = "movswq" }
		end
		local mnem = mnems[w]
		if not mnem then return nil end
		local srcW = (c == 0xB6 or c == 0xBE) and 1 or 2
		local rmT, np = RM(srcW)
		if not rmT then return nil end
		return mnem .. " " .. rmT .. ",%" .. gprName(regNumAt(b, p + 2, pf.rexR), w * 8), np - pos
	end

	-- popcnt (F3 0F B8), bsf/bsr (0F BC/BD): src r/m, dst reg
	if c == 0xB8 or c == 0xBC or c == 0xBD then
		-- popcnt REQUIRES the F3 prefix; bsf/bsr take none
		if c == 0xB8 and (not f3 or f2) then return nil end
		if c ~= 0xB8 and (f2 or f3) then return nil end
		local mnem = (c == 0xB8 and "popcnt") or (c == 0xBC and "bsf" or "bsr")
		local rmT, np = RM(w)
		if not rmT then return nil end
		return mnem .. suf .. " " .. rmT .. "," .. R(regNumAt(b, p + 2, pf.rexR)), np - pos
	end

	-- bt/bts/btr/btc r/m, imm8 (0F BA /4-/7)
	if c == 0xBA then
		local subop = regNumAt(b, p + 2, 0)
		local mnem = ({ [4] = "bt", [5] = "bts", [6] = "btr", [7] = "btc" })[subop]
		if not mnem then return nil end
		local rmT, np = RM(w)
		if not rmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. suf .. " $" .. v .. "," .. rmT, q - pos
	end

	-- xadd r/m, r (0F C0/C1)
	if c == 0xC0 or c == 0xC1 then
		local width = (c == 0xC0) and 1 or w
		local rmT, np = RM(width)
		if not rmT then return nil end
		local regT
		if width == 1 then
			regT = "%" .. byteRegName(regNumAt(b, p + 2, pf.rexR), rex)
		else
			regT = "%" .. gprName(regNumAt(b, p + 2, pf.rexR), width * 8)
		end
		return "xadd" .. SUFFIX[width] .. " " .. regT .. "," .. rmT, np - pos
	end

	-- cmpxchg8b/16b, rdrand, rdseed (0F C7)
	if c == 0xC7 then
		local subop = regNumAt(b, p + 2, 0)
		if subop == 1 then
			local rmT, np = RM(8)
			if not rmT then return nil end
			if pf.rexW then return "cmpxchg16b " .. rmT, np - pos end
			if not osz then return "cmpxchg8b " .. rmT, np - pos end
			return nil
		end
		if subop == 6 or subop == 7 then
			-- rdrand/rdseed: the destination is the rm field (a register); the
			-- AT&T mnemonic takes NO size suffix (the register gives the width)
			local modrm = b[p + 2]
			if not (modrm and math.floor(modrm / 64) % 4 == 3) then return nil end
			local dw = (pf.rexW and 8) or (osz and 2) or 4
			local dest = (dw == 1) and ("%" .. byteRegName((modrm % 8) + pf.rexB, rex))
				or ("%" .. gprName((modrm % 8) + pf.rexB, dw * 8))
			return ((subop == 6) and "rdrand" or "rdseed") .. " " .. dest, p + 3 - pos
		end
		return nil
	end

	-- bswap (0F C8+r)
	if c >= 0xC8 and c <= 0xCF then
		if osz then return nil end
		local rw = (pf.rexW and 8) or 4
		return "bswap" .. SUFFIX[rw] .. " %" .. gprName((c - 0xC8) + pf.rexB, rw * 8), p + 2 - pos
	end

	-- movd/movq GPR<->xmm (66 0F 6E/7E, F3 0F 7E, 66 0F D6)
	if c == 0x6E and osz then
		local rmT, np = RM(4)
		if not rmT then return nil end
		return "movd " .. rmT .. "," .. xmmRegT(), np - pos
	end
	if c == 0x7E and osz and not f3 then
		local rmT, np = RM(4)
		if not rmT then return nil end
		return "movd " .. xmmRegT() .. "," .. rmT, np - pos
	end
	if c == 0x7E and f3 then
		local modrm = b[p + 2]
		if modrm and math.floor(modrm / 64) % 4 == 3 then
			-- movq xmm, xmm (the register form of the F3 0F 7E load)
			return "movq %xmm" .. ((modrm % 8) + pf.rexB) .. ","
				.. xmmRegT(), p + 3 - pos
		end
		local rmT, np = RM(8)
		if not rmT then return nil end
		return "movq " .. rmT .. "," .. xmmRegT(), np - pos
	end
	if c == 0xD6 and osz then
		local modrm = b[p + 2]
		if modrm and math.floor(modrm / 64) % 4 == 3 then
			-- 66 0F D6 with a register rm is a movq xmm, xmm (gas re-encodes it
			-- with the F3 0F 7E form — same semantics, canonical spelling)
			return "movq " .. xmmRegT() .. ",%xmm" .. ((modrm % 8) + pf.rexB), p + 3 - pos
		end
		local rmT, np = RM(8)
		if not rmT then return nil end
		return "movq " .. xmmRegT() .. "," .. rmT, np - pos
	end

	-- movdqa (66) / movdqu (F3) loads (0F 6F) and stores (0F 7F)
	if c == 0x6F or c == 0x7F then
		if f2 or rex ~= 0 then return nil end
		if not (osz or f3) then return nil end
		local mnem = f3 and "movdqu" or "movdqa"
		local xmT, np = XM()
		if not xmT then return nil end
		if c == 0x6F then return mnem .. " " .. xmT .. "," .. xmmRegT(), np - pos end
		return mnem .. " " .. xmmRegT() .. "," .. xmT, np - pos
	end

	-- pshufd (66) / pshuflw (F2) / pshufhw (F3) (0F 70)
	if c == 0x70 then
		if not (osz or f2 or f3) then return nil end
		local mnem = f2 and "pshuflw" or (f3 and "pshufhw" or "pshufd")
		local xmT, np = XM()
		if not xmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. " $" .. v .. "," .. xmT .. "," .. xmmRegT(), q - pos
	end

	-- shift-by-immediate groups (0F 71/72/73, 66 prefix)
	if c == 0x71 or c == 0x72 or c == 0x73 then
		if not osz or f2 or f3 then return nil end
		local subop = regNumAt(b, p + 2, 0)
		local mnems = (c == 0x71)
			and ({ [2] = "psrlw", [4] = "psraw", [6] = "psllw" })
			or ((c == 0x72) and ({ [2] = "psrld", [4] = "psrad", [6] = "pslld" })
			or ({ [2] = "psrlq", [3] = "psrldq", [6] = "psllq", [7] = "pslldq" }))
		local mnem = mnems[subop]
		if not mnem then return nil end
		local xmT, np = XM()
		if not xmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. " $" .. v .. "," .. xmT, q - pos
	end

	-- movmskps/pd (0F 50), pmovmskb (0F D7): dst GPR32, src xmm
	if c == 0x50 or c == 0xD7 then
		if c == 0x50 and (f2 or f3) then return nil end
		if c == 0xD7 and (not osz or f2 or f3) then return nil end
		-- the GPR destination is the reg field and the xmm source the rm field
		-- (a register-only instruction)
		local modrm = b[p + 2]
		if not (modrm and math.floor(modrm / 64) % 4 == 3) then return nil end
		return ((c == 0x50) and (osz and "movmskpd" or "movmskps") or "pmovmskb")
			.. " %xmm" .. ((modrm % 8) + pf.rexB) .. ","
			.. R32(regNumAt(b, p + 2, pf.rexR)), p + 3 - pos
	end

	-- pinsrw (0F C4) / pextrw (0F C5)
	if c == 0xC4 or c == 0xC5 then
		if not osz or f2 or f3 then return nil end
		local opT, np
		if c == 0xC4 then
			opT, np = RM(4)
			if not opT then return nil end
		else
			opT, np = XM()
			if not opT then return nil end
		end
		local v, q = imm8u(np)
		if not v then return nil end
		if c == 0xC4 then
			return "pinsrw $" .. v .. "," .. opT .. "," .. xmmRegT(), q - pos
		end
		return "pextrw $" .. v .. "," .. opT .. "," .. R32(regNumAt(b, p + 2, pf.rexR)), q - pos
	end

	-- the FP / packed two-op maps
	local e2 = SSE2OP[c]
	if e2 then
		local mnem = (f2 and e2.f2) or (f3 and e2.f3) or (osz and e2.p66) or e2.np
		if not mnem then return nil end
		if e2.gprRm then
			-- cvtsi2ss/sd: the rm operand is a GPR32 or memory (the REX.W/0x66
			-- prefixes select 64/16-bit sources); the AT&T spelling carries the
			-- source-width suffix so a memory source re-encodes exactly
			local rmT, np = RM(w)
			if not rmT then return nil end
			return mnem .. (e2.gprSuf and suf or "") .. " " .. rmT .. "," .. xmmRegT(), np - pos
		end
		return sse2(mnem, e2.dir, e2.imm)
	end
	local pn = PSSE2OP[c]
	if pn then return psse2(pn, false) end

	-- cvttpd2dq / cvtpd2dq / cvtdq2pd (0F E6) and movntdq (0F E7, store only)
	if c == 0xE6 then
		local mnem = (f2 and "cvtpd2dq") or (f3 and "cvtdq2pd") or (osz and "cvttpd2dq") or nil
		if not mnem then return nil end
		local xmT, np = XM()
		if not xmT then return nil end
		return mnem .. " " .. xmT .. "," .. xmmRegT(), np - pos
	end
	if c == 0xE7 then
		if not osz or f2 or f3 then return nil end
		local xmT, np = XM()
		if not xmT then return nil end
		return "movntdq " .. xmmRegT() .. "," .. xmT, np - pos
	end
	return nil
end

-- Decode one 0F 38-map instruction (three-byte opcodes).
decode0F38 = function(b, pos, pf, p)
	local rex, osz, f2, f3 = pf.rex, pf.osz, pf.f2, pf.f3
	local w = (pf.rexW and 8) or (osz and 2) or 4
	local suf = SUFFIX[w]
	local c = b[p + 2]
	if c == nil then return nil end
	local function rmFn(width)
		return function(n)
			if width == 1 then return "%" .. byteRegName(n, rex) end
			return "%" .. gprName(n, width * 8)
		end
	end
	local function XM()
		return decodeModRM(b, p + 3, pf.rexR, pf.rexB, pf.rexX, function(n) return "%xmm" .. n end)
	end
	local function RM(width)
		return decodeModRM(b, p + 3, pf.rexR, pf.rexB, pf.rexX, rmFn(width))
	end
	local function xmmRegT()
		return "%xmm" .. regNumAt(b, p + 3, pf.rexR)
	end

	-- SHA-NI (NP 0F 38 C8-CD)
	local SHA = { [0xC8] = "sha1nexte", [0xC9] = "sha1msg1", [0xCA] = "sha1msg2",
		[0xCB] = "sha256rnds2", [0xCC] = "sha256msg1", [0xCD] = "sha256msg2" }
	if SHA[c] and not osz and not f2 and not f3 then
		local rmT, np = XM()
		if not rmT then return nil end
		return SHA[c] .. " " .. rmT .. "," .. xmmRegT(), np - pos
	end

	-- AES-NI (66 0F 38 DB-DF)
	local AES = { [0xDB] = "aesimc", [0xDC] = "aesenc", [0xDD] = "aesenclast",
		[0xDE] = "aesdec", [0xDF] = "aesdeclast" }
	if AES[c] and osz and not f2 and not f3 then
		local rmT, np = XM()
		if not rmT then return nil end
		return AES[c] .. " " .. rmT .. "," .. xmmRegT(), np - pos
	end

	-- movbe (0F 38 F0 load / F1 store) and crc32 (F2 0F 38 F0/F1)
	if c == 0xF0 or c == 0xF1 then
		local rmT, np
		if f2 then
			-- crc32: dst GPR (RMW), src r/m — AT&T lists the r/m first. The
			-- accumulator is always 32-bit (64-bit with REX.W); the r/m source
			-- width selects the mnemonic (crc32b/w/l/q)
			local mnem = (c == 0xF0) and "crc32b" or (osz and "crc32w" or (pf.rexW and "crc32q" or "crc32l"))
			local width = (c == 0xF0) and 1 or w
			rmT, np = RM(width)
			if not rmT then return nil end
			local dw = (pf.rexW and 8) or 4
			return mnem .. " " .. rmT .. ",%" .. gprName(regNumAt(b, p + 3, pf.rexR), dw * 8), np - pos
		end
		if f3 then return nil end
		local mnem = (pf.rexW and "movbeq") or (osz and "movbew") or "movbel"
		local width = w
		rmT, np = RM(width)
		if not rmT then return nil end
		local regT = "%" .. gprName(regNumAt(b, p + 3, pf.rexR), width * 8)
		if c == 0xF0 then return mnem .. " " .. rmT .. "," .. regT, np - pos end
		return mnem .. " " .. regT .. "," .. rmT, np - pos
	end

	-- packed-integer SSSE3/SSE4.1 (66 only)
	if osz and not f2 and not f3 then
		local mnem = PSSE38OP[c]
		if mnem then
			local rmT, np = XM()
			if not rmT then return nil end
			return mnem .. " " .. rmT .. "," .. xmmRegT(), np - pos
		end
		-- ptest (0F 38 17)
		if c == 0x17 then
			local rmT, np = XM()
			if not rmT then return nil end
			return "ptest " .. rmT .. "," .. xmmRegT(), np - pos
		end
		-- movntdqa (0F 38 2A, load only)
		if c == 0x2A then
			local rmT, np = XM()
			if not rmT then return nil end
			return "movntdqa " .. rmT .. "," .. xmmRegT(), np - pos
		end
		-- the pmovsx*/pmovzx* families (0F 38 20-25/30-35)
		local widen = ({ [0x20] = "pmovsxbw", [0x21] = "pmovsxbd", [0x22] = "pmovsxbq",
			[0x23] = "pmovsxwd", [0x24] = "pmovsxwq", [0x25] = "pmovsxdq",
			[0x30] = "pmovzxbw", [0x31] = "pmovzxbd", [0x32] = "pmovzxbq",
			[0x33] = "pmovzxwd", [0x34] = "pmovzxwq", [0x35] = "pmovzxdq" })[c]
		if widen then
			local rmT, np = XM()
			if not rmT then return nil end
			return widen .. " " .. rmT .. "," .. xmmRegT(), np - pos
		end
	end
	return nil
end

-- Decode one 0F 3A-map instruction (three-byte opcodes with an imm8).
decode0F3A = function(b, pos, pf, p)
	local rex, osz, f2, f3 = pf.rex, pf.osz, pf.f2, pf.f3
	local w = (pf.rexW and 8) or (osz and 2) or 4
	local c = b[p + 2]
	if c == nil then return nil end
	local function rmFn(width)
		return function(n)
			if width == 1 then return "%" .. byteRegName(n, rex) end
			return "%" .. gprName(n, width * 8)
		end
	end
	local function XM()
		return decodeModRM(b, p + 3, pf.rexR, pf.rexB, pf.rexX, function(n) return "%xmm" .. n end)
	end
	local function RM(width)
		return decodeModRM(b, p + 3, pf.rexR, pf.rexB, pf.rexX, rmFn(width))
	end
	local function xmmRegT()
		return "%xmm" .. regNumAt(b, p + 3, pf.rexR)
	end
	local function imm8u(q)
		if not b[q] then return nil end
		return b[q], q + 1
	end
	-- imm8, r/m, xmm (the dominant 3-operand packed shape)
	local function sse3(mnem, srcIsXm)
		if not osz then return nil end
		if f2 or f3 then return nil end
		local xmT, np = XM()
		if not xmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		if srcIsXm then
			return mnem .. " $" .. v .. "," .. xmT .. "," .. xmmRegT(), q - pos
		end
		return mnem .. " $" .. v .. "," .. xmmRegT() .. "," .. xmT, q - pos
	end

	-- sha1rnds4 (NP 0F 3A CC)
	if c == 0xCC and not osz and not f2 and not f3 then
		local rmT, np = XM()
		if not rmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return "sha1rnds4 $" .. v .. "," .. rmT .. "," .. xmmRegT(), q - pos
	end
	-- aeskeygenassist (66 0F 3A DF)
	if c == 0xDF and osz and not f2 and not f3 then
		local rmT, np = XM()
		if not rmT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return "aeskeygenassist $" .. v .. "," .. rmT .. "," .. xmmRegT(), q - pos
	end

	local sse3ops = {
		[0x08] = "roundps", [0x09] = "roundpd", [0x0A] = "roundss",
		[0x0B] = "roundsd", [0x0C] = "blendps", [0x0D] = "blendpd",
		[0x0E] = "pblendw", [0x0F] = "palignr", [0x40] = "dpps",
		[0x41] = "dppd", [0x42] = "mpsadbw", [0x44] = "pclmulqdq",
	}
	local m = sse3ops[c]
	if m then return sse3(m, true) end
	-- pextrb/pextrw/pextrd/pextrq/extractps: imm8, xmm, r/m
	local extract = ({ [0x14] = "pextrb", [0x15] = "pextrw", [0x16] = "pextrd",
		[0x17] = "extractps" })[c]
	if extract then
		if not osz or f2 or f3 then return nil end
		-- pextr*: the rm field is the DESTINATION (r/m8, r/m16, r/m32 or r/m64 —
		-- the register form of the byte/word extracts is r32), the reg field is
		-- the source xmm, and the imm8 follows the ModRM
		local dstW = (c == 0x14 and 4) or (c == 0x15 and 2)
			or (c == 0x16 and ((pf.rexW and 8) or 4)) or 4
		local mnem = (c == 0x16) and (dstW == 8 and "pextrq" or "pextrd") or extract
		local dstT, np = decodeModRM(b, p + 3, pf.rexR, pf.rexB, pf.rexX, rmFn(dstW))
		if not dstT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. " $" .. v .. "," .. xmmRegT() .. "," .. dstT, q - pos
	end
	-- pinsrb/pinsrd/pinsrq/insertps: imm8, r/m, xmm
	local insert = ({ [0x20] = "pinsrb", [0x22] = "pinsrd", [0x21] = "insertps" })[c]
	if insert then
		if not osz or f2 or f3 then return nil end
		if c == 0x21 then
			-- insertps takes an xmm-or-m32 source
			local xmT, np = XM()
			if not xmT then return nil end
			local v, q = imm8u(np)
			if not v then return nil end
			return insert .. " $" .. v .. "," .. xmT .. "," .. xmmRegT(), q - pos
		end
		-- pinsrb/pinsrd/pinsrq: the register form of the r/m8 source is r32;
		-- the 66 prefix is part of the opcode (REX.W alone selects 64)
		local srcW = (c == 0x20 and 4) or ((pf.rexW and 8) or 4)
		local mnem = (c == 0x22) and (srcW == 8 and "pinsrq" or "pinsrd") or insert
		local srcT, np = RM(srcW)
		if not srcT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return mnem .. " $" .. v .. "," .. srcT .. "," .. xmmRegT(), q - pos
	end
	if c == 0x22 and pf.rexW then
		if not osz or f2 or f3 then return nil end
		local srcT, np = RM(8)
		if not srcT then return nil end
		local v, q = imm8u(np)
		if not v then return nil end
		return "pinsrq $" .. v .. "," .. srcT .. "," .. xmmRegT(), q - pos
	end
	-- pcmpestrm/pcmpestri/pcmpistrm/pcmpistri: decodable, but sarcasm's FP model
	-- rejects them (implicit xmm0 mask use) — decoding still names the instruction
	local pcmp = ({ [0x60] = "pcmpestrm", [0x61] = "pcmpestri",
		[0x62] = "pcmpistrm", [0x63] = "pcmpistri" })[c]
	if pcmp then return sse3(pcmp, true) end
	return nil
end

-- VEX/EVEX inverted prefix bit -> extension (0 or 8): bit CLEAR means extended.
local function vexExt(byte, mask) return (byte % (mask * 2)) < mask and 8 or 0 end

-- Try to decode one instruction at b[pos]; returns (text, length) or nil for a
-- branch that the caller must complete, a third value `rel` carries the branch
-- displacement. Anything undecodable returns nil.
local function matchInsn(b, pos)
	local c0 = b[pos]
	if not c0 then return nil end

	-- VEX (C4 3-byte / C5 2-byte) and EVEX (62) prefixes
	if c0 == 0xC4 and b[pos + 1] and b[pos + 2] then
		local b2, b3 = b[pos + 1], b[pos + 2]
		local map = b2 % 32
		local rE = vexExt(b2, 128)   -- R̅
		local xE = vexExt(b2, 64)    -- X̅
		local bE = vexExt(b2, 32)    -- B̅
		local W = math.floor(b3 / 128) % 2
		local vvvv = 15 - (math.floor(b3 / 8) % 16)
		local L = math.floor(b3 / 4) % 2
		local pp = b3 % 4
		local op = b[pos + 3]
		if map == 2 and pp == 3 and L == 1 and (op == 0xCB or op == 0xCC or op == 0xCD) then
			-- SHA512-NI (VEX.256.F2.0F38.W0; always .byte in perlasm — gas before
			-- 2.42 does not know them):
			--   vsha512rnds2 ymm1, ymm2, xmm3/m128 (CB): dst=reg, src1=vvvv, src2=rm
			--   vsha512msg1  ymm1, xmm3/m128       (CC): dst=reg, src=rm (vvvv unused)
			--   vsha512msg2  ymm1, ymm3/m256       (CD): dst=reg, src=rm (vvvv unused)
			local regN = (math.floor(b[pos + 4] / 8) % 8) + rE
			if op == 0xCB then
				local rmT, np = decodeModRM(b, pos + 4, rE, bE, xE, function(n) return "%xmm" .. n end)
				if not rmT then return nil end
				return "vsha512rnds2 " .. rmT .. ",%ymm" .. vvvv .. ",%ymm" .. regN, np - pos
			end
			local rmClass = (op == 0xCC) and "xmm" or "ymm"
			local rmT, np = decodeModRM(b, pos + 4, rE, bE, xE, function(n) return "%" .. rmClass .. n end)
			if not rmT then return nil end
			local mnem = (op == 0xCC) and "vsha512msg1" or "vsha512msg2"
			return mnem .. " " .. rmT .. ",%ymm" .. regN, np - pos
		end
		if map == 2 and pp == 3 and L == 0 and op == 0xF6 then
			-- mulx (VEX.F2.0F38.Wx F6): dst=reg, src1=vvvv, src2=rm (GPR or mem)
			local w = (W == 1) and 64 or 32
			local mnem = (W == 1) and "mulxq" or "mulxl"
			local rmT, np = decodeModRM(b, pos + 4, rE, bE, xE, function(n)
				return "%" .. gprName(n, w) end)
			if not rmT then return nil end
			return mnem .. " " .. rmT .. ",%" .. gprName(vvvv, w) .. ",%"
				.. gprName((math.floor(b[pos + 4] / 8) % 8) + rE, w), np - pos
		end
		if map == 1 and pp == 2 and (op == 0x6F or op == 0x7F) then
			-- vmovdqu (VEX.F3.0F 6F/7F, L=0 xmm / L=1 ymm)
			local vec = (L == 1) and "ymm" or "xmm"
			local rmT, np = decodeModRM(b, pos + 4, rE, bE, xE, function(n) return "%" .. vec .. n end)
			if not rmT then return nil end
			local regT = "%" .. vec .. ((math.floor(b[pos + 4] / 8) % 8) + rE)
			if op == 0x6F then return "vmovdqu " .. rmT .. "," .. regT, np - pos
			else return "vmovdqu " .. regT .. "," .. rmT, np - pos end
		end
		return nil
	end

	-- EVEX (narrow: the vpshrd*-imm forms OpenSSL emits for aesni-xts)
	if c0 == 0x62 and b[pos + 1] and b[pos + 2] and b[pos + 3] then
		local P0, P1, P2 = b[pos + 1], b[pos + 2], b[pos + 3]
		local op = b[pos + 4]
		-- P0: R̅ X̅ B̅ R̅' 0 0 m m ; require the two zero bits clear and map 0F3A
		--     (mm=3) — i.e. the low nibble is exactly 0011.
		-- P1: W v̅vvv 1 pp ; require the fixed one bit set and pp=01 (66).
		-- P2: z L' L b V̅' aaa ; require z=0, L'L=00 (xmm), b=0, aaa=0 (no
		--     writemask) — only the V̅' bit (vvvv's 4th bit, 0 for regs < 16)
		--     may be set.
		if (P0 % 16) == 3
			and (P1 % 8) >= 4 and (P1 % 4) == 1
			and (P2 == 0 or P2 == 8)
			and op == 0x73 then
			local W = math.floor(P1 / 128) % 2
			local rE = vexExt(P0, 128) + (vexExt(P0, 16) * 2)   -- R̅ + R̅' (bits 3,4)
			local bE = vexExt(P0, 32)
			local vvvv = (15 - (math.floor(P1 / 8) % 16)) + (vexExt(P2, 8) * 2)
			local mnem = (W == 1) and "vpshrdq" or "vpshrdd"
			local modrm = b[pos + 5]
			if modrm and math.floor(modrm / 64) % 4 == 3 then
				local dst = (math.floor(modrm / 8) % 8) + rE
				local src2 = (modrm % 8) + bE
				local imm = b[pos + 6]
				if dst < 16 and vvvv < 16 and src2 < 16 and imm then
					return mnem .. " $" .. imm .. ",%xmm" .. src2 .. ",%xmm" .. vvvv
						.. ",%xmm" .. dst, 7
				end
			end
		end
		return nil
	end

	-- leading addr-size prefixes (0x67): gas alignment padding emits lone 0x67
	-- bytes; a 0x67 in front of a real instruction truncates that instruction's
	-- effective addresses, so it may never be folded away.
	local q = pos
	while b[q] == 0x67 do q += 1 end
	if q > pos then
		if not b[q] then return "nop", q - pos end
		return nil
	end

	-- legacy prefix scan: 66 (operand size), F2/F3, REX 40-4F, and the cs/ds/
	-- es/ss no-op hints (2e/3e/26/36). fs/gs (64/65) change addressing and are
	-- never folded; lock (f0) is a run-level marker handled by the caller.
	local pf, p = { rex = 0, osz = false, f2 = false, f3 = false, hint = false }, pos
	while true do
		local x = b[p]
		if x == 0x66 then pf.osz = true; p += 1
		elseif x == 0xF2 then pf.f2 = true; p += 1
		elseif x == 0xF3 then pf.f3 = true; p += 1
		elseif x and x >= 0x40 and x <= 0x4F then pf.rex = x; p += 1
		elseif x == 0x2E or x == 0x3E or x == 0x26 or x == 0x36 then pf.hint = true; p += 1
		else break end
	end
	pf.rexW = (pf.rex % 16) >= 8
	pf.rexR = ((pf.rex % 8) >= 4) and 8 or 0
	pf.rexX = ((pf.rex % 4) >= 2) and 8 or 0
	pf.rexB = (pf.rex % 2) >= 1 and 8 or 0

	local c = b[p]
	if not c then
		-- trailing filler prefixes: a lone 66/2e/3e/26/36 (no REX, no F2/F3) is
		-- one of gas's standalone padding bytes — a nop.
		if pf.rex == 0 and not pf.f2 and not pf.f3 and (pf.osz or pf.hint) then
			return "nop", p - pos
		end
		return nil
	end

	if c == 0x0F then return decode0F(b, pos, pf, p) end
	return decodeLegacy(b, pos, pf, p)
end

-- Data-directive run collection ------------------------------------------------
--
-- A RUN of adjacent data directives (.byte/.word/.short/.long/.quad) is one
-- candidate instruction stream. Each directive contributes its values in
-- LITTLE-ENDIAN byte order (x86_64 streams: `.long 0x9066A4F3` contributes
-- f3 a4 66 90 — the rep movsb plus its 2-byte nop pad). Only plain integer
-- literals are accepted (decimal / 0x-hex / 0b-binary, optionally negative,
-- within the directive's range and exactly representable): anything else — a
-- symbolic expression, a relocation, an out-of-range or double-inexact value —
-- keeps its data spelling, so it hits the in-body-data rejection (jump tables
-- and literal pools are NOT supported).

local DATA_SIZES = {
	[".byte"] = 1, [".word"] = 2, [".short"] = 2, [".2byte"] = 2,
	[".long"] = 4, [".int"] = 4, [".4byte"] = 4,
	[".quad"] = 8, [".8byte"] = 8,
}

-- Parse a directive's argument list into little-endian bytes, or nil when any
-- field is not a plain in-range integer literal (such a directive is data — a
-- symbol, an expression, an unrepresentable value — not an encoding). Values at
-- or beyond 2^53 stay data: doubles are inexact there, and a decoded constant
-- must be byte-exact.
local function dataArgBytes(name, args)
	local size = DATA_SIZES[name]
	if not size then return nil end
	local bytes = {}
	for _, f in splitOperands(args or "") do
		local t = string.gsub(f, "^%s+", "")
		t = string.gsub(t, "%s+$", "")
		local neg = false
		if string.sub(t, 1, 1) == "-" then neg = true; t = string.sub(t, 2) end
		local field = nil
		local hexdigits = string.match(t, "^0[xX]([%x]+)$")
		if hexdigits then
			-- parse hex as TEXT: doubles are inexact past 2^53, and an 8-byte
			-- constant (a movabsq immediate) must decode byte-exactly
			if #hexdigits > size * 2 or neg then return nil end
			if #hexdigits % 2 == 1 then hexdigits = "0" .. hexdigits end
			local le = {}
			for i = #hexdigits - 1, 1, -2 do
				table.insert(le, tonumber(string.sub(hexdigits, i, i + 1), 16))
			end
			field = le
		else
			local v
			if string.match(t, "^%d+$") or string.match(t, "^0[bB][01]+$") then
				v = tonumber(t)
			end
			if v == nil or v ~= math.floor(v) then return nil end
			if neg then v = -v end
			local limit = 2 ^ (size * 8)
			if v < 0 then
				if v < -(2 ^ 53) then return nil end
				if v < -limit / 2 then return nil end   -- out of the signed range
			else
				if v >= limit or v >= 2 ^ 53 then return nil end
			end
			field = {}
			for i = 0, size - 1 do
				table.insert(field, math.floor(v / 2 ^ (8 * i)) % 256)
			end
		end
		for i = 1, size do
			table.insert(bytes, field[i] or 0)
		end
	end
	if #bytes == 0 then return nil end
	return bytes
end

-- Find the longest table entry matching `bytes` at `pos` (first-listed wins a
-- length tie). Returns the entry and its length, or nil.
local function matchBytes(bytes, pos)
	local best, bestLen = nil, 0
	for _, e in BYTE_DECODE do
		local eb = e.bytes
		if #eb > bestLen and pos + #eb - 1 <= #bytes then
			local match = true
			for k = 1, #eb do
				if bytes[pos + k - 1] ~= eb[k] then match = false break end
			end
			if match then best, bestLen = e, #eb end
		end
	end
	return best, bestLen
end

-- Synthetic labels for decoded branches. A per-process monotonic sequence keeps
-- every name unique across files, functions and localcall clones.
local decodedLabelSeq = 0


-- Build the statement list for a decoded run (branch labels + re-parsed texts).
-- Returns nil when any instruction text does not re-parse or a branch target is
-- not an instruction boundary inside the run (the whole run then stays data —
-- a decoded text must be exactly as parseable as a spelled one).
local function buildRunStatements(insns, bytes, lines, file)
	-- every branch target must be an instruction boundary INSIDE the run
	local starts = {}
	for _, in_ in insns do starts[in_.start] = true end
	local labels = {}
	for _, in_ in insns do
		if in_.rel then
			local t = in_.start + in_.len + in_.rel
			if t < 1 or t > #bytes or not starts[t] then return nil end
			if not labels[t] then
				decodedLabelSeq += 1
				labels[t] = ".Lsarcasm_byte" .. tostring(decodedLabelSeq)
			end
		end
	end
	local out = {}
	for _, in_ in insns do
		if labels[in_.start] then
			table.insert(out, { kind = "label", name = labels[in_.start], line = in_.line, file = file })
		end
		local text = in_.text
		if in_.rel then
			text = text .. " " .. labels[in_.start + in_.len + in_.rel]
		end
		local ok, insn = pcall(parseInsnText, text, "att")
		if not ok then return nil end
		insn.line = in_.line
		insn.file = file
		table.insert(out, insn)
	end
	return out
end

-- Decode a run of data bytes into a statement list (labels + instructions), or
-- nil when the run does not decode COMPLETELY — a partial decode is never
-- emitted (an undecodable tail would silently become data mid-body, and the
-- whole run must keep its original spelling for the downstream rejection).
-- bytes/lines are parallel 1-based arrays; lines[i] is the source line of
-- bytes[i] (feeding the decoded statements' origins). file is the input path
-- stamped on the run's source statements (carried onto the decoded ones). A
-- second return value is
-- the buffer position of an unjoined trailing lock byte (nil otherwise).
local function decodeRunStatements(bytes, lines, file)
	local insns = {}
	local pos = 1
	local lockPos = nil   -- buffer position of a pending (unjoined) lock byte
	while pos <= #bytes do
		-- greedy longest match: the fixed table and the pattern decoder compete,
		-- the longer match wins and ties go to the table (so the table's exact
		-- spellings keep their established semantics — e.g. `.byte 0xf3,0xc3`
		-- decodes to plain `ret`, not `rep ret`)
		local e, elen = matchBytes(bytes, pos)
		local text, len, rel = nil, 0, nil
		local ptext, plen, prel = matchInsn(bytes, pos)
		if ptext and (not e or plen > elen) then
			text, len, rel = ptext, plen, prel
		elseif e then
			if e.lock then
				-- a lock byte is a marker: it joins the NEXT decoded instruction
				-- of this run (the instruction's parse text gets the spelled
				-- `lock` prefix, validated downstream like spelled lock forms)
				lockPos = pos
				pos += elen
				continue
			end
			text, len = e.text, elen
		end
		if not text and bytes[pos] == 0xF0 then
			-- a lock byte joining the instruction that follows it in the same
			-- run (no table entry matched here)
			local t2, l2, r2 = matchInsn(bytes, pos + 1)
			if t2 then text, len, rel = "lock " .. t2, l2 + 1, r2 end
		end
		if not text or not len or len <= 0 then return nil end
		if lockPos then
			text = "lock " .. text
			lockPos = nil
		end
		table.insert(insns, { start = pos, text = text, len = len,
			line = lines[pos], rel = rel })
		pos += len
	end
	-- a lock byte pending at the run's end never joined anything: report it so
	-- the caller can try joining it with the spelled instruction that follows
	if lockPos then return nil, lockPos end
	return buildRunStatements(insns, bytes, lines, file)
end


-- Rewrite the decodable data-directive runs of a function body (a statement
-- array as produced by parseString and splitFunctions) into insn statements.
-- Returns the new body; bodies with no data directives are returned unchanged.
-- Runs that do not fully decode are kept byte-for-byte as written, so the
-- downstream "data in a function body" rejection fires exactly as it does
-- without this pass. x86_64-only: the encodings (and the perlasm idiom) do not
-- exist on arm64, whose top-level/body data rules are unchanged.
function x86parse.decodeByteInsns(body)
	local hasData = false
	for _, st in body do
		if st.kind == "directive" and DATA_SIZES[st.name] then hasData = true break end
	end
	if not hasData then return body end

	local out = {}
	local i, n = 1, #body
	while i <= n do
		local st = body[i]
		if not (st.kind == "directive" and DATA_SIZES[st.name]) then
			table.insert(out, st)
			i += 1
		else
			-- maximal run of ADJACENT data directives [i..j]. A label (or any
			-- other statement) breaks the run: bytes on either side of a label
			-- are not one instruction — a branch to the label would land in the
			-- middle of the encoding.
			local j = i
			while j + 1 <= n and body[j + 1].kind == "directive"
				and DATA_SIZES[body[j + 1].name] do
				j += 1
			end
			-- Accumulate the run's byte values (tracking the source line of each
			-- byte for the decoded instructions' origins). A statement whose
			-- args are not all plain literal values leaves the WHOLE run data.
			local bytes, lines = {}, {}
			local overflow = nil
			for k = i, j do
				local vals = dataArgBytes(body[k].name, body[k].args)
				if not vals then overflow = k break end
				for _, v in vals do
					table.insert(bytes, v)
					table.insert(lines, body[k].line)
				end
			end
			-- An annotation on the run TRANSFERS to the first decoded
			-- instruction (`.long 0x9066A4F3 #! stack buffer (...)` is a decoded
			-- rep with its annotation). Two or more annotations on one run are
			-- ambiguous — the run stays data (and is rejected downstream with
			-- its annotation error).
			local ann, annCount = nil, 0
			for k = i, j do
				if body[k].annotation then annCount += 1; ann = body[k].annotation end
			end
			-- A lone lock byte joins the SPELLED instruction following the run
			-- (OpenSSL's `.byte 0xf0` + `cmpxchgl ...` idiom): tag it exactly
			-- like the parser's spelled-`lock` path does. Any non-instruction
			-- follower leaves the lock byte as data (it is rejected below).
			local lockJoined = false
			if not overflow and #bytes == 1 and bytes[1] == 0xF0 and annCount == 0 then
				local nxt = body[j + 1]
				if nxt and nxt.kind == "insn" then
					nxt.lockPrefix = true
					lockJoined = true
				end
			end
			local decoded = (not overflow and #bytes > 0 and not lockJoined)
				and decodeRunStatements(bytes, lines, body[i].file) or nil
			if decoded then
				for _, dst in decoded do table.insert(out, dst) end
				if ann then
					for _, dst in decoded do
						if dst.kind == "insn" then dst.annotation = ann break end
					end
				end
			elseif lockJoined then
				-- the lone lock byte moved onto the spelled instruction that
				-- follows the run: emit nothing for the run itself (exactly what
				-- the spelled `lock` prefix spelling produces)
			else
				-- Nothing decoded (or not fully decoded): keep the run
				-- byte-for-byte as written — the downstream data rejection sees
				-- the original directive text.
				for k = i, j do table.insert(out, body[k]) end
			end
			i = j + 1
		end
	end
	return out
end

return x86parse
