 .text
.globl _start

_start:
  // BSFQ  
  movq $0, %rcx
  bsfl %ecx, %eax
  
  movq $1, %rcx
  bsfl %ecx, %eax
  
  movq $0x80, %rcx
  bsfl %ecx, %eax
  
  movq $0x55, %rcx
  bsfl %ecx, %eax
  
  movq $0xAA, %rcx
  bsfl %ecx, %eax
  
  movq $0x0F, %rcx
  bsfl %ecx, %eax
  
  movq $0xF7, %rcx
  bsfl %ecx, %eax
  
  movq $0xFE, %rcx
  bsfl %ecx, %eax
  
  movq $0x8000, %rcx
  bsfl %ecx, %eax
  
  movq $0x5500, %rcx
  bsfl %ecx, %eax
  
  movq $0xAA00, %rcx
  bsfl %ecx, %eax
  
  movq $0x0F0F, %rcx
  bsfl %ecx, %eax
  
  movq $0xF7F7, %rcx
  bsfl %ecx, %eax
  
  movq $0xFEFE, %rcx
  bsfl %ecx, %eax
  
  movq $0x80000000, %rcx
  bsfl %ecx, %eax
  
  movq $0x55000000, %rcx
  bsfl %ecx, %eax
  
  movq $0xAA000000, %rcx
  bsfl %ecx, %eax
  
  movq $0x0F0F0F0F, %rcx
  bsfl %ecx, %eax
  
  movq $0xF7F7F7F7, %rcx
  bsfl %ecx, %eax
  
  movq $0xFEFEFEFE, %rcx
  bsfl %ecx, %eax
  
  movq $0x8000000000000000, %rcx
  bsfl %ecx, %eax
  
  movq $0x5500000000000000, %rcx
  bsfl %ecx, %eax
  
  movq $0xAA00000000000000, %rcx
  bsfl %ecx, %eax
  
  movq $0x0F0F0F0F0F0F0F0F, %rcx
  bsfl %ecx, %eax
  
  movq $0xF7F7F7F7F7F7F7F7, %rcx
  bsfl %ecx, %eax
  
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  bsfl %ecx, %eax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
