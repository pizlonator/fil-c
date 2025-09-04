.*: Assembler messages:
.*:16: Error: untraceable control flow for func 'foo'

AARCH64 GAS  .*

   1              	# Testcase for a variety of change of flow instructions
   2              	# Because some of these are indirect branches, SCFI will bail out
   3              	# with an error.  This test merely checks that the ginsn creation
   4              	# process can handle these insns gracefully.
   5              		.text
   6              		.align  2
   7              		.global foo
   8              		.type   foo, %function
   8              	ginsn: SYM FUNC_BEGIN
   9              	foo:
   9              	ginsn: SYM foo
  10 \?\?\?\? 00000094 		bl      dump_bt
  10              	ginsn: CALL
  11 \?\?\?\? 02000014 		b       .L3
  11              	ginsn: JMP 
  12 \?\?\?\? 20021FD6 		br      x17
  12              	ginsn: JMP %r17, 
  13              	.L3:
  13              	ginsn: SYM .L3
  14 \?\?\?\? 60003FD6 		blr     x3
  14              	ginsn: CALL
  15 \?\?\?\? C0035FD6 		ret
  15              	ginsn: RET
  16              		.size   foo, .-foo
  16              	ginsn: SYM FUNC_END
