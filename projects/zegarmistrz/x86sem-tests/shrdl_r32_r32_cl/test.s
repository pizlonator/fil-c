 .text
.globl _start

_start:
  // SHRDL  
  movq $0, %rax
  movq $0, %rbx
  movq $0, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $1, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  movq $15, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  movq $15, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $15, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $15, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $15, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $15, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  movq $16, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  movq $16, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $16, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $16, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $16, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $16, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  movq $31, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $32, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  movq $63, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  movq $64, %rcx
  shrdl %cl, %eax, %ebx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
