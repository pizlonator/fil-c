 .text
.globl _start

_start:
  // MOVS2  
  // movs_r64_r16
  addq $65535, %rax
  movswq %ax, %rax
  addq $32767, %rbx
  movswq %bx, %rcx
  
  // movs_r64_r32
  movq $4294967295, %rax
  movslq %eax, %rax
  addq $2147483647, %rbx
  movslq %ebx, %rcx
  
  // movs_r64_r8
  addq $255, %rax
  movsbq %al, %rax
  addq $127, %rbx
  movsbq %bl, %rcx
  
  
  // MOVS1  
  movq $0x48, %r8
  movsbq %r8b, %rax
  
  movq $0x48, %r8
  movswq %r8w, %rax
  
  movq $0x48, %r8
  movslq %r8d, %rax
  
  
  movq $0x8d, %r8
  movsbq %r8b, %rax
  
  movq $0x8d, %r8
  movswq %r8w, %rax
  
  movq $0x8d, %r8
  movslq %r8d, %rax
  
  
  movq $0x7c, %r8
  movsbq %r8b, %rax
  
  movq $0x7c, %r8
  movswq %r8w, %rax
  
  movq $0x7c, %r8
  movslq %r8d, %rax
  
  
  movq $0x24, %r8
  movsbq %r8b, %rax
  
  movq $0x24, %r8
  movswq %r8w, %rax
  
  movq $0x24, %r8
  movslq %r8d, %rax
  
  
  movq $0xf8, %r8
  movsbq %r8b, %rax
  
  movq $0xf8, %r8
  movswq %r8w, %rax
  
  movq $0xf8, %r8
  movslq %r8d, %rax
  
  
  movq $0x74, %r8
  movsbq %r8b, %rax
  
  movq $0x74, %r8
  movswq %r8w, %rax
  
  movq $0x74, %r8
  movslq %r8d, %rax
  
  
  movq $0x24, %r8
  movsbq %r8b, %rax
  
  movq $0x24, %r8
  movswq %r8w, %rax
  
  movq $0x24, %r8
  movslq %r8d, %rax
  
  
  movq $0xf8, %r8
  movsbq %r8b, %rax
  
  movq $0xf8, %r8
  movswq %r8w, %rax
  
  movq $0xf8, %r8
  movslq %r8d, %rax
  

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
