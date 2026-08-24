/* Intel-syntax variant of the comment acceptance test: GAS accepts C++-style
   // line comments and C block comments in .intel_syntax mode; sarcasm's x86
   parser must strip them (and not confuse the arm64-looking tokens inside the
   comments below with real instructions). */
	.intel_syntax noprefix
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! int(int)
// a c++-style comment line mentioning x0 x1 ldr sp
	lea	eax, [rdi+5]    // trailing comment
/* block comment:
   another line with rax rbx rcx tokens */
	add	eax, 37
	ret                         // done
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
