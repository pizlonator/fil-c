 .text
.globl _start

_start:
  // SCASB  
  leaq -8(%rsp), %rdi
  movq $0x1121314151617181, %rax
  pushq %rax
  movq $8, %rcx
  repe scasb



  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
