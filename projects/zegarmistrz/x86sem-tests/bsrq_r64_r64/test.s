 .text
.globl _start

_start:
  // BSRQ  
  movq $0, %rcx
  bsrq %rcx, %rax
  
  movq $1, %rcx
  bsrq %rcx, %rax
  
  movq $0x80, %rcx
  bsrq %rcx, %rax
  
  movq $0x55, %rcx
  bsrq %rcx, %rax
  
  movq $0xAA, %rcx
  bsrq %rcx, %rax
  
  movq $0x0F, %rcx
  bsrq %rcx, %rax
  
  movq $0xF7, %rcx
  bsrq %rcx, %rax
  
  movq $0xFE, %rcx
  bsrq %rcx, %rax
  
  movq $0x8000, %rcx
  bsrq %rcx, %rax
  
  movq $0x5500, %rcx
  bsrq %rcx, %rax
  
  movq $0xAA00, %rcx
  bsrq %rcx, %rax
  
  movq $0x0F0F, %rcx
  bsrq %rcx, %rax
  
  movq $0xF7F7, %rcx
  bsrq %rcx, %rax
  
  movq $0xFEFE, %rcx
  bsrq %rcx, %rax
  
  movq $0x80000000, %rcx
  bsrq %rcx, %rax
  
  movq $0x55000000, %rcx
  bsrq %rcx, %rax
  
  movq $0xAA000000, %rcx
  bsrq %rcx, %rax
  
  movq $0x0F0F0F0F, %rcx
  bsrq %rcx, %rax
  
  movq $0xF7F7F7F7, %rcx
  bsrq %rcx, %rax
  
  movq $0xFEFEFEFE, %rcx
  bsrq %rcx, %rax
  
  movq $0x8000000000000000, %rcx
  bsrq %rcx, %rax
  
  movq $0x5500000000000000, %rcx
  bsrq %rcx, %rax
  
  movq $0xAA00000000000000, %rcx
  bsrq %rcx, %rax
  
  movq $0x0F0F0F0F0F0F0F0F, %rcx
  bsrq %rcx, %rax
  
  movq $0xF7F7F7F7F7F7F7F7, %rcx
  bsrq %rcx, %rax
  
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  bsrq %rcx, %rax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
