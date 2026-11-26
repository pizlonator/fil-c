 .text
.globl _start

_start:
  // TEST2  
  
  movq $0x0, %rax
  movq $0x0, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x1, %rax
  movq $0x1, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x80, %rax
  movq $0x80, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x55, %rax
  movq $0x55, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xAA, %rax
  movq $0xAA, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x0F, %rax
  movq $0x0F, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xF7, %rax
  movq $0xF7, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFE, %rax
  movq $0xFE, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x7F, %rax
  movq $0x7F, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x8000, %rax
  movq $0x8000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x5500, %rax
  movq $0x5500, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xAA00, %rax
  movq $0xAA00, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x0F0F, %rax
  movq $0x0F0F, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xF7F7, %rax
  movq $0xF7F7, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFEFE, %rax
  movq $0xFEFE, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x7FFF, %rax
  movq $0x7FFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x80000000, %rax
  movq $0x80000000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x55000000, %rax
  movq $0x55000000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xAA000000, %rax
  movq $0xAA000000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x0F0F0F0F, %rax
  movq $0x0F0F0F0F, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xF7F7F7F7, %rax
  movq $0xF7F7F7F7, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFEFEFEFE, %rax
  movq $0xFEFEFEFE, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x7FFFFFFF, %rax
  movq $0x7FFFFFFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x8000000000000000, %rax
  movq $0x8000000000000000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x5500000000000000, %rax
  movq $0x5500000000000000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xAA00000000000000, %rax
  movq $0xAA00000000000000, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $0x7FFFFFFFFFFFFFFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx
  
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  rolq $0x1, %rax
  rolw $0x1, %bx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
