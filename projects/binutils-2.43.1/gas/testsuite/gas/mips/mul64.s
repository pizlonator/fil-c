# Sanity check the 64 bit mul macro versions.

foo:
	dmul	$4,$5,$6
	dmul	$4,$5,1
	dmulo	$4,$5,$6
	dmulou	$4,$5,$6

	.align	4, 0
	.space	16
