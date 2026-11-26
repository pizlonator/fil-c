 .text
.globl _start

_start:
  // BSRL  
  movq $0, %rcx
  bsrl %ecx, %eax
  
  movq $1, %rcx
  bsrl %ecx, %eax
  
  movq $0x80, %rcx
  bsrl %ecx, %eax
  
  movq $0x55, %rcx
  bsrl %ecx, %eax
  
  movq $0xAA, %rcx
  bsrl %ecx, %eax
  
  movq $0x0F, %rcx
  bsrl %ecx, %eax
  
  movq $0xF7, %rcx
  bsrl %ecx, %eax
  
  movq $0xFE, %rcx
  bsrl %ecx, %eax
  
  movq $0x8000, %rcx
  bsrl %ecx, %eax
  
  movq $0x5500, %rcx
  bsrl %ecx, %eax
  
  movq $0xAA00, %rcx
  bsrl %ecx, %eax
  
  movq $0x0F0F, %rcx
  bsrl %ecx, %eax
  
  movq $0xF7F7, %rcx
  bsrl %ecx, %eax
  
  movq $0xFEFE, %rcx
  bsrl %ecx, %eax
  
  movq $0x80000000, %rcx
  bsrl %ecx, %eax
  
  movq $0x55000000, %rcx
  bsrl %ecx, %eax
  
  movq $0xAA000000, %rcx
  bsrl %ecx, %eax
  
  movq $0x0F0F0F0F, %rcx
  bsrl %ecx, %eax
  
  movq $0xF7F7F7F7, %rcx
  bsrl %ecx, %eax
  
  movq $0xFEFEFEFE, %rcx
  bsrl %ecx, %eax
  
  movq $0x8000000000000000, %rcx
  bsrl %ecx, %eax
  
  movq $0x5500000000000000, %rcx
  bsrl %ecx, %eax
  
  movq $0xAA00000000000000, %rcx
  bsrl %ecx, %eax
  
  movq $0x0F0F0F0F0F0F0F0F, %rcx
  bsrl %ecx, %eax
  
  movq $0xF7F7F7F7F7F7F7F7, %rcx
  bsrl %ecx, %eax
  
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  bsrl %ecx, %eax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
