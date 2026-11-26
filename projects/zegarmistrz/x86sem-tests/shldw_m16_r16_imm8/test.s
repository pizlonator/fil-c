 .text
.globl _start

_start:
  // SHLDQ  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $0, %ax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $1, %ax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $15, %ax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shldw $15, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $15, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $15, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $15, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $15, %ax, (%rsp)
  
  movq $0, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $16, %ax, (%rsp)
  
  movq $1, %rax
  movq $1, %rbx
  pushq %rbx
  shldw $16, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $16, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $16, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $16, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $16, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $31, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $32, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $63, %ax, (%rsp)
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  pushq %rbx
  shldw $64, %ax, (%rsp)

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
