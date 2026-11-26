 .text
.globl _start

_start:
  // SCASB  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  stosb
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  stosb
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  stosb
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  stosb


  // SCASW  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $0x4141, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $0xFF00, %rax
  stosw
  
  leaq -8(%rsp), %rdi
  movq $0xF00F, %rax
  stosw

  // SCASL  
  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0x4141, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFF00, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xF00F, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0x41414141, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF0000, %rax
  stosl
  
  
  leaq -8(%rsp), %rdi
  movq $0xFFFFFFFF, %rax
  stosl

  // SCASQ  
  leaq -8(%rsp), %rdi
  movq $0, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $1, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xAA, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFF, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0x4141, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFF00, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xF00F, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0x41414141, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF0000, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFFFFFF, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0x4141414141414141, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFF0000FFFF0000, %rax
  stosq
  
  leaq -8(%rsp), %rdi
  movq $0xFFFFFFFFFFFFFFFF, %rax
  stosq


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
