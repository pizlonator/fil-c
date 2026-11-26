 .text
.globl _start

_start:
  // SCASB  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  scasb
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  scasb
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  scasb
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  scasb


  // SCASW  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $0x4141, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $0xFF00, %rax
  scasw
  
  leaq -8(%rsp), %rdi
  movq $0xF00F, %rax
  scasw

  // SCASL  
  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0x4141, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFF00, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xF00F, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0x41414141, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF0000, %rax
  scasl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFFFFFFFF, %rax
  scasl

  // SCASQ  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0x4141, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFF00, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xF00F, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0x41414141, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF0000, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFFFFFF, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0x4141414141414141, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF0000FFFF0000, %rax
  scasq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFFFFFFFFFFFFFF, %rax
  scasq


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
