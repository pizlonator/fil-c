foo:
	cv.beqimm t0, -16, foo
	cv.beqimm t4, 5, foo
	cv.beqimm t2, 15, foo
	cv.bneimm t0, -16, foo
	cv.bneimm t4, 5, foo
	cv.bneimm t2, 15, foo
