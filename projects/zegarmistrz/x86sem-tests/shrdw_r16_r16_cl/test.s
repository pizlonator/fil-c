 .text
.globl _start

_start:
  // SHRDW  
  movq $0, %rax
  movq $0, %rbx
  movq $0, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $1, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  movq $15, %rcx
  shrdw %cl, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  movq $15, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $15, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $15, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $15, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $15, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0, %rbx
  movq $16, %rcx
  shrdw %cl, %ax, %bx
  
  movq $1, %rax
  movq $1, %rbx
  movq $16, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $16, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $16, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $16, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $16, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  movq $31, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $32, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  movq $63, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  movq $64, %rcx
  shrdw %cl, %ax, %bx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
