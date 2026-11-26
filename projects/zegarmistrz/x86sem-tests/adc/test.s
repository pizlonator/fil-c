 .text
.globl _start

_start:
  // ADC1  
  addq $61438, %rax
  addq $16777215, %rcx
  adcw %ax,%ax
  adcw %cx,%ax
  addq $2147483647, %rax
  addq $2, %rcx
  adcl %ecx,%eax
  addq $2147483647, %rax
  addq $1, %rcx
  adcq %rax,%rax
  adcq %rcx,%rax
  addq $61438, %rax
  addq $1, %rcx
  adcb %cl,%al
  adcb %cl,%al

  // ADCal  
  movq $0x1, %rbx
  movq $0, %rax
  adcb  %bl, %al
  
  movq $0x1, %rbx
  movq $1, %rax
  adcb  %bl, %al
  
  movq $0x1, %rbx
  movq $0x7F, %rax
  adcb  %bl, %al
  
  movq $0x1, %rbx
  movq $0xFF, %rax
  adcb  %bl, %al

  // ADCax  
  movq $0x1, %rbx
  movq $0, %rax
  adcw %bx, %ax
  
  movq $0x1, %rbx
  movq $1, %rax
  adcw %bx, %ax
  
  movq $0x1, %rbx
  movq $0x7F, %rax
  adcw %bx, %ax
  
  movq $0x1, %rbx
  movq $0xFF, %rax
  adcw %bx, %ax
  
  movq $0x1, %rbx
  movq $0x7FFF, %rax
  adcw %bx, %ax
  
  movq $0x1, %rbx
  movq $0xFFFF, %rax
  adcw %bx, %ax

  // ADCeax  
  movq $0x1, %rbx
  movq $0, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $1, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $0x7F, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $0xFF, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $0x7FFF, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $0xFFFF, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $0x7FFFFFFF, %rax
  adcl %ebx, %eax
  
  movq $0x1, %rbx
  movq $0xFFFFFFFF, %rax
  adcl %ebx, %eax

  // ADCr8i8  
  movq $0x1, %rax
  movq $0, %rbx
  adcb %al, %bl
  
  movq $0x1, %rax
  movq $1, %rbx
  adcb %al, %bl
  
  movq $0x1, %rax
  movq $0x7F, %rbx
  adcb %al, %bl
  
  movq $0x1, %rax
  movq $0xFF, %rbx
  adcb %al, %bl

  // ADCr8u8_64  
  movq $0x7f, %rax
  movq $0, %r8
  adcb %al, %r8b
  
  movq $0x7f, %rax
  movq $1, %r8
  adcb %al, %r8b
  
  movq $0x7f, %rax
  movq $0x7F, %r8
  adcb %al, %r8b
  
  movq $0x7f, %rax
  movq $0xFF, %r8
  adcb %al, %r8b

  // ADCr8s8_64  
  movq $0xff, %rax
  movq $0, %r8
  adcb %al, %r8b
  
  movq $0xff, %rax
  movq $1, %r8
  adcb %al, %r8b
  
  movq $0xff, %rax
  movq $0x7F, %r8
  adcb %al, %r8b
  
  movq $0xff, %rax
  movq $0xFF, %r8
  adcb %al, %r8b

  // ADCr8r8  
  movq $0, %rbx
  movq $0, %rax
  adcb %al, %bl
  
  movq $1, %rbx
  movq $0, %rax
  adcb %al, %bl
  
  movq $0xFF, %rbx
  movq $1, %rax
  adcb %al, %bl
  
  movq $0x7F, %rbx
  movq $1, %rax
  adcb %al, %bl
  
  movq $0x7F, %rbx
  movq $0xFF, %rax
  adcb %al, %bl
  
  movq $0xFF, %rbx
  movq $0xFF, %rax
  adcb %al, %bl

  // ADCr16u8  
  movq $0x7f, %r8
  movq $0, %rax
  adcw %r8w, %ax
  
  movq $0x7f, %r8
  movq $1, %rax
  adcw %r8w, %ax
  
  movq $0x7f, %r8
  movq $0x7F, %rax
  adcw %r8w, %ax
  
  movq $0x7f, %r8
  movq $0xFF, %rax
  adcw %r8w, %ax
  
  movq $0x7f, %r8
  movq $0x7FFF, %rax
  adcw %r8w, %ax
  
  movq $0x7f, %r8
  movq $0xFFFF, %rax
  adcw %r8w, %ax

  // ADCr16s8  
  movq $0xff, %r8
  movq $0, %rax
  adcw %r8w, %ax
  
  movq $0xff, %r8
  movq $1, %rax
  adcw %r8w, %ax
  
  movq $0xff, %r8
  movq $0x7F, %rax
  adcw %r8w, %ax
  
  movq $0xff, %r8
  movq $0xFF, %rax
  adcw %r8w, %ax
  
  movq $0xff, %r8
  movq $0x7FFF, %rax
  adcw %r8w, %ax
  
  movq $0xff, %r8
  movq $0xFFFF, %rax
  adcw %r8w, %ax

  // ADCr16i16  
  movq $0xffff, %r8
  movq $0, %rax
  adcw %r8w, %ax
  
  movq $0xffff, %r8
  movq $1, %rax
  adcw %r8w, %ax
  
  movq $0xffff, %r8
  movq $0x7F, %rax
  adcw %r8w, %ax
  
  movq $0xffff, %r8
  movq $0xFF, %rax
  adcw %r8w, %ax
  
  movq $0xffff, %r8
  movq $0x7FFF, %rax
  adcw %r8w, %ax
  
  movq $0xffff, %r8
  movq $0xFFFF, %rax
  adcw %r8w, %ax

  // ADCr16r16  
  movq $0, %r8
  movq $0, %rax
  adcw %r8w, %ax
  
  movq $0, %r8
  movq $1, %rax
  adcw %r8w, %ax
  
  movq $1, %r8
  movq $0xFF, %rax
  adcw %r8w, %ax
  
  movq $1, %r8
  movq $0x7F, %rax
  adcw %r8w, %ax
  
  movq $0xFF, %r8
  movq $0x7F, %rax
  adcw %r8w, %ax
  
  movq $0xFF, %r8
  movq $0xFF, %rax
  adcw %r8w, %ax
  
  movq $1, %r8
  movq $0x7FFF, %rax
  adcw %r8w, %ax
  
  movq $0xFFFF, %r8
  movq $0x7FFF, %rax
  adcw %r8w, %ax
  
  movq $0xFFFF, %r8
  movq $0xFFFF, %rax
  adcw %r8w, %ax

  // ADCr32u8  
  movq $0x7f, %r8
  movq $0, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $1, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $0x7F, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $0xFF, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $0x7FFF, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $0xFFFF, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $0x7FFFFFFF, %rax
  adcl %r8d, %eax
  
  movq $0x7f, %r8
  movq $0xFFFFFFFF, %rax
  adcl %r8d, %eax

  // ADCr32s8  
  movq $0xff, %r8
  movq $0, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $1, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $0x7F, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $0xFF, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $0x7FFF, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $0xFFFF, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $0x7FFFFFFF, %rax
  adcl %r8d, %eax
  
  movq $0xff, %r8
  movq $0xFFFFFFFF, %rax
  adcl %r8d, %eax

  // ADCr32i32  
  movq $0x7fffffff, %r8
  movq $0, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $1, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $0x7F, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $0xFF, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $0x7FFF, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $0xFFFF, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $0x7FFFFFFF, %rax
  adcl %r8d, %eax
  
  movq $0x7fffffff, %r8
  movq $0xFFFFFFFF, %rax
  adcl %r8d, %eax

  // ADCr32r32  
  movq $0, %r8
  movq $0, %rax
  adcl %r8d, %eax
  
  movq $0, %r8
  movq $1, %rax
  adcl %r8d, %eax
  
  movq $1, %r8
  movq $0xFFFFFFFF, %rax
  adcl %r8d, %eax
  
  movq $0xFFFFFFFF, %r8
  movq $0xFFFFFFFF, %rax
  adcl %r8d, %eax
  
  movq $1, %r8
  movq $0x7FFFFFFF, %rax
  adcl %r8d, %eax
  
  movq $0x10, %r8
  movq $0, %rax
  adcl %r8d, %eax
  
  movq $0x10, %r8
  movq $0x7F, %rax
  adcl %r8d, %eax

  // ADCr64u8_64  
  movq $0x7f, %r8
  movq $0, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $1, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0x7F, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0xFF, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0x7FFF, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0xFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0x7FFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0xFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7f, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax

  // ADCr64s8_64  
  movq $0xff, %r8
  movq $0, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $1, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0x7F, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0xFF, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0x7FFF, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0xFFFF, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0x7FFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0xFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xff, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax

  // ADCr64u32_64  
  movq $0x7fffffff, %r8
  movq $0, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $1, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0x7F, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0xFF, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0x7FFF, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0xFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0x7FFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0xFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7fffffff, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax

  // ADCr64s32_64  
  movq $0xffffffff, %r8
  movq $0, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $1, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0x7F, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0xFF, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0x7FFF, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0xFFFF, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0x7FFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0xFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xffffffff, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax

  // ADCr64r64_64  
  movq $0, %r8
  movq $0, %rax
  adcq %r8, %rax
  
  movq $0, %r8
  movq $1, %rax
  adcq %r8, %rax
  
  movq $1, %r8
  movq $0xFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xFFFFFFFF, %r8
  movq $0xFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $1, %r8
  movq $0x7FFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x10, %r8
  movq $0, %rax
  adcq %r8, %rax
  
  movq $0x10, %r8
  movq $0x7F, %rax
  adcq %r8, %rax
  
  movq $1, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0x7FFFFFFFFFFFFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax
  
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  adcq %r8, %rax

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
