 .text
.globl _start

_start:
  // BSRW  
  movq $0, %rcx
  bsrw %cx, %ax
  
  movq $1, %rcx
  bsrw %cx, %ax
  
  movq $0x80, %rcx
  bsrw %cx, %ax
  
  movq $0x55, %rcx
  bsrw %cx, %ax
  
  movq $0xAA, %rcx
  bsrw %cx, %ax
  
  movq $0x0F, %rcx
  bsrw %cx, %ax
  
  movq $0xF7, %rcx
  bsrw %cx, %ax
  
  movq $0xFE, %rcx
  bsrw %cx, %ax
  
  movq $0x8000, %rcx
  bsrw %cx, %ax
  
  movq $0x5500, %rcx
  bsrw %cx, %ax
  
  movq $0xAA00, %rcx
  bsrw %cx, %ax
  
  movq $0x0F0F, %rcx
  bsrw %cx, %ax
  
  movq $0xF7F7, %rcx
  bsrw %cx, %ax
  
  movq $0xFEFE, %rcx
  bsrw %cx, %ax
  
  movq $0x80000000, %rcx
  bsrw %cx, %ax
  
  movq $0x55000000, %rcx
  bsrw %cx, %ax
  
  movq $0xAA000000, %rcx
  bsrw %cx, %ax
  
  movq $0x0F0F0F0F, %rcx
  bsrw %cx, %ax
  
  movq $0xF7F7F7F7, %rcx
  bsrw %cx, %ax
  
  movq $0xFEFEFEFE, %rcx
  bsrw %cx, %ax
  
  movq $0x8000000000000000, %rcx
  bsrw %cx, %ax
  
  movq $0x5500000000000000, %rcx
  bsrw %cx, %ax
  
  movq $0xAA00000000000000, %rcx
  bsrw %cx, %ax
  
  movq $0x0F0F0F0F0F0F0F0F, %rcx
  bsrw %cx, %ax
  
  movq $0xF7F7F7F7F7F7F7F7, %rcx
  bsrw %cx, %ax
  
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  bsrw %cx, %ax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
