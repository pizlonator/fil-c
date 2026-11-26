 .text
.globl _start

_start:
  // SHRDQ  
  movq $0, %rax
  movq $0, %rbx
  shrdl $0, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  shrdl $1, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $1, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  shrdl $15, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  shrdl $15, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $15, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $15, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $15, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $15, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  shrdl $16, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  shrdl $16, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $16, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $16, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $16, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $16, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $31, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $32, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $63, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shrdl $64, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shrdl $64, %eax, %ebx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
