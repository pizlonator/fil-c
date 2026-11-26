 .text
.globl _start

_start:
  // SHRDQ  
  movq $0, %rax
  movq $0, %rbx
  shrdq $0, %rax, %rbx
  
  movq $0, %rax
  movq $0, %rbx
  shrdq $1, %rax, %rbx
  
  movq $1, %rax
  movq $1, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $1, %rax, %rbx
  
  movq $0, %rax
  movq $0, %rbx
  shrdq $15, %rax, %rbx
  
  movq $1, %rax
  movq $1, %rbx
  shrdq $15, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $15, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $15, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $15, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $15, %rax, %rbx
  
  movq $0, %rax
  movq $0, %rbx
  shrdq $16, %rax, %rbx
  
  movq $1, %rax
  movq $1, %rbx
  shrdq $16, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $16, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $16, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $16, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $16, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $31, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $32, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $63, %rax, %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdq $64, %rax, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdq $64, %rax, %rbx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
