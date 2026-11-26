 .text
.globl _start

_start:
  // MOVS2  
  leaq -8(%rsp), %rsi
  leaq -16(%rsp), %rdi
  movq $0x1121314151617181, %rax
  pushq %rax
  movq $1, %rcx
  rep movsq
  movq -8(%rdi), %rbx
  

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
