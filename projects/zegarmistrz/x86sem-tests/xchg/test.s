 .text
.globl _start

_start:
  // XCHG  
  // same registers
  movq $0, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0, %rax
  movq $0xFFFFFFFFFFFFFFFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $1, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $1, %rax
  movq $0x7FFFFFFFFFFFFFFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x41, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x41, %rax
  movq $0xFEFEFEFEFEFEFEFE, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x51, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x51, %rax
  movq $0xEFEFEFEFEFEFEFEF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x80, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x80, %rax
  movq $0xF7F7F7F7F7F7F7F7, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x55, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x55, %rax
  movq $0x0F0F0F0F0F0F0F0F, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAA, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAA, %rax
  movq $0xAAAAAAAAAAAAAAAA, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x0F, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x0F, %rax
  movq $0xAA00000000000000, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xF7, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xF7, %rax
  movq $0x5555555555555555, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFE, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFE, %rax
  movq $0x5500000000000000, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xEF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xEF, %rax
  movq $0x8000000000000000, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x7F, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x7F, %rax
  movq $0x5115151515115151, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFF, %rax
  movq $0x4141414141414141, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x4141, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x4141, %rax
  movq $0xFFFFFFFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x5151, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x5151, %rax
  movq $0x7FFFFFFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x8000, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x8000, %rax
  movq $0xEFEFEFEF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x5500, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x5500, %rax
  movq $0xFEFEFEFE, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x5555, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x5555, %rax
  movq $0xF7F7F7F7, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAA00, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAA00, %rax
  movq $0x0F0F0F0F, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAAAA, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAAAA, %rax
  movq $0xAAAAAAAA, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x0F0F, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x0F0F, %rax
  movq $0xAAFFFFFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xF7F7, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xF7F7, %rax
  movq $0x55555555, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFEFE, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFEFE, %rax
  movq $0x55000000, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xEFEF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xEFEF, %rax
  movq $0x51515151, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x7FFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x7FFF, %rax
  movq $0x41414141, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFFFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFFFF, %rax
  movq $0xFFFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x41414141, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x41414141, %rax
  movq $0x7FFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x51515151, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x51515151, %rax
  movq $0xEFEF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x55000000, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x55000000, %rax
  movq $0xFEFE, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x55555555, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x55555555, %rax
  movq $0xF7F7, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAAFFFFFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAAFFFFFF, %rax
  movq $0x0F0F, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAAAAAAAA, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAAAAAAAA, %rax
  movq $0xAAAA, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x0F0F0F0F, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x0F0F0F0F, %rax
  movq $0xAA00, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xF7F7F7F7, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xF7F7F7F7, %rax
  movq $0x5555, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFEFEFEFE, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFEFEFEFE, %rax
  movq $0x5500, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xEFEFEFEF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xEFEFEFEF, %rax
  movq $0x8000, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x7FFFFFFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x7FFFFFFF, %rax
  movq $0x5151, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFFFFFFFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFFFFFFFF, %rax
  movq $0x4141, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x4141414141414141, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x4141414141414141, %rax
  movq $0xFF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x5115151515115151, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x5115151515115151, %rax
  movq $0x7F, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x8000000000000000, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x8000000000000000, %rax
  movq $0xEF, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x5500000000000000, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x5500000000000000, %rax
  movq $0xFE, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x5555555555555555, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x5555555555555555, %rax
  movq $0xF7, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAA00000000000000, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAA00000000000000, %rax
  movq $0x0F, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xAAAAAAAAAAAAAAAA, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xAAAAAAAAAAAAAAAA, %rax
  movq $0xAA, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x0F0F0F0F0F0F0F0F, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x0F0F0F0F0F0F0F0F, %rax
  movq $0x55, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xF7F7F7F7F7F7F7F7, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xF7F7F7F7F7F7F7F7, %rax
  movq $0x80, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xEFEFEFEFEFEFEFEF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xEFEFEFEFEFEFEFEF, %rax
  movq $0x51, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFEFEFEFEFEFEFEFE, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFEFEFEFEFEFEFEFE, %rax
  movq $0x41, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0x7FFFFFFFFFFFFFFF, %rax
  movq $1, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  
  
  // same registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  
  xchgb %al,  %al
  xchgb %ah,  %ah
  xchgb %al,  %ah
  xchgb %ah,  %al
  xchgw %ax, %ax
  xchgl %eax, %eax
  xchgq %rax, %rax
  
  // same registers
  movq $0xFFFFFFFFFFFFFFFF, %rax
  movq $0, %rbx
  
  xchgb %al,  %bl
  xchgb %ah,  %bh
  xchgb %al,  %bh
  xchgb %ah,  %bl
  
  xchgw %ax, %bx
  xchgw %bx, %ax
  xchgl %eax, %ebx
  xchgl %ebx, %eax
  xchgq %rax, %rbx
  xchgq %rbx, %rax
  


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
