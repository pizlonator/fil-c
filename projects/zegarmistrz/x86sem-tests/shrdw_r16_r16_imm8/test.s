 .text
.globl _start

_start:
  // SHRDW  
  movq $0, %rax
  movq $0, %rbx
  shrdw $0, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  shrdw $1, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  shrdw $1, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $1, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $1, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $1, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $1, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $1, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $1, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $1, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  shrdw $15, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  shrdw $15, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $15, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $15, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $15, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $15, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  shrdw $16, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  shrdw $16, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $16, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $16, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $16, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $16, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $31, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $32, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $32, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $32, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $32, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $32, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $32, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $32, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $63, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdw $64, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $64, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdw $64, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdw $64, %ax, %bx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
