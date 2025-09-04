	.text
	.sse_check warning
sse2avx:

# Tests for op xmm/mem128, xmm
	{evex}	cvtdq2ps %xmm4,%xmm6
	{evex}	cvtdq2ps (%rcx),%xmm4
	{evex}	cvtpd2dq %xmm4,%xmm6
	{evex}	cvtpd2dq (%rcx),%xmm4
	{evex}	cvtpd2ps %xmm4,%xmm6
	{evex}	cvtpd2ps (%rcx),%xmm4
	{evex}	cvtps2dq %xmm4,%xmm6
	{evex}	cvtps2dq (%rcx),%xmm4
	{evex}	cvttpd2dq %xmm4,%xmm6
	{evex}	cvttpd2dq (%rcx),%xmm4
	{evex}	cvttps2dq %xmm4,%xmm6
	{evex}	cvttps2dq (%rcx),%xmm4
	{evex}	movapd %xmm4,%xmm6
	{evex}	movapd (%rcx),%xmm4
	{evex}	movaps %xmm4,%xmm6
	{evex}	movaps (%rcx),%xmm4
	{evex}	movdqa %xmm4,%xmm6
	{evex}	movdqa (%rcx),%xmm4
	{evex}	movdqu %xmm4,%xmm6
	{evex}	movdqu (%rcx),%xmm4
	{evex}	movshdup %xmm4,%xmm6
	{evex}	movshdup (%rcx),%xmm4
	{evex}	movsldup %xmm4,%xmm6
	{evex}	movsldup (%rcx),%xmm4
	{evex}	movupd %xmm4,%xmm6
	{evex}	movupd (%rcx),%xmm4
	{evex}	movups %xmm4,%xmm6
	{evex}	movups (%rcx),%xmm4
	{evex}	pabsb %xmm4,%xmm6
	{evex}	pabsb (%rcx),%xmm4
	{evex}	pabsw %xmm4,%xmm6
	{evex}	pabsw (%rcx),%xmm4
	{evex}	pabsd %xmm4,%xmm6
	{evex}	pabsd (%rcx),%xmm4
	{evex}	rcpps %xmm4,%xmm6
	{evex}	rcpps (%rcx),%xmm4
	{evex}	rsqrtps %xmm4,%xmm6
	{evex}	rsqrtps (%rcx),%xmm4
	{evex}	sqrtpd %xmm4,%xmm6
	{evex}	sqrtpd (%rcx),%xmm4
	{evex}	sqrtps %xmm4,%xmm6
	{evex}	sqrtps (%rcx),%xmm4

# Tests for op xmm, xmm/mem128
	{evex}	movapd %xmm4,%xmm6
	{evex}	movapd %xmm4,(%rcx)
	{evex}	movaps %xmm4,%xmm6
	{evex}	movaps %xmm4,(%rcx)
	{evex}	movdqa %xmm4,%xmm6
	{evex}	movdqa %xmm4,(%rcx)
	{evex}	movdqu %xmm4,%xmm6
	{evex}	movdqu %xmm4,(%rcx)
	{evex}	movupd %xmm4,%xmm6
	{evex}	movupd %xmm4,(%rcx)
	{evex}	movups %xmm4,%xmm6
	{evex}	movups %xmm4,(%rcx)

# Tests for op mem128, xmm
	{evex}	lddqu (%rcx),%xmm4
	{evex}	movntdqa (%rcx),%xmm4

# Tests for op xmm, mem128
	{evex}	movntdq %xmm4,(%rcx)
	{evex}	movntpd %xmm4,(%rcx)
	{evex}	movntps %xmm4,(%rcx)

# Tests for op xmm/mem128, xmm[, xmm]
	{evex}	addpd %xmm4,%xmm6
	{evex}	addpd (%rcx),%xmm6
	{evex}	addps %xmm4,%xmm6
	{evex}	addps (%rcx),%xmm6
	{evex}	aesenc %xmm4,%xmm6
	{evex}	aesenc (%rcx),%xmm6
	{evex}	aesenclast %xmm4,%xmm6
	{evex}	aesenclast (%rcx),%xmm6
	{evex}	aesdec %xmm4,%xmm6
	{evex}	aesdec (%rcx),%xmm6
	{evex}	aesdeclast %xmm4,%xmm6
	{evex}	aesdeclast (%rcx),%xmm6
	{evex}	andnpd %xmm4,%xmm6
	{evex}	andnpd (%rcx),%xmm6
	{evex}	andnps %xmm4,%xmm6
	{evex}	andnps (%rcx),%xmm6
	{evex}	andpd %xmm4,%xmm6
	{evex}	andpd (%rcx),%xmm6
	{evex}	andps %xmm4,%xmm6
	{evex}	andps (%rcx),%xmm6
	{evex}	divpd %xmm4,%xmm6
	{evex}	divpd (%rcx),%xmm6
	{evex}	divps %xmm4,%xmm6
	{evex}	divps (%rcx),%xmm6
	{evex}	gf2p8mulb %xmm4,%xmm6
	{evex}	gf2p8mulb (%rcx),%xmm6
	{evex}	maxpd %xmm4,%xmm6
	{evex}	maxpd (%rcx),%xmm6
	{evex}	maxps %xmm4,%xmm6
	{evex}	maxps (%rcx),%xmm6
	{evex}	minpd %xmm4,%xmm6
	{evex}	minpd (%rcx),%xmm6
	{evex}	minps %xmm4,%xmm6
	{evex}	minps (%rcx),%xmm6
	{evex}	mulpd %xmm4,%xmm6
	{evex}	mulpd (%rcx),%xmm6
	{evex}	mulps %xmm4,%xmm6
	{evex}	mulps (%rcx),%xmm6
	{evex}	orpd %xmm4,%xmm6
	{evex}	orpd (%rcx),%xmm6
	{evex}	orps %xmm4,%xmm6
	{evex}	orps (%rcx),%xmm6
	{evex}	packsswb %xmm4,%xmm6
	{evex}	packsswb (%rcx),%xmm6
	{evex}	packssdw %xmm4,%xmm6
	{evex}	packssdw (%rcx),%xmm6
	{evex}	packuswb %xmm4,%xmm6
	{evex}	packuswb (%rcx),%xmm6
	{evex}	packusdw %xmm4,%xmm6
	{evex}	packusdw (%rcx),%xmm6
	{evex}	paddb %xmm4,%xmm6
	{evex}	paddb (%rcx),%xmm6
	{evex}	paddw %xmm4,%xmm6
	{evex}	paddw (%rcx),%xmm6
	{evex}	paddd %xmm4,%xmm6
	{evex}	paddd (%rcx),%xmm6
	{evex}	paddq %xmm4,%xmm6
	{evex}	paddq (%rcx),%xmm6
	{evex}	paddsb %xmm4,%xmm6
	{evex}	paddsb (%rcx),%xmm6
	{evex}	paddsw %xmm4,%xmm6
	{evex}	paddsw (%rcx),%xmm6
	{evex}	paddusb %xmm4,%xmm6
	{evex}	paddusb (%rcx),%xmm6
	{evex}	paddusw %xmm4,%xmm6
	{evex}	paddusw (%rcx),%xmm6
	{evex}	pand %xmm4,%xmm6
	{evex}	pand (%rcx),%xmm6
	{evex}	pandn %xmm4,%xmm6
	{evex}	pandn (%rcx),%xmm6
	{evex}	pavgb %xmm4,%xmm6
	{evex}	pavgb (%rcx),%xmm6
	{evex}	pavgw %xmm4,%xmm6
	{evex}	pavgw (%rcx),%xmm6
	{evex}	pclmullqlqdq %xmm4,%xmm6
	{evex}	pclmullqlqdq (%rcx),%xmm6
	{evex}	pclmulhqlqdq %xmm4,%xmm6
	{evex}	pclmulhqlqdq (%rcx),%xmm6
	{evex}	pclmullqhqdq %xmm4,%xmm6
	{evex}	pclmullqhqdq (%rcx),%xmm6
	{evex}	pclmulhqhqdq %xmm4,%xmm6
	{evex}	pclmulhqhqdq (%rcx),%xmm6
	{evex}	pmaddwd %xmm4,%xmm6
	{evex}	pmaddwd (%rcx),%xmm6
	{evex}	pmaddubsw %xmm4,%xmm6
	{evex}	pmaddubsw (%rcx),%xmm6
	{evex}	pmaxsb %xmm4,%xmm6
	{evex}	pmaxsb (%rcx),%xmm6
	{evex}	pmaxsw %xmm4,%xmm6
	{evex}	pmaxsw (%rcx),%xmm6
	{evex}	pmaxsd %xmm4,%xmm6
	{evex}	pmaxsd (%rcx),%xmm6
	{evex}	pmaxub %xmm4,%xmm6
	{evex}	pmaxub (%rcx),%xmm6
	{evex}	pmaxuw %xmm4,%xmm6
	{evex}	pmaxuw (%rcx),%xmm6
	{evex}	pmaxud %xmm4,%xmm6
	{evex}	pmaxud (%rcx),%xmm6
	{evex}	pminsb %xmm4,%xmm6
	{evex}	pminsb (%rcx),%xmm6
	{evex}	pminsw %xmm4,%xmm6
	{evex}	pminsw (%rcx),%xmm6
	{evex}	pminsd %xmm4,%xmm6
	{evex}	pminsd (%rcx),%xmm6
	{evex}	pminub %xmm4,%xmm6
	{evex}	pminub (%rcx),%xmm6
	{evex}	pminuw %xmm4,%xmm6
	{evex}	pminuw (%rcx),%xmm6
	{evex}	pminud %xmm4,%xmm6
	{evex}	pminud (%rcx),%xmm6
	{evex}	pmulhuw %xmm4,%xmm6
	{evex}	pmulhuw (%rcx),%xmm6
	{evex}	pmulhrsw %xmm4,%xmm6
	{evex}	pmulhrsw (%rcx),%xmm6
	{evex}	pmulhw %xmm4,%xmm6
	{evex}	pmulhw (%rcx),%xmm6
	{evex}	pmullw %xmm4,%xmm6
	{evex}	pmullw (%rcx),%xmm6
	{evex}	pmulld %xmm4,%xmm6
	{evex}	pmulld (%rcx),%xmm6
	{evex}	pmuludq %xmm4,%xmm6
	{evex}	pmuludq (%rcx),%xmm6
	{evex}	pmuldq %xmm4,%xmm6
	{evex}	pmuldq (%rcx),%xmm6
	{evex}	por %xmm4,%xmm6
	{evex}	por (%rcx),%xmm6
	{evex}	psadbw %xmm4,%xmm6
	{evex}	psadbw (%rcx),%xmm6
	{evex}	pshufb %xmm4,%xmm6
	{evex}	pshufb (%rcx),%xmm6
	{evex}	psllw %xmm4,%xmm6
	{evex}	psllw (%rcx),%xmm6
	{evex}	pslld %xmm4,%xmm6
	{evex}	pslld (%rcx),%xmm6
	{evex}	psllq %xmm4,%xmm6
	{evex}	psllq (%rcx),%xmm6
	{evex}	psraw %xmm4,%xmm6
	{evex}	psraw (%rcx),%xmm6
	{evex}	psrad %xmm4,%xmm6
	{evex}	psrad (%rcx),%xmm6
	{evex}	psrlw %xmm4,%xmm6
	{evex}	psrlw (%rcx),%xmm6
	{evex}	psrld %xmm4,%xmm6
	{evex}	psrld (%rcx),%xmm6
	{evex}	psrlq %xmm4,%xmm6
	{evex}	psrlq (%rcx),%xmm6
	{evex}	psubb %xmm4,%xmm6
	{evex}	psubb (%rcx),%xmm6
	{evex}	psubw %xmm4,%xmm6
	{evex}	psubw (%rcx),%xmm6
	{evex}	psubd %xmm4,%xmm6
	{evex}	psubd (%rcx),%xmm6
	{evex}	psubq %xmm4,%xmm6
	{evex}	psubq (%rcx),%xmm6
	{evex}	psubsb %xmm4,%xmm6
	{evex}	psubsb (%rcx),%xmm6
	{evex}	psubsw %xmm4,%xmm6
	{evex}	psubsw (%rcx),%xmm6
	{evex}	psubusb %xmm4,%xmm6
	{evex}	psubusb (%rcx),%xmm6
	{evex}	psubusw %xmm4,%xmm6
	{evex}	psubusw (%rcx),%xmm6
	{evex}	punpckhbw %xmm4,%xmm6
	{evex}	punpckhbw (%rcx),%xmm6
	{evex}	punpckhwd %xmm4,%xmm6
	{evex}	punpckhwd (%rcx),%xmm6
	{evex}	punpckhdq %xmm4,%xmm6
	{evex}	punpckhdq (%rcx),%xmm6
	{evex}	punpckhqdq %xmm4,%xmm6
	{evex}	punpckhqdq (%rcx),%xmm6
	{evex}	punpcklbw %xmm4,%xmm6
	{evex}	punpcklbw (%rcx),%xmm6
	{evex}	punpcklwd %xmm4,%xmm6
	{evex}	punpcklwd (%rcx),%xmm6
	{evex}	punpckldq %xmm4,%xmm6
	{evex}	punpckldq (%rcx),%xmm6
	{evex}	punpcklqdq %xmm4,%xmm6
	{evex}	punpcklqdq (%rcx),%xmm6
	{evex}	pxor %xmm4,%xmm6
	{evex}	pxor (%rcx),%xmm6
	{evex}	subpd %xmm4,%xmm6
	{evex}	subpd (%rcx),%xmm6
	{evex}	subps %xmm4,%xmm6
	{evex}	subps (%rcx),%xmm6
	{evex}	unpckhpd %xmm4,%xmm6
	{evex}	unpckhpd (%rcx),%xmm6
	{evex}	unpckhps %xmm4,%xmm6
	{evex}	unpckhps (%rcx),%xmm6
	{evex}	unpcklpd %xmm4,%xmm6
	{evex}	unpcklpd (%rcx),%xmm6
	{evex}	unpcklps %xmm4,%xmm6
	{evex}	unpcklps (%rcx),%xmm6
	{evex}	xorpd %xmm4,%xmm6
	{evex}	xorpd (%rcx),%xmm6
	{evex}	xorps %xmm4,%xmm6
	{evex}	xorps (%rcx),%xmm6

# Tests for op imm8, xmm/mem128, xmm
	{evex}	pshufd $100,%xmm4,%xmm6
	{evex}	pshufd $100,(%rcx),%xmm6
	{evex}	pshufhw $100,%xmm4,%xmm6
	{evex}	pshufhw $100,(%rcx),%xmm6
	{evex}	pshuflw $100,%xmm4,%xmm6
	{evex}	pshuflw $100,(%rcx),%xmm6
	{evex}	roundpd $4,%xmm4,%xmm6
	{evex}	roundpd $4,(%rcx),%xmm6
	{evex}	roundps $4,%xmm4,%xmm6
	{evex}	roundps $4,(%rcx),%xmm6

# Tests for op imm8, xmm/mem128, xmm[, xmm]
	{evex}	gf2p8affineqb $100,%xmm4,%xmm6
	{evex}	gf2p8affineqb $100,(%rcx),%xmm6
	{evex}	gf2p8affineinvqb $100,%xmm4,%xmm6
	{evex}	gf2p8affineinvqb $100,(%rcx),%xmm6
	{evex}	palignr $100,%xmm4,%xmm6
	{evex}	palignr $100,(%rcx),%xmm6
	{evex}	pclmulqdq $100,%xmm4,%xmm6
	{evex}	pclmulqdq $100,(%rcx),%xmm6
	{evex}	shufpd $100,%xmm4,%xmm6
	{evex}	shufpd $100,(%rcx),%xmm6
	{evex}	shufps $100,%xmm4,%xmm6
	{evex}	shufps $100,(%rcx),%xmm6

# Tests for op xmm/mem64, xmm
	{evex}	comisd %xmm4,%xmm6
	{evex}	comisd (%rcx),%xmm4
	{evex}	cvtdq2pd %xmm4,%xmm6
	{evex}	cvtdq2pd (%rcx),%xmm4
	{evex}	cvtpi2pd (%rcx),%xmm4
	{evex}	cvtps2pd %xmm4,%xmm6
	{evex}	cvtps2pd (%rcx),%xmm4
	{evex}	movddup %xmm4,%xmm6
	{evex}	movddup (%rcx),%xmm4
	{evex}	pmovsxbw %xmm4,%xmm6
	{evex}	pmovsxbw (%rcx),%xmm4
	{evex}	pmovsxwd %xmm4,%xmm6
	{evex}	pmovsxwd (%rcx),%xmm4
	{evex}	pmovsxdq %xmm4,%xmm6
	{evex}	pmovsxdq (%rcx),%xmm4
	{evex}	pmovzxbw %xmm4,%xmm6
	{evex}	pmovzxbw (%rcx),%xmm4
	{evex}	pmovzxwd %xmm4,%xmm6
	{evex}	pmovzxwd (%rcx),%xmm4
	{evex}	pmovzxdq %xmm4,%xmm6
	{evex}	pmovzxdq (%rcx),%xmm4
	{evex}	ucomisd %xmm4,%xmm6
	{evex}	ucomisd (%rcx),%xmm4

# Tests for op mem64, xmm
	{evex}	movsd (%rcx),%xmm4

# Tests for op xmm, mem64
	{evex}	movlpd %xmm4,(%rcx)
	{evex}	movlps %xmm4,(%rcx)
	{evex}	movhpd %xmm4,(%rcx)
	{evex}	movhps %xmm4,(%rcx)
	{evex}	movsd %xmm4,(%rcx)

# Tests for op xmm, regq/mem64
# Tests for op regq/mem64, xmm
	{evex}	movd %xmm4,%rcx
	{evex}	movd %rcx,%xmm4
	{evex}	movq %xmm4,%rcx
	{evex}	movq %rcx,%xmm4
	{evex}	movq %xmm4,(%rcx)
	{evex}	movq (%rcx),%xmm4

# Tests for op xmm/mem64, regl
	{evex}	cvtsd2si %xmm4,%ecx
	{evex}	cvtsd2si (%rcx),%ecx
	{evex}	cvttsd2si %xmm4,%ecx
	{evex}	cvttsd2si (%rcx),%ecx

# Tests for op xmm/mem64, regq
	{evex}	cvtsd2si %xmm4,%rcx
	{evex}	cvtsd2si (%rcx),%rcx
	{evex}	cvttsd2si %xmm4,%rcx
	{evex}	cvttsd2si (%rcx),%rcx

# Tests for op regq/mem64, xmm[, xmm]
	{evex}	cvtsi2sdq %rcx,%xmm4
	{evex}	cvtsi2sdq (%rcx),%xmm4
	{evex}	cvtsi2ssq %rcx,%xmm4
	{evex}	cvtsi2ssq (%rcx),%xmm4

# Tests for op imm8, regq/mem64, xmm[, xmm]
	{evex}	pinsrq $100,%rcx,%xmm4
	{evex}	pinsrq $100,(%rcx),%xmm4

# Testsf for op imm8, xmm, regq/mem64
	{evex}	pextrq $100,%xmm4,%rcx
	{evex}	pextrq $100,%xmm4,(%rcx)

# Tests for op mem64, xmm[, xmm]
	{evex}	movlpd (%rcx),%xmm4
	{evex}	movlps (%rcx),%xmm4
	{evex}	movhpd (%rcx),%xmm4
	{evex}	movhps (%rcx),%xmm4

# Tests for op imm8, xmm/mem64, xmm[, xmm]
	{evex}	roundsd $4,%xmm4,%xmm6
	{evex}	roundsd $4,(%rcx),%xmm6

# Tests for op xmm/mem64, xmm[, xmm]
	{evex}	addsd %xmm4,%xmm6
	{evex}	addsd (%rcx),%xmm6
	{evex}	cvtsd2ss %xmm4,%xmm6
	{evex}	cvtsd2ss (%rcx),%xmm6
	{evex}	divsd %xmm4,%xmm6
	{evex}	divsd (%rcx),%xmm6
	{evex}	maxsd %xmm4,%xmm6
	{evex}	maxsd (%rcx),%xmm6
	{evex}	minsd %xmm4,%xmm6
	{evex}	minsd (%rcx),%xmm6
	{evex}	mulsd %xmm4,%xmm6
	{evex}	mulsd (%rcx),%xmm6
	{evex}	sqrtsd %xmm4,%xmm6
	{evex}	sqrtsd (%rcx),%xmm6
	{evex}	subsd %xmm4,%xmm6
	{evex}	subsd (%rcx),%xmm6

# Tests for op xmm/mem32, xmm[, xmm]
	{evex}	addss %xmm4,%xmm6
	{evex}	addss (%rcx),%xmm6
	{evex}	cvtss2sd %xmm4,%xmm6
	{evex}	cvtss2sd (%rcx),%xmm6
	{evex}	divss %xmm4,%xmm6
	{evex}	divss (%rcx),%xmm6
	{evex}	maxss %xmm4,%xmm6
	{evex}	maxss (%rcx),%xmm6
	{evex}	minss %xmm4,%xmm6
	{evex}	minss (%rcx),%xmm6
	{evex}	mulss %xmm4,%xmm6
	{evex}	mulss (%rcx),%xmm6
	{evex}	rcpss %xmm4,%xmm6
	{evex}	rcpss (%rcx),%xmm6
	{evex}	rsqrtss %xmm4,%xmm6
	{evex}	rsqrtss (%rcx),%xmm6
	{evex}	sqrtss %xmm4,%xmm6
	{evex}	sqrtss (%rcx),%xmm6
	{evex}	subss %xmm4,%xmm6
	{evex}	subss (%rcx),%xmm6

# Tests for op xmm/mem32, xmm
	{evex}	comiss %xmm4,%xmm6
	{evex}	comiss (%rcx),%xmm4
	{evex}	pmovsxbd %xmm4,%xmm6
	{evex}	pmovsxbd (%rcx),%xmm4
	{evex}	pmovsxwq %xmm4,%xmm6
	{evex}	pmovsxwq (%rcx),%xmm4
	{evex}	pmovzxbd %xmm4,%xmm6
	{evex}	pmovzxbd (%rcx),%xmm4
	{evex}	pmovzxwq %xmm4,%xmm6
	{evex}	pmovzxwq (%rcx),%xmm4
	{evex}	ucomiss %xmm4,%xmm6
	{evex}	ucomiss (%rcx),%xmm4

# Tests for op mem32, xmm
	{evex}	movss (%rcx),%xmm4

# Tests for op xmm, mem32
	{evex}	movss %xmm4,(%rcx)

# Tests for op xmm, regl/mem32
# Tests for op regl/mem32, xmm
	{evex}	movd %xmm4,%ecx
	{evex}	movd %xmm4,(%rcx)
	{evex}	movd %ecx,%xmm4
	{evex}	movd (%rcx),%xmm4

# Tests for op xmm/mem32, regl
	{evex}	cvtss2si %xmm4,%ecx
	{evex}	cvtss2si (%rcx),%ecx
	{evex}	cvttss2si %xmm4,%ecx
	{evex}	cvttss2si (%rcx),%ecx

# Tests for op xmm/mem32, regq
	{evex}	cvtss2si %xmm4,%rcx
	{evex}	cvtss2si (%rcx),%rcx
	{evex}	cvttss2si %xmm4,%rcx
	{evex}	cvttss2si (%rcx),%rcx

# Tests for op imm8, xmm, regq/mem32
	{evex}	extractps $100,%xmm4,%rcx
	{evex}	extractps $100,%xmm4,(%rcx)

# Tests for op imm8, xmm, regl/mem32
	{evex}	pextrd $100,%xmm4,%ecx
	{evex}	pextrd $100,%xmm4,(%rcx)
	{evex}	extractps $100,%xmm4,%ecx
	{evex}	extractps $100,%xmm4,(%rcx)

# Tests for op regl/mem32, xmm[, xmm]
	{evex}	cvtsi2sd %ecx,%xmm4
	{evex}	cvtsi2sdl (%rcx),%xmm4
	{evex}	cvtsi2ss %ecx,%xmm4
	{evex}	cvtsi2ssl (%rcx),%xmm4

# Tests for op imm8, xmm/mem32, xmm[, xmm]
	{evex}	insertps $100,%xmm4,%xmm6
	{evex}	insertps $100,(%rcx),%xmm6
	{evex}	roundss $4,%xmm4,%xmm6
	{evex}	roundss $4,(%rcx),%xmm6

# Tests for op xmm/m16, xmm
	{evex}	pmovsxbq %xmm4,%xmm6
	{evex}	pmovsxbq (%rcx),%xmm4
	{evex}	pmovzxbq %xmm4,%xmm6
	{evex}	pmovzxbq (%rcx),%xmm4

# Tests for op imm8, xmm, regl/mem16
	{evex}	pextrw $100,%xmm4,%ecx
	{evex}	pextrw $100,%xmm4,%rcx
	{evex}	pextrw $100,%xmm4,(%rcx)

# Tests for op imm8, regl/mem16, xmm[, xmm]
	{evex}	pinsrw $100,%ecx,%xmm4
	{evex}	pinsrw $100,%rcx,%xmm4
	{evex}	pinsrw $100,(%rcx),%xmm4

# Tests for op imm8, xmm, regl/mem8
	{evex}	pextrb $100,%xmm4,%ecx
	{evex}	pextrb $100,%xmm4,%rcx
	{evex}	pextrb $100,%xmm4,(%rcx)

# Tests for op imm8, regl/mem8, xmm[, xmm]
	{evex}	pinsrb $100,%ecx,%xmm4
	{evex}	pinsrb $100,%rcx,%xmm4
	{evex}	pinsrb $100,(%rcx),%xmm4

# Tests for op xmm, xmm
	{evex}	movq %xmm4,%xmm6

# Tests for op xmm, xmm[, xmm]
	{evex}	movhlps %xmm4,%xmm6
	{evex}	movlhps %xmm4,%xmm6
	{evex}	movsd %xmm4,%xmm6
	{evex}	movss %xmm4,%xmm6

# Tests for op imm8, xmm[, xmm]
	{evex}	pslld $100,%xmm4
	{evex}	pslldq $100,%xmm4
	{evex}	psllq $100,%xmm4
	{evex}	psllw $100,%xmm4
	{evex}	psrad $100,%xmm4
	{evex}	psraw $100,%xmm4
	{evex}	psrld $100,%xmm4
	{evex}	psrldq $100,%xmm4
	{evex}	psrlq $100,%xmm4
	{evex}	psrlw $100,%xmm4
