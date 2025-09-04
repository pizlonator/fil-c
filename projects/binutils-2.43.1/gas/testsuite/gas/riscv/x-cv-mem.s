	# xcvmem - lbpost
	cv.lb t0, (t2), -2048
	cv.lb t4, (t1), 15
	cv.lb t3, (t5), 2047
	# xcvmem - lbrr
	cv.lb t0, t1(t2)
	cv.lb t4, t3(t1)
	cv.lb t3, t0(t5)
	# xcvmem - lbrrpost
	cv.lb t0, (t2), t1
	cv.lb t4, (t1), t3
	cv.lb t3, (t5), t0

	# xcvmem - lbupost
	cv.lbu t0, (t2), -2048
	cv.lbu t4, (t1), 15
	cv.lbu t3, (t5), 2047
	# xcvmem - lburr
	cv.lbu t0, t1(t2)
	cv.lbu t4, t3(t1)
	cv.lbu t3, t0(t5)
	# xcvmem - lburrpost
	cv.lbu t0, (t2), t1
	cv.lbu t4, (t1), t3
	cv.lbu t3, (t5), t0

	# xcvmem - lhpost
	cv.lh t0, (t2), -2048
	cv.lh t4, (t1), 15
	cv.lh t3, (t5), 2047
	# xcvmem - lhrr
	cv.lh t0, t1(t2)
	cv.lh t4, t3(t1)
	cv.lh t3, t0(t5)
	# xcvmem - lhrrpost
	cv.lh t0, (t2), t1
	cv.lh t4, (t1), t3
	cv.lh t3, (t5), t0

	# xcvmem - lhupost
	cv.lhu t0, (t2), -2048
	cv.lhu t4, (t1), 15
	cv.lhu t3, (t5), 2047
	# xcvmem - lhurr
	cv.lhu t0, t1(t2)
	cv.lhu t4, t3(t1)
	cv.lhu t3, t0(t5)
	# xcvmem - lhurrpost
	cv.lhu t0, (t2), t1
	cv.lhu t4, (t1), t3
	cv.lhu t3, (t5), t0

	# xcvmem - lwpost
	cv.lw t0, (t2), -2048
	cv.lw t4, (t1), 15
	cv.lw t3, (t5), 2047
	# xcvmem - lwrr
	cv.lw t0, t1(t2)
	cv.lw t4, t3(t1)
	cv.lw t3, t0(t5)
	# xcvmem - lwrrpost
	cv.lw t0, (t2), t1
	cv.lw t4, (t1), t3
	cv.lw t3, (t5), t0

	# xcvmem - sbpost
	cv.sb t6, (t1), -2048
	cv.sb t4, (t2), 100
	cv.sb t3, (t4), 2047
	# xcvmem - sbrr
	cv.sb t0, t1(t2)
	cv.sb t4, t3(t1)
	cv.sb t3, t0(t5)
	# xcvmem - sbrrpost
	cv.sb t0, (t2), t1
	cv.sb t4, (t1), t3
	cv.sb t3, (t5), t0

	# xcvmem - shpost
	cv.sh t3, (t5), -2048
	cv.sh t1, (t6), 60
	cv.sh t2, (t4), 2047
	# xcvmem - shrr
	cv.sh t1, t3(t4)
	cv.sh t4, t2(t1)
	cv.sh t6, t5(t2)
	# xcvmem - shrrpost
	cv.sh t1, (t2), t6
	cv.sh t2, (t1), t5
	cv.sh t5, (t3), t4

	# xcvmem - swpost
	cv.sw t1, (t4), -2048
	cv.sw t3, (t5), 100
	cv.sw t1, (t2), 2047
	# xcvmem - swrr
	cv.sw t1, t2(t4)
	cv.sw t3, t2(t5)
	cv.sw t1, t5(t2)
	# xcvmem - swrrpost
	cv.sw t5, (t2), t6
	cv.sw t3, (t4), t1
	cv.sw t2, (t1), t4
