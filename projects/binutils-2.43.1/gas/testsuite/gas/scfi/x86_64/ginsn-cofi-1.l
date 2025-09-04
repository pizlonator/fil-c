.*: Assembler messages:
.*:20: Error: untraceable control flow for func 'foo'
GAS LISTING .*


   1              	# Testcase with a variety of "change of flow instructions"
   2              	#
   3              	# This test does not have much going on wrt synthesis of CFI;
   4              	# it just aims to ensure x8_64 -> ginsn decoding behaves
   5              	# gracefully for these "change of flow instructions"
   6              		.text
   7              		.globl  foo
   8              		.type   foo, @function
   8              	ginsn: SYM FUNC_BEGIN
   9              	foo:
   9              	ginsn: SYM foo
  10 \?\?\?\? 4801D0   		addq    %rdx, %rax
  10              	ginsn: ADD %r1, %r0, %r0
  11 \?\?\?\? E200     		loop    foo
  11              	ginsn: JCC 
  12 \?\?\?\? 3EFFE0   		notrack jmp     \*%rax
  12              	ginsn: JMP %r0, 
  13 \?\?\?\? 41FFD0   		call    \*%r8
  13              	ginsn: CALL
  14 \?\?\?\? 67E305   		jecxz   .L179
  14              	ginsn: JCC 
  15 \?\?\?\? FF6730   		jmp     \*48\(%rdi\)
  15              	ginsn: JMP %r5, 
  16 \?\?\?\? 7000     		jo      .L179
  16              	ginsn: JCC 
  17              	.L179:
  17              	ginsn: SYM .L179
  18 \?\?\?\? C3       		ret
  18              	ginsn: RET
  19              	.LFE0:
  19              	ginsn: SYM .LFE0
  20              		.size   foo, .-foo
  20              	ginsn: SYM FUNC_END
