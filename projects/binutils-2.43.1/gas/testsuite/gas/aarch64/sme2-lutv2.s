	movt zt0, z0
	movt zt0[0, mul vl], z31
	movt zt0[3, mul vl], z0
	movt zt0[3, mul vl], z31
	movt zt0[2, mul vl], z25

	luti4	{ z0.b - z3.b }, zt0, { z0 - z1 }
	luti4	{ z28.b - z31.b }, zt0, { z0 - z1 }
	luti4	{ z0.b - z3.b }, zt0, { z30 - z31 }
	luti4	{ z20.b - z23.b }, zt0, { z12 - z13 }

	luti4	{ z0.b, z4.b, z8.b, z12.b }, zt0, { z0 - z1 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { z0 - z1 }
	luti4	{ z0.b, z4.b, z8.b, z12.b }, zt0, { z30 - z31 }
	luti4	{ z17.b, z21.b, z25.b, z29.b }, zt0, { z12 - z13 }

	// Explicitly listing registers in stride 1 variant
	luti4	{ z20.b, z21.b, z22.b, z23.b }, zt0, { z12 - z13 }

	// Invalid instructions because sz != 00
	.inst	0xc08b2194
	.inst	0xc09b2191
