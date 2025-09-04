	.text
_start:
	addq	foo@GOTTPOFF(%rip), %rax
	movq	foo@GOTTPOFF(%rip), %rax

	addq	foo@GOTTPOFF(%rip), %r16
	movq	foo@GOTTPOFF(%rip), %r20

	.intel_syntax noprefix

	add	rax, QWORD PTR [rip + foo@GOTTPOFF]
	mov	rax, QWORD PTR [rip + foo@GOTTPOFF]

	add	r16, QWORD PTR [rip + foo@GOTTPOFF]
	mov	r20, QWORD PTR [rip + foo@GOTTPOFF]

	.att_syntax prefix

	addq	%r8, foo@GOTTPOFF(%rip), %r16
	addq	foo@GOTTPOFF(%rip), %rax, %r12
	{nf} addq	%r8, foo@GOTTPOFF(%rip), %r16
	{nf} addq	foo@GOTTPOFF(%rip), %rax, %r12

	.intel_syntax noprefix

	add	r16, QWORD PTR [rip + foo@GOTTPOFF], r8
	add	r12, rax, QWORD PTR [rip + foo@GOTTPOFF]
	{nf} add	r16, QWORD PTR [rip + foo@GOTTPOFF], r8
	{nf} add	r12, rax, QWORD PTR [rip + foo@GOTTPOFF]

	.att_syntax prefix

	{nf} addq	foo@GOTTPOFF(%rip), %rax
	{nf} addq	foo@GOTTPOFF(%rip), %r16

	.intel_syntax noprefix

	{nf} add	rax, QWORD PTR [rip + foo@GOTTPOFF]
	{nf} add	r16, QWORD PTR [rip + foo@GOTTPOFF]
