GAS LISTING .*


   1              	# Testcase for DWARF regnum ginsn API
   2              		.text
   3              		.globl  foo
   4              		.type   foo, @function
   4              	ginsn: SYM FUNC_BEGIN
   5              	foo:
   5              	ginsn: SYM foo
   6 0000 55       		push   %rbp
   6              	ginsn: SUB %r7, 8, %r7
   6              	ginsn: STORE %r6, \[%r7\+0\]
   7 0001 4889E5   		mov    %rsp, %rbp
   7              	ginsn: MOV %r7, %r6
   8              	
   9 0004 8A40FE   		mov    -0x2\(%rax\), %al
   9              	ginsn: MOV \[%r0\+-2\], %r0
  10 0007 8A58FE   		mov    -0x2\(%rax\), %bl
  10              	ginsn: MOV \[%r0\+-2\], %r3
  11 000a 8A50FE   		mov    -0x2\(%rax\), %dl
  11              	ginsn: MOV \[%r0\+-2\], %r1
  12 000d 8A60FE   		mov    -0x2\(%rax\), %ah
  12              	ginsn: MOV \[%r0\+-2\], %r0
  13 0010 8A68FE   		mov    -0x2\(%rax\), %ch
  13              	ginsn: MOV \[%r0\+-2\], %r2
  14 0013 8A70FE   		mov    -0x2\(%rax\), %dh
  14              	ginsn: MOV \[%r0\+-2\], %r1
  15 0016 8A78FE   		mov    -0x2\(%rax\), %bh
  15              	ginsn: MOV \[%r0\+-2\], %r3
  16              	
  17 0019 408A40FE 		mov    -0x2\(%rax\), %axl
  17              	ginsn: MOV \[%r0\+-2\], %r0
  18 001d 408A60FE 		mov    -0x2\(%rax\), %spl
  18              	ginsn: MOV \[%r0\+-2\], %r7
  19              	
  20 0021 668B40FE 		mov    -0x2\(%rax\), %ax
  20              	ginsn: MOV \[%r0\+-2\], %r0
  21 0025 66448B40 		mov    -0x2\(%rax\), %r8w
  21      FE
  21              	ginsn: MOV \[%r0\+-2\], %r8
  22 002a 668B60FE 		mov    -0x2\(%rax\), %sp
  22              	ginsn: MOV \[%r0\+-2\], %r7
  23              	
  24 002e 4080C408 		add    \$8, %spl
  24              	ginsn: OTH 0, 0, %r7
  25              	
  26 0032 8B40FE   		mov    -0x2\(%rax\), %eax
  26              	ginsn: MOV \[%r0\+-2\], %r0
  27 0035 448B40FE 		mov    -0x2\(%rax\), %r8d
  27              	ginsn: MOV \[%r0\+-2\], %r8
  28              	
  29 0039 81C40040 		add    \$16384, %esp
  29      0000
  29              	ginsn: OTH 0, 0, %r7
  30              	
  31 003f 4883C508 		add    \$8, %rbp
  31              	ginsn: ADD %r6, 8, %r6
  32              	
  33 0043 488D05FE 		lea    -0x2\(%rip\), %rax
GAS LISTING .*


  33      FFFFFF
  33              	ginsn: ADD %r4, -2, %r0
  34 004a 67488905 		mov    %rax, 0x2\(%eip\)
  34      02000000 
  34              	ginsn: MOV %r0, \[%r4\+2\]
  35 0052 67488B05 		mov    -0x2\(%eip\), %rax
  35      FEFFFFFF 
  35              	ginsn: MOV \[%r4\+-2\], %r0
  36              	
  37 005a 4889EC   		mov    %rbp, %rsp
  37              	ginsn: MOV %r6, %r7
  38 005d 5D       		pop    %rbp
  38              	ginsn: LOAD \[%r7\+0\], %r6
  38              	ginsn: ADD %r7, 8, %r7
  39 005e C3       		ret
  39              	ginsn: RET
  40              	.LFE0:
  40              	ginsn: SYM .LFE0
  41              		.size   foo, .-foo
  41              	ginsn: SYM FUNC_END

#pass
