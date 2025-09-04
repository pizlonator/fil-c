GAS LISTING .*


   1              	## Testcase with a variety of add.
   2              	## Some add insns valid in 64-bit mode may not be processed for SCFI.
   3              		.text
   4              		.globl foo
   5              		.type foo, @function
   5              	ginsn: SYM FUNC_BEGIN
   6              	foo:
   6              	ginsn: SYM foo
   7 0000 54       		push %rsp
   7              	ginsn: SUB %r7, 8, %r7
   7              	ginsn: STORE %r7, \[%r7\+0\]
   8 0001 01C5     		add %eax, %ebp
   8              	ginsn: ADD %r0, %r6, %r6
   9 0003 4889E5   		movq %rsp, %rbp
   9              	ginsn: MOV %r7, %r6
  10              	
  11 0006 48010425 		addq %rax, symbol
  11      00000000 
  12 000e 03042500 		add symbol, %eax
  12      000000
  13 0015 670320   		add \(%eax\), %esp
  13              	ginsn: ADD \[%r0\+0\], %r7, %r7
  14 0018 67012405 		add %esp, \(,%eax\)
  14      00000000 
  15 0020 67032405 		add \(,%eax\), %esp
  15      00000000 
  15              	ginsn: ADD \[%r0\+0\], %r7, %r7
  16              	
  17 0028 4801C3   		addq %rax, %rbx
  17              	ginsn: ADD %r0, %r3, %r3
  18 002b 01C3     		add %eax, %ebx
  18              	ginsn: ADD %r0, %r3, %r3
  19 002d 83C408   		add \$8, %esp
  19              	ginsn: OTH 0, 0, %r7
  20              	
  21 0030 4883D408 		adc \$8, %rsp
  21              	ginsn: OTH 0, 0, %r7
  22              	
  23 0034 488345F0 		addq \$1, -16\(%rbp\)
  23      01
  24              	
  25 0039 4803D8   		{load} addq %rax, %rbx
  25              	ginsn: ADD %r0, %r3, %r3
  26              	
  27 003c C3       		ret
  27              	ginsn: RET
  28              	.LFE0:
  28              	ginsn: SYM .LFE0
  29              		.size foo, .-foo
  29              	ginsn: SYM FUNC_END
