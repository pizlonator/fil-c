	# xcvalu - architecture set
	.option push
	.option arch, rv32i
	.include "x-cv-alu.s"
	.option pop

	# xcvalu - boundaries
	cv.subnr 10, t3, t6	# Destination must be of type register
	cv.addrnr t4, 26, t6	# Source 1 must be of type register
	cv.subunr t6, t3, 15	# Source 2 must be of type register
	cv.clipu t0, t3, t6	# Five bit immediate must be an absolute value
	cv.addn t0, t3, t6, t2	# Five bit immediate must be an absolute value
	cv.clipu t0, t3, -10	# Five bit immediate must be an absolute value in range [0, 31]
	cv.clipu t0, t3, 500	# Five bit immediate must be an absolute value in range [0, 31]
	cv.addn t0, t3, t6, -60	# Five bit immediate must be an absolute value in range [0, 31]
	cv.addn t0, t3, t6, 302	# Five bit immediate must be an absolute value in range [0, 31]
	cv.clipu t0, t3, -1	# Five bit immediate must be an absolute value in range [0, 31]
	cv.clipu t0, t3, 32	# Five bit immediate must be an absolute value in range [0, 31]
	cv.addn t0, t3, t6, -1	# Five bit immediate must be an absolute value in range [0, 31]
	cv.addn t0, t3, t6, 32	# Five bit immediate must be an absolute value in range [0, 31]

	# xcvalu - destination must be of type register
	cv.abs 5,t2
	cv.slet 10,t2,t6
	cv.sletu 11,t2,t6
	cv.min 15,t2,t6
	cv.minu 16,t2,t6
	cv.max 8,t2,t6
	cv.maxu 3,t2,t6
	cv.exths 2,t2
	cv.exthz 6,t2
	cv.extbs 4,t2
	cv.extbz 7,t2
	cv.clip 17,t2,5
	cv.clipu 11,t2,5
	cv.clipr 16,t2,t6
	cv.clipur 15,t2,t6
	cv.addn 9,t2,t0,4
	cv.addun 30,t2,t0,4
	cv.addrn 21,t0,t3,9
	cv.addurn 6,t0,t3,14
	cv.addnr 2,t0,t3
	cv.addunr 26,t0,t3
	cv.addrnr 3,t0,t3
	cv.addurnr 14,t0,t3
	cv.subn 15,t0,t3,6
	cv.subun 9,t0,t3,24
	cv.subrn 24,t0,t3,21
	cv.suburn 25,t0,t3,3
	cv.subnr 3,t0,t3
	cv.subunr 12,t0,t3
	cv.subrnr 13,t0,t3
	cv.suburnr 8,t0,t3

	# xcvalu - source 1 must be of type register
	cv.abs t4,5
	cv.slet t4,7,t6
	cv.sletu t4,3,t6
	cv.min t4,5,t6
	cv.minu t4,3,t6
	cv.max t4,4,t6
	cv.maxu t4,6,t6
	cv.exths t4,30
	cv.exthz t4,23
	cv.extbs t4,25
	cv.extbz t4,21
	cv.clip t4,2,5
	cv.clipu t4,16,5
	cv.clipr t4,17,t6
	cv.clipur t4,14,t6
	cv.addn t4,5,t0,4
	cv.addun t4,18,t0,4
	cv.addrn t6,19,t3,9
	cv.addurn t6,4,t3,14
	cv.addnr t6,6,t3
	cv.addunr t6,7,t3
	cv.addrnr t6,9,t3
	cv.addurnr t6,5,t3
	cv.subn t6,11,t3,6
	cv.subun t6,14,t3,24
	cv.subrn t6,15,t3,21
	cv.suburn t6,24,t3,3
	cv.subnr t6,4,t3
	cv.subunr t6,8,t3
	cv.subrnr t6,7,t3
	cv.suburnr t6,6,t3

	# xcvalu - source 2 must be of type register
	cv.slet t4,t2,3
	cv.sletu t4,t2,4
	cv.min t4,t2,13
	cv.minu t4,t2,7
	cv.max t4,t2,17
	cv.maxu t4,t2,30
	cv.clipr t4,t2,18
	cv.clipur t4,t2,29
	cv.addn t4,t2,24,4
	cv.addun t4,t2,6,4
	cv.addrn t6,t0,7,9
	cv.addurn t6,t0,18,14
	cv.addnr t6,t0,15
	cv.addunr t6,t0,24
	cv.addrnr t6,t0,3
	cv.addurnr t6,t0,2
	cv.subn t6,t0,1,6
	cv.subun t6,t0,8,24
	cv.subrn t6,t0,18,21
	cv.suburn t6,t0,25,3
	cv.subnr t6,t0,14
	cv.subunr t6,t0,7
	cv.subrnr t6,t0,18
	cv.suburnr t6,t0,26

	# xcvalu - five bit immediate must be an absolute value
	cv.clip t4,t2,t3
	cv.clipu t4,t2,t3

	# xcvalu - five bit immediate must be an absolute value
	cv.addn t4,t2,t0,t3
	cv.addun t4,t2,t0,t3
	cv.addrn t6,t0,t3,t2
	cv.addurn t6,t0,t3,t2
	cv.subn t6,t0,t3,t2
	cv.subun t6,t0,t3,t2
	cv.subrn t6,t0,t3,t2
	cv.suburn t6,t0,t3,t2

	# xcvalu - five bit immediate must be an absolute value in range [0, 31]
	cv.clip t0,t3,-1
	cv.clipu t0,t3,-1
	cv.clip t0,t3,-400
	cv.clipu t0,t3,-985
	cv.clip t0,t3,32
	cv.clipu t0,t3,32
	cv.clip t0,t3,859
	cv.clipu t0,t3,7283

	# xcvalu - five bit immediate must be an absolute value in range [0, 31]
	cv.addn t4,t2,t0,-1
	cv.addun t4,t2,t0,-1
	cv.addrn t6,t0,t3,-1
	cv.addurn t6,t0,t3,-1
	cv.subn t6,t0,t3,-1
	cv.subun t6,t0,t3,-1
	cv.subrn t6,t0,t3,-1
	cv.suburn t6,t0,t3,-1
	cv.addn t4,t2,t0,-34
	cv.addun t4,t2,t0,-3556
	cv.addrn t6,t0,t3,-212
	cv.addurn t6,t0,t3,-6584
	cv.subn t6,t0,t3,-89
	cv.subun t6,t0,t3,-9034
	cv.subrn t6,t0,t3,-234
	cv.suburn t6,t0,t3,-284
	cv.addn t4,t2,t0,32
	cv.addun t4,t2,t0,32
	cv.addrn t6,t0,t3,32
	cv.addurn t6,t0,t3,32
	cv.subn t6,t0,t3,32
	cv.subun t6,t0,t3,32
	cv.subrn t6,t0,t3,32
	cv.suburn t6,t0,t3,32
	cv.addn t4,t2,t0,320
	cv.addun t4,t2,t0,34534
	cv.addrn t6,t0,t3,254
	cv.addurn t6,t0,t3,398
	cv.subn t6,t0,t3,89
	cv.subun t6,t0,t3,3489
	cv.subrn t6,t0,t3,143
	cv.suburn t6,t0,t3,234
