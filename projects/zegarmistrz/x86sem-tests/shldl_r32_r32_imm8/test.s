 .text
.globl _start

_start:
  // SHLDL  
  movq $0, %rax
  movq $0, %rbx
  shldl $0, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  shldl $1, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  shldl $1, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $1, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $1, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $1, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $1, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $1, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $1, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldl $1, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  shldl $15, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  shldl $15, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $15, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $15, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $15, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $15, %eax, %ebx
  
  movq $0, %rax
  movq $0, %rbx
  shldl $16, %eax, %ebx
  
  movq $1, %rax
  movq $1, %rbx
  shldl $16, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $16, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $16, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $16, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $16, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldl $31, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $32, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $32, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $32, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $32, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $32, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $32, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldl $32, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldl $63, %eax, %ebx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0xFFFF, %rax
  movq $0, %rbx
  shldl $64, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0, %rbx
  shldl $64, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  shldl $64, %eax, %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  shldl $64, %eax, %ebx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
