 .text
.globl _start

_start:
  // XCHG  
  movq  $0,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $1,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x41,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x51,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x80,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x55,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xF7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x7F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x4141,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x5151,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x8000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x5500,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x5555,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAA00,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAAAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x0F0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xF7F7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFEFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xEFEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x7FFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x41414141,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x51515151,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x55000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x55555555,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAAFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAAAAAAAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x0F0F0F0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xF7F7F7F7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFEFEFEFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xEFEFEFEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x7FFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFFFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x4141414141414141,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x5115151515115151,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x8000000000000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x5500000000000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x5555555555555555,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAA00000000000000,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xAAAAAAAAAAAAAAAA,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x0F0F0F0F0F0F0F0F,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xF7F7F7F7F7F7F7F7,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xEFEFEFEFEFEFEFEF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFEFEFEFEFEFEFEFE,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0x7FFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  
  
  movq  $0xFFFFFFFFFFFFFFFF,  %rax
  movq  %rax, %xmm0
  
  movddup %xmm0, %xmm1
  movsldup %xmm0, %xmm2
  movshdup %xmm0, %xmm3
  vbroadcastsd %xmm0, %ymm4
  vbroadcastss %xmm0, %ymm5
  
  vmovmskpd %xmm1, %ebx
  vmovmskpd %xmm2, %ecx
  vmovmskpd %xmm3, %edx
  
  vmovmskpd %xmm1, %rbx
  vmovmskpd %xmm2, %rcx
  vmovmskpd %xmm3, %rdx
  
  movmskpd %xmm1, %rbx
  movmskpd %xmm2, %rcx
  movmskpd %xmm3, %rdx
  
  vmovmskpd %ymm4, %rdi
  vmovmskpd %ymm5, %rsi
  


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
