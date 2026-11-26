 .text
.globl _start

_start:
  // SHLDL  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $0, %eax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $1, %eax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $15, %eax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shrdl $15, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $15, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $15, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $15, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $15, %eax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $16, %eax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shrdl $16, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $16, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $16, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $16, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $16, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $31, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $32, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $63, %eax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shrdl $64, %eax, (%rsp)

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
