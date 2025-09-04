	.text
	.sse_check warning
sse2avx:

# Tests for op mem128, xmm
	cvtdq2ps 16(%r16),%xmm4
	cvtpd2dq 16(%r16),%xmm4
	cvtpd2ps 16(%r16),%xmm4
	cvtps2dq 16(%r16),%xmm4
	cvttpd2dq 16(%r16),%xmm4
	cvttps2dq 16(%r16),%xmm4
	lddqu 16(%r16),%xmm4
	movapd 16(%r16),%xmm4
	movaps 16(%r16),%xmm4
	movdqa 16(%r16),%xmm4
	movdqu 16(%r16),%xmm4
	movntdqa 16(%r16),%xmm4
	movshdup 16(%r16),%xmm4
	movsldup 16(%r16),%xmm4
	movupd 16(%r16),%xmm4
	movups 16(%r16),%xmm4
	pabsb 16(%r16),%xmm4
	pabsw 16(%r16),%xmm4
	pabsd 16(%r16),%xmm4
	rcpps 16(%r16),%xmm4
	rsqrtps 16(%r16),%xmm4
	sqrtpd 16(%r16),%xmm4
	sqrtps 16(%r16),%xmm4

# Tests for op xmm, mem128
	movapd %xmm4,16(%r16)
	movaps %xmm4,16(%r16)
	movdqa %xmm4,16(%r16)
	movdqu %xmm4,16(%r16)
	movntdq %xmm4,16(%r16)
	movntpd %xmm4,16(%r16)
	movntps %xmm4,16(%r16)
	movupd %xmm4,16(%r16)
	movups %xmm4,16(%r16)

# Tests for op mem128, xmm[, xmm]
	addpd 16(%r16),%xmm6
	addps 16(%r16),%xmm6
	aesenc 16(%r16),%xmm6
	aesenclast 16(%r16),%xmm6
	aesdec 16(%r16),%xmm6
	aesdeclast 16(%r16),%xmm6
	andnpd 16(%r16),%xmm6
	andnps 16(%r16),%xmm6
	andpd 16(%r16),%xmm6
	andps 16(%r16),%xmm6
	divpd 16(%r16),%xmm6
	divps 16(%r16),%xmm6
	gf2p8mulb 16(%r16),%xmm6
	maxpd 16(%r16),%xmm6
	maxps 16(%r16),%xmm6
	minpd 16(%r16),%xmm6
	minps 16(%r16),%xmm6
	mulpd 16(%r16),%xmm6
	mulps 16(%r16),%xmm6
	orpd 16(%r16),%xmm6
	orps 16(%r16),%xmm6
	packsswb 16(%r16),%xmm6
	packssdw 16(%r16),%xmm6
	packuswb 16(%r16),%xmm6
	packusdw 16(%r16),%xmm6
	paddb 16(%r16),%xmm6
	paddw 16(%r16),%xmm6
	paddd 16(%r16),%xmm6
	paddq 16(%r16),%xmm6
	paddsb 16(%r16),%xmm6
	paddsw 16(%r16),%xmm6
	paddusb 16(%r16),%xmm6
	paddusw 16(%r16),%xmm6
	pand 16(%r16),%xmm6
	pandn 16(%r16),%xmm6
	pavgb 16(%r16),%xmm6
	pavgw 16(%r16),%xmm6
	pclmullqlqdq 16(%r16),%xmm6
	pclmulhqlqdq 16(%r16),%xmm6
	pclmullqhqdq 16(%r16),%xmm6
	pclmulhqhqdq 16(%r16),%xmm6
	pmaddwd 16(%r16),%xmm6
	pmaddubsw 16(%r16),%xmm6
	pmaxsb 16(%r16),%xmm6
	pmaxsw 16(%r16),%xmm6
	pmaxsd 16(%r16),%xmm6
	pmaxub 16(%r16),%xmm6
	pmaxuw 16(%r16),%xmm6
	pmaxud 16(%r16),%xmm6
	pminsb 16(%r16),%xmm6
	pminsw 16(%r16),%xmm6
	pminsd 16(%r16),%xmm6
	pminub 16(%r16),%xmm6
	pminuw 16(%r16),%xmm6
	pminud 16(%r16),%xmm6
	pmuldq 16(%r16),%xmm6
	pmulhuw 16(%r16),%xmm6
	pmulhrsw 16(%r16),%xmm6
	pmulhw 16(%r16),%xmm6
	pmullw 16(%r16),%xmm6
	pmulld 16(%r16),%xmm6
	pmuludq 16(%r16),%xmm6
	por 16(%r16),%xmm6
	psadbw 16(%r16),%xmm6
	pshufb 16(%r16),%xmm6
	psllw 16(%r16),%xmm6
	pslld 16(%r16),%xmm6
	psllq 16(%r16),%xmm6
	psraw 16(%r16),%xmm6
	psrad 16(%r16),%xmm6
	psrlw 16(%r16),%xmm6
	psrld 16(%r16),%xmm6
	psrlq 16(%r16),%xmm6
	psubb 16(%r16),%xmm6
	psubw 16(%r16),%xmm6
	psubd 16(%r16),%xmm6
	psubq 16(%r16),%xmm6
	psubsb 16(%r16),%xmm6
	psubsw 16(%r16),%xmm6
	psubusb 16(%r16),%xmm6
	psubusw 16(%r16),%xmm6
	punpckhbw 16(%r16),%xmm6
	punpckhwd 16(%r16),%xmm6
	punpckhdq 16(%r16),%xmm6
	punpckhqdq 16(%r16),%xmm6
	punpcklbw 16(%r16),%xmm6
	punpcklwd 16(%r16),%xmm6
	punpckldq 16(%r16),%xmm6
	punpcklqdq 16(%r16),%xmm6
	pxor 16(%r16),%xmm6
	subpd 16(%r16),%xmm6
	subps 16(%r16),%xmm6
	unpckhpd 16(%r16),%xmm6
	unpckhps 16(%r16),%xmm6
	unpcklpd 16(%r16),%xmm6
	unpcklps 16(%r16),%xmm6
	xorpd 16(%r16),%xmm6
	xorps 16(%r16),%xmm6

# Tests for op imm8, mem128, xmm
	pshufd $100,16(%r16),%xmm6
	pshufhw $100,16(%r16),%xmm6
	pshuflw $100,16(%r16),%xmm6
	roundpd $4,16(%r16),%xmm6
	roundps $4,16(%r16),%xmm6

# Tests for op imm8, mem128, xmm[, xmm]
	gf2p8affineqb $100,16(%r16),%xmm6
	gf2p8affineinvqb $100,16(%r16),%xmm6
	palignr $100,16(%r16),%xmm6
	pclmulqdq $100,16(%r16),%xmm6
	shufpd $100,16(%r16),%xmm6
	shufps $100,16(%r16),%xmm6

# Tests for op mem64, xmm
	comisd 16(%r16),%xmm4
	cvtdq2pd 16(%r16),%xmm4
	cvtpi2pd 16(%r16),%xmm4
	cvtps2pd 16(%r16),%xmm4
	movddup 16(%r16),%xmm4
	movsd 16(%r16),%xmm4
	pmovsxbw 16(%r16),%xmm4
	pmovsxwd 16(%r16),%xmm4
	pmovsxdq 16(%r16),%xmm4
	pmovzxbw 16(%r16),%xmm4
	pmovzxwd 16(%r16),%xmm4
	pmovzxdq 16(%r16),%xmm4
	ucomisd 16(%r16),%xmm4

# Tests for op xmm, mem64
	movlpd %xmm4,16(%r16)
	movlps %xmm4,16(%r16)
	movhpd %xmm4,16(%r16)
	movhps %xmm4,16(%r16)
	movsd %xmm4,16(%r16)

# Tests for op mem64, xmm[, xmm]
	movlpd 16(%r16),%xmm4
	movlps 16(%r16),%xmm4
	movhpd 16(%r16),%xmm4
	movhps 16(%r16),%xmm4

# Tests for op xmm, regq/mem64
# Tests for op regq/mem64, xmm
	movd %xmm4,%r16
	movd %r16,%xmm4
	movq %xmm4,%r16
	movq %r16,%xmm4
	movq %xmm4,16(%r16)
	movq 16(%r16),%xmm4

# Tests for op xmm/mem64, regl
	cvtsd2si %xmm4,%r16d
	cvtsd2si 16(%r16),%ecx
	cvttsd2si %xmm4,%r16d
	cvttsd2si 16(%r16),%ecx

# Tests for op xmm/mem64, regq
	cvtsd2si %xmm4,%r16
	cvtsd2si 16(%r16),%rcx
	cvttsd2si %xmm4,%r16
	cvttsd2si 16(%r16),%rcx

# Tests for op regq/mem64, xmm[, xmm]
	cvtsi2sdq %r16,%xmm4
	cvtsi2sdq 16(%r16),%xmm4
	cvtsi2ssq %r16,%xmm4
	cvtsi2ssq 16(%r16),%xmm4

# Tests for op imm8, regq/mem64, xmm[, xmm]
	pinsrq $100,%r16,%xmm4
	pinsrq $100,16(%r16),%xmm4

# Tests for op imm8, xmm, regq/mem64
	pextrq $100,%xmm4,%r16
	pextrq $100,%xmm4,16(%r16)

# Tests for op imm8, mem64, xmm[, xmm]
	roundsd $4,16(%r16),%xmm6

# Tests for op mem64, xmm[, xmm]
	addsd 16(%r16),%xmm6
	cvtsd2ss 16(%r16),%xmm6
	divsd 16(%r16),%xmm6
	maxsd 16(%r16),%xmm6
	minsd 16(%r16),%xmm6
	mulsd 16(%r16),%xmm6
	sqrtsd 16(%r16),%xmm6
	subsd 16(%r16),%xmm6

# Tests for op mem32, xmm[, xmm]
	addss 16(%r16),%xmm6
	cvtss2sd 16(%r16),%xmm6
	divss 16(%r16),%xmm6
	maxss 16(%r16),%xmm6
	minss 16(%r16),%xmm6
	mulss 16(%r16),%xmm6
	rcpss 16(%r16),%xmm6
	rsqrtss 16(%r16),%xmm6
	sqrtss 16(%r16),%xmm6
	subss 16(%r16),%xmm6

# Tests for op mem32, xmm
	comiss 16(%r16),%xmm4
	movss 16(%r16),%xmm4
	pmovsxbd 16(%r16),%xmm4
	pmovsxwq 16(%r16),%xmm4
	pmovzxbd 16(%r16),%xmm4
	pmovzxwq 16(%r16),%xmm4
	ucomiss 16(%r16),%xmm4

# Tests for op xmm, mem32
	movss %xmm4,16(%r16)

# Tests for op xmm, regl/mem32
# Tests for op regl/mem32, xmm
	movd %xmm4,%r16d
	movd %xmm4,16(%r16)
	movd %r16d,%xmm4
	movd 16(%r16),%xmm4

# Tests for op xmm/mem32, regl
	cvtss2si %xmm4,%r16d
	cvtss2si 16(%r16),%ecx
	cvttss2si %xmm4,%r16d
	cvttss2si 16(%r16),%ecx

# Tests for op xmm/mem32, regq
	cvtss2si %xmm4,%r16
	cvtss2si 16(%r16),%rcx
	cvttss2si %xmm4,%r16
	cvttss2si 16(%r16),%rcx

# Tests for op imm8, xmm, regq/mem32
	extractps $100,%xmm4,%r16

# Tests for op imm8, xmm, regl/mem32
	pextrd $100,%xmm4,%r16d
	pextrd $100,%xmm4,16(%r16)
	extractps $100,%xmm4,%r16d
	extractps $100,%xmm4,16(%r16)

# Tests for op imm8, regl/mem32, xmm[, xmm]
	pinsrd $100,%r16d,%xmm4
	pinsrd $100,16(%r16),%xmm4

# Tests for op regl/mem32, xmm[, xmm]
	cvtsi2sd %r16d,%xmm4
	cvtsi2sd 16(%r16),%xmm4
	cvtsi2ss %r16d,%xmm4
	cvtsi2ss 16(%r16),%xmm4

# Tests for op imm8, mem32, xmm[, xmm]
	insertps $100,16(%r16),%xmm6
	roundss $4,16(%r16),%xmm6

# Tests for op mem16, xmm
	pmovsxbq 16(%r16),%xmm4
	pmovzxbq 16(%r16),%xmm4

# Tests for op imm8, xmm, regl/mem16
	pextrw $100,%xmm4,%r16d
	pextrw $100,%xmm4,%r16
	pextrw $100,%xmm4,16(%r16)

# Tests for op imm8, regl/mem16, xmm[, xmm]
	pinsrw $100,%r16d,%xmm4
	pinsrw $100,%r16,%xmm4
	pinsrw $100,16(%r16),%xmm4

# Tests for op imm8, xmm, regl/mem8
	pextrb $100,%xmm4,%r16d
	pextrb $100,%xmm4,%r16
	pextrb $100,%xmm4,16(%r16)

# Tests for op imm8, regl/mem8, xmm[, xmm]
	pinsrb $100,%r16d,%xmm4
	pinsrb $100,%r16,%xmm4
	pinsrb $100,16(%r16),%xmm4
