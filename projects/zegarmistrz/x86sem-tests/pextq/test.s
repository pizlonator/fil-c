 .text
.globl _start

_start:
  // PDEP  
  movq $0, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $1, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $8, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $9, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $15, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $16, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $31, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $32, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $63, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $1, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFE, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFE, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x80000000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x55000000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA000000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFE, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x8000000000000000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x5500000000000000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xAA00000000000000, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0x0, %rcx
  pextq %rax, %rcx, %rbx
  
  movq $0x0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rcx
  pextq %rax, %rcx, %rbx

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
