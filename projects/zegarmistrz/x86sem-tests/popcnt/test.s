 .text
.globl _start

_start:
  // POPCNT  
  movq $0x0, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x0, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x1, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x1, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x80, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x80, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x55, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x55, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xAA, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xAA, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x0F, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x0F, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xF7, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xF7, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFE, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFE, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x7F, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x7F, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x8000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x8000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x5500, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x5500, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xAA00, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xAA00, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x0F0F, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x0F0F, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xF7F7, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xF7F7, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFEFE, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFEFE, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x7FFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x7FFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFFFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFFFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x80000000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x80000000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x55000000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x55000000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xAA000000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xAA000000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x0F0F0F0F, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x0F0F0F0F, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xF7F7F7F7, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xF7F7F7F7, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFEFEFEFE, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFEFEFEFE, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x7FFFFFFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x7FFFFFFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFFFFFFFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFFFFFFFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x8000000000000000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x8000000000000000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x5500000000000000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x5500000000000000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xAA00000000000000, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xAA00000000000000, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0x7FFFFFFFFFFFFFFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  popcntq %rax, %rbx
  popcntq %rbx, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  popcntw %ax, %bx
  popcntw %bx, %ax
  popcntw %ax, %bx
  popcntw %bx, %ax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
