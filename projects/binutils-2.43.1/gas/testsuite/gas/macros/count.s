
	.macro mac1 count
	.print "\@"
	.print "\+"
	.if \count > 1
	mac1 \count-1
	.endif
	.endm

	.macro mac2 count
	.print "\@"
	.print "\+"
	.if \count > 1
	mac2 \count-1
	.endif
	.endm

	mac1 2
	mac2 3
