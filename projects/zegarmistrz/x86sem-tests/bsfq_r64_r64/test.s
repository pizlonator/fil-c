 .text
.globl _start

_start:
  // BSFQ  
  movq $0, %rcx
  bsfq %rcx, %rax
  
  movq $1, %rcx
  bsfq %rcx, %rax
  
  movq $0x80, %rcx
  bsfq %rcx, %rax
  
  movq $0x55, %rcx
  bsfq %rcx, %rax
  
  movq $0xAA, %rcx
  bsfq %rcx, %rax
  
  movq $0x0F, %rcx
  bsfq %rcx, %rax
  
  movq $0xF7, %rcx
  bsfq %rcx, %rax
  
  movq $0xFE, %rcx
  bsfq %rcx, %rax
  
  movq $0x8000, %rcx
  bsfq %rcx, %rax
  
  movq $0x5500, %rcx
  bsfq %rcx, %rax
  
  movq $0xAA00, %rcx
  bsfq %rcx, %rax
  
  movq $0x0F0F, %rcx
  bsfq %rcx, %rax
  
  movq $0xF7F7, %rcx
  bsfq %rcx, %rax
  
  movq $0xFEFE, %rcx
  bsfq %rcx, %rax
  
  movq $0x80000000, %rcx
  bsfq %rcx, %rax
  
  movq $0x55000000, %rcx
  bsfq %rcx, %rax
  
  movq $0xAA000000, %rcx
  bsfq %rcx, %rax
  
  movq $0x0F0F0F0F, %rcx
  bsfq %rcx, %rax
  
  movq $0xF7F7F7F7, %rcx
  bsfq %rcx, %rax
  
  movq $0xFEFEFEFE, %rcx
  bsfq %rcx, %rax
  
  movq $0x8000000000000000, %rcx
  bsfq %rcx, %rax
  
  movq $0x5500000000000000, %rcx
  bsfq %rcx, %rax
  
  movq $0xAA00000000000000, %rcx
  bsfq %rcx, %rax
  
  movq $0x0F0F0F0F0F0F0F0F, %rcx
  bsfq %rcx, %rax
  
  movq $0xF7F7F7F7F7F7F7F7, %rcx
  bsfq %rcx, %rax
  
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  bsfq %rcx, %rax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
