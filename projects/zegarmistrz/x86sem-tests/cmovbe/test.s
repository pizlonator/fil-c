 .text
.globl _start

_start:
  // XCHG  
  // same registers
  movq $0, %rax
  movq $0, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $1, %rax
  movq $1, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x41, %rax
  movq $0x41, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x51, %rax
  movq $0x51, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x80, %rax
  movq $0x80, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x55, %rax
  movq $0x55, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAA, %rax
  movq $0xAA, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x0F, %rax
  movq $0x0F, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xF7, %rax
  movq $0xF7, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFE, %rax
  movq $0xFE, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xEF, %rax
  movq $0xEF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x7F, %rax
  movq $0x7F, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFF, %rax
  movq $0xFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x4141, %rax
  movq $0x4141, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x5151, %rax
  movq $0x5151, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x8000, %rax
  movq $0x8000, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x5500, %rax
  movq $0x5500, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x5555, %rax
  movq $0x5555, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAA00, %rax
  movq $0xAA00, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAAAA, %rax
  movq $0xAAAA, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x0F0F, %rax
  movq $0x0F0F, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xF7F7, %rax
  movq $0xF7F7, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFEFE, %rax
  movq $0xFEFE, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xEFEF, %rax
  movq $0xEFEF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x7FFF, %rax
  movq $0x7FFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x41414141, %rax
  movq $0x41414141, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x51515151, %rax
  movq $0x51515151, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x55000000, %rax
  movq $0x55000000, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x55555555, %rax
  movq $0x55555555, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAAFFFFFF, %rax
  movq $0xAAFFFFFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAAAAAAAA, %rax
  movq $0xAAAAAAAA, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x0F0F0F0F, %rax
  movq $0x0F0F0F0F, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xF7F7F7F7, %rax
  movq $0xF7F7F7F7, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFEFEFEFE, %rax
  movq $0xFEFEFEFE, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xEFEFEFEF, %rax
  movq $0xEFEFEFEF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x7FFFFFFF, %rax
  movq $0x7FFFFFFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x4141414141414141, %rax
  movq $0x4141414141414141, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x5115151515115151, %rax
  movq $0x5115151515115151, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x8000000000000000, %rax
  movq $0x8000000000000000, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x5500000000000000, %rax
  movq $0x5500000000000000, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x5555555555555555, %rax
  movq $0x5555555555555555, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAA00000000000000, %rax
  movq $0xAA00000000000000, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xAAAAAAAAAAAAAAAA, %rax
  movq $0xAAAAAAAAAAAAAAAA, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xEFEFEFEFEFEFEFEF, %rax
  movq $0xEFEFEFEFEFEFEFEF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $0x7FFFFFFFFFFFFFFF, %rbx
  
  cmovbeq %rax, %rbx
  
  
  
  // same registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  
  cmovbeq %rax, %rbx
  
  


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
