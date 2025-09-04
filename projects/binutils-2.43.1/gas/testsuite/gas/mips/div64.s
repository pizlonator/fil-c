# Sanity check the 64 bit div macro versions.
foo:
	ddiv	$4,$5,$6
	ddivu	$4,$5,2
	drem	$4,$5,0x8000
	dremu	$4,$5,-0x8000

# Force some (non-delay-slot) zero bytes, to make 'objdump' print ...
	.align	4, 0
	.space	16
