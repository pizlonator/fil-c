	# xcvelw - architecture set
	.option push
	.option arch, rv32i
	.include "x-cv-elw.s"
	.option pop

	# xcvelw - immediate Boundary Tests
	cv.elw x5,-2049(x6)
	cv.elw x5,2048(x6)

	# xcvelw - register Boundary Tests
	cv.elw x-1,1024(x-1)
	cv.elw x32,1024(x32)
