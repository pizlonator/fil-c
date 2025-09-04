# Check illegal APX-ZU instructions

	.allow_index_reg
	.text
_start:
	imulzub $0xa,%bl,%al
	imulzud $0xa,%ebx,%eax
	imulzu $0xa,%rbx,%rax
	imulzub $0xaaaa,%bl,%al
	imulzud $0xaaaa,%ebx,%eax
	imulzu $0xaaaa,%rbx,%rax
	imulzu $0xaaaa,%ebx,%rax
	imulzu $0xaaaa,%ebx,%rax
	setzuno  %ah
	setzub   %bx
	setzuae  %r8w
	setzue   %r9
	setzune  %r10d
	setzube  %eax
	setzua   %ebx
	setzus   %r18w
	setzuns  %r19w
	setzup   %r20d
	setzunp  %r21w
	setzul   %r22w
	setzuge  %r23d
	setzule  %r24w
	setzug   %r25w
