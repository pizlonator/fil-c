 .text
.globl _start

_start:
  // LOOP
  movq $2, %rcx
L1:
  loop L1

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
