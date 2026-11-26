 .text
.globl _start

_start:
  // SHLDW  
  movq $0, %rax
  movq $0, %rbx
  shldw $0, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  shldw $1, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  shldw $1, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $1, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $1, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $1, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $1, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $1, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $1, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldw $1, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  shldw $15, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  shldw $15, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $15, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $15, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $15, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $15, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  shldw $16, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  shldw $16, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $16, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $16, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $16, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $16, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldw $31, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $32, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $32, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $32, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $32, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $32, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $32, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldw $32, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldw $63, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldw $64, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldw $64, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldw $64, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldw $64, %ax, %bx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
