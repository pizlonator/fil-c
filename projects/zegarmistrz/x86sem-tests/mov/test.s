 .text
.globl _start

_start:
  // MOVQ1  
  // swap %rax %rbx using %rcx
  movq $-71777214294589696, %rax
  movq $71777214294589695, %rbx
  movq %rax, %rcx
  movq %rbx, %rax
  movq %rcx, %rbx
  // %rax: 00000000 11111111 00000000 11111111 00000000 11111111 00000000 11111111
  // %rbx: 11111111 00000000 11111111 00000000 11111111 00000000 11111111 00000000
  movb %ah, %al
  // %rax: 00000000 11111111 00000000 11111111 00000000 11111111 00000000 00000000
  movq $255, %rcx
  movb %cl, %ah
  // %rax: 00000000 11111111 00000000 11111111 00000000 11111111 11111111 00000000
  
  
  // MOVr64r64  
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq %rax, %rbx
  
  movq $1, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq %rax, %rbx
  
  movq $0x41, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq %rax, %rbx
  
  movq $0x4141, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq %rax, %rbx
  
  movq $0x41414141, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq %rax, %rbx
  
  movq $0x4141414141414141, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  movq %rax, %rbx

  // MOVr64i32  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0x41414141, %rbx
  movq %rbx, %rax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
