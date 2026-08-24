-- Unit test for architecture + syntax auto-detection and the dual-syntax x86 parser.
-- Verifies detect() picks the right (arch, syntax) and that AT&T and Intel inputs parse
-- to the same operand model.

local detect = require("../sarcasm/detect")
local x86parse = require("../sarcasm/x86_64_parse")

local pass, fail = 0, 0
local function ck(name, got, want)
	if got == want then print("  ok: " .. name); pass += 1
	else print("  FAIL: " .. name .. " (got '" .. tostring(got) .. "' want '" .. tostring(want) .. "')"); fail += 1 end
end

-- detection
local a1, s1 = detect.detect("\tmovzbl\t(%rdi), %eax\n\tret\n")
ck("detect AT&T arch", a1, "x86_64"); ck("detect AT&T syntax", s1, "att")

local a2, s2 = detect.detect("\t.intel_syntax noprefix\n\tmovzx eax, BYTE PTR [rdi]\n")
ck("detect Intel arch", a2, "x86_64"); ck("detect Intel syntax", s2, "intel")

local a3 = detect.detect("\t.arch armv8-a\n\tldrb w0, [x0]\n\tret\n")
ck("detect ARM64 arch", a3, "arm64")

local a4, s4 = detect.detect("\tmov eax, DWORD PTR [rdi+rsi*4]\n\tret\n")
ck("detect Intel (no directive) arch", a4, "x86_64"); ck("detect Intel (no directive) syntax", s4, "intel")

-- dual-syntax parser equivalence: AT&T and Intel produce the same normalized model
local att = x86parse.parseString("\tmovzbl\t(%rdi,%rsi,1), %eax\n", "att")
local intel = x86parse.parseString("\tmovzx\teax, BYTE PTR [rdi+rsi*1]\n", "intel")
local function firstInsn(stmts) for _, s in stmts do if s.kind == "insn" then return s end end end
local ia, ii = firstInsn(att), firstInsn(intel)
ck("att/intel same dest reg", ia.operands[1].name, ii.operands[1].name)
ck("att/intel same mem base", ia.operands[2].base.name, ii.operands[2].base.name)
ck("att/intel same mem index", ia.operands[2].index.name, ii.operands[2].index.name)
ck("att/intel same mem scale", ia.operands[2].scale, ii.operands[2].scale)

-- x86 markers inside comments and string literals of an arm64 file must not spoof the
-- architecture (the file body is arm64: ldr/w0/x0/sp).

local ARM_BODY = "\t.text\nfoo: ;! int(ptr)\n\tldr w0, [x0]\n\tret\n\t.size foo, .-foo\n"

local a5 = detect.detect(ARM_BODY .. "\t// .intel_syntax noprefix was here\n")
ck("arm64 + // .intel_syntax comment", a5, "arm64")

local a6 = detect.detect(ARM_BODY .. "\t// .att_syntax prefix\n")
ck("arm64 + // .att_syntax comment", a6, "arm64")

local a7 = detect.detect(ARM_BODY .. "\t// endbr64 not needed here\n")
ck("arm64 + // endbr64 comment", a7, "arm64")

local a8 = detect.detect(ARM_BODY .. "\t// QWORD PTR [rax] memory\n")
ck("arm64 + // PTR comment", a8, "arm64")

local a9 = detect.detect(ARM_BODY .. "\t// rbp is used like rdi or rsi\n")
ck("arm64 + // register-name comment", a9, "arm64")

local a10 = detect.detect(ARM_BODY .. "\t/* .intel_syntax noprefix */\n")
ck("arm64 + /* .intel_syntax */ comment", a10, "arm64")

local a11 = detect.detect(ARM_BODY .. "\t/* multi\n\t   %rax %rbx endbr64 PTR\n\t   line */\n")
ck("arm64 + multiline /* */ comment with markers", a11, "arm64")

local a12 = detect.detect(ARM_BODY .. '\t.section\t.rodata\n\t.asciz\t".intel_syntax noprefix"\n')
ck("arm64 + .intel_syntax in string", a12, "arm64")

local a13 = detect.detect(ARM_BODY .. '\t.section\t.rodata\n\t.asciz\t".att_syntax"\n')
ck("arm64 + .att_syntax in string", a13, "arm64")

local a14 = detect.detect(ARM_BODY .. '\t.section\t.rodata\n\t.asciz\t"%rax %rdi"\n')
ck("arm64 + %registers in string", a14, "arm64")

local a15 = detect.detect(ARM_BODY .. '\t.section\t.rodata\n\t.asciz\t"endbr64"\n')
ck("arm64 + endbr64 in string", a15, "arm64")

local a16 = detect.detect(ARM_BODY .. '\t.section\t.rodata\n\t.asciz\t" eax "\n')
ck("arm64 + ' eax ' in string", a16, "arm64")

local a17 = detect.detect(ARM_BODY .. "# %rax %rbx line comment\n")
ck("arm64 + #-comment with registers", a17, "arm64")

-- mid-line `#` on arm64 is an immediate, not a comment; either way the file stays arm64
local a18 = detect.detect("\tsub\tsp, sp, #48\n\tldr\tx0, [x0]\n\tadd\tsp, sp, #48\n\tret\n")
ck("arm64 + mid-line # immediates", a18, "arm64")

-- an escaped quote inside a string must not end the string early and expose markers
local a19 = detect.detect(ARM_BODY .. '\t.asciz\t"a\\" %rax"\n')
ck("arm64 + escaped quote in string", a19, "arm64")

-- a quote inside a // comment must not open a string and hide real arm tokens
local a20 = detect.detect(ARM_BODY .. '\t// he said "hi\n')
ck("arm64 + quote inside // comment", a20, "arm64")

-- conversely: arm-looking tokens inside comments of an x86 file must not hide it
local a21, s21 = detect.detect("\t.text\n\t// x0 x1 ldr sp arm tokens\n\tmovl\t(%rdi), %eax\n\tret\n")
ck("x86 att + // arm tokens comment arch", a21, "x86_64")
ck("x86 att + // arm tokens comment syntax", s21, "att")

local a22, s22 = detect.detect("\tmovl\t(%rdi), %eax\t# x0 x1 ldr sp\n\tret\n")
ck("x86 att + # arm tokens comment arch", a22, "x86_64")
ck("x86 att + # arm tokens comment syntax", s22, "att")

-- the x86 parser accepts // and /* */ comment lines (valid in x86 GAS), and does not
-- mangle // inside a string literal
local xc = x86parse.parseString('\t.text\n// a c++-style comment\n\t/* block\n\t   comment */\n\tmovl\t(%rdi), %eax\n\t.asciz\t"http://example"\n\tret\n', "att")
local sawMov, sawAsciz = false, false
for _, s in xc do
	if s.kind == "insn" and s.mnemonic == "movl" then sawMov = true end
	if s.kind == "directive" and s.name == ".asciz" and string.find(s.args, "http://example", 1, true) then sawAsciz = true end
end
ck("x86 parse: mov survives // and /* */ comments", sawMov, true)
ck("x86 parse: // inside .asciz string preserved", sawAsciz, true)

print("detect-test: " .. pass .. " passed, " .. fail .. " failed")
