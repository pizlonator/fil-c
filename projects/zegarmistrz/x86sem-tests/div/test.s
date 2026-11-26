 .text
.globl _start

_start:
  // DIVax  
  movq $0, %rax
  movq $1, %rbx
  
  divb %bl
  
  movq $1, %rax
  movq $1, %rbx
  
  divb %bl
  
  movq $1, %rax
  movq $2, %rbx
  
  divb %bl
  
  movq $0xFFFF, %rax
  movq $1, %rbx
  
  divb %bl
  
  movq $0xFFFF, %rax
  movq $2, %rbx
  
  divb %bl
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  
  divb %bl
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  
  divb %bl
  
  movq $0x7FFF, %rax
  movq $1, %rbx
  
  divb %bl
  
  movq $0x7FFF, %rax
  movq $2, %rbx
  
  divb %bl
  
  movq $0x7FFF, %rax
  movq $0xFF, %rbx
  
  divb %bl
  
  movq $0x7FFF, %rax
  movq $0xF7, %rbx
  
  divb %bl

  // DIVdxax  
  movq $0, %rax
  movq $1, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $1, %rax
  movq $1, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $1, %rax
  movq $2, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $1, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $2, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $1, %rbx
  movq $0xFFFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $2, %rbx
  movq $0xFFFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0xFFFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0xFFFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $1, %rbx
  movq $0x7FFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $2, %rbx
  movq $0x7FFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0x7FFF, %rdx
  
  divw %bx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0x7FFF, %rdx
  
  divw %bx

  // DIVedxeax  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0xFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0xFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0x7FFF, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0x7FFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFF, %rbx
  movq $0xFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xF7, %rbx
  movq $0xFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFF, %rbx
  movq $0x7FFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xF7, %rbx
  movq $0x7FFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xffffffff, %rax
  movq $32, %rbx
  movq $0xffffffff, %rdx
  
  divl %ebx

  // DIVrdxrax_64  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0xFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0xFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xFF, %rbx
  movq $0x7FFF, %rdx
  
  divl %ebx
  
  movq $0xFFFF, %rax
  movq $0xF7, %rbx
  movq $0x7FFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFF, %rbx
  movq $0xFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xF7, %rbx
  movq $0xFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xFF, %rbx
  movq $0x7FFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFF, %rax
  movq $0xF7, %rbx
  movq $0x7FFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFF, %rbx
  movq $0xFFFFFFFFFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xF7, %rbx
  movq $0xFFFFFFFFFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFF, %rbx
  movq $0x7FFFFFFFFFFFFFFF, %rdx
  
  divl %ebx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xF7, %rbx
  movq $0x7FFFFFFFFFFFFFFF, %rdx
  
  divl %ebx

  // DIVrdxrax_64  
  movq $0x41, %rax
  movq $0x41, %rbx
  movq $0x41, %rdx
  
  divq %rbx
  
  movq $0x51, %rax
  movq $0x51, %rbx
  movq $0x51, %rdx
  
  divq %rbx
  
  movq $0x80, %rax
  movq $0x80, %rbx
  movq $0x80, %rdx
  
  divq %rbx
  
  movq $0x55, %rax
  movq $0x55, %rbx
  movq $0x55, %rdx
  
  divq %rbx
  
  movq $0xAA, %rax
  movq $0xAA, %rbx
  movq $0xAA, %rdx
  
  divq %rbx
  
  movq $0x0F, %rax
  movq $0x0F, %rbx
  movq $0x0F, %rdx
  
  divq %rbx
  
  movq $0xF7, %rax
  movq $0xF7, %rbx
  movq $0xF7, %rdx
  
  divq %rbx
  
  movq $0xFE, %rax
  movq $0xFE, %rbx
  movq $0xFE, %rdx
  
  divq %rbx
  
  movq $0xEF, %rax
  movq $0xEF, %rbx
  movq $0xEF, %rdx
  
  divq %rbx
  
  movq $0x7F, %rax
  movq $0x7F, %rbx
  movq $0x7F, %rdx
  
  divq %rbx
  
  movq $0xFF, %rax
  movq $0xFF, %rbx
  movq $0xFF, %rdx
  
  divq %rbx
  
  movq $0x4141, %rax
  movq $0x4141, %rbx
  movq $0x4141, %rdx
  
  divq %rbx
  
  movq $0x5151, %rax
  movq $0x5151, %rbx
  movq $0x5151, %rdx
  
  divq %rbx
  
  movq $0x8000, %rax
  movq $0x8000, %rbx
  movq $0x8000, %rdx
  
  divq %rbx
  
  movq $0x5500, %rax
  movq $0x5500, %rbx
  movq $0x5500, %rdx
  
  divq %rbx
  
  movq $0x5555, %rax
  movq $0x5555, %rbx
  movq $0x5555, %rdx
  
  divq %rbx
  
  movq $0xAA00, %rax
  movq $0xAA00, %rbx
  movq $0xAA00, %rdx
  
  divq %rbx
  
  movq $0xAAAA, %rax
  movq $0xAAAA, %rbx
  movq $0xAAAA, %rdx
  
  divq %rbx
  
  movq $0x0F0F, %rax
  movq $0x0F0F, %rbx
  movq $0x0F0F, %rdx
  
  divq %rbx
  
  movq $0xF7F7, %rax
  movq $0xF7F7, %rbx
  movq $0xF7F7, %rdx
  
  divq %rbx
  
  movq $0xFEFE, %rax
  movq $0xFEFE, %rbx
  movq $0xFEFE, %rdx
  
  divq %rbx
  
  movq $0xEFEF, %rax
  movq $0xEFEF, %rbx
  movq $0xEFEF, %rdx
  
  divq %rbx
  
  movq $0x7FFF, %rax
  movq $0x7FFF, %rbx
  movq $0x7FFF, %rdx
  
  divq %rbx
  
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  movq $0xFFFF, %rdx
  
  divq %rbx
  
  movq $0x41414141, %rax
  movq $0x41414141, %rbx
  movq $0x41414141, %rdx
  
  divq %rbx
  
  movq $0x51515151, %rax
  movq $0x51515151, %rbx
  movq $0x51515151, %rdx
  
  divq %rbx
  
  movq $0x55000000, %rax
  movq $0x55000000, %rbx
  movq $0x55000000, %rdx
  
  divq %rbx
  
  movq $0x55555555, %rax
  movq $0x55555555, %rbx
  movq $0x55555555, %rdx
  
  divq %rbx
  
  movq $0xAAFFFFFF, %rax
  movq $0xAAFFFFFF, %rbx
  movq $0xAAFFFFFF, %rdx
  
  divq %rbx
  
  movq $0xAAAAAAAA, %rax
  movq $0xAAAAAAAA, %rbx
  movq $0xAAAAAAAA, %rdx
  
  divq %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $0x0F0F0F0F, %rbx
  movq $0x0F0F0F0F, %rdx
  
  divq %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $0xF7F7F7F7, %rbx
  movq $0xF7F7F7F7, %rdx
  
  divq %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $0xFEFEFEFE, %rbx
  movq $0xFEFEFEFE, %rdx
  
  divq %rbx
  
  movq $0xEFEFEFEF, %rax
  movq $0xEFEFEFEF, %rbx
  movq $0xEFEFEFEF, %rdx
  
  divq %rbx
  
  movq $0x4141414141414141, %rax
  movq $0x4141414141414141, %rbx
  movq $0x4141414141414141, %rdx
  
  divq %rbx
  
  movq $0x5115151515115151, %rax
  movq $0x5115151515115151, %rbx
  movq $0x5115151515115151, %rdx
  
  divq %rbx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
