/* x86 GAS accepts C++-style // comments and C block comments; sarcasm's x86
   parser must strip them (and not confuse the arm64-looking tokens inside the
   comments below with real instructions). The lone `;` below is GAS's statement
   separator forming an empty statement (valid x86 GAS; it starts with a
   character outside the mnemonic set, so it exercises the parser's passthrough
   fallback — dropped like any other no-op directive). */
	.text
	.globl	foo
	.type	foo, @function
foo:                            ;! int(int)
// a c++-style comment line mentioning x0 x1 ldr sp
	leal	5(%rdi), %eax   // trailing comment: add 5
/* a block comment
   spanning lines, with movq %rax, %rbx inside */
	addl	$37, %eax
;
	ret                         // done
	.size	foo, .-foo
	.section	.note.GNU-stack,"",@progbits
