	.text
pextr:
	pextrd	$0, %xmm1, %edx
	pextrd	$0, %xmm1, (%rdx)

	pextrd	$0, %xmm1, %r18d
	pextrd	$0, %xmm1, (%r18)

	vpextrd	$0, %xmm1, %edx
	vpextrd	$0, %xmm1, (%rdx)

	vpextrd	$0, %xmm17, %edx
	vpextrd	$0, %xmm1, %r18d
	vpextrd	$0, %xmm1, (%r18)

	pextrq	$0, %xmm1, %rdx
	pextrq	$0, %xmm1, (%rdx)

	pextrq	$0, %xmm1, %r18
	pextrq	$0, %xmm1, (%r18)

	vpextrq	$0, %xmm1, %rdx
	vpextrq	$0, %xmm1, (%rdx)

	vpextrq	$0, %xmm17, %rdx
	vpextrq	$0, %xmm1, %r18
	vpextrq	$0, %xmm1, (%r18)
