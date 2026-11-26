 .text
.globl _start

_start:
  // XCHG  
  // Same registers
  movq $0, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $1, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $1, %rax
  movq $0x7FFFFFFFFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x41, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x41, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x51, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x51, %rax
  movq $0xEFEFEFEFEFEFEFEF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x80, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x80, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x55, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x55, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAA, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAA, %rax
  movq $0xAAAAAAAAAAAAAAAA, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x0F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x0F, %rax
  movq $0xAA00000000000000, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xF7, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xF7, %rax
  movq $0x5555555555555555, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFE, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFE, %rax
  movq $0x5500000000000000, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xEF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xEF, %rax
  movq $0x8000000000000000, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7F, %rax
  movq $0x5115151515115151, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFF, %rax
  movq $0x4141414141414141, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x4141, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x4141, %rax
  movq $0xFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x5151, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x5151, %rax
  movq $0x7FFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x8000, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x8000, %rax
  movq $0xEFEFEFEF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x5500, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x5500, %rax
  movq $0xFEFEFEFE, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x5555, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x5555, %rax
  movq $0xF7F7F7F7, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAA00, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAA00, %rax
  movq $0x0F0F0F0F, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAAAA, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAAAA, %rax
  movq $0xAAAAAAAA, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x0F0F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x0F0F, %rax
  movq $0xAAFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xF7F7, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xF7F7, %rax
  movq $0x55555555, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFEFE, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFEFE, %rax
  movq $0x55000000, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xEFEF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xEFEF, %rax
  movq $0x51515151, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFF, %rax
  movq $0x41414141, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x41414141, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x41414141, %rax
  movq $0x7FFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x51515151, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x51515151, %rax
  movq $0xEFEF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x55000000, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x55000000, %rax
  movq $0xFEFE, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x55555555, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x55555555, %rax
  movq $0xF7F7, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAAFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAAFFFFFF, %rax
  movq $0x0F0F, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAAAAAAAA, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAAAAAAAA, %rax
  movq $0xAAAA, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x0F0F0F0F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x0F0F0F0F, %rax
  movq $0xAA00, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xF7F7F7F7, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xF7F7F7F7, %rax
  movq $0x5555, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFEFEFEFE, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFEFEFEFE, %rax
  movq $0x5500, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xEFEFEFEF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xEFEFEFEF, %rax
  movq $0x8000, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFFFFFF, %rax
  movq $0x5151, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFFFFFF, %rax
  movq $0x4141, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x4141414141414141, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x4141414141414141, %rax
  movq $0xFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x5115151515115151, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x5115151515115151, %rax
  movq $0x7F, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x8000000000000000, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x8000000000000000, %rax
  movq $0xEF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x5500000000000000, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x5500000000000000, %rax
  movq $0xFE, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x5555555555555555, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x5555555555555555, %rax
  movq $0xF7, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAA00000000000000, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAA00000000000000, %rax
  movq $0x0F, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xAAAAAAAAAAAAAAAA, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xAAAAAAAAAAAAAAAA, %rax
  movq $0xAA, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x0F0F0F0F0F0F0F0F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x55, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xF7F7F7F7F7F7F7F7, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x80, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xEFEFEFEFEFEFEFEF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xEFEFEFEFEFEFEFEF, %rax
  movq $0x51, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFEFEFEFEFEFEFEFE, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x41, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0, %rax
  movq $0, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $1, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $1, %rax
  movq $0, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFF, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7F, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7F, %rax
  movq $0xFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFF, %rax
  movq $0xFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFF, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFF, %rax
  movq $0xFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFFFFFF, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFFFFFF, %rax
  movq $0xFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFFFFFF, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0, %rax
  movq $0x10, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7F, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7F, %rax
  movq $0x10, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $1, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $0x7FFFFFFFFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  
  
  // Same registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  
  xaddb %al,  %al
  xaddb %al,  %ah
  xaddb %ah,  %al
  xaddb %ah,  %ah
  xaddw %ax,  %ax
  xaddl %eax,  %eax
  xaddq %rax,  %rax
  
  // Diff registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  
  xaddb %bl,  %al
  xaddb %bl,  %ah
  xaddb %bh,  %al
  xaddb %bh,  %ah
  xaddw %bx,  %ax
  xaddl %ebx,  %eax
  xaddq %rbx,  %rax
  
  


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
