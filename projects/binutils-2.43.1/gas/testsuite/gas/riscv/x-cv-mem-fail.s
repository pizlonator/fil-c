	# xcvmem - architecture set
	.option push
	.option arch, rv32i
	.include "x-cv-mem.s"
	.option pop

	# xcvmem - destination operand must be a register
	cv.lb 20, 10(t1)
	cv.lb 32, (t2), 15
	cv.lb 40, t2(t3)
	cv.lb 28, (t4), t3
	cv.lbu 16, 20(t5)
	cv.lbu 20, (t6), 30
	cv.lbu 44, t4(t1)
	cv.lbu 48, (t2), t5
	cv.lh 52, 25(t3)
	cv.lh 12, (t4), 10
	cv.lh 16, t6(t5)
	cv.lh 36, (t6), t1
	cv.lhu 24, 35(t1)
	cv.lhu 12, (t2), 13
	cv.lhu 32, t2(t3)
	cv.lhu 40, (t4), t3
	cv.lw 44, 18(t5)
	cv.lw 48, (t6), 8
	cv.lw 24, t4(t1)
	cv.lw 12, (t2), t5

	# xcvmem - source operand must be a register
	cv.sb 12, 10(t1)
	cv.sb 14, (t2), 20
	cv.sb 16, t1(t3)
	cv.sb 20, (t4), t2
	cv.sh 30, 30(t5)
	cv.sh 15, (t6), 40
	cv.sh 45, t3(t1)
	cv.sh 52, (t2), t4
	cv.sw 12, 12(t3)
	cv.sw 10, (t4), 16
	cv.sw 82, t5(t5)
	cv.sw 14, (t1), t6

	# xcvmem - base operand must be a register
	cv.sb t0, 10(12)
	cv.sb t1, (24), 20
	cv.sb t2, t1(25)
	cv.sb t3, (75), t2
	cv.sh t4, 30(13)
	cv.sh t5, (16), 40
	cv.sh t6, t3(31)
	cv.sh t0, (37), t4
	cv.sw t1, 12(51)
	cv.sw t2, (43), 16
	cv.sw t3, t5(61)
	cv.sw t4, (67), t6
	cv.lb t0, 12(12)
	cv.lb t1, (24), 13
	cv.lb t2, t3(25)
	cv.lb t3, (75), t4
	cv.lbu t4, 22(51)
	cv.lbu t5, (43), 10
	cv.lbu t6, t5(61)
	cv.lbu t0, (67), t6
	cv.lh t1, 19(13)
	cv.lh t2, (16), 41
	cv.lh t3, t0(31)
	cv.lh t4, (37), t1
	cv.lhu t5, 15(14)
	cv.lhu t6, (17), 12
	cv.lhu t0, t2(14)
	cv.lhu t1, (39), t3
	cv.lw t2, 4(24)
	cv.lw t3, (21), 6
	cv.lw t5, t4(16)
	cv.lw t4, (47), t5

	# xcvmem - offset operand must be in range [-2048, 2047]
	cv.lb t2, -2049(t1)
	cv.lb t2, (t1), -2049
	cv.lbu t2, -2049(t1)
	cv.lbu t2, (t1), -2049
	cv.lh t2, -2049(t1)
	cv.lh t2, (t1), -2049
	cv.lhu t2, -2049(t1)
	cv.lhu t2, (t1), -2049
	cv.lw t2, -2049(t1)
	cv.lw t2, (t1), -2049
	cv.lb t2, -3678(t1)
	cv.lb t2, (t1), -3678
	cv.lbu t2, -3678(t1)
	cv.lbu t2, (t1), -3678
	cv.lh t2, -3678(t1)
	cv.lh t2, (t1), -3678
	cv.lhu t2, -3678(t1)
	cv.lhu t2, (t1), -3678
	cv.lw t2, -3678(t1)
	cv.lw t2, (t1), -3678
	cv.lb t2, 2048(t1)
	cv.lb t2, (t1), 2048
	cv.lbu t2, 2048(t1)
	cv.lbu t2, (t1), 2048
	cv.lh t2, 2048(t1)
	cv.lh t2, (t1), 2048
	cv.lhu t2, 2048(t1)
	cv.lhu t2, (t1), 2048
	cv.lw t2, 2048(t1)
	cv.lw t2, (t1), 2048
	cv.lb t2, 4595(t1)
	cv.lb t2, (t1), 4595
	cv.lbu t2, 4595(t1)
	cv.lbu t2, (t1), 4595
	cv.lh t2, 4595(t1)
	cv.lh t2, (t1), 4595
	cv.lhu t2, 4595(t1)
	cv.lhu t2, (t1), 4595
	cv.lw t2, 4595(t1)
	cv.lw t2, (t1), 4595

	# xcvmem - offset operand must be in range [-2048, 2047]
	cv.sb t2, -2049(t1)
	cv.sb t2, (t1), -2049
	cv.sh t2, -2049(t1)
	cv.sh t2, (t1), -2049
	cv.sw t2, -2049(t1)
	cv.sw t2, (t1), -2049
	cv.sb t2, -3669(t1)
	cv.sb t2, (t1), -3669
	cv.sh t2, -3669(t1)
	cv.sh t2, (t1), -3669
	cv.sw t2, -3669(t1)
	cv.sw t2, (t1), -3669
	cv.sb t2, 2048(t1)
	cv.sb t2, (t1), 2048
	cv.sh t2, 2048(t1)
	cv.sh t2, (t1), 2048
	cv.sw t2, 2048(t1)
	cv.sw t2, (t1), 2048
	cv.sb t2, 5341(t1)
	cv.sb t2, (t1), 5341
	cv.sh t2, 5341(t1)
	cv.sh t2, (t1), 5341
	cv.sw t2, 5341(t1)
	cv.sw t2, (t1), 5341
