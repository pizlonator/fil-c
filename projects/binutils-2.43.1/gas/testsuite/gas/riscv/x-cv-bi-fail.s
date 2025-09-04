	# xcvbi - architecture set
	.option push
	.option arch, rv32i
	.include "x-cv-bi.s"
	.option pop

foo:
	# xcvbi - comparison target must be a register
	cv.beqimm 20, 10, foo
	cv.bneimm 8, -4, foo

	# xcvbi - comparison value must be an immediate
	cv.beqimm t0, t1, foo
	cv.bneimm t3, t4, foo

	# xcvbi - comparison value must be an immediate in range [-16, +15]
	cv.beqimm t0, -17, foo
	cv.beqimm t2, -32, foo
	cv.beqimm t4, 16, foo
	cv.beqimm t3, 44, foo
	cv.bneimm t0, -17, foo
	cv.bneimm t2, -32, foo
	cv.bneimm t4, 16, foo
	cv.bneimm t3, 44, foo
