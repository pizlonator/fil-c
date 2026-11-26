 .text
.globl _start

_start:
  // BSFQ  
  movq $0, %rcx
  bsfw %cx, %ax
  
  movq $1, %rcx
  bsfw %cx, %ax
  
  movq $0x80, %rcx
  bsfw %cx, %ax
  
  movq $0x55, %rcx
  bsfw %cx, %ax
  
  movq $0xAA, %rcx
  bsfw %cx, %ax
  
  movq $0x0F, %rcx
  bsfw %cx, %ax
  
  movq $0xF7, %rcx
  bsfw %cx, %ax
  
  movq $0xFE, %rcx
  bsfw %cx, %ax
  
  movq $0x8000, %rcx
  bsfw %cx, %ax
  
  movq $0x5500, %rcx
  bsfw %cx, %ax
  
  movq $0xAA00, %rcx
  bsfw %cx, %ax
  
  movq $0x0F0F, %rcx
  bsfw %cx, %ax
  
  movq $0xF7F7, %rcx
  bsfw %cx, %ax
  
  movq $0xFEFE, %rcx
  bsfw %cx, %ax
  
  movq $0x80000000, %rcx
  bsfw %cx, %ax
  
  movq $0x55000000, %rcx
  bsfw %cx, %ax
  
  movq $0xAA000000, %rcx
  bsfw %cx, %ax
  
  movq $0x0F0F0F0F, %rcx
  bsfw %cx, %ax
  
  movq $0xF7F7F7F7, %rcx
  bsfw %cx, %ax
  
  movq $0xFEFEFEFE, %rcx
  bsfw %cx, %ax
  
  movq $0x8000000000000000, %rcx
  bsfw %cx, %ax
  
  movq $0x5500000000000000, %rcx
  bsfw %cx, %ax
  
  movq $0xAA00000000000000, %rcx
  bsfw %cx, %ax
  
  movq $0x0F0F0F0F0F0F0F0F, %rcx
  bsfw %cx, %ax
  
  movq $0xF7F7F7F7F7F7F7F7, %rcx
  bsfw %cx, %ax
  
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  bsfw %cx, %ax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
