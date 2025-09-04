	.text

	.arch	armv9.5-a
	// CPA
	addpt   x0, x0, x0
	// FAMINMAX
	famax	v0.4h, v0.4h, v0.4h
	// LUT
	luti2	v0.16b, { v0.16b }, v0[0]
