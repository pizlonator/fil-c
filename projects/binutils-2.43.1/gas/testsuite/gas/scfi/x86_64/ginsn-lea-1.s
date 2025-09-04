## Testcase with a variety of lea.
	.text
	.globl  foo
	.type   foo, @function
foo:
	lea  symbol, %rbp
	lea  0x9090, %rbp
	lea  -0x2(%rip), %rax
	lea  -0x1(%eax,%ebx), %ebp
	lea  0x55(%eax,%ebx,2), %ebp
	lea  -0x3(,%ebx,1), %ecx
	lea  -0x3(,%ebx,), %ecx
	lea  -0x3(,%ebx,2), %ecx

	.allow_index_reg
	lea  (%rax,%riz),%rbp
	lea  (%rax,4),%rbp
	lea  (%rax,%riz,4),%rbp
	lea  sym(,%riz), %rbp
	lea  (,%riz), %rbp
.LFE0:
	.size   foo, .-foo
