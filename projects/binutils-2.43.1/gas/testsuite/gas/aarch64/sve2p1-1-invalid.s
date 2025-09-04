	.irp op1 addqv, andqv, smaxqv, sminqv, umaxqv, uminqv, eorqv
	movprfx z3, z5
	\op1 v0.16b, p0, z0.b
	movprfx z3, z5
	\op1 v0.8h, p0, z0.h
	movprfx z3, z5
	\op1 v0.4s, p0, z0.s
	movprfx z3, z5
	\op1 v0.2d, p0, z0.d
	.endr
	.irp op1 addqv, andqv, smaxqv, sminqv, umaxqv, uminqv, eorqv, faddqv, fmaxnmqv, fmaxqv, fminnmqv, fminqv
	movprfx   z0.d, p0/m, z31.d
	\op1 v0.8h, p0, z0.h
	movprfx   z0.d, p0/m, z31.d
	\op1 v0.4s, p0, z0.s
	movprfx   z0.d, p0/m, z31.d
	\op1 v0.2d, p0, z0.d
	.endr
	.irp op1 addqv, andqv, smaxqv, sminqv, umaxqv, uminqv, eorqv, faddqv, fmaxnmqv, fmaxqv, fminnmqv, fminqv
	movprfx   z0.d, p0/z, z31.d
	\op1 v0.8h, p0, z0.h
	movprfx   z0.d, p0/z, z31.d
	\op1 v0.4s, p0, z0.s
	movprfx   z0.d, p0/z, z31.d
	\op1 v0.2d, p0, z0.d
	.endr
