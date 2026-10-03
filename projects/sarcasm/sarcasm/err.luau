--!nonstrict
-- Central diagnostic raising for sarcasm. Every user-facing rejection goes through
-- err.at/err.here/err.plain so that error messages carry the source location of the
-- construct being rejected. The message format is:
--
--   sarcasm: <file>:<line>: <message>
--
-- Statements in the parse IR carry `file` (the input path as given on the command
-- line) and `line` (1-based source line). err.at derives the location from the
-- statement it is given; err.here uses the parser's current line while parsing;
-- err.plain raises without a location (sarcasm.main then prepends the input path).
--
-- err.luau must never require another sarcasm module (it sits at the bottom of the
-- require graph).

local err = {}

-- Fallback file used when a statement has a line but no file (e.g. statements
-- synthesized by rewriting passes that only copy `line`). Set by sarcasm.main to
-- the input path as given on the command line.
local defaultFile: string? = nil

function err.setDefaultFile(f: string?)
	defaultFile = f
end

-- Parser-side dynamic location: the parsers raise errors from deep helpers that
-- only receive strings, so parseString publishes the file at entry and the current
-- 1-based line number at the top of each line-loop iteration.
local parseFile: string? = nil
local parseLine: number? = nil

function err.beginParse(f: string?)
	parseFile = f
	parseLine = nil
end

function err.atParseLine(n: number?)
	parseLine = n
end

function err.endParse()
	parseFile = nil
	parseLine = nil
end

-- "file:line" for a statement-like table (anything with .file/.line), or nil if
-- no location can be derived. Pure: no parser-state fallback (used to embed
-- SECONDARY locations, which are always real statements).
function err.locStr(st: any): string?
	if type(st) ~= "table" then
		return nil
	end
	local f = st.file or defaultFile
	local l = st.line
	if type(f) == "string" and type(l) == "number" then
		return f .. ":" .. l
	end
	return nil
end

-- Raise a rejection located at statement `st` (nil-tolerant: falls back to an
-- unlocated message, which sarcasm.main presents with the input path prefix).
function err.at(st: any, msg: string)
	local loc = err.locStr(st)
	if loc then
		error("sarcasm: " .. loc .. ": " .. msg, 0)
	end
	error("sarcasm: " .. msg, 0)
end

-- Raise a rejection located at the parser's current line (see beginParse /
-- atParseLine). Falls back to an unlocated message outside parsing.
function err.here(msg: string)
	if parseFile and parseLine then
		error("sarcasm: " .. parseFile .. ":" .. parseLine .. ": " .. msg, 0)
	end
	error("sarcasm: " .. msg, 0)
end

-- Raise a rejection with no source location (whole-input errors). sarcasm.main
-- prepends the input path, preserving today's "sarcasm: <input>: <msg>" shape.
function err.plain(msg: string)
	error("sarcasm: " .. msg, 0)
end

return err
