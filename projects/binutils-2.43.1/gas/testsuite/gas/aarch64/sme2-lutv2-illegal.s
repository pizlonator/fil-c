	// Operand mismatch
	luti4	{ z20.b - z23.h }, zt0, { z13 - z14 }
	luti4	{ z19.b, z23.b, z27.b, z31.h }, zt0, { z13 - z14 }

	// Incorrect operands
	movt	z3, zt0
	movt 	z3[0], zt0
	movt 	zt1, z25

	luti4	zt0, { z0.b - z3.b }, { z0 - z1 }
	luti4	{ z0.b - z3.b }, { z0.b - z1.b } { z0 - z1 }
	luti4	{ }, zt0, { z12 - z13 }
	luti4	{ z20.b - z23.b }, zt0, { }
	luti4	{ z20.b - z23.b }, z3, { z12 - z13 }
	luti4	{ z19.b - z22.b }, zt0, { z12 - z13 }
	luti4	{ z20.b - z23.b }, zt0, { z13 - z14 }

	luti4	zt0, { z0.b, z4.b, z8.b, z12.b }, { z0 - z1 }
	luti4	{ z0.b, z4.b, z8.b, z12.b }, { z0.b - z1.b } { z0 - z1 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, z3, { z12 - z13 }
	luti4	{ z18.b, z22.b, z26.b, z30.b }, zt0, { z12 - z13 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { z13 - z14 }

	// Disallowed types
	luti4	{ z20.s - z23.s }, zt0, { z20 - z21 }
	luti4	{ z19.s, z23.s, z27.s, z31.s }, zt0, { z20 - z21 }

	// Index with qualifiers
	movt zt0.b, z31
	movt zt0.b[0, mul vl], z31

	// Incorrect number of operands
	movt	zt0
	movt 	zt0[1, mul vl]
	movt	zt0, z23, z31
	movt	zt0[1, mul vl], z23, z31

	luti4	{ z20.b - z23.b }
	luti4	{ z20.b - z23.b }, zt0
	luti4	{ z20.b - z23.b }, zt0, { z12 - z13 }, { z20 - z21 }

	luti4	{ z19.b, z23.b, z27.b, z31.b }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0 { z12 - z13 }, { z20 - z21 }

	// Spelling mistakes
	movt	zy0, z16
	movt	zt1, z16
	movt	zy0[1, mul vl], z16
	movt	zt0, y16
	movt	zt0[1, mul vl], y16

	luti4	{ z20.b - y23.b }, zt0, { z12 - z13 }
	luti4	{ z20.x - z23.b }, zt0, { z12 - z13 }
	luti4	{ z20.b - z23.b }, zy0, { z12 - z13 }
	luti4	{ z20.b - y23.b }, zt0, { y12 - z13 }

	luti4	{ z19.b, z23.b, z27.b, y31.b }, zt0, { z12 - z13 }
	luti4	{ z19.x, z23.b, z27.b, z31.b }, zt0, { z12 - z13 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zy0, { z12 - z13 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { y12 - z13 }

	// Missing qualifiers
	luti4	{ z20 - z23.b }, zt0, { z12 - z13 }
	luti4	{ z19.b, z23, z27.b, z31.b }, zt0, { z12 - z13 }

	// Qualifier on the wrong operand
	movt 	zt, z25.b
	luti4	{ z20.b - z23.b }, zt0, { z12.b - z13.b }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { z12.b - z13.b }

	// Index on the wrong operand
	movt	zt0, z25[1]
	movt	zt0, z25[1, mul vl]

	// Out of range numbers
	movt	zt0[2, mul vl], z32
	movt	zt0[4, mul vl], z25
	movt	zt0[-1, mul vl], z25

	luti4	{ z32.b - z36.b }, zt0, { z12 - z13 }
	luti4	{ z20.b - z23.b }, zt0, { z32 - z33 }

	luti4	{ z32.b, z36.b, z40.b, z44.b }, zt0, { z12 - z13 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { z32 - z33 }

	// Incorrect stride
	luti4	{ z20.b - z24.b }, zt0, { z12 - z13 }
	luti4	{ z20.b - z22.b }, zt0, { z12 - z13 }
	luti4	{ z20.b - z23.b }, zt0, { z12 - z14 }
	luti4	{ z20.b - z23.b }, zt0, { z12 - z12 }
	luti4	{ z19.b, z24.b, z27.b, z31.b }, zt0, { z12 - z13 }
	luti4	{ z19.b, z22.b, z27.b, z31.b }, zt0, { z12 - z13 }
	luti4	{ z19.b, z23.b, z27.b, z31.b }, zt0, { z12 - z14 }
