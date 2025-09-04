# Check 64bit APX_F instructions with nf pseudo prefix


        .text
_start:
	{nf}	add	$123, %bl
	{nf}	add	$123, %bl, %dl
	{nf}	add	$123, %dx
	{nf}	add	$123, %dx, %ax
	{nf}	add	$123, %ecx
	{nf}	add	$123, %ecx, %edx
	{nf}	add	$123, %r9
	{nf}	add	$123, %r9, %r31
	{nf}	addb	$123, 291(%r8, %rax, 4)
	{nf}	add	$123, 291(%r8, %rax, 4), %bl
	{nf}	addw	$123, 291(%r8, %rax, 4)
	{nf}	add	$123, 291(%r8, %rax, 4), %dx
	{nf}	addl	$123, 291(%r8, %rax, 4)
	{nf}	add	$123, 291(%r8, %rax, 4), %ecx
	{nf}	addq	$123, 291(%r8, %rax, 4)
	{nf}	add	$123, 291(%r8, %rax, 4), %r9
	{nf}	add	%bl, %dl
	{nf}	add	%bl, %dl, %r8b
	{nf}	add	%bl, 291(%r8, %rax, 4)
	{nf}	add	%bl, 291(%r8, %rax, 4), %dl
	{nf}	add	%dx, %ax
	{nf}	add	%dx, %ax, %r9w
	{nf}	add	%dx, 291(%r8, %rax, 4)
	{nf}	add	%dx, 291(%r8, %rax, 4), %ax
	{nf}	add	%ecx, %edx
	{nf}	add	%ecx, %edx, %r10d
	{nf}	add	%ecx, 291(%r8, %rax, 4)
	{nf}	add	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	add	%r9, %r31
	{nf}	add	%r9, %r31, %r11
	{nf}	add	%r9, 291(%r8, %rax, 4)
	{nf}	add	%r9, 291(%r8, %rax, 4), %r31
	{nf}	add	291(%r8, %rax, 4), %bl
	{nf}	add	291(%r8, %rax, 4), %bl, %dl
	{nf}	add	291(%r8, %rax, 4), %dx
	{nf}	add	291(%r8, %rax, 4), %dx, %ax
	{nf}	add	291(%r8, %rax, 4), %ecx
	{nf}	add	291(%r8, %rax, 4), %ecx, %edx
	{nf}	add	291(%r8, %rax, 4), %r9
	{nf}	add	291(%r8, %rax, 4), %r9, %r31
	{nf}	and	$123, %bl
	{nf}	and	$123, %bl, %dl
	{nf}	and	$123, %dx
	{nf}	and	$123, %dx, %ax
	{nf}	and	$123, %ecx
	{nf}	and	$123, %ecx, %edx
	{nf}	and	$123, %r9
	{nf}	and	$123, %r9, %r31
	{nf}	andb	$123, 291(%r8, %rax, 4)
	{nf}	and	$123, 291(%r8, %rax, 4), %bl
	{nf}	andw	$123, 291(%r8, %rax, 4)
	{nf}	and	$123, 291(%r8, %rax, 4), %dx
	{nf}	andl	$123, 291(%r8, %rax, 4)
	{nf}	and	$123, 291(%r8, %rax, 4), %ecx
	{nf}	andq	$123, 291(%r8, %rax, 4)
	{nf}	and	$123, 291(%r8, %rax, 4), %r9
	{nf}	and	%bl, %dl
	{nf}	and	%bl, %dl, %r8b
	{nf}	and	%bl, 291(%r8, %rax, 4)
	{nf}	and	%bl, 291(%r8, %rax, 4), %dl
	{nf}	and	%dx, %ax
	{nf}	and	%dx, %ax, %r9w
	{nf}	and	%dx, 291(%r8, %rax, 4)
	{nf}	and	%dx, 291(%r8, %rax, 4), %ax
	{nf}	and	%ecx, %edx
	{nf}	and	%ecx, %edx, %r10d
	{nf}	and	%ecx, 291(%r8, %rax, 4)
	{nf}	and	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	and	%r9, %r31
	{nf}	and	%r9, %r31, %r11
	{nf}	and	%r9, 291(%r8, %rax, 4)
	{nf}	and	%r9, 291(%r8, %rax, 4), %r31
	{nf}	and	291(%r8, %rax, 4), %bl
	{nf}	and	291(%r8, %rax, 4), %bl, %dl
	{nf}	and	291(%r8, %rax, 4), %dx
	{nf}	and	291(%r8, %rax, 4), %dx, %ax
	{nf}	and	291(%r8, %rax, 4), %ecx
	{nf}	and	291(%r8, %rax, 4), %ecx, %edx
	{nf}	and	291(%r8, %rax, 4), %r9
	{nf}	and	291(%r8, %rax, 4), %r9, %r31
	{nf}	andn	%ecx, %edx, %r10d
	{nf}	andn	%r9, %r31, %r11
	{nf}	andn	291(%r8, %rax, 4), %ecx, %edx
	{nf}	andn	291(%r8, %rax, 4), %r9, %r31
	{nf}	bextr	%ecx, %edx, %r10d
	{nf}	bextr	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	bextr	%r9, %r31, %r11
	{nf}	bextr	%r9, 291(%r8, %rax, 4), %r31
	{nf}	blsi	%ecx, %edx
	{nf}	blsi	%r9, %r31
	{nf}	blsi	291(%r8, %rax, 4), %ecx
	{nf}	blsi	291(%r8, %rax, 4), %r9
	{nf}	blsmsk	%ecx, %edx
	{nf}	blsmsk	%r9, %r31
	{nf}	blsmsk	291(%r8, %rax, 4), %ecx
	{nf}	blsmsk	291(%r8, %rax, 4), %r9
	{nf}	blsr	%ecx, %edx
	{nf}	blsr	%r9, %r31
	{nf}	blsr	291(%r8, %rax, 4), %ecx
	{nf}	blsr	291(%r8, %rax, 4), %r9
	{nf}	bzhi	%ecx, %edx, %r10d
	{nf}	bzhi	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	bzhi	%r9, %r31, %r11
	{nf}	bzhi	%r9, 291(%r8, %rax, 4), %r31
	{nf}	clr	%r31
	{nf}	dec	%bl
	{nf}	dec	%bl, %dl
	{nf}	dec	%dx
	{nf}	dec	%dx, %ax
	{nf}	dec	%ecx
	{nf}	dec	%ecx, %edx
	{nf}	dec	%r9
	{nf}	dec	%r9, %r31
	{nf}	decb	291(%r8, %rax, 4)
	{nf}	dec	291(%r8, %rax, 4), %bl
	{nf}	decw	291(%r8, %rax, 4)
	{nf}	dec	291(%r8, %rax, 4), %dx
	{nf}	decl	291(%r8, %rax, 4)
	{nf}	dec	291(%r8, %rax, 4), %ecx
	{nf}	decq	291(%r8, %rax, 4)
	{nf}	dec	291(%r8, %rax, 4), %r9
	{nf}	div	%bl
	{nf}	div	%dx
	{nf}	div	%ecx
	{nf}	div	%r9
	{nf}	divb	291(%r8, %rax, 4)
	{nf}	divw	291(%r8, %rax, 4)
	{nf}	divl	291(%r8, %rax, 4)
	{nf}	divq	291(%r8, %rax, 4)
	{nf}	idiv	%bl
	{nf}	idiv	%bl, %al
	{nf}	idiv	%dx
	{nf}	idiv	%dx, %ax
	{nf}	idiv	%ecx
	{nf}	idiv	%ecx, %eax
	{nf}	idiv	%r9
	{nf}	idiv	%r9, %rax
	{nf}	idivb	291(%r8, %rax, 4)
	{nf}	idivb	291(%r8, %rax, 4), %al
	{nf}	idivw	291(%r8, %rax, 4)
	{nf}	idivw	291(%r8, %rax, 4), %ax
	{nf}	idivl	291(%r8, %rax, 4)
	{nf}	idivl	291(%r8, %rax, 4), %eax
	{nf}	idivq	291(%r8, %rax, 4)
	{nf}	idivq	291(%r8, %rax, 4), %rax
	{nf}	imul	%bl
	{nf}	imul	%dx
	{nf}	imul	%dx, %ax
	{nf}	imul	%dx, %ax, %r9w
	{nf}	imul	%ecx
	{nf}	imul	%ecx, %edx
	{nf}	imul	%ecx, %edx, %r10d
	{nf}	imul	%r9
	{nf}	imul	%r9, %r31
	{nf}	imul	%r9, %r31, %r11
	{nf}	imulb	291(%r8, %rax, 4)
	{nf}	imulw	291(%r8, %rax, 4)
	{nf}	imul	291(%r8, %rax, 4), %dx
	{nf}	imul	291(%r8, %rax, 4), %dx, %ax
	{nf}	imull	291(%r8, %rax, 4)
	{nf}	imul	291(%r8, %rax, 4), %ecx
	{nf}	imul	291(%r8, %rax, 4), %ecx, %edx
	{nf}	imulq	291(%r8, %rax, 4)
	{nf}	imul	291(%r8, %rax, 4), %r9
	{nf}	imul	291(%r8, %rax, 4), %r9, %r31
	{nf}	imul	$0x7b, %dx, %ax
	{nf}	imul	$0x7b, %ecx, %edx
	{nf}	imul	$0x7b, %r9, %r15
	{nf}	imul	$0x7b, %r9
	{nf}	imul	$0x7b, 291(%r8, %rax, 4), %dx
	{nf}	imul	$0x7b, 291(%r8, %rax, 4), %ecx
	{nf}	imul	$0x7b, 291(%r8, %rax, 4), %r9
	{nf}	imul	$0xff90, %dx, %ax
	{nf}	imul	$0xff90, %ecx, %edx
	{nf}	imul	$0xff90, %r9, %r15
	{nf}	imul	$0xff90, %r9
	{nf}	imul	$0xff90, 291(%r8, %rax, 4), %dx
	{nf}	imul	$0xff90, 291(%r8, %rax, 4), %ecx
	{nf}	imul	$0xff90, 291(%r8, %rax, 4), %r9
	{nf}	inc	%bl
	{nf}	inc	%bl, %dl
	{nf}	inc	%dx
	{nf}	inc	%dx, %ax
	{nf}	inc	%ecx
	{nf}	inc	%ecx, %edx
	{nf}	inc	%r9
	{nf}	inc	%r9, %r31
	{nf}	incb	291(%r8, %rax, 4)
	{nf}	inc	291(%r8, %rax, 4), %bl
	{nf}	incw	291(%r8, %rax, 4)
	{nf}	inc	291(%r8, %rax, 4), %dx
	{nf}	incl	291(%r8, %rax, 4)
	{nf}	inc	291(%r8, %rax, 4), %ecx
	{nf}	incq	291(%r8, %rax, 4)
	{nf}	inc	291(%r8, %rax, 4), %r9
	{nf}	lzcnt	%dx, %ax
	{nf}	lzcnt	%ecx, %edx
	{nf}	lzcnt	%r9, %r31
	{nf}	lzcnt	291(%r8, %rax, 4), %dx
	{nf}	lzcnt	291(%r8, %rax, 4), %ecx
	{nf}	lzcnt	291(%r8, %rax, 4), %r9
	{nf}	mul	%bl
	{nf}	mul	%dx
	{nf}	mul	%ecx
	{nf}	mul	%r9
	{nf}	mulb	291(%r8, %rax, 4)
	{nf}	mulw	291(%r8, %rax, 4)
	{nf}	mull	291(%r8, %rax, 4)
	{nf}	mulq	291(%r8, %rax, 4)
	{nf}	neg	%bl
	{nf}	neg	%bl, %dl
	{nf}	neg	%dx
	{nf}	neg	%dx, %ax
	{nf}	neg	%ecx
	{nf}	neg	%ecx, %edx
	{nf}	neg	%r9
	{nf}	neg	%r9, %r31
	{nf}	negb	291(%r8, %rax, 4)
	{nf}	neg	291(%r8, %rax, 4), %bl
	{nf}	negw	291(%r8, %rax, 4)
	{nf}	neg	291(%r8, %rax, 4), %dx
	{nf}	negl	291(%r8, %rax, 4)
	{nf}	neg	291(%r8, %rax, 4), %ecx
	{nf}	negq	291(%r8, %rax, 4)
	{nf}	neg	291(%r8, %rax, 4), %r9
	{nf}	or	$123, %bl
	{nf}	or	$123, %bl, %dl
	{nf}	or	$123, %dx
	{nf}	or	$123, %dx, %ax
	{nf}	or	$123, %ecx
	{nf}	or	$123, %ecx, %edx
	{nf}	or	$123, %r9
	{nf}	or	$123, %r9, %r31
	{nf}	orb	$123, 291(%r8, %rax, 4)
	{nf}	or	$123, 291(%r8, %rax, 4), %bl
	{nf}	orw	$123, 291(%r8, %rax, 4)
	{nf}	or	$123, 291(%r8, %rax, 4), %dx
	{nf}	orl	$123, 291(%r8, %rax, 4)
	{nf}	or	$123, 291(%r8, %rax, 4), %ecx
	{nf}	orq	$123, 291(%r8, %rax, 4)
	{nf}	or	$123, 291(%r8, %rax, 4), %r9
	{nf}	or	%bl, %dl
	{nf}	or	%bl, %dl, %r8b
	{nf}	or	%bl, 291(%r8, %rax, 4)
	{nf}	or	%bl, 291(%r8, %rax, 4), %dl
	{nf}	or	%dx, %ax
	{nf}	or	%dx, %ax, %r9w
	{nf}	or	%dx, 291(%r8, %rax, 4)
	{nf}	or	%dx, 291(%r8, %rax, 4), %ax
	{nf}	or	%ecx, %edx
	{nf}	or	%ecx, %edx, %r10d
	{nf}	or	%ecx, 291(%r8, %rax, 4)
	{nf}	or	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	or	%r9, %r31
	{nf}	or	%r9, %r31, %r11
	{nf}	or	%r9, 291(%r8, %rax, 4)
	{nf}	or	%r9, 291(%r8, %rax, 4), %r31
	{nf}	or	291(%r8, %rax, 4), %bl
	{nf}	or	291(%r8, %rax, 4), %bl, %dl
	{nf}	or	291(%r8, %rax, 4), %dx
	{nf}	or	291(%r8, %rax, 4), %dx, %ax
	{nf}	or	291(%r8, %rax, 4), %ecx
	{nf}	or	291(%r8, %rax, 4), %ecx, %edx
	{nf}	or	291(%r8, %rax, 4), %r9
	{nf}	or	291(%r8, %rax, 4), %r9, %r31
	{nf}	popcnt	%dx, %ax
	{nf}	popcnt	%ecx, %edx
	{nf}	popcnt	%r9, %r31
	{nf}	popcnt	291(%r8, %rax, 4), %dx
	{nf}	popcnt	291(%r8, %rax, 4), %ecx
	{nf}	popcnt	291(%r8, %rax, 4), %r9
	{nf}	rol	$1, %bl
	{nf}	rol	$1, %bl, %dl
	{nf}	rol	$1, %dx
	{nf}	rol	$1, %dx, %ax
	{nf}	rol	$1, %ecx
	{nf}	rol	$1, %ecx, %edx
	{nf}	rol	$1, %r9
	{nf}	rol	$1, %r9, %r31
	{nf}	rolb	$1, 291(%r8, %rax, 4)
	{nf}	rol	$1, 291(%r8, %rax, 4), %bl
	{nf}	rolw	$1, 291(%r8, %rax, 4)
	{nf}	rol	$1, 291(%r8, %rax, 4), %dx
	{nf}	roll	$1, 291(%r8, %rax, 4)
	{nf}	rol	$1, 291(%r8, %rax, 4), %ecx
	{nf}	rolq	$1, 291(%r8, %rax, 4)
	{nf}	rol	$1, 291(%r8, %rax, 4), %r9
	{nf}	rol	$123, %bl
	{nf}	rol	$123, %bl, %dl
	{nf}	rol	$123, %dx
	{nf}	rol	$123, %dx, %ax
	{nf}	rol	$123, %ecx
	{nf}	rol	$123, %ecx, %edx
	{nf}	rol	$123, %r9
	{nf}	rol	$123, %r9, %r31
	{nf}	rolb	$123, 291(%r8, %rax, 4)
	{nf}	rol	$123, 291(%r8, %rax, 4), %bl
	{nf}	rolw	$123, 291(%r8, %rax, 4)
	{nf}	rol	$123, 291(%r8, %rax, 4), %dx
	{nf}	roll	$123, 291(%r8, %rax, 4)
	{nf}	rol	$123, 291(%r8, %rax, 4), %ecx
	{nf}	rolq	$123, 291(%r8, %rax, 4)
	{nf}	rol	$123, 291(%r8, %rax, 4), %r9
	{nf}	rol	%cl, %bl
	{nf}	rol	%cl, %bl, %dl
	{nf}	rol	%cl, %dx
	{nf}	rol	%cl, %dx, %ax
	{nf}	rol	%cl, %ecx
	{nf}	rol	%cl, %ecx, %edx
	{nf}	rol	%cl, %r9
	{nf}	rol	%cl, %r9, %r31
	{nf}	rolb	%cl, 291(%r8, %rax, 4)
	{nf}	rol	%cl, 291(%r8, %rax, 4), %bl
	{nf}	rolw	%cl, 291(%r8, %rax, 4)
	{nf}	rol	%cl, 291(%r8, %rax, 4), %dx
	{nf}	roll	%cl, 291(%r8, %rax, 4)
	{nf}	rol	%cl, 291(%r8, %rax, 4), %ecx
	{nf}	rolq	%cl, 291(%r8, %rax, 4)
	{nf}	rol	%cl, 291(%r8, %rax, 4), %r9
	{nf}	ror	$1, %bl
	{nf}	ror	$1, %bl, %dl
	{nf}	ror	$1, %dx
	{nf}	ror	$1, %dx, %ax
	{nf}	ror	$1, %ecx
	{nf}	ror	$1, %ecx, %edx
	{nf}	ror	$1, %r9
	{nf}	ror	$1, %r9, %r31
	{nf}	rorb	$1, 291(%r8, %rax, 4)
	{nf}	ror	$1, 291(%r8, %rax, 4), %bl
	{nf}	rorw	$1, 291(%r8, %rax, 4)
	{nf}	ror	$1, 291(%r8, %rax, 4), %dx
	{nf}	rorl	$1, 291(%r8, %rax, 4)
	{nf}	ror	$1, 291(%r8, %rax, 4), %ecx
	{nf}	rorq	$1, 291(%r8, %rax, 4)
	{nf}	ror	$1, 291(%r8, %rax, 4), %r9
	{nf}	ror	$123, %bl
	{nf}	ror	$123, %bl, %dl
	{nf}	ror	$123, %dx
	{nf}	ror	$123, %dx, %ax
	{nf}	ror	$123, %ecx
	{nf}	ror	$123, %ecx, %edx
	{nf}	ror	$123, %r9
	{nf}	ror	$123, %r9, %r31
	{nf}	rorb	$123, 291(%r8, %rax, 4)
	{nf}	ror	$123, 291(%r8, %rax, 4), %bl
	{nf}	rorw	$123, 291(%r8, %rax, 4)
	{nf}	ror	$123, 291(%r8, %rax, 4), %dx
	{nf}	rorl	$123, 291(%r8, %rax, 4)
	{nf}	ror	$123, 291(%r8, %rax, 4), %ecx
	{nf}	rorq	$123, 291(%r8, %rax, 4)
	{nf}	ror	$123, 291(%r8, %rax, 4), %r9
	{nf}	ror	%cl, %bl
	{nf}	ror	%cl, %bl, %dl
	{nf}	ror	%cl, %dx
	{nf}	ror	%cl, %dx, %ax
	{nf}	ror	%cl, %ecx
	{nf}	ror	%cl, %ecx, %edx
	{nf}	ror	%cl, %r9
	{nf}	ror	%cl, %r9, %r31
	{nf}	rorb	%cl, 291(%r8, %rax, 4)
	{nf}	ror	%cl, 291(%r8, %rax, 4), %bl
	{nf}	rorw	%cl, 291(%r8, %rax, 4)
	{nf}	ror	%cl, 291(%r8, %rax, 4), %dx
	{nf}	rorl	%cl, 291(%r8, %rax, 4)
	{nf}	ror	%cl, 291(%r8, %rax, 4), %ecx
	{nf}	rorq	%cl, 291(%r8, %rax, 4)
	{nf}	ror	%cl, 291(%r8, %rax, 4), %r9
	{nf}	sal	$1, %bl
	{nf}	sal	$1, %bl, %dl
	{nf}	sal	$1, %dx
	{nf}	sal	$1, %dx, %ax
	{nf}	sal	$1, %ecx
	{nf}	sal	$1, %ecx, %edx
	{nf}	sal	$1, %r9
	{nf}	sal	$1, %r9, %r31
	{nf}	salb	$1, 291(%r8, %rax, 4)
	{nf}	sal	$1, 291(%r8, %rax, 4), %bl
	{nf}	salw	$1, 291(%r8, %rax, 4)
	{nf}	sal	$1, 291(%r8, %rax, 4), %dx
	{nf}	sall	$1, 291(%r8, %rax, 4)
	{nf}	sal	$1, 291(%r8, %rax, 4), %ecx
	{nf}	salq	$1, 291(%r8, %rax, 4)
	{nf}	sal	$1, 291(%r8, %rax, 4), %r9
	{nf}	sal	$123, %bl
	{nf}	sal	$123, %bl, %dl
	{nf}	sal	$123, %dx
	{nf}	sal	$123, %dx, %ax
	{nf}	sal	$123, %ecx
	{nf}	sal	$123, %ecx, %edx
	{nf}	sal	$123, %r9
	{nf}	sal	$123, %r9, %r31
	{nf}	salb	$123, 291(%r8, %rax, 4)
	{nf}	sal	$123, 291(%r8, %rax, 4), %bl
	{nf}	salw	$123, 291(%r8, %rax, 4)
	{nf}	sal	$123, 291(%r8, %rax, 4), %dx
	{nf}	sall	$123, 291(%r8, %rax, 4)
	{nf}	sal	$123, 291(%r8, %rax, 4), %ecx
	{nf}	salq	$123, 291(%r8, %rax, 4)
	{nf}	sal	$123, 291(%r8, %rax, 4), %r9
	{nf}	sal	%cl, %bl
	{nf}	sal	%cl, %bl, %dl
	{nf}	sal	%cl, %dx
	{nf}	sal	%cl, %dx, %ax
	{nf}	sal	%cl, %ecx
	{nf}	sal	%cl, %ecx, %edx
	{nf}	sal	%cl, %r9
	{nf}	sal	%cl, %r9, %r31
	{nf}	salb	%cl, 291(%r8, %rax, 4)
	{nf}	sal	%cl, 291(%r8, %rax, 4), %bl
	{nf}	salw	%cl, 291(%r8, %rax, 4)
	{nf}	sal	%cl, 291(%r8, %rax, 4), %dx
	{nf}	sall	%cl, 291(%r8, %rax, 4)
	{nf}	sal	%cl, 291(%r8, %rax, 4), %ecx
	{nf}	salq	%cl, 291(%r8, %rax, 4)
	{nf}	sal	%cl, 291(%r8, %rax, 4), %r9
	{nf}	sar	$1, %bl
	{nf}	sar	$1, %bl, %dl
	{nf}	sar	$1, %dx
	{nf}	sar	$1, %dx, %ax
	{nf}	sar	$1, %ecx
	{nf}	sar	$1, %ecx, %edx
	{nf}	sar	$1, %r9
	{nf}	sar	$1, %r9, %r31
	{nf}	sarb	$1, 291(%r8, %rax, 4)
	{nf}	sar	$1, 291(%r8, %rax, 4), %bl
	{nf}	sarw	$1, 291(%r8, %rax, 4)
	{nf}	sar	$1, 291(%r8, %rax, 4), %dx
	{nf}	sarl	$1, 291(%r8, %rax, 4)
	{nf}	sar	$1, 291(%r8, %rax, 4), %ecx
	{nf}	sarq	$1, 291(%r8, %rax, 4)
	{nf}	sar	$1, 291(%r8, %rax, 4), %r9
	{nf}	sar	$123, %bl
	{nf}	sar	$123, %bl, %dl
	{nf}	sar	$123, %dx
	{nf}	sar	$123, %dx, %ax
	{nf}	sar	$123, %ecx
	{nf}	sar	$123, %ecx, %edx
	{nf}	sar	$123, %r9
	{nf}	sar	$123, %r9, %r31
	{nf}	sarb	$123, 291(%r8, %rax, 4)
	{nf}	sar	$123, 291(%r8, %rax, 4), %bl
	{nf}	sarw	$123, 291(%r8, %rax, 4)
	{nf}	sar	$123, 291(%r8, %rax, 4), %dx
	{nf}	sarl	$123, 291(%r8, %rax, 4)
	{nf}	sar	$123, 291(%r8, %rax, 4), %ecx
	{nf}	sarq	$123, 291(%r8, %rax, 4)
	{nf}	sar	$123, 291(%r8, %rax, 4), %r9
	{nf}	sar	%cl, %bl
	{nf}	sar	%cl, %bl, %dl
	{nf}	sar	%cl, %dx
	{nf}	sar	%cl, %dx, %ax
	{nf}	sar	%cl, %ecx
	{nf}	sar	%cl, %ecx, %edx
	{nf}	sar	%cl, %r9
	{nf}	sar	%cl, %r9, %r31
	{nf}	sarb	%cl, 291(%r8, %rax, 4)
	{nf}	sar	%cl, 291(%r8, %rax, 4), %bl
	{nf}	sarw	%cl, 291(%r8, %rax, 4)
	{nf}	sar	%cl, 291(%r8, %rax, 4), %dx
	{nf}	sarl	%cl, 291(%r8, %rax, 4)
	{nf}	sar	%cl, 291(%r8, %rax, 4), %ecx
	{nf}	sarq	%cl, 291(%r8, %rax, 4)
	{nf}	sar	%cl, 291(%r8, %rax, 4), %r9
	{nf}	shl	$1, %bl
	{nf}	shl	$1, %bl, %dl
	{nf}	shl	$1, %dx
	{nf}	shl	$1, %dx, %ax
	{nf}	shl	$1, %ecx
	{nf}	shl	$1, %ecx, %edx
	{nf}	shl	$1, %r9
	{nf}	shl	$1, %r9, %r31
	{nf}	shlb	$1, 291(%r8, %rax, 4)
	{nf}	shl	$1, 291(%r8, %rax, 4), %bl
	{nf}	shlw	$1, 291(%r8, %rax, 4)
	{nf}	shl	$1, 291(%r8, %rax, 4), %dx
	{nf}	shll	$1, 291(%r8, %rax, 4)
	{nf}	shl	$1, 291(%r8, %rax, 4), %ecx
	{nf}	shlq	$1, 291(%r8, %rax, 4)
	{nf}	shl	$1, 291(%r8, %rax, 4), %r9
	{nf}	shl	$123, %bl
	{nf}	shl	$123, %bl, %dl
	{nf}	shl	$123, %dx
	{nf}	shl	$123, %dx, %ax
	{nf}	shl	$123, %ecx
	{nf}	shl	$123, %ecx, %edx
	{nf}	shl	$123, %r9
	{nf}	shl	$123, %r9, %r31
	{nf}	shlb	$123, 291(%r8, %rax, 4)
	{nf}	shl	$123, 291(%r8, %rax, 4), %bl
	{nf}	shlw	$123, 291(%r8, %rax, 4)
	{nf}	shl	$123, 291(%r8, %rax, 4), %dx
	{nf}	shll	$123, 291(%r8, %rax, 4)
	{nf}	shl	$123, 291(%r8, %rax, 4), %ecx
	{nf}	shlq	$123, 291(%r8, %rax, 4)
	{nf}	shl	$123, 291(%r8, %rax, 4), %r9
	{nf}	shl	%cl, %bl
	{nf}	shl	%cl, %bl, %dl
	{nf}	shl	%cl, %dx
	{nf}	shl	%cl, %dx, %ax
	{nf}	shl	%cl, %ecx
	{nf}	shl	%cl, %ecx, %edx
	{nf}	shl	%cl, %r9
	{nf}	shl	%cl, %r9, %r31
	{nf}	shlb	%cl, 291(%r8, %rax, 4)
	{nf}	shl	%cl, 291(%r8, %rax, 4), %bl
	{nf}	shlw	%cl, 291(%r8, %rax, 4)
	{nf}	shl	%cl, 291(%r8, %rax, 4), %dx
	{nf}	shll	%cl, 291(%r8, %rax, 4)
	{nf}	shl	%cl, 291(%r8, %rax, 4), %ecx
	{nf}	shlq	%cl, 291(%r8, %rax, 4)
	{nf}	shl	%cl, 291(%r8, %rax, 4), %r9
	{nf}	shld	$123, %dx, %ax
	{nf}	shld	$123, %dx, %ax, %r9w
	{nf}	shld	$123, %dx, 291(%r8, %rax, 4)
	{nf}	shld	$123, %dx, 291(%r8, %rax, 4), %ax
	{nf}	shld	$123, %ecx, %edx
	{nf}	shld	$123, %ecx, %edx, %r10d
	{nf}	shld	$123, %ecx, 291(%r8, %rax, 4)
	{nf}	shld	$123, %ecx, 291(%r8, %rax, 4), %edx
	{nf}	shld	$123, %r9, %r31
	{nf}	shld	$123, %r9, %r31, %r11
	{nf}	shld	$123, %r9, 291(%r8, %rax, 4)
	{nf}	shld	$123, %r9, 291(%r8, %rax, 4), %r31
	{nf}	shld	%cl, %dx, %ax
	{nf}	shld	%cl, %dx, %ax, %r9w
	{nf}	shld	%cl, %dx, 291(%r8, %rax, 4)
	{nf}	shld	%cl, %dx, 291(%r8, %rax, 4), %ax
	{nf}	shld	%cl, %ecx, %edx
	{nf}	shld	%cl, %ecx, %edx, %r10d
	{nf}	shld	%cl, %ecx, 291(%r8, %rax, 4)
	{nf}	shld	%cl, %ecx, 291(%r8, %rax, 4), %edx
	{nf}	shld	%cl, %r9, %r31
	{nf}	shld	%cl, %r9, %r31, %r11
	{nf}	shld	%cl, %r9, 291(%r8, %rax, 4)
	{nf}	shld	%cl, %r9, 291(%r8, %rax, 4), %r31
	{nf}	shr	$1, %bl
	{nf}	shr	$1, %bl, %dl
	{nf}	shr	$1, %dx
	{nf}	shr	$1, %dx, %ax
	{nf}	shr	$1, %ecx
	{nf}	shr	$1, %ecx, %edx
	{nf}	shr	$1, %r9
	{nf}	shr	$1, %r9, %r31
	{nf}	shrb	$1, 291(%r8, %rax, 4)
	{nf}	shr	$1, 291(%r8, %rax, 4), %bl
	{nf}	shrw	$1, 291(%r8, %rax, 4)
	{nf}	shr	$1, 291(%r8, %rax, 4), %dx
	{nf}	shrl	$1, 291(%r8, %rax, 4)
	{nf}	shr	$1, 291(%r8, %rax, 4), %ecx
	{nf}	shrq	$1, 291(%r8, %rax, 4)
	{nf}	shr	$1, 291(%r8, %rax, 4), %r9
	{nf}	shr	$123, %bl
	{nf}	shr	$123, %bl, %dl
	{nf}	shr	$123, %dx
	{nf}	shr	$123, %dx, %ax
	{nf}	shr	$123, %ecx
	{nf}	shr	$123, %ecx, %edx
	{nf}	shr	$123, %r9
	{nf}	shr	$123, %r9, %r31
	{nf}	shrb	$123, 291(%r8, %rax, 4)
	{nf}	shr	$123, 291(%r8, %rax, 4), %bl
	{nf}	shrw	$123, 291(%r8, %rax, 4)
	{nf}	shr	$123, 291(%r8, %rax, 4), %dx
	{nf}	shrl	$123, 291(%r8, %rax, 4)
	{nf}	shr	$123, 291(%r8, %rax, 4), %ecx
	{nf}	shrq	$123, 291(%r8, %rax, 4)
	{nf}	shr	$123, 291(%r8, %rax, 4), %r9
	{nf}	shr	%cl, %bl
	{nf}	shr	%cl, %bl, %dl
	{nf}	shr	%cl, %dx
	{nf}	shr	%cl, %dx, %ax
	{nf}	shr	%cl, %ecx
	{nf}	shr	%cl, %ecx, %edx
	{nf}	shr	%cl, %r9
	{nf}	shr	%cl, %r9, %r31
	{nf}	shrb	%cl, 291(%r8, %rax, 4)
	{nf}	shr	%cl, 291(%r8, %rax, 4), %bl
	{nf}	shrw	%cl, 291(%r8, %rax, 4)
	{nf}	shr	%cl, 291(%r8, %rax, 4), %dx
	{nf}	shrl	%cl, 291(%r8, %rax, 4)
	{nf}	shr	%cl, 291(%r8, %rax, 4), %ecx
	{nf}	shrq	%cl, 291(%r8, %rax, 4)
	{nf}	shr	%cl, 291(%r8, %rax, 4), %r9
	{nf}	shrd	$123, %dx, %ax
	{nf}	shrd	$123, %dx, %ax, %r9w
	{nf}	shrd	$123, %dx, 291(%r8, %rax, 4)
	{nf}	shrd	$123, %dx, 291(%r8, %rax, 4), %ax
	{nf}	shrd	$123, %ecx, %edx
	{nf}	shrd	$123, %ecx, %edx, %r10d
	{nf}	shrd	$123, %ecx, 291(%r8, %rax, 4)
	{nf}	shrd	$123, %ecx, 291(%r8, %rax, 4), %edx
	{nf}	shrd	$123, %r9, %r31
	{nf}	shrd	$123, %r9, %r31, %r11
	{nf}	shrd	$123, %r9, 291(%r8, %rax, 4)
	{nf}	shrd	$123, %r9, 291(%r8, %rax, 4), %r31
	{nf}	shrd	%cl, %dx, %ax
	{nf}	shrd	%cl, %dx, %ax, %r9w
	{nf}	shrd	%cl, %dx, 291(%r8, %rax, 4)
	{nf}	shrd	%cl, %dx, 291(%r8, %rax, 4), %ax
	{nf}	shrd	%cl, %ecx, %edx
	{nf}	shrd	%cl, %ecx, %edx, %r10d
	{nf}	shrd	%cl, %ecx, 291(%r8, %rax, 4)
	{nf}	shrd	%cl, %ecx, 291(%r8, %rax, 4), %edx
	{nf}	shrd	%cl, %r9, %r31
	{nf}	shrd	%cl, %r9, %r31, %r11
	{nf}	shrd	%cl, %r9, 291(%r8, %rax, 4)
	{nf}	shrd	%cl, %r9, 291(%r8, %rax, 4), %r31
	{nf}	sub	$123, %bl
	{nf}	sub	$123, %bl, %dl
	{nf}	sub	$123, %dx
	{nf}	sub	$123, %dx, %ax
	{nf}	sub	$123, %ecx
	{nf}	sub	$123, %ecx, %edx
	{nf}	sub	$123, %r9
	{nf}	sub	$123, %r9, %r31
	{nf}	subb	$123, 291(%r8, %rax, 4)
	{nf}	sub	$123, 291(%r8, %rax, 4), %bl
	{nf}	subw	$123, 291(%r8, %rax, 4)
	{nf}	sub	$123, 291(%r8, %rax, 4), %dx
	{nf}	subl	$123, 291(%r8, %rax, 4)
	{nf}	sub	$123, 291(%r8, %rax, 4), %ecx
	{nf}	subq	$123, 291(%r8, %rax, 4)
	{nf}	sub	$123, 291(%r8, %rax, 4), %r9
	{nf}	sub	%bl, %dl
	{nf}	sub	%bl, %dl, %r8b
	{nf}	sub	%bl, 291(%r8, %rax, 4)
	{nf}	sub	%bl, 291(%r8, %rax, 4), %dl
	{nf}	sub	%dx, %ax
	{nf}	sub	%dx, %ax, %r9w
	{nf}	sub	%dx, 291(%r8, %rax, 4)
	{nf}	sub	%dx, 291(%r8, %rax, 4), %ax
	{nf}	sub	%ecx, %edx
	{nf}	sub	%ecx, %edx, %r10d
	{nf}	sub	%ecx, 291(%r8, %rax, 4)
	{nf}	sub	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	sub	%r9, %r31
	{nf}	sub	%r9, %r31, %r11
	{nf}	sub	%r9, 291(%r8, %rax, 4)
	{nf}	sub	%r9, 291(%r8, %rax, 4), %r31
	{nf}	sub	291(%r8, %rax, 4), %bl
	{nf}	sub	291(%r8, %rax, 4), %bl, %dl
	{nf}	sub	291(%r8, %rax, 4), %dx
	{nf}	sub	291(%r8, %rax, 4), %dx, %ax
	{nf}	sub	291(%r8, %rax, 4), %ecx
	{nf}	sub	291(%r8, %rax, 4), %ecx, %edx
	{nf}	sub	291(%r8, %rax, 4), %r9
	{nf}	sub	291(%r8, %rax, 4), %r9, %r31
	{nf}	tzcnt	%dx, %ax
	{nf}	tzcnt	%ecx, %edx
	{nf}	tzcnt	%r9, %r31
	{nf}	tzcnt	291(%r8, %rax, 4), %dx
	{nf}	tzcnt	291(%r8, %rax, 4), %ecx
	{nf}	tzcnt	291(%r8, %rax, 4), %r9
	{nf}	xor	$123, %bl
	{nf}	xor	$123, %bl, %dl
	{nf}	xor	$123, %dx
	{nf}	xor	$123, %dx, %ax
	{nf}	xor	$123, %ecx
	{nf}	xor	$123, %ecx, %edx
	{nf}	xor	$123, %r9
	{nf}	xor	$123, %r9, %r31
	{nf}	xorb	$123, 291(%r8, %rax, 4)
	{nf}	xor	$123, 291(%r8, %rax, 4), %bl
	{nf}	xorw	$123, 291(%r8, %rax, 4)
	{nf}	xor	$123, 291(%r8, %rax, 4), %dx
	{nf}	xorl	$123, 291(%r8, %rax, 4)
	{nf}	xor	$123, 291(%r8, %rax, 4), %ecx
	{nf}	xorq	$123, 291(%r8, %rax, 4)
	{nf}	xor	$123, 291(%r8, %rax, 4), %r9
	{nf}	xor	%bl, %dl
	{nf}	xor	%bl, %dl, %r8b
	{nf}	xor	%bl, 291(%r8, %rax, 4)
	{nf}	xor	%bl, 291(%r8, %rax, 4), %dl
	{nf}	xor	%dx, %ax
	{nf}	xor	%dx, %ax, %r9w
	{nf}	xor	%dx, 291(%r8, %rax, 4)
	{nf}	xor	%dx, 291(%r8, %rax, 4), %ax
	{nf}	xor	%ecx, %edx
	{nf}	xor	%ecx, %edx, %r10d
	{nf}	xor	%ecx, 291(%r8, %rax, 4)
	{nf}	xor	%ecx, 291(%r8, %rax, 4), %edx
	{nf}	xor	%r9, %r31
	{nf}	xor	%r9, %r31, %r11
	{nf}	xor	%r9, 291(%r8, %rax, 4)
	{nf}	xor	%r9, 291(%r8, %rax, 4), %r31
	{nf}	xor	291(%r8, %rax, 4), %bl
	{nf}	xor	291(%r8, %rax, 4), %bl, %dl
	{nf}	xor	291(%r8, %rax, 4), %dx
	{nf}	xor	291(%r8, %rax, 4), %dx, %ax
	{nf}	xor	291(%r8, %rax, 4), %ecx
	{nf}	xor	291(%r8, %rax, 4), %ecx, %edx
	{nf}	xor	291(%r8, %rax, 4), %r9
	{nf}	xor	291(%r8, %rax, 4), %r9, %r31

	.intel_syntax noprefix
intel:
	{nf}	add	bl, 123
	{nf}	add	dl, bl, 123
	{nf}	add	dx, 123
	{nf}	add	ax, dx, 123
	{nf}	add	ecx, 123
	{nf}	add	edx, ecx, 123
	{nf}	add	r9, 123
	{nf}	add	r31, r9, 123
	{nf}	add	BYTE PTR [r8+rax*4+291], 123
	{nf}	add	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	add	WORD PTR [r8+rax*4+291], 123
	{nf}	add	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	add	DWORD PTR [r8+rax*4+291], 123
	{nf}	add	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	add	QWORD PTR [r8+rax*4+291], 123
	{nf}	add	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	add	dl, bl
	{nf}	add	r8b, dl, bl
	{nf}	add	BYTE PTR [r8+rax*4+291], bl
	{nf}	add	dl, BYTE PTR [r8+rax*4+291], bl
	{nf}	add	ax, dx
	{nf}	add	r9w, ax, dx
	{nf}	add	WORD PTR [r8+rax*4+291], dx
	{nf}	add	ax, WORD PTR [r8+rax*4+291], dx
	{nf}	add	edx, ecx
	{nf}	add	r10d, edx, ecx
	{nf}	add	DWORD PTR [r8+rax*4+291], ecx
	{nf}	add	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	add	r31, r9
	{nf}	add	r11, r31, r9
	{nf}	add	QWORD PTR [r8+rax*4+291], r9
	{nf}	add	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	add	bl, BYTE PTR [r8+rax*4+291]
	{nf}	add	dl, bl, BYTE PTR [r8+rax*4+291]
	{nf}	add	dx, WORD PTR [r8+rax*4+291]
	{nf}	add	ax, dx, WORD PTR [r8+rax*4+291]
	{nf}	add	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	add	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	add	r9, QWORD PTR [r8+rax*4+291]
	{nf}	add	r31, r9, QWORD PTR [r8+rax*4+291]
	{nf}	and	bl, 123
	{nf}	and	dl, bl, 123
	{nf}	and	dx, 123
	{nf}	and	ax, dx, 123
	{nf}	and	ecx, 123
	{nf}	and	edx, ecx, 123
	{nf}	and	r9, 123
	{nf}	and	r31, r9, 123
	{nf}	and	BYTE PTR [r8+rax*4+291], 123
	{nf}	and	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	and	WORD PTR [r8+rax*4+291], 123
	{nf}	and	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	and	DWORD PTR [r8+rax*4+291], 123
	{nf}	and	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	and	QWORD PTR [r8+rax*4+291], 123
	{nf}	and	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	and	dl, bl
	{nf}	and	r8b, dl, bl
	{nf}	and	BYTE PTR [r8+rax*4+291], bl
	{nf}	and	dl, BYTE PTR [r8+rax*4+291], bl
	{nf}	and	ax, dx
	{nf}	and	r9w, ax, dx
	{nf}	and	WORD PTR [r8+rax*4+291], dx
	{nf}	and	ax, WORD PTR [r8+rax*4+291], dx
	{nf}	and	edx, ecx
	{nf}	and	r10d, edx, ecx
	{nf}	and	DWORD PTR [r8+rax*4+291], ecx
	{nf}	and	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	and	r31, r9
	{nf}	and	r11, r31, r9
	{nf}	and	QWORD PTR [r8+rax*4+291], r9
	{nf}	and	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	and	bl, BYTE PTR [r8+rax*4+291]
	{nf}	and	dl, bl, BYTE PTR [r8+rax*4+291]
	{nf}	and	dx, WORD PTR [r8+rax*4+291]
	{nf}	and	ax, dx, WORD PTR [r8+rax*4+291]
	{nf}	and	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	and	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	and	r9, QWORD PTR [r8+rax*4+291]
	{nf}	and	r31, r9, QWORD PTR [r8+rax*4+291]
	{nf}	andn	r10d, edx, ecx
	{nf}	andn	r11, r31, r9
	{nf}	andn	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	andn	r31, r9, QWORD PTR [r8+rax*4+291]
	{nf}	bextr	r10d, edx, ecx
	{nf}	bextr	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	bextr	r11, r31, r9
	{nf}	bextr	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	blsi	edx, ecx
	{nf}	blsi	r31, r9
	{nf}	blsi	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	blsi	r9, QWORD PTR [r8+rax*4+291]
	{nf}	blsmsk	edx, ecx
	{nf}	blsmsk	r31, r9
	{nf}	blsmsk	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	blsmsk	r9, QWORD PTR [r8+rax*4+291]
	{nf}	blsr	edx, ecx
	{nf}	blsr	r31, r9
	{nf}	blsr	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	blsr	r9, QWORD PTR [r8+rax*4+291]
	{nf}	bzhi	r10d, edx, ecx
	{nf}	bzhi	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	bzhi	r11, r31, r9
	{nf}	bzhi	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	clr	r31
	{nf}	dec	bl
	{nf}	dec	dl, bl
	{nf}	dec	dx
	{nf}	dec	ax, dx
	{nf}	dec	ecx
	{nf}	dec	edx, ecx
	{nf}	dec	r9
	{nf}	dec	r31, r9
	{nf}	dec	BYTE PTR [r8+rax*4+291]
	{nf}	dec	bl, BYTE PTR [r8+rax*4+291]
	{nf}	dec	WORD PTR [r8+rax*4+291]
	{nf}	dec	dx, WORD PTR [r8+rax*4+291]
	{nf}	dec	DWORD PTR [r8+rax*4+291]
	{nf}	dec	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	dec	QWORD PTR [r8+rax*4+291]
	{nf}	dec	r9, QWORD PTR [r8+rax*4+291]
	{nf}	div	bl
	{nf}	div	dx
	{nf}	div	ecx
	{nf}	div	r9
	{nf}	div	BYTE PTR [r8+rax*4+291]
	{nf}	div	WORD PTR [r8+rax*4+291]
	{nf}	div	DWORD PTR [r8+rax*4+291]
	{nf}	div	QWORD PTR [r8+rax*4+291]
	{nf}	idiv	bl
	{nf}	idiv	al, bl
	{nf}	idiv	dx
	{nf}	idiv	ax, dx
	{nf}	idiv	ecx
	{nf}	idiv	eax, ecx
	{nf}	idiv	r9
	{nf}	idiv	rax, r9
	{nf}	idiv	BYTE PTR [r8+rax*4+291]
	{nf}	idiv	al, BYTE PTR [r8+rax*4+291]
	{nf}	idiv	WORD PTR [r8+rax*4+291]
	{nf}	idiv	ax, WORD PTR [r8+rax*4+291]
	{nf}	idiv	DWORD PTR [r8+rax*4+291]
	{nf}	idiv	eax, DWORD PTR [r8+rax*4+291]
	{nf}	idiv	QWORD PTR [r8+rax*4+291]
	{nf}	idiv	rax, QWORD PTR [r8+rax*4+291]
	{nf}	imul	bl
	{nf}	imul	dx
	{nf}	imul	ax, dx
	{nf}	imul	r9w, ax, dx
	{nf}	imul	ecx
	{nf}	imul	edx, ecx
	{nf}	imul	r10d, edx, ecx
	{nf}	imul	r9
	{nf}	imul	r31, r9
	{nf}	imul	r11, r31, r9
	{nf}	imul	BYTE PTR [r8+rax*4+291]
	{nf}	imul	WORD PTR [r8+rax*4+291]
	{nf}	imul	dx, WORD PTR [r8+rax*4+291]
	{nf}	imul	ax, dx, WORD PTR [r8+rax*4+291]
	{nf}	imul	DWORD PTR [r8+rax*4+291]
	{nf}	imul	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	imul	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	imul	QWORD PTR [r8+rax*4+291]
	{nf}	imul	r9, QWORD PTR [r8+rax*4+291]
	{nf}	imul	r31, r9, QWORD PTR [r8+rax*4+291]
	{nf}	inc	bl
	{nf}	inc	dl, bl
	{nf}	inc	dx
	{nf}	inc	ax, dx
	{nf}	inc	ecx
	{nf}	inc	edx, ecx
	{nf}	inc	r9
	{nf}	inc	r31, r9
	{nf}	inc	BYTE PTR [r8+rax*4+291]
	{nf}	inc	bl, BYTE PTR [r8+rax*4+291]
	{nf}	inc	WORD PTR [r8+rax*4+291]
	{nf}	inc	dx, WORD PTR [r8+rax*4+291]
	{nf}	inc	DWORD PTR [r8+rax*4+291]
	{nf}	inc	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	inc	QWORD PTR [r8+rax*4+291]
	{nf}	inc	r9, QWORD PTR [r8+rax*4+291]
	{nf}	lzcnt	ax, dx
	{nf}	lzcnt	edx, ecx
	{nf}	lzcnt	r31, r9
	{nf}	lzcnt	dx, WORD PTR [r8+rax*4+291]
	{nf}	lzcnt	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	lzcnt	r9, QWORD PTR [r8+rax*4+291]
	{nf}	mul	bl
	{nf}	mul	dx
	{nf}	mul	ecx
	{nf}	mul	r9
	{nf}	mul	BYTE PTR [r8+rax*4+291]
	{nf}	mul	WORD PTR [r8+rax*4+291]
	{nf}	mul	DWORD PTR [r8+rax*4+291]
	{nf}	mul	QWORD PTR [r8+rax*4+291]
	{nf}	neg	bl
	{nf}	neg	dl, bl
	{nf}	neg	dx
	{nf}	neg	ax, dx
	{nf}	neg	ecx
	{nf}	neg	edx, ecx
	{nf}	neg	r9
	{nf}	neg	r31, r9
	{nf}	neg	BYTE PTR [r8+rax*4+291]
	{nf}	neg	bl, BYTE PTR [r8+rax*4+291]
	{nf}	neg	WORD PTR [r8+rax*4+291]
	{nf}	neg	dx, WORD PTR [r8+rax*4+291]
	{nf}	neg	DWORD PTR [r8+rax*4+291]
	{nf}	neg	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	neg	QWORD PTR [r8+rax*4+291]
	{nf}	neg	r9, QWORD PTR [r8+rax*4+291]
	{nf}	or	bl, 123
	{nf}	or	dl, bl, 123
	{nf}	or	dx, 123
	{nf}	or	ax, dx, 123
	{nf}	or	ecx, 123
	{nf}	or	edx, ecx, 123
	{nf}	or	r9, 123
	{nf}	or	r31, r9, 123
	{nf}	or	BYTE PTR [r8+rax*4+291], 123
	{nf}	or	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	or	WORD PTR [r8+rax*4+291], 123
	{nf}	or	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	or	DWORD PTR [r8+rax*4+291], 123
	{nf}	or	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	or	QWORD PTR [r8+rax*4+291], 123
	{nf}	or	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	or	dl, bl
	{nf}	or	r8b, dl, bl
	{nf}	or	BYTE PTR [r8+rax*4+291], bl
	{nf}	or	dl, BYTE PTR [r8+rax*4+291], bl
	{nf}	or	ax, dx
	{nf}	or	r9w, ax, dx
	{nf}	or	WORD PTR [r8+rax*4+291], dx
	{nf}	or	ax, WORD PTR [r8+rax*4+291], dx
	{nf}	or	edx, ecx
	{nf}	or	r10d, edx, ecx
	{nf}	or	DWORD PTR [r8+rax*4+291], ecx
	{nf}	or	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	or	r31, r9
	{nf}	or	r11, r31, r9
	{nf}	or	QWORD PTR [r8+rax*4+291], r9
	{nf}	or	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	or	bl, BYTE PTR [r8+rax*4+291]
	{nf}	or	dl, bl, BYTE PTR [r8+rax*4+291]
	{nf}	or	dx, WORD PTR [r8+rax*4+291]
	{nf}	or	ax, dx, WORD PTR [r8+rax*4+291]
	{nf}	or	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	or	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	or	r9, QWORD PTR [r8+rax*4+291]
	{nf}	or	r31, r9, QWORD PTR [r8+rax*4+291]
	{nf}	popcnt	ax, dx
	{nf}	popcnt	edx, ecx
	{nf}	popcnt	r31, r9
	{nf}	popcnt	dx, WORD PTR [r8+rax*4+291]
	{nf}	popcnt	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	popcnt	r9, QWORD PTR [r8+rax*4+291]
	{nf}	rol	bl, 1
	{nf}	rol	dl, bl, 1
	{nf}	rol	dx, 1
	{nf}	rol	ax, dx, 1
	{nf}	rol	ecx, 1
	{nf}	rol	edx, ecx, 1
	{nf}	rol	r9, 1
	{nf}	rol	r31, r9, 1
	{nf}	rol	BYTE PTR [r8+rax*4+291], 1
	{nf}	rol	bl, BYTE PTR [r8+rax*4+291], 1
	{nf}	rol	WORD PTR [r8+rax*4+291], 1
	{nf}	rol	dx, WORD PTR [r8+rax*4+291], 1
	{nf}	rol	DWORD PTR [r8+rax*4+291], 1
	{nf}	rol	ecx, DWORD PTR [r8+rax*4+291], 1
	{nf}	rol	QWORD PTR [r8+rax*4+291], 1
	{nf}	rol	r9, QWORD PTR [r8+rax*4+291], 1
	{nf}	rol	bl, 123
	{nf}	rol	dl, bl, 123
	{nf}	rol	dx, 123
	{nf}	rol	ax, dx, 123
	{nf}	rol	ecx, 123
	{nf}	rol	edx, ecx, 123
	{nf}	rol	r9, 123
	{nf}	rol	r31, r9, 123
	{nf}	rol	BYTE PTR [r8+rax*4+291], 123
	{nf}	rol	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	rol	WORD PTR [r8+rax*4+291], 123
	{nf}	rol	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	rol	DWORD PTR [r8+rax*4+291], 123
	{nf}	rol	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	rol	QWORD PTR [r8+rax*4+291], 123
	{nf}	rol	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	rol	bl, cl
	{nf}	rol	dl, bl, cl
	{nf}	rol	dx, cl
	{nf}	rol	ax, dx, cl
	{nf}	rol	ecx, cl
	{nf}	rol	edx, ecx, cl
	{nf}	rol	r9, cl
	{nf}	rol	r31, r9, cl
	{nf}	rol	BYTE PTR [r8+rax*4+291], cl
	{nf}	rol	bl, BYTE PTR [r8+rax*4+291], cl
	{nf}	rol	WORD PTR [r8+rax*4+291], cl
	{nf}	rol	dx, WORD PTR [r8+rax*4+291], cl
	{nf}	rol	DWORD PTR [r8+rax*4+291], cl
	{nf}	rol	ecx, DWORD PTR [r8+rax*4+291], cl
	{nf}	rol	QWORD PTR [r8+rax*4+291], cl
	{nf}	rol	r9, QWORD PTR [r8+rax*4+291], cl
	{nf}	ror	bl, 1
	{nf}	ror	dl, bl, 1
	{nf}	ror	dx, 1
	{nf}	ror	ax, dx, 1
	{nf}	ror	ecx, 1
	{nf}	ror	edx, ecx, 1
	{nf}	ror	r9, 1
	{nf}	ror	r31, r9, 1
	{nf}	ror	BYTE PTR [r8+rax*4+291], 1
	{nf}	ror	bl, BYTE PTR [r8+rax*4+291], 1
	{nf}	ror	WORD PTR [r8+rax*4+291], 1
	{nf}	ror	dx, WORD PTR [r8+rax*4+291], 1
	{nf}	ror	DWORD PTR [r8+rax*4+291], 1
	{nf}	ror	ecx, DWORD PTR [r8+rax*4+291], 1
	{nf}	ror	QWORD PTR [r8+rax*4+291], 1
	{nf}	ror	r9, QWORD PTR [r8+rax*4+291], 1
	{nf}	ror	bl, 123
	{nf}	ror	dl, bl, 123
	{nf}	ror	dx, 123
	{nf}	ror	ax, dx, 123
	{nf}	ror	ecx, 123
	{nf}	ror	edx, ecx, 123
	{nf}	ror	r9, 123
	{nf}	ror	r31, r9, 123
	{nf}	ror	BYTE PTR [r8+rax*4+291], 123
	{nf}	ror	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	ror	WORD PTR [r8+rax*4+291], 123
	{nf}	ror	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	ror	DWORD PTR [r8+rax*4+291], 123
	{nf}	ror	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	ror	QWORD PTR [r8+rax*4+291], 123
	{nf}	ror	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	ror	bl, cl
	{nf}	ror	dl, bl, cl
	{nf}	ror	dx, cl
	{nf}	ror	ax, dx, cl
	{nf}	ror	ecx, cl
	{nf}	ror	edx, ecx, cl
	{nf}	ror	r9, cl
	{nf}	ror	r31, r9, cl
	{nf}	ror	BYTE PTR [r8+rax*4+291], cl
	{nf}	ror	bl, BYTE PTR [r8+rax*4+291], cl
	{nf}	ror	WORD PTR [r8+rax*4+291], cl
	{nf}	ror	dx, WORD PTR [r8+rax*4+291], cl
	{nf}	ror	DWORD PTR [r8+rax*4+291], cl
	{nf}	ror	ecx, DWORD PTR [r8+rax*4+291], cl
	{nf}	ror	QWORD PTR [r8+rax*4+291], cl
	{nf}	ror	r9, QWORD PTR [r8+rax*4+291], cl
	{nf}	sal	bl, 1
	{nf}	sal	dl, bl, 1
	{nf}	sal	dx, 1
	{nf}	sal	ax, dx, 1
	{nf}	sal	ecx, 1
	{nf}	sal	edx, ecx, 1
	{nf}	sal	r9, 1
	{nf}	sal	r31, r9, 1
	{nf}	sal	BYTE PTR [r8+rax*4+291], 1
	{nf}	sal	bl, BYTE PTR [r8+rax*4+291], 1
	{nf}	sal	WORD PTR [r8+rax*4+291], 1
	{nf}	sal	dx, WORD PTR [r8+rax*4+291], 1
	{nf}	sal	DWORD PTR [r8+rax*4+291], 1
	{nf}	sal	ecx, DWORD PTR [r8+rax*4+291], 1
	{nf}	sal	QWORD PTR [r8+rax*4+291], 1
	{nf}	sal	r9, QWORD PTR [r8+rax*4+291], 1
	{nf}	sal	bl, 123
	{nf}	sal	dl, bl, 123
	{nf}	sal	dx, 123
	{nf}	sal	ax, dx, 123
	{nf}	sal	ecx, 123
	{nf}	sal	edx, ecx, 123
	{nf}	sal	r9, 123
	{nf}	sal	r31, r9, 123
	{nf}	sal	BYTE PTR [r8+rax*4+291], 123
	{nf}	sal	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	sal	WORD PTR [r8+rax*4+291], 123
	{nf}	sal	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	sal	DWORD PTR [r8+rax*4+291], 123
	{nf}	sal	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	sal	QWORD PTR [r8+rax*4+291], 123
	{nf}	sal	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	sal	bl, cl
	{nf}	sal	dl, bl, cl
	{nf}	sal	dx, cl
	{nf}	sal	ax, dx, cl
	{nf}	sal	ecx, cl
	{nf}	sal	edx, ecx, cl
	{nf}	sal	r9, cl
	{nf}	sal	r31, r9, cl
	{nf}	sal	BYTE PTR [r8+rax*4+291], cl
	{nf}	sal	bl, BYTE PTR [r8+rax*4+291], cl
	{nf}	sal	WORD PTR [r8+rax*4+291], cl
	{nf}	sal	dx, WORD PTR [r8+rax*4+291], cl
	{nf}	sal	DWORD PTR [r8+rax*4+291], cl
	{nf}	sal	ecx, DWORD PTR [r8+rax*4+291], cl
	{nf}	sal	QWORD PTR [r8+rax*4+291], cl
	{nf}	sal	r9, QWORD PTR [r8+rax*4+291], cl
	{nf}	sar	bl, 1
	{nf}	sar	dl, bl, 1
	{nf}	sar	dx, 1
	{nf}	sar	ax, dx, 1
	{nf}	sar	ecx, 1
	{nf}	sar	edx, ecx, 1
	{nf}	sar	r9, 1
	{nf}	sar	r31, r9, 1
	{nf}	sar	BYTE PTR [r8+rax*4+291], 1
	{nf}	sar	bl, BYTE PTR [r8+rax*4+291], 1
	{nf}	sar	WORD PTR [r8+rax*4+291], 1
	{nf}	sar	dx, WORD PTR [r8+rax*4+291], 1
	{nf}	sar	DWORD PTR [r8+rax*4+291], 1
	{nf}	sar	ecx, DWORD PTR [r8+rax*4+291], 1
	{nf}	sar	QWORD PTR [r8+rax*4+291], 1
	{nf}	sar	r9, QWORD PTR [r8+rax*4+291], 1
	{nf}	sar	bl, 123
	{nf}	sar	dl, bl, 123
	{nf}	sar	dx, 123
	{nf}	sar	ax, dx, 123
	{nf}	sar	ecx, 123
	{nf}	sar	edx, ecx, 123
	{nf}	sar	r9, 123
	{nf}	sar	r31, r9, 123
	{nf}	sar	BYTE PTR [r8+rax*4+291], 123
	{nf}	sar	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	sar	WORD PTR [r8+rax*4+291], 123
	{nf}	sar	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	sar	DWORD PTR [r8+rax*4+291], 123
	{nf}	sar	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	sar	QWORD PTR [r8+rax*4+291], 123
	{nf}	sar	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	sar	bl, cl
	{nf}	sar	dl, bl, cl
	{nf}	sar	dx, cl
	{nf}	sar	ax, dx, cl
	{nf}	sar	ecx, cl
	{nf}	sar	edx, ecx, cl
	{nf}	sar	r9, cl
	{nf}	sar	r31, r9, cl
	{nf}	sar	BYTE PTR [r8+rax*4+291], cl
	{nf}	sar	bl, BYTE PTR [r8+rax*4+291], cl
	{nf}	sar	WORD PTR [r8+rax*4+291], cl
	{nf}	sar	dx, WORD PTR [r8+rax*4+291], cl
	{nf}	sar	DWORD PTR [r8+rax*4+291], cl
	{nf}	sar	ecx, DWORD PTR [r8+rax*4+291], cl
	{nf}	sar	QWORD PTR [r8+rax*4+291], cl
	{nf}	sar	r9, QWORD PTR [r8+rax*4+291], cl
	{nf}	shl	bl, 1
	{nf}	shl	dl, bl, 1
	{nf}	shl	dx, 1
	{nf}	shl	ax, dx, 1
	{nf}	shl	ecx, 1
	{nf}	shl	edx, ecx, 1
	{nf}	shl	r9, 1
	{nf}	shl	r31, r9, 1
	{nf}	shl	BYTE PTR [r8+rax*4+291], 1
	{nf}	shl	bl, BYTE PTR [r8+rax*4+291], 1
	{nf}	shl	WORD PTR [r8+rax*4+291], 1
	{nf}	shl	dx, WORD PTR [r8+rax*4+291], 1
	{nf}	shl	DWORD PTR [r8+rax*4+291], 1
	{nf}	shl	ecx, DWORD PTR [r8+rax*4+291], 1
	{nf}	shl	QWORD PTR [r8+rax*4+291], 1
	{nf}	shl	r9, QWORD PTR [r8+rax*4+291], 1
	{nf}	shl	bl, 123
	{nf}	shl	dl, bl, 123
	{nf}	shl	dx, 123
	{nf}	shl	ax, dx, 123
	{nf}	shl	ecx, 123
	{nf}	shl	edx, ecx, 123
	{nf}	shl	r9, 123
	{nf}	shl	r31, r9, 123
	{nf}	shl	BYTE PTR [r8+rax*4+291], 123
	{nf}	shl	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	shl	WORD PTR [r8+rax*4+291], 123
	{nf}	shl	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	shl	DWORD PTR [r8+rax*4+291], 123
	{nf}	shl	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	shl	QWORD PTR [r8+rax*4+291], 123
	{nf}	shl	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	shl	bl, cl
	{nf}	shl	dl, bl, cl
	{nf}	shl	dx, cl
	{nf}	shl	ax, dx, cl
	{nf}	shl	ecx, cl
	{nf}	shl	edx, ecx, cl
	{nf}	shl	r9, cl
	{nf}	shl	r31, r9, cl
	{nf}	shl	BYTE PTR [r8+rax*4+291], cl
	{nf}	shl	bl, BYTE PTR [r8+rax*4+291], cl
	{nf}	shl	WORD PTR [r8+rax*4+291], cl
	{nf}	shl	dx, WORD PTR [r8+rax*4+291], cl
	{nf}	shl	DWORD PTR [r8+rax*4+291], cl
	{nf}	shl	ecx, DWORD PTR [r8+rax*4+291], cl
	{nf}	shl	QWORD PTR [r8+rax*4+291], cl
	{nf}	shl	r9, QWORD PTR [r8+rax*4+291], cl
	{nf}	shld	ax, dx, 123
	{nf}	shld	r9w, ax, dx, 123
	{nf}	shld	WORD PTR [r8+rax*4+291], dx, 123
	{nf}	shld	ax, WORD PTR [r8+rax*4+291], dx, 123
	{nf}	shld	edx, ecx, 123
	{nf}	shld	r10d, edx, ecx, 123
	{nf}	shld	DWORD PTR [r8+rax*4+291], ecx, 123
	{nf}	shld	edx, DWORD PTR [r8+rax*4+291], ecx, 123
	{nf}	shld	r31, r9, 123
	{nf}	shld	r11, r31, r9, 123
	{nf}	shld	QWORD PTR [r8+rax*4+291], r9, 123
	{nf}	shld	r31, QWORD PTR [r8+rax*4+291], r9, 123
	{nf}	shld	ax, dx, cl
	{nf}	shld	r9w, ax, dx, cl
	{nf}	shld	WORD PTR [r8+rax*4+291], dx, cl
	{nf}	shld	ax, WORD PTR [r8+rax*4+291], dx, cl
	{nf}	shld	edx, ecx, cl
	{nf}	shld	r10d, edx, ecx, cl
	{nf}	shld	DWORD PTR [r8+rax*4+291], ecx, cl
	{nf}	shld	edx, DWORD PTR [r8+rax*4+291], ecx, cl
	{nf}	shld	r31, r9, cl
	{nf}	shld	r11, r31, r9, cl
	{nf}	shld	QWORD PTR [r8+rax*4+291], r9, cl
	{nf}	shld	r31, QWORD PTR [r8+rax*4+291], r9, cl
	{nf}	shr	bl, 1
	{nf}	shr	dl, bl, 1
	{nf}	shr	dx, 1
	{nf}	shr	ax, dx, 1
	{nf}	shr	ecx, 1
	{nf}	shr	edx, ecx, 1
	{nf}	shr	r9, 1
	{nf}	shr	r31, r9, 1
	{nf}	shr	BYTE PTR [r8+rax*4+291], 1
	{nf}	shr	bl, BYTE PTR [r8+rax*4+291], 1
	{nf}	shr	WORD PTR [r8+rax*4+291], 1
	{nf}	shr	dx, WORD PTR [r8+rax*4+291], 1
	{nf}	shr	DWORD PTR [r8+rax*4+291], 1
	{nf}	shr	ecx, DWORD PTR [r8+rax*4+291], 1
	{nf}	shr	QWORD PTR [r8+rax*4+291], 1
	{nf}	shr	r9, QWORD PTR [r8+rax*4+291], 1
	{nf}	shr	bl, 123
	{nf}	shr	dl, bl, 123
	{nf}	shr	dx, 123
	{nf}	shr	ax, dx, 123
	{nf}	shr	ecx, 123
	{nf}	shr	edx, ecx, 123
	{nf}	shr	r9, 123
	{nf}	shr	r31, r9, 123
	{nf}	shr	BYTE PTR [r8+rax*4+291], 123
	{nf}	shr	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	shr	WORD PTR [r8+rax*4+291], 123
	{nf}	shr	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	shr	DWORD PTR [r8+rax*4+291], 123
	{nf}	shr	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	shr	QWORD PTR [r8+rax*4+291], 123
	{nf}	shr	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	shr	bl, cl
	{nf}	shr	dl, bl, cl
	{nf}	shr	dx, cl
	{nf}	shr	ax, dx, cl
	{nf}	shr	ecx, cl
	{nf}	shr	edx, ecx, cl
	{nf}	shr	r9, cl
	{nf}	shr	r31, r9, cl
	{nf}	shr	BYTE PTR [r8+rax*4+291], cl
	{nf}	shr	bl, BYTE PTR [r8+rax*4+291], cl
	{nf}	shr	WORD PTR [r8+rax*4+291], cl
	{nf}	shr	dx, WORD PTR [r8+rax*4+291], cl
	{nf}	shr	DWORD PTR [r8+rax*4+291], cl
	{nf}	shr	ecx, DWORD PTR [r8+rax*4+291], cl
	{nf}	shr	QWORD PTR [r8+rax*4+291], cl
	{nf}	shr	r9, QWORD PTR [r8+rax*4+291], cl
	{nf}	shrd	ax, dx, 123
	{nf}	shrd	r9w, ax, dx, 123
	{nf}	shrd	WORD PTR [r8+rax*4+291], dx, 123
	{nf}	shrd	ax, WORD PTR [r8+rax*4+291], dx, 123
	{nf}	shrd	edx, ecx, 123
	{nf}	shrd	r10d, edx, ecx, 123
	{nf}	shrd	DWORD PTR [r8+rax*4+291], ecx, 123
	{nf}	shrd	edx, DWORD PTR [r8+rax*4+291], ecx, 123
	{nf}	shrd	r31, r9, 123
	{nf}	shrd	r11, r31, r9, 123
	{nf}	shrd	QWORD PTR [r8+rax*4+291], r9, 123
	{nf}	shrd	r31, QWORD PTR [r8+rax*4+291], r9, 123
	{nf}	shrd	ax, dx, cl
	{nf}	shrd	r9w, ax, dx, cl
	{nf}	shrd	WORD PTR [r8+rax*4+291], dx, cl
	{nf}	shrd	ax, WORD PTR [r8+rax*4+291], dx, cl
	{nf}	shrd	edx, ecx, cl
	{nf}	shrd	r10d, edx, ecx, cl
	{nf}	shrd	DWORD PTR [r8+rax*4+291], ecx, cl
	{nf}	shrd	edx, DWORD PTR [r8+rax*4+291], ecx, cl
	{nf}	shrd	r31, r9, cl
	{nf}	shrd	r11, r31, r9, cl
	{nf}	shrd	QWORD PTR [r8+rax*4+291], r9, cl
	{nf}	shrd	r31, QWORD PTR [r8+rax*4+291], r9, cl
	{nf}	sub	bl, 123
	{nf}	sub	dl, bl, 123
	{nf}	sub	dx, 123
	{nf}	sub	ax, dx, 123
	{nf}	sub	ecx, 123
	{nf}	sub	edx, ecx, 123
	{nf}	sub	r9, 123
	{nf}	sub	r31, r9, 123
	{nf}	sub	BYTE PTR [r8+rax*4+291], 123
	{nf}	sub	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	sub	WORD PTR [r8+rax*4+291], 123
	{nf}	sub	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	sub	DWORD PTR [r8+rax*4+291], 123
	{nf}	sub	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	sub	QWORD PTR [r8+rax*4+291], 123
	{nf}	sub	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	sub	dl, bl
	{nf}	sub	r8b, dl, bl
	{nf}	sub	BYTE PTR [r8+rax*4+291], bl
	{nf}	sub	dl, BYTE PTR [r8+rax*4+291], bl
	{nf}	sub	ax, dx
	{nf}	sub	r9w, ax, dx
	{nf}	sub	WORD PTR [r8+rax*4+291], dx
	{nf}	sub	ax, WORD PTR [r8+rax*4+291], dx
	{nf}	sub	edx, ecx
	{nf}	sub	r10d, edx, ecx
	{nf}	sub	DWORD PTR [r8+rax*4+291], ecx
	{nf}	sub	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	sub	r31, r9
	{nf}	sub	r11, r31, r9
	{nf}	sub	QWORD PTR [r8+rax*4+291], r9
	{nf}	sub	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	sub	bl, BYTE PTR [r8+rax*4+291]
	{nf}	sub	dl, bl, BYTE PTR [r8+rax*4+291]
	{nf}	sub	dx, WORD PTR [r8+rax*4+291]
	{nf}	sub	ax, dx, WORD PTR [r8+rax*4+291]
	{nf}	sub	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	sub	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	sub	r9, QWORD PTR [r8+rax*4+291]
	{nf}	sub	r31, r9, QWORD PTR [r8+rax*4+291]
	{nf}	tzcnt	ax, dx
	{nf}	tzcnt	edx, ecx
	{nf}	tzcnt	r31, r9
	{nf}	tzcnt	dx, WORD PTR [r8+rax*4+291]
	{nf}	tzcnt	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	tzcnt	r9, QWORD PTR [r8+rax*4+291]
	{nf}	xor	bl, 123
	{nf}	xor	dl, bl, 123
	{nf}	xor	dx, 123
	{nf}	xor	ax, dx, 123
	{nf}	xor	ecx, 123
	{nf}	xor	edx, ecx, 123
	{nf}	xor	r9, 123
	{nf}	xor	r31, r9, 123
	{nf}	xor	BYTE PTR [r8+rax*4+291], 123
	{nf}	xor	bl, BYTE PTR [r8+rax*4+291], 123
	{nf}	xor	WORD PTR [r8+rax*4+291], 123
	{nf}	xor	dx, WORD PTR [r8+rax*4+291], 123
	{nf}	xor	DWORD PTR [r8+rax*4+291], 123
	{nf}	xor	ecx, DWORD PTR [r8+rax*4+291], 123
	{nf}	xor	QWORD PTR [r8+rax*4+291], 123
	{nf}	xor	r9, QWORD PTR [r8+rax*4+291], 123
	{nf}	xor	dl, bl
	{nf}	xor	r8b, dl, bl
	{nf}	xor	BYTE PTR [r8+rax*4+291], bl
	{nf}	xor	dl, BYTE PTR [r8+rax*4+291], bl
	{nf}	xor	ax, dx
	{nf}	xor	r9w, ax, dx
	{nf}	xor	WORD PTR [r8+rax*4+291], dx
	{nf}	xor	ax, WORD PTR [r8+rax*4+291], dx
	{nf}	xor	edx, ecx
	{nf}	xor	r10d, edx, ecx
	{nf}	xor	DWORD PTR [r8+rax*4+291], ecx
	{nf}	xor	edx, DWORD PTR [r8+rax*4+291], ecx
	{nf}	xor	r31, r9
	{nf}	xor	r11, r31, r9
	{nf}	xor	QWORD PTR [r8+rax*4+291], r9
	{nf}	xor	r31, QWORD PTR [r8+rax*4+291], r9
	{nf}	xor	bl, BYTE PTR [r8+rax*4+291]
	{nf}	xor	dl, bl, BYTE PTR [r8+rax*4+291]
	{nf}	xor	dx, WORD PTR [r8+rax*4+291]
	{nf}	xor	ax, dx, WORD PTR [r8+rax*4+291]
	{nf}	xor	ecx, DWORD PTR [r8+rax*4+291]
	{nf}	xor	edx, ecx, DWORD PTR [r8+rax*4+291]
	{nf}	xor	r9, QWORD PTR [r8+rax*4+291]
	{nf}	xor	r31, r9, QWORD PTR [r8+rax*4+291]

	.att_syntax prefix
optimize:
	.irp op, add, sub
	{nf}	\op	$128, %bl
	{nf}	\op	$128, %bl, %dl
	{nf}	\op	$128, %dx
	{nf}	\op	$128, %dx, %ax
	{nf}	\op	$128, %ecx
	{nf}	\op	$128, %ecx, %edx
	{nf}	\op	$128, %r9
	{nf}	\op	$128, %r9, %r31
	{nf}	\op\()b	$128, (%rax)
	{nf}	\op	$128, (%rax), %bl
	{nf}	\op\()w	$128, (%rax)
	{nf}	\op	$128, (%rax), %dx
	{nf}	\op\()l	$128, (%rax)
	{nf}	\op	$128, (%rax), %ecx
	{nf}	\op\()q	$128, (%rax)
	{nf}	\op	$128, (%rax), %r9

	{nf}	\op	$1, %bl
	{nf}	\op	$1, %bl, %dl
	{nf}	\op	$1, %dx
	{nf}	\op	$1, %dx, %ax
	{nf}	\op	$1, %ecx
	{nf}	\op	$1, %ecx, %edx
	{nf}	\op	$1, %r9
	{nf}	\op	$1, %r9, %r31
	{nf}	\op\()b	$1, (%rax)
	{nf}	\op	$1, (%rax), %bl
	{nf}	\op\()w	$1, (%rax)
	{nf}	\op	$1, (%rax), %dx
	{nf}	\op\()l	$1, (%rax)
	{nf}	\op	$1, (%rax), %ecx
	{nf}	\op\()q	$1, (%rax)
	{nf}	\op	$1, (%rax), %r9

	{nf}	\op	$0xff, %bl
	{nf}	\op	$-1, %bl, %dl
	{nf}	\op	$0xffff, %dx
	{nf}	\op	$-1, %dx, %ax
	{nf}	\op	$0xffffffff, %ecx
	{nf}	\op	$-1, %ecx, %edx
	{nf}	\op	$-1, %r9
	{nf}	\op	$-1, %r9, %r31
	{nf}	\op\()b	$0xff, (%rax)
	{nf}	\op	$-1, (%rax), %bl
	{nf}	\op\()w	$0xffff, (%rax)
	{nf}	\op	$-1, (%rax), %dx
	{nf}	\op\()l	$0xffffffff, (%rax)
	{nf}	\op	$-1, (%rax), %ecx
	{nf}	\op\()q	$-1, (%rax)
	{nf}	\op	$-1, (%rax), %r9
	.endr

	.irp dir, l, r
	{nf}	ro\dir	$7, %dl
	{nf}	ro\dir	$7, %dl, %al
	{nf}	ro\dir	$15, %dx
	{nf}	ro\dir	$15, %dx, %ax
	{nf}	ro\dir	$31, %edx
	{nf}	ro\dir	$31, %edx, %eax
	{nf}	ro\dir	$63, %rdx
	{nf}	ro\dir	$63, %rdx, %rax

	{nf}	ro\dir\()b	$7, (%rdx)
	{nf}	ro\dir		$7, (%rdx), %al
	{nf}	ro\dir\()w	$15, (%rdx)
	{nf}	ro\dir		$15, (%rdx), %ax
	{nf}	ro\dir\()l	$31, (%rdx)
	{nf}	ro\dir		$31, (%rdx), %eax
	{nf}	ro\dir\()q	$63, (%rdx)
	{nf}	ro\dir		$63, (%rdx), %rax
	.endr

	.irp r, "", e, r
	{nf} imul $3, %\r\(cx), %\r\(dx)
	{nf} imul $5, %\r\(bp), %\r\(dx)
	{nf} imul $9, %\r\(cx), %\r\(bp)

	# Note: %\r\(sp) source form needs leaving alone.
	{nf} imul $3, %\r\(sp), %\r\(dx)
	{nf} imul $5, %\r\(sp)

	.ifeqs "\r",""
	# Note: (16-bit) ZU form needs leaving alone.
	{nf} imulzu $3, %cx, %dx
	{nf} imulzu $5, %cx
	# Note: 16-bit forms requiring REX2 and Disp8 want leaving alone with -Os.
	{nf} imul $3, %bp, %r16w
	{nf} imul $5, %r21w, %dx
	{nf} imul $9, %r21w
	.endif

	# Note: 2-6 want leaving alone with -Os.
	.irp n, 1, 2, 6, 7
	# Note: 16-bit 3-operand src!=dst non-ZU form needs leaving alone.
	{nf} imul $1<<\n, %\r\()dx, %\r\()cx
	{nf} imul $1<<\n, (%rdx), %\r\()cx
	{nf} imul $1<<\n, %\r\()cx, %\r\()cx
	{nf} imul $1<<\n, %\r\()cx

	.ifeqs "\r",""
	{nf} imulzu $1<<\n, %dx, %cx
	{nf} imulzu $1<<\n, (%rdx), %cx
	{nf} imulzu $1<<\n, %cx, %cx
	{nf} imulzu $1<<\n, %cx
	.endif
	.endr

	.ifeqs "\r",""
	# Note: 3-operand src!=dst non-ZU form needs leaving alone.
	{nf} imul $1<<15, %dx, %cx
	{nf} imul $-1<<15, (%rdx), %cx
	{nf} imul $1<<15, %cx, %cx
	{nf} imul $-1<<15, %cx
	{nf} imulzu $1<<15, %cx
	.endif

	.ifeqs "\r","e"
	{nf} imul $1<<31, %edx, %ecx
	{nf} imul $-1<<31, (%rdx), %ecx
	.endif

	.ifeqs "\r","r"
	{nf} imul $1<<30, %rdx, %rcx
	# Needs leaving alone.
	{nf} imul $-1<<31, %rdx, %rcx
	.endif
	.endr
