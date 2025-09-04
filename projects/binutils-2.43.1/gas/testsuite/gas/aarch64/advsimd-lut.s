	// Valid luti2 instructions
	luti2	v0.16b, { v0.16b }, v0[0]
	luti2	v31.16b, { v0.16b }, v0[0]
	luti2	v0.16b, { v31.16b }, v0[0]
	luti2	v0.16b, { v0.16b }, v31[0]
	luti2	v0.16b, { v0.16b }, v31[3]
	luti2	v17.16b, { v21.16b }, v27[2]

	luti2	v0.8h, { v0.8h }, v0[0]
	luti2	v31.8h, { v0.8h }, v0[0]
	luti2	v0.8h, { v31.8h }, v0[0]
	luti2	v0.8h, { v0.8h }, v31[0]
	luti2	v0.8h, { v0.8h }, v0[7]
	luti2	v17.8h, { v21.8h }, v27[4]

	// Valid luti4 instructions
	luti4	v0.16b, { v0.16b }, v0[0]
	luti4	v31.16b, { v0.16b }, v0[0]
	luti4	v0.16b, { v31.16b }, v0[0]
	luti4	v0.16b, { v0.16b }, v31[0]
	luti4	v0.16b, { v0.16b }, v0[1]
	luti4	v17.16b, { v21.16b }, v27[1]

	luti4	v0.8h, { v0.8h, v1.8h }, v0[0]
	luti4	v31.8h, { v0.8h, v1.8h }, v0[0]
	luti4	v0.8h, { v31.8h, v0.8h }, v0[0]
	luti4	v0.8h, { v0.8h, v1.8h }, v31[0]
	luti4	v0.8h, { v0.8h, v1.8h }, v0[3]
	luti4	v17.8h, { v21.8h, v22.8h }, v27[2]
