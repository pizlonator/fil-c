AARCH64 GAS  .*

   1              	# Testcase for a variety of ld st instructions.
   2              	# stg ops must generate the sp update arithmetic insn, when applicable
   3              	# due to writeback.
   4              		.text
   5              		.align  2
   6              		.global foo
   7              		.type   foo, %function
   7              	ginsn: SYM FUNC_BEGIN
   8              	foo:
   8              	ginsn: SYM foo
   9              	# ldstpair_indexed
  10 0000 FF7F8629 		stp     wzr, wzr, \[sp, 48\]!
  10              	ginsn: ADD %r31, 48, %r31
  11 0004 E00782A9 		stp     x0, x1, \[sp, 32\]!
  11              	ginsn: ADD %r31, 32, %r31
  11              	ginsn: STORE %r0, \[%r31\+0\]
  11              	ginsn: STORE %r1, \[%r31\+8\]
  12 0008 E827BC6D 		stp     d8, d9, \[sp, -64\]!
  12              	ginsn: ADD %r31, -64, %r31
  12              	ginsn: STORE %r72, \[%r31\+0\]
  12              	ginsn: STORE %r73, \[%r31\+8\]
  13 000c E827C46C 		ldp     d8, d9, \[sp\], 64
  13              	ginsn: LOAD \[%r31\+0\], %r72
  13              	ginsn: LOAD \[%r31\+8\], %r73
  13              	ginsn: ADD %r31, 64, %r31
  14              	# 32-bit FP regs
  15 0010 E51BB42D 		stp     s5, s6, \[sp, -96\]!
  15              	ginsn: ADD %r31, -96, %r31
  16 0014 E51BCC2C 		ldp     s5, s6, \[sp\], 96
  16              	ginsn: ADD %r31, 96, %r31
  17              	# 32-bit INT regs
  18 0018 E10BB029 		stp     w1, w2, \[sp, -128\]!
  18              	ginsn: ADD %r31, -128, %r31
  19 001c E10BD028 		ldp     w1, w2, \[sp\], 128
  19              	ginsn: ADD %r31, 128, %r31
  20              	# ldstpair_off
  21 0020 E00702AD 		stp     q0, q1, \[sp, 64\]
  21              	ginsn: STORE %r64, \[%r31\+64\]
  21              	ginsn: STORE %r65, \[%r31\+80\]
  22 0024 FF7F0629 		stp     wzr, wzr, \[sp, 48\]
  23 0028 1F7840AD 		ldp     q31, q30, \[x0\]
  23              	ginsn: LOAD \[%r0\+0\], %r95
  23              	ginsn: LOAD \[%r0\+16\], %r94
  24 002c FF4F01A9 	        stp     xzr, x19, \[sp, 16\]
  24              	ginsn: STORE %r19, \[%r31\+24\]
  25 0030 F37F01A9 	        stp     x19, xzr, \[sp, 16\]
  25              	ginsn: STORE %r19, \[%r31\+16\]
  26              	# ldst_imm9
  27 0034 E78F4F38 		ldrb    w7, \[sp, 248\]!
  27              	ginsn: ADD %r31, 248, %r31
  28 0038 FD0FC33C 		ldr     q29, \[sp, 48\]!
  28              	ginsn: ADD %r31, 48, %r31
  28              	ginsn: LOAD \[%r31\+0\], %r93
  29 003c FF0F42B8 		ldr     wzr, \[sp, 32\]!
  29              	ginsn: ADD %r31, 32, %r31
  30 0040 E30742F8 		ldr     x3, \[sp\], 32
  30              	ginsn: LOAD \[%r31\+0\], %r3
AARCH64 GAS  .*


  30              	ginsn: ADD %r31, 32, %r31
  31              	# 32-bit ldr
  32 0044 E10744BC 		ldr     s1, \[sp\], 64
  32              	ginsn: ADD %r31, 64, %r31
  33              	# ldst_pos
  34 0048 FF3340B9 		ldr     wzr, \[sp, 48\]
  35 004c FD1300F9 		str     x29, \[sp, 32\]
  35              	ginsn: STORE %r29, \[%r31\+32\]
  36 0050 FD1340F9 		ldr     x29, \[sp, 32\]
  36              	ginsn: LOAD \[%r31\+32\], %r29
  37              	# store tag
  38 0054 FF2F20D9 		stg     sp, \[sp, 32\]!
  38              	ginsn: ADD %r31, 32, %r31
  39              	# store tag pair
  40 0058 FBF38169 		stgp    x27, x28, \[sp, 48\]!
  40              	ginsn: ADD %r31, 48, %r31
  41              	# ldpsw / ldrsw
  42 005c F353E069 		ldpsw   x19, x20, \[sp, -256\]!
  42              	ginsn: ADD %r31, -256, %r31
  43 0060 F50F98B8 		ldrsw   x21, \[sp, -128\]!
  43              	ginsn: ADD %r31, -128, %r31
  44              	# ldrsb / ldrsh
  45 0064 F70F9938 		ldrsb   x23, \[sp, -112\]!
  45              	ginsn: ADD %r31, -112, %r31
  46 0068 F88F9978 		ldrsh   x24, \[sp, -104\]!
  46              	ginsn: ADD %r31, -104, %r31
  47              	# ldrb / ldrh / strb / strh
  48 006c F90F5A38 		ldrb    w25, \[sp, -96\]!
  48              	ginsn: ADD %r31, -96, %r31
  49 0070 FA8F5A78 		ldrh    w26, \[sp, -88\]!
  49              	ginsn: ADD %r31, -88, %r31
  50 0074 F90F1A38 		strb    w25, \[sp, -96\]!
  50              	ginsn: ADD %r31, -96, %r31
  51 0078 FA8F1A78 		strh    w26, \[sp, -88\]!
  51              	ginsn: ADD %r31, -88, %r31
  52 007c C0035FD6 		ret
  52              	ginsn: RET
  53              		.size   foo, .-foo
  53              	ginsn: SYM FUNC_END
