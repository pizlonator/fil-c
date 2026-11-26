 .text
.globl _start

_start:
  // SHLDQ  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $0, %rax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $1, %rax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $15, %rax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shrdq $15, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $15, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $15, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $15, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $15, %rax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $16, %rax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shrdq $16, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $16, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $16, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $16, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $16, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $31, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $32, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $63, %rax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdq $64, %rax, (%rsp)

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
