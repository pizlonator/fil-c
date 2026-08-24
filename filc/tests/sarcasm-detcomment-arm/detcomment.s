/* Arch auto-detection must ignore x86 markers that appear only inside comments
   and string literals: this is an ARM64 file even though it mentions
   .intel_syntax, endbr64, %rax, QWORD PTR and friends below.  If detection
   were fooled, the x86 parser would choke on the arm64 instructions and the
   test would fail to compile. */
// .intel_syntax noprefix would switch to x86 intel if honored
// .att_syntax prefix
// endbr64 is an x86 CET marker
// QWORD PTR [rax] and rbp, rdi, rsi are x86 tokens
	.text
	.global	detfoo
	.type	detfoo, %function
detfoo:                         ;! int(int)
	add	w0, w0, #37
	ret
	.size	detfoo, .-detfoo
/* a block comment spanning
   %rcx %rdx endbr64
   multiple lines */
	.section	.rodata
.Lstr1:
	.asciz	".intel_syntax noprefix"
.Lstr2:
	.asciz	"%rax %rdi"
.Lstr3:
	.asciz	"endbr64"
.Lstr4:
	.asciz	" eax QWORD PTR [rbx] "
	.section	.note.GNU-stack,"",@progbits
