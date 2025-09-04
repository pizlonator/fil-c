# Check instructions with optimized encoding

	.macro noopt insn:vararg
	.ifdef USE_PREFIX
	{nooptimize} \insn
	.else
	\insn
	.endif
	.endm

	.text
_start:
	noopt testl $0x7f, %eax

	noopt lock xchg	%ecx, (%edx)
	noopt lock xchg	(%ecx), %edx

	noopt vmovdqa32	%ymm1, %ymm2
	noopt vmovdqa64	%ymm1, %ymm2
	noopt vmovdqu8	%xmm1, %xmm2
	noopt vmovdqu16	%xmm1, %xmm2
	noopt vmovdqu32	%xmm1, %xmm2
	noopt vmovdqu64	%xmm1, %xmm2

	noopt vpandd	%xmm2, %xmm3, %xmm4
	noopt vpandq	%ymm2, %ymm3, %ymm4
	noopt vpandnd	%ymm2, %ymm3, %ymm4
	noopt vpandnq	%xmm2, %xmm3, %xmm4
	noopt vpord	%xmm2, %xmm3, %xmm4
	noopt vporq	%ymm2, %ymm3, %ymm4
	noopt vpxord	%ymm2, %ymm3, %ymm4
	noopt vpxorq	%xmm2, %xmm3, %xmm4
