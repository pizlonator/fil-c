	.weak bar
	.text
	.global _start
	.type _start,%function
_start:
	mov	bar@GOTPCREL(%rip), %rax
	mov	foo@GOTPCREL(%rip), %rax
	.size _start, . - _start
