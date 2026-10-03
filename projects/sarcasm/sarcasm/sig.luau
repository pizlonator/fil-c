--!nonstrict
-- Fil-C signature encoding and parsing of ;! function-signature annotations.
--
-- Annotation grammar (as seen in the *-sarcasm.s inputs):
--   RET(ARG0, ARG1, ...)        e.g. "unsigned(ptr)", "int(ptr, size_t)", "void(ptr,ptr)"
-- Types are C-ish tokens; we only need to know each type's Fil-C type class.

local err = require("./err")

local sig = {}

-- Fil-C type-class indices (see ABI-NOTES.md).
local TYPE_INT = 0
local TYPE_FLOAT = 1
local TYPE_DOUBLE = 2
local TYPE_LONGDOUBLE = 3
local TYPE_VEC4 = 4
local TYPE_VEC5 = 5
local TYPE_VEC6 = 6
local TYPE_PTR = 7

-- Integer-class type words: a token classifies as integer only if EVERY
-- whitespace-separated word (the token is normalized and lowercased first) is
-- in this set — int, unsigned, long, size_t, the stdint names, bool, enum,
-- etc. Anything else (e.g. `__m128`, a typo) is a clean parse error instead of
-- silently misclassifying the token as an integer type.
local INTEGER_WORDS = {
	int = true, long = true, short = true, char = true, signed = true,
	unsigned = true, bool = true, _bool = true, enum = true,
	size_t = true, ssize_t = true, ptrdiff_t = true,
	int8_t = true, int16_t = true, int32_t = true, int64_t = true,
	uint8_t = true, uint16_t = true, uint32_t = true, uint64_t = true,
	intptr_t = true, uintptr_t = true,
}

-- Map a C type token (already trimmed, may contain spaces) to a type class.
-- Raises a clean `sarcasm:` error for a token that names no known type class.
-- `st` (optional) is the statement carrying the annotation, used to locate the
-- error (nil → unlocated).
local function classifyType(tok: string, st: any): number?
	tok = (string.gsub(tok, "%s+", " "))
	tok = (string.gsub(tok, "^%s*(.-)%s*$", "%1"))
	local low = string.lower(tok)
	if low == "void" or low == "" then
		return nil
	end
	if low == "ptr" or string.find(low, "%*") then
		return TYPE_PTR
	end
	if low == "float" then
		return TYPE_FLOAT
	end
	if low == "double" then
		return TYPE_DOUBLE
	end
	if low == "long double" then
		return TYPE_LONGDOUBLE
	end
	-- vector classes: recognized so the signature class checks REJECT them with a
	-- precise message (SIMD signatures are not supported on either architecture)
	-- instead of silently misclassifying the token as an integer type.
	if low == "vec4" then
		return TYPE_VEC4
	end
	if low == "vec5" then
		return TYPE_VEC5
	end
	if low == "vec6" then
		return TYPE_VEC6
	end
	for word in string.gmatch(low, "%S+") do
		if not INTEGER_WORDS[word] then
			local msg = "unknown type '" .. tok .. "' in signature annotation"
			if st then err.at(st, msg) else err.plain(msg) end
		end
	end
	-- all-integer words: int, unsigned, long, size_t, uint64_t, enums, etc.
	return TYPE_INT
end
sig.classifyType = classifyType

-- enc(t1..tn) = sum_{i} 11^(i-1) * (1 + t_i); 0 if empty.
local function encodeList(types: { number }): number
	local acc = 0
	local mult = 1
	for _, t in types do
		acc += mult * (1 + t)
		mult *= 11
	end
	return acc
end

-- Split "a, b, c" respecting nested parens.
local function splitArgs(s: string): { string }
	local out = {}
	local depth = 0
	local cur = {}
	for i = 1, #s do
		local c = string.sub(s, i, i)
		if c == "(" then depth += 1; table.insert(cur, c)
		elseif c == ")" then depth -= 1; table.insert(cur, c)
		elseif c == "," and depth == 0 then
			table.insert(out, table.concat(cur)); cur = {}
		else table.insert(cur, c) end
	end
	local last = table.concat(cur)
	if (string.gsub(last, "%s", "")) ~= "" then
		table.insert(out, last)
	end
	return out
end

-- Parse "RET(ARGS)" into a descriptor:
--   { retTypes = {..}, argTypes = {..}, signature = <number>, argWords = <number>,
--     retWords = <number>, isVoid = <bool> }
-- `st` (optional) is the statement carrying the annotation; when given, parse
-- errors are located at it (else they are unlocated).
function sig.parseSignature(ann: string, st: any)
	local retStr, argStr = string.match(ann, "^%s*(.-)%s*%((.*)%)%s*$")
	if not retStr then
		local msg = "cannot parse function signature annotation: " .. ann
		if st then err.at(st, msg) else err.plain(msg) end
	end
	local retTypes = {}
	local rc = classifyType(retStr, st)
	if rc ~= nil then
		table.insert(retTypes, rc)
	end
	local argTypes = {}
	for _, a in splitArgs(argStr) do
		local c = classifyType(a, st)
		-- void as a lone arg means no args
		if c ~= nil then
			table.insert(argTypes, c)
		end
	end
	local Ret = encodeList(retTypes)
	local Arg = encodeList(argTypes)
	local signature = 1 + Ret + 133 * Arg
	return {
		retTypes = retTypes,
		argTypes = argTypes,
		signature = signature,
		argWords = #argTypes,      -- each scalar/pointer arg occupies one 8-byte word
		retWords = #retTypes,
		isVoid = #retTypes == 0,
		retIsPtr = retTypes[1] == TYPE_PTR,
	}
end

-- Canonical names for the classes classifyType can produce (for diagnostics).
local CLASS_NAME = {
	[TYPE_INT] = "integer", [TYPE_FLOAT] = "float", [TYPE_DOUBLE] = "double",
	[TYPE_LONGDOUBLE] = "long double", [TYPE_PTR] = "ptr",
	[TYPE_VEC4] = "vec4", [TYPE_VEC5] = "vec5", [TYPE_VEC6] = "vec6",
}

-- Is this class a supported floating-point class (float/double)? Architecture-
-- neutral: on every target the FP classes are float (4 bytes, low 32 bits of a v
-- register) and double (8 bytes, a full v register).
function sig.isFpClass(t: number): boolean
	return t == TYPE_FLOAT or t == TYPE_DOUBLE
end

-- Byte width of a supported floating-point class (float=4, double=8), nil otherwise.
function sig.fpWidthBytes(t: number): number?
	if t == TYPE_FLOAT then return 4 end
	if t == TYPE_DOUBLE then return 8 end
	return nil
end

-- Floating-point (and vector) type classes on x86_64: float and double ARE
-- supported (the fast CC passes FP arguments in xmm0..xmm7 in declaration order
-- among FP args and returns them in xmm0, and sarcasm's transform marshals them —
-- see x86_64_codegen / x86_64_glue). Long double stays rejected: filcc gives
-- long-double signatures NO fast CC at all (signature word 0 and a pure buffer-CC
-- entrypoint), so sarcasm's signature formula would not even match the ABI. The
-- vector classes (vec4-6) stay rejected too: a vec4 signature would need full
-- vector-register CC machinery (vec5/vec6 additionally get filcc "memory-class"
-- signature words that do not follow the formula at all).
--
-- On arm64 float and double are supported identically (v0..v7 / v0); `opts.arch ==
-- "arm64"` selects that behavior, and `opts` == nil means x86_64 (both x86 call
-- sites pass { arch = "x86_64" } explicitly).
--
-- `where` optionally prefixes a context (e.g. "call to 'foo'"). `st` (optional)
-- is the statement carrying the annotation/call; when given, rejections are
-- located at it (else they are unlocated). Raises a clean sarcasm: error naming
-- the offending type.
function sig.checkSupportedClasses(sd, where, opts, st)
	local prefix = (where and (where .. ": ") or "")
	local function check(t)
		if t == TYPE_INT or t == TYPE_PTR then return end
		if t == TYPE_FLOAT or t == TYPE_DOUBLE then return end
		if t == TYPE_LONGDOUBLE then
			local arch = (opts and opts.arch == "arm64") and "arm64" or "x86_64"
			local msg = prefix .. "only float and double floating-point types are supported"
				.. " in signatures on " .. arch .. ": long double"
			if st then err.at(st, msg) else err.plain(msg) end
		end
		local arch = (opts and opts.arch == "arm64") and "arm64" or "x86_64"
		local msg = prefix .. "vector types are not supported in signatures on " .. arch
			.. " (SIMD signatures are not supported): "
			.. (CLASS_NAME[t] or ("type class " .. tostring(t)))
		if st then err.at(st, msg) else err.plain(msg) end
	end
	for _, t in (sd.retTypes or {}) do check(t) end
	for _, t in (sd.argTypes or {}) do check(t) end
end

sig.TYPE_INT = TYPE_INT
sig.TYPE_PTR = TYPE_PTR
sig.TYPE_FLOAT = TYPE_FLOAT
sig.TYPE_DOUBLE = TYPE_DOUBLE
sig.TYPE_LONGDOUBLE = TYPE_LONGDOUBLE

-- The `;! ... ptr` annotation family (exact strings, matched against the whole
-- annotation). `load ptr` / `store ptr` annotate a plain 8-byte GPR pointer
-- load/store; `atomic load ptr` / `atomic store ptr` their runtime atomic
-- forms; `atomic ptr` a pointer compare-exchange (cmpxchg on x86_64, cas on
-- arm64); `load store ptr` and `atomic load store ptr` annotate pointer
-- read-modify-write operations (non-atomic resp. atomic, the latter either
-- per-access or, with `lock`, a compare-exchange loop — on arm64 the atomic
-- form lowers on an LSE RMW and `load store ptr` is rejected). Shape validation
-- lives in the per-arch backend (ptrAccessKind / ptrAtomicShape).
sig.PTR_ANNOTATIONS = {
	["load ptr"] = true,
	["store ptr"] = true,
	["load store ptr"] = true,
	["atomic ptr"] = true,
	["atomic load ptr"] = true,
	["atomic store ptr"] = true,
	["atomic load store ptr"] = true,
}

return sig
