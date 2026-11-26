 .text
.globl _start

_start:
  // DECr8  
  movq  $0, %rax
  decb %al
  
  movq  $1, %rax
  decb %al
  
  movq  $0x7F, %rax
  decb %al
  
  movq  $0xFF, %rax
  decb %al
  
  movq  $0x7FFF, %rax
  decb %al
  
  movq  $0xFFFF, %rax
  decb %al
  
  movq  $0x7FFFFFFF, %rax
  decb %al
  
  movq  $0xFFFFFFFF, %rax
  decb %al
  
  movq  $0x7FFFFFFFFFFFFFFF, %rax
  decb %al
  
  movq  $0xFFFFFFFFFFFFFFFF, %rax
  decb %al


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
