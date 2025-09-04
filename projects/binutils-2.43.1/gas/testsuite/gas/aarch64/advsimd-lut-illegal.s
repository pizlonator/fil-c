	// Operand mismatch
	luti2	v2.16b, { v4.8h }, v8[1]
	luti2	v2.8h, { v4.16b }, v8[1]
	luti4	v2.16b, { v4.8h }, v8[5]
	luti4	v2.8h, { v4.16b }, v8[5]

	// Incorrect operand types
	.macro incorrect_operand_types op, operand_type
	\op	v2\operand_type, v4\operand_type, v8[1]
	\op	x12, { v4\operand_type }, v8[1]
	\op	v2\operand_type, { x12 }, v8[1]
	\op	v2\operand_type, { v4\operand_type }, x12[1]
	.endm

	incorrect_operand_types luti2 .16b
	incorrect_operand_types luti2 .8h
	incorrect_operand_types luti4 .16b
	incorrect_operand_types luti4 .8h
	luti4	v2.8h, { v4.8h, x12 }, v8[1]

	// Disallowed types
	luti2	v2.8b, { v4.8b }, v8[1]
	luti2	v2.4h, { v4.4h }, v8[1]
	luti4	v2.8b, { v4.8b }, v8[5]
	luti4	v2.4h, { v4.4h, v5.4h }, v8[5]

	// Incorrect number of operands
	.macro 	incorrect_operands, op, operand_type
	\op	v2\operand_type
	\op	v2\operand_type, { v4\operand_type }
	\op	v2\operand_type, { v4\operand_type }, v8[1], v16\operand_type
	\op	v2\operand_type, { v4\operand_type }, v8[1], { v16\operand_type }
	.endm

	incorrect_operands luti2 .16b
	incorrect_operands luti2 .8h
	incorrect_operands luti4 .16b
	luti4	v2.8h
	luti4	v2.8h, { v4.8h, v5.8h }
	luti4	v2.8h, { v4.8h, v5.8h }, v8[1], v16.8h
	luti4	v2.8h, { v4.8h, v5.8h }, v8[1], { v16.8h }

	// Spelling mistakes
	.macro 	spelling_mistakes, op, operand_type, incorrect_operand_type
	\op	v2\operand_type, { v4\incorrect_operand_type }, v8[1]
	\op	v2\incorrect_operand_type, { v4\operand_type }, v8[1]
	.endm

	spelling_mistakes luti2, .16b, .16t
	spelling_mistakes luti2, .8h, .8m
	spelling_mistakes luti4, .16b, .16t
	luti4	v2.8h, { v4.8h, v5.8t }, v8[1]
	luti4	v2.8t, { v4.8h, v5.8h }, v8[1]

	// Missing qualifiers
	.macro	missing_qualifiers, op, operand_type
	\op	v2\operand_type, { v4 }, v8[1]
	\op	v2, { v4\operand_type }, v8[1]
	\op	2\operand_type, { v4\operand_type }, v8[1]
	\op	v2\operand_type, { 4\operand_type }, v8[1]
	.endm

	missing_qualifiers luti2, .16b
	missing_qualifiers luti2, .8h
	missing_qualifiers luti4, .16b
	luti4	v2.16b, { v4, v5.16b }, v8[1]
	luti4	v2, { v4.16b, v5.16b }, v8[1]
	luti4	2.16b, { v4.16b, v5.16b }, v8[1]
	luti4	v2.16b, { v4.16b, 5.16b }, v8[1]

	// Index with qualifiers
	luti2	v17.16b, { v21.16b }, v27.16b[3]
	luti2	v17.8h, { v21.8h }, v27.8h[4]
	luti4	v17.16b, { v21.16b }, v27.16b[1]
	luti4	v17.8h, { v21.8h, v22.8h }, v27.8h[2]

	// Index on the wrong operand
	.macro index_wrong_operand, op, operand_type
	\op	v17\operand_type[1], { v0\operand_type }, v31\operand_type
	\op	v17[1], { v0\operand_type }, v31\operand_type
	\op	v17\operand_type, { v0\operand_type[1] }, v31\operand_type
	\op	v17\operand_type, { v0[1] }, v31\operand_type
	.endm

	index_wrong_operand luti2, .16b
	index_wrong_operand luti2, .8h
	index_wrong_operand luti4, .16b
	luti4	v17.8h[1], { v0.8h, v1.8h }, v31.8h
	luti4	v17[1], { v0.8h, v1.8h }, v31.8h
	luti4	v17.8h, { v0.8h[1], v1.8h }, v31.8h
	luti4	v17.8h, { v0[1], v1.8h }, v31.8h

	// Missing index
	luti2	v2.16b, { v4.16b }, v8.16b
	luti2	v2.16b, { v4.16b }, v8
	luti2	v2.8h, { v4.8h }, v8.8h
	luti2	v2.8h, { v4.8h }, v8
	luti4	v2.16b, { v4.16b }, v8.16b
	luti4	v2.16b, { v4.16b }, v8
	luti4	v2.8h, { v4.8h, v5.8h }, v8.8h
	luti4	v2.8h, { v4.8h, v5.8h }, v8

	// Out of range operands
	.macro	out_of_range, op, operand_type, max_index_plus_one
	\op	v32\operand_type, { v4\operand_type }, v8[1]
	\op	v2\operand_type, { v32\operand_type }, v8[1]
	\op	v2\operand_type, { v4\operand_type }, v32[1]
	\op	v2\operand_type, { v4\operand_type }, v8[\max_index_plus_one]
	\op	v2\operand_type, { v4\operand_type }, v8[-1]
	.endm

	out_of_range luti2, .16b, 4
	out_of_range luti2, .8h, 8
	out_of_range luti4, .16b, 2
	luti4	v32.8h, { v4.8h, v5.8h }, v8[1]
	luti4	v2.8h, { v31.8h, v32.8h }, v8[1]
	luti4	v2.8h, { v4.8h, v5.8h }, v32[1]
	luti4	v2.8h, { v4.8h, v5.8h }, v8[4]
	luti4	v2.8h, { v4.8h, v5.8h }, v8[-1]

	// Incorrect stride of operand
	luti4	v2.8h, { v4.8h, v6.8h }, v8[2]

	// Incorrect operands
	luti2	v17.16b, { v21.16b, v22.16b }, v27[2]
	luti2	v17.8h, { v21.8h, v22.8h }, v27[4]
	luti4	v17.16b, { v21.16b, v22.16b }, v27[1]
	luti4	v17.8h, { v21.8h }, v27[2]
