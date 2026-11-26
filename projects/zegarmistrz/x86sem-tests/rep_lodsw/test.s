 .text
.globl _start

_start:
  // MOVS2  
  leaq -8(%rsp), %rsi
  movq $0x1121314151617181, %rbx
  pushq %rbx
  movq $4, %rcx
  rep lodsw

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
