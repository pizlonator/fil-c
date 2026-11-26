 .text
.globl _start

_start:
  // COMISS  
  movq  $0,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x41,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x51,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x7F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x4141,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x5151,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x5555,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAAAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xEFEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x7FFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x41414141,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x51515151,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x55555555,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAAFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAAAAAAAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xEFEFEFEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x7FFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFFFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x4141414141414141,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x5115151515115151,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x5555555555555555,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xAAAAAAAAAAAAAAAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xEFEFEFEFEFEFEFEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0x7FFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  
  vbroadcastsd %xmm1, %ymm4
  vbroadcastsd %xmm2, %ymm5
  vbroadcastsd %xmm3, %ymm6
  
  vbroadcastss %xmm1, %ymm7
  vbroadcastss %xmm2, %ymm8
  vbroadcastss %xmm3, %ymm9
  
  divsd %xmm4, %xmm5
  divsd %xmm6, %xmm7
  divsd %xmm8, %xmm9
  
  divss %xmm5, %xmm4
  divss %xmm7, %xmm6
  divss %xmm9, %xmm8
  
  divpd %xmm4, %xmm5
  divpd %xmm6, %xmm7
  divpd %xmm8, %xmm9
  
  divps %xmm5, %xmm4
  divps %xmm7, %xmm6
  divps %xmm9, %xmm8


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
