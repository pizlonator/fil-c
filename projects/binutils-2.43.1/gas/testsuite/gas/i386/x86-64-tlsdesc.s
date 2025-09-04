	.text
_start:
	leaq	foo@TLSDESC(%rip), %rax

	leaq	foo@TLSDESC(%rip), %r16
	leaq	foo@TLSDESC(%rip), %r20

	.intel_syntax noprefix

	lea	rax, QWORD PTR [rip + foo@TLSDESC]

	lea	r16, QWORD PTR [rip + foo@TLSDESC]
	lea	r20, QWORD PTR [rip + foo@TLSDESC]
