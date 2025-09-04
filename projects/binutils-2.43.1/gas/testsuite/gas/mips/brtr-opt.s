# We use XOR placeholders to avoid implicit NOPs causing output variation.
	.text
	.set	nodsp
	.set	noeva
	.set	nomips16e2
	.set	nomt
	.set	novirt
foo:
	div	$6, $4, $0
	divu	$6, $4, 0
	rem	$6, $4, $2
	xor	$16, $16
	remu	$6, $4, $2
	xor	$16, $16
	xor	$17, $17
	mulo	$6, $4, $2
	xor	$16, $16
	mulou	$6, $4, 2

	.set	mips1
	div	$6, $4, $0
	divu	$6, $4, 0
	rem	$6, $4, $2
	xor	$16, $16
	remu	$6, $4, $2
	xor	$16, $16
	xor	$17, $17
	mulo	$6, $4, $2
	xor	$16, $16
	mulou	$6, $4, 2

	.set	mips2
	div	$6, $4, $0
	divu	$6, $4, 0
	rem	$6, $4, $2
	xor	$16, $16
	remu	$6, $4, $2
	xor	$16, $16
	xor	$17, $17
	mulo	$6, $4, $2
	xor	$16, $16
	mulou	$6, $4, 2

# Force some (non-delay-slot) zero bytes, to make 'objdump' print ...
	.align	4, 0
	.space	16
