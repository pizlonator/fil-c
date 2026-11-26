 .text
.globl _start

_start:
  // SHLDQ  
  movq $0, %rax
  movq $0, %rbx
  shldq $0, %rax, %rbx
  
  movq $0, %rax
  movq $0, %rbx
  shldq $1, %rax, %rbx
  
  movq $1, %rax
  movq $1, %rbx
  shldq $1, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $1, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $1, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $1, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $1, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $1, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $1, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldq $1, %rax, %rbx
  
  movq $0, %rax
  movq $0, %rbx
  shldq $15, %rax, %rbx
  
  movq $1, %rax
  movq $1, %rbx
  shldq $15, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $15, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $15, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $15, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $15, %rax, %rbx
  
  movq $0, %rax
  movq $0, %rbx
  shldq $16, %rax, %rbx
  
  movq $1, %rax
  movq $1, %rbx
  shldq $16, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $16, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $16, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $16, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $16, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldq $31, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $32, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $32, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $32, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $32, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $32, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $32, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldq $32, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldq $63, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldq $64, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldq $64, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldq $64, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldq $64, %rax, %rbx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
