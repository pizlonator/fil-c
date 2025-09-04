.*: Assembler messages:
.*:13: Error: SCFI: unsupported stack manipulation pattern
.*:19: Error: SCFI: forward pass failed for func 'foo'

AARCH64 GAS  .*


   1              	# Testcase for a variety of arith instructions
   2              		.text
   3              		.align  2
   4              		.global foo
   5              		.type   foo, %function
   5              	ginsn: SYM FUNC_BEGIN
   6              	foo:
   6              	ginsn: SYM foo
   7 \?\?\?\? FF830091 		add     sp, sp, 32
   7              	ginsn: ADD %r31, 32, %r31
   8 \?\?\?\? FD5B21B1 		adds    x29, sp, 2134
   8              	ginsn: ADD %r31, 2134, %r29
   9 \?\?\?\? FF8300D1 		sub     sp, sp, 32
   9              	ginsn: SUB %r31, 32, %r31
  10 \?\?\?\? FD5B21F1 		subs    x29, sp, 2134
  10              	ginsn: SUB %r31, 2134, %r29
  11 \?\?\?\? FD63228B 		add     x29, sp, x2
  11              	ginsn: ADD %r31, %r2, %r29
  12 \?\?\?\? FD6323CB 		sub     x29, sp, x3
  12              	ginsn: SUB %r31, %r3, %r29
  13 \?\?\?\? BF63238B 		add     sp, x29, x3
  13              	ginsn: ADD %r29, %r3, %r31
  14 \?\?\?\? BF6322CB 		sub     sp, x29, x2
  14              	ginsn: SUB %r29, %r2, %r31
  15 \?\?\?\? 1F48220B 		add     wsp, w0, w2, lsl 2
  15              	ginsn: OTH 0, 0, %r31
  16 \?\?\?\? A4C0228B 		add     x4, x5, x2, sxtw
  17 \?\?\?\? BFC0228B 		add     sp, x5, x2, sxtw
  17              	ginsn: OTH 0, 0, %r31
  18 \?\?\?\? C0035FD6 		ret
  18              	ginsn: RET
  19              		.size   foo, .-foo
  19              	ginsn: SYM FUNC_END
