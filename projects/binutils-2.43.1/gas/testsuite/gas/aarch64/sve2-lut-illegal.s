	// Operand mismatch
	luti2	z2.b, { z5.h }, z7[1]
	luti2	z2.h, { z5.b }, z7[1]
	luti4	z2.b, { z5.h }, z7[1]
	luti4	z2.h, { z5.b }, z7[1]
	luti4	z2.b, { z5.h, z6.h }, z12[1]
	luti4	z2.h, { z5.b, z6.b }, z12[1]
	luti4	z2.b, { z5.b, z6.h }, z12[1]
	luti4	z2.h, { z5.h, z6.b }, z12[1]

	// Incorrect operands
	.macro incorrect_operands, op, operand_type
	\op	z5\operand_type, { }, z7[1]
	\op	{ z5\operand_type }, z6\operand_type, z7[1]
	\op	z5\operand_type, { z7\operand_type, z8\operand_type }, z9[1]
	.endm

	incorrect_operands luti2 .b
	incorrect_operands luti2 .h
	incorrect_operands luti4 .b
	luti4	z5.h, { }, z7[1]
	luti4	{ z5.h }, z6.h, z7[1]
	luti4	z5.h, { z7.h, z9.h }, z3[3]

	// Disallowed types
	luti2	z5.s, { z7.s }, z9[1]
	luti4	z5.s, { z7.s }, z9[1]
	luti4	z5.s, { z7.s, z8.s }, z9[1]

	// Incorrect number of operands
	.macro incorrect_number_of_operands, op, operand_type
	\op	z5\operand_type
	\op	z5\operand_type, { z7\operand_type }
	\op	z5\operand_type, { z7\operand_type }, z9[1], z11\operand_type
	.endm
	incorrect_number_of_operands luti2 .b
	incorrect_number_of_operands luti2 .h
	incorrect_number_of_operands luti4 .b
	luti4	z5.h
	luti4	z5.h, { z7.h }
	luti4	z5.h, { z7.h }, z9[1], z11.h
	luti4	z5.h, { z7.h, z8.h }
	luti4	z5.h, { z7.h, z8.h }, z9[1], z11.h

	// Spelling mistakes
	.macro spelling_mistakes, op, operand_type
	\op	z5.t, { z7\operand_type }, z9[1]
	\op	z5\operand_type, { z7.t }, z9[1]
	\op	z5\operand_type, { z7\operand_type }, z9\operand_type
	.endm

	spelling_mistakes luti2 .b
	spelling_mistakes luti2 .h
	spelling_mistakes luti4 .b
	luti4	z5.t, { z7.h, z8.h }, z9[1]
	luti4	z5.h, { z7.t, z8.h }, z9[1]
	luti4	z5.h, { z7.h, z8.t }, z9[1]
	luti4	z5.h, { z7.h, z8.h }, z9.h

	// Missing qualifiers
	.macro missing_qualifiers, op, operand_type
	\op	5\operand_type, { z7\operand_type }, z9[1]
	\op	z5\operand_type, { z7 }, z9[1]
	\op	z5\operand_type, { 7\operand_type }, z9[1]
	\op	z5\operand_type, { z7\operand_type }, 9[1]
	.endm

	missing_qualifiers luti2 .b
	missing_qualifiers luti2 .h
	missing_qualifiers luti4 .b
	luti4	z5.h, { z7, z8.h }, z9[1]
	luti4	5.h, { z7.h, z8.h }, z9[1]
	luti4	z5.h, { 7.h, z8.h }, z9[1]
	luti4	z5.h, { z7.h, z8.h }, 9[1]

	// Index with qualifiers
	luti2	z5.b, { z7.b }, z9.b[2]
	luti2	z5.h, { z7.h }, z9.h[2]
	luti4	z5.b, { z7.b }, z9.b[1]
	luti4	z5.h, { z7.h }, z9.h[2]
	luti4	z5.h, { z7.h, z8.h }, z9.h[2]

	// Index on the wrong operand
	.macro index_wrong_operand op, operand_type
	\op	z4\operand_type[2], { z9\operand_type }, z15
	\op	z4[2], { z9\operand_type }, z15
	\op	z4\operand_type, { z9\operand_type[2] }, z15
	\op	z4[2], { z9[2] }, z15
	.endm

	index_wrong_operand luti2 .b
	index_wrong_operand luti2 .h
	index_wrong_operand luti4 .b
	luti4	z4.h[2], { z9.h, z10.h }, z15
	luti4	z4[2], { z9.h, z10.h }, z15
	luti4	z4.h, { z9.h, z10.h[2] }, z15
	luti4	z4[2], { z9, z10[2] }, z15

	// Missing index
	luti2	z4.b, { z9.b }, z15.b
	luti2	z4.b, { z9.b }, z15
	luti2	z4.h, { z9.h }, z15.h
	luti2	z4.h, { z9.h }, z15

	luti4	z4.b, { z9.b }, z15.b
	luti4	z4.b, { z9.b }, z15
	luti4	z4.h, { z9.h }, z15.h
	luti4	z4.h, { z9.h }, z15
	luti4	z4.h, { z9.h, z10.h }, z15.h
	luti4	z4.h, { z9.h, z10.h }, z15

	// Out of range numbers
	.macro out_of_range op, operand_type, max_index_plus_one
	\op	z32\operand_type, { z9\operand_type }, z15[1]
	\op	z4\operand_type, { z32\operand_type }, z4[1]
	\op	z4\operand_type, { z9\operand_type }, z32[1]
	\op	z2\operand_type, { z9\operand_type }, z4[\max_index_plus_one]
	\op	z4\operand_type, { z9\operand_type }, z15[-1]
	.endm

	out_of_range luti2, .b, 4
	out_of_range luti2, .h, 8
	out_of_range luti4, .b, 2
	luti4	z32.h, { z9.h }, z4[2]
	luti4	z4.h, { z32.h }, z15[2]
	luti4	z4.h, { z9.h }, z32[2]
	luti4	z2.h, { z9.h }, z15[4]
	luti4	z4.h, { z9.h }, z15[-1]
