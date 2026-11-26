 .text
.globl _start

_start:
  // TEST1  
  movq $0x8000000000000000, %rax
  movq $0x0, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x0, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x0, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x0, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x0, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x1, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x1, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x1, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x1, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x1, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x1, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x80, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x80, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x80, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x80, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x80, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x80, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x55, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x55, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x55, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x55, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x55, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x55, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xAA, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xAA, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xAA, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xAA, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xAA, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xAA, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xF7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xF7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xF7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xF7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xF7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xF7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x8000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x8000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x8000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x8000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x8000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x8000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x5500, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x5500, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x5500, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x5500, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x5500, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x5500, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xAA00, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xAA00, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xAA00, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xAA00, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xAA00, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xAA00, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xF7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xF7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xF7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xF7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xF7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xF7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x80000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x80000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x80000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x80000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x80000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x80000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x55000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x55000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x55000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x55000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x55000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x55000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xAA000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xAA000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xAA000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xAA000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xAA000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xAA000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xF7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xF7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xF7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xF7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xF7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xF7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x8000000080000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x8000000080000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x8000000080000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x8000000080000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x8000000080000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x8000000080000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x5500000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x5500000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x5500000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x5500000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x5500000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x5500000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xAA00000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xAA00000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xAA00000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xAA00000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xAA00000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xAA00000000000000, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x8000000000000000, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x5500000000000000, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xAA00000000000000, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  sarxq %rax, %rbx, %rcx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
