.*: Assembler messages:
.*: Warning: scale factor of 4 without an index register
GAS LISTING .*


   1              	## Testcase with a variety of lea.
   2              		.text
   3              		.globl  foo
   4              		.type   foo, @function
   4              	ginsn: SYM FUNC_BEGIN
   5              	foo:
   5              	ginsn: SYM foo
   6 0000 488D2C25 		lea  symbol, %rbp
   6      00000000 
   6              	ginsn: OTH 0, 0, %r6
   7 0008 488D2C25 		lea  0x9090, %rbp
   7      90900000 
   7              	ginsn: OTH 0, 0, %r6
   8 0010 488D05FE 		lea  -0x2\(%rip\), %rax
   8      FFFFFF
   8              	ginsn: ADD %r4, -2, %r0
   9 0017 678D6C18 		lea  -0x1\(%eax,%ebx\), %ebp
   9      FF
   9              	ginsn: OTH 0, 0, %r6
  10 001c 678D6C58 		lea  0x55\(%eax,%ebx,2\), %ebp
  10      55
  10              	ginsn: OTH 0, 0, %r6
  11 0021 678D0C1D 		lea  -0x3\(,%ebx,1\), %ecx
  11      FDFFFFFF 
  11              	ginsn: ADD %r3, -3, %r2
  12 0029 678D0C1D 		lea  -0x3\(,%ebx,\), %ecx
  12      FDFFFFFF 
  12              	ginsn: ADD %r3, -3, %r2
  13 0031 678D0C5D 		lea  -0x3\(,%ebx,2\), %ecx
  13      FDFFFFFF 
  14              	
  15              		.allow_index_reg
  16 0039 488D2C20 		lea  \(%rax,%riz\),%rbp
  16              	ginsn: MOV %r0, %r6
  17 003d 488D28   		lea  \(%rax,4\),%rbp
\*\*\*\*  Warning: scale factor of 4 without an index register
  17              	ginsn: MOV %r0, %r6
  18 0040 488D2CA0 		lea  \(%rax,%riz,4\),%rbp
  18              	ginsn: MOV %r0, %r6
  19 0044 488D2C25 		lea  sym\(,%riz\), %rbp
  19      00000000 
  19              	ginsn: OTH 0, 0, %r6
  20 004c 488D2C25 		lea  \(,%riz\), %rbp
  20      00000000 
  20              	ginsn: OTH 0, 0, %r6
  21              	.LFE0:
  21              	ginsn: SYM .LFE0
  22              		.size   foo, .-foo
  22              	ginsn: SYM FUNC_END
