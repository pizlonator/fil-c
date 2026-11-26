 .text
.globl _start

_start:
  // ORr64u8_64  
  movq $0, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $1, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0x7F, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0xFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0x7FFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0xFFFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0x7FFFFFFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0xFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $1, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  orq %r8, %rax
  
  movq $0, %r8
  movq $0x10, %rax
  orq %r8, %rax
  
  movq $0x7F, %r8
  movq $0x10, %rax
  orq %r8, %rax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
