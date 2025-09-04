.*: Assembler messages:
.*:13: Error: SCFI: unsupported stack manipulation pattern
.*:33: Error: SCFI: forward pass failed for func 'foo'

AARCH64 GAS  .*


   1              	# Testcase for a variety of misc instructions.
   2              	# Ensure graceful handling, irrespective of ginsn generation.
   3              	# - ldrsw, ldpsw ops need a mention: they are 32-bit loads from memory
   4              	#   and cannot be used for reg restore.  For CFI purposes, the width has to be
   5              	#   8 bytes or more. Expect no memory ginsn ever as ginsn do not track width
   6              	#   of memory accesses yet.
   7              	symbol:
   8 \?\?\?\? 7700     		.string "w"
   9              	
  10              		.type   foo, %function
  10              	ginsn: SYM FUNC_BEGIN
  11              	foo:
  11              	ginsn: SYM foo
  12 \?\?\?\? 00000000 		adrp    x0, symbol
  12      0090
  13 \?\?\?\? 1F000091 		add     sp, x0, :lo12:symbol
  13              	ginsn: OTH 0, 0, %r31
  14 \?\?\?\? 2000A0F2 		movk    x0, 0x1, lsl 16
  15 \?\?\?\? E10380F9 		prfm    PLDL1STRM, \[sp\]
  16 \?\?\?\? E013DF9A 		irg     x0, sp
  17 \?\?\?\? 000820D9 		stg     x0, \[x0\]
  18 \?\?\?\? F5DB3C69 		stgp    x21, x22, \[sp, -112\]
  19 \?\?\?\? E03FBF91 		addg    x0, sp, #0x3f0, #0xf
  20 \?\?\?\? 1F3CAA91 		addg    sp, x0, #0x2a0, #0xf
  20              	ginsn: OTH 0, 0, %r31
  21 \?\?\?\? F4577069 		ldpsw   x20, x21, \[sp, -128\]
  22 \?\?\?\? F3534069 	        ldpsw   x19, x20, \[sp\]
  23 \?\?\?\? 9B0080B9 	        ldrsw   x27, \[x4\]
  24 \?\?\?\? F4038039 		ldrsb   x20, \[sp\]
  25 \?\?\?\? F4038079 		ldrsh   x20, \[sp\]
  26              	# Test a few insn with wzr / xzr
  27 \?\?\?\? 3F0480D2 	        mov     xzr, 33
  28 \?\?\?\? E0031FAA 	        mov     x0, xzr
  29 \?\?\?\? 1F00018B 	        add     xzr, x0, x1
  30 \?\?\?\? 3F603F8B 	        add     sp, x1, xzr
  30              	ginsn: ADD %r1, %r31, %r31
  31 \?\?\?\? FF031F8B 		add     xzr, xzr, xzr
  32 \?\?\?\? C0035FD6 		ret
  32              	ginsn: RET
  33              		.size   foo,.-foo
  33              	ginsn: SYM FUNC_END
