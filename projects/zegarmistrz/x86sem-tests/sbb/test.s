 .text
.globl _start

_start:
  // SBB1: CF-setup variants (SBB consumes the borrow; stc/clc/cmc and
  // arithmetic carries both seed CF)
  stc
  movq $5, %rax
  movq $2, %rbx
  sbbq %rbx, %rax
  clc
  movq $5, %rax
  sbbq %rbx, %rax
  cmc
  movq $5, %rax
  sbbq %rbx, %rax
  clc
  movq $5, %rax
  movq $2, %rbx
  addq $-1, %rax
  sbbq %rbx, %rax
  stc
  movq $2, %rax
  sbbq %rbx, %rax
  stc
  movq $3, %rax
  sbbq %rbx, %rax
  clc
  movq $2, %rax
  sbbq %rbx, %rax

  // SBBal (u8 corners, CF=0 and CF=1)
  clc
  movq $0x1, %rbx
  movq $0, %rax
  sbbb  %bl, %al

  stc
  movq $0x1, %rbx
  movq $0, %rax
  sbbb  %bl, %al

  clc
  movq $0x1, %rbx
  movq $1, %rax
  sbbb  %bl, %al

  stc
  movq $0x1, %rbx
  movq $1, %rax
  sbbb  %bl, %al

  clc
  movq $0x1, %rbx
  movq $0x7F, %rax
  sbbb  %bl, %al

  stc
  movq $0x1, %rbx
  movq $0x7F, %rax
  sbbb  %bl, %al

  clc
  movq $0x1, %rbx
  movq $0xFF, %rax
  sbbb  %bl, %al

  stc
  movq $0x1, %rbx
  movq $0xFF, %rax
  sbbb  %bl, %al

  // SBBal s8 corners (0x80 = -128, 0xff = -1)
  clc
  movq $0x80, %rbx
  movq $0x80, %rax
  sbbb  %bl, %al

  stc
  movq $0x80, %rbx
  movq $0x80, %rax
  sbbb  %bl, %al

  clc
  movq $0xff, %rbx
  movq $0x80, %rax
  sbbb  %bl, %al

  stc
  movq $0xff, %rbx
  movq $0x7f, %rax
  sbbb  %bl, %al

  // SBBax (i16 corners)
  clc
  movq $0x1, %rbx
  movq $0, %rax
  sbbw %bx, %ax

  stc
  movq $0x1, %rbx
  movq $0, %rax
  sbbw %bx, %ax

  clc
  movq $0x1, %rbx
  movq $1, %rax
  sbbw %bx, %ax

  stc
  movq $0x1, %rbx
  movq $1, %rax
  sbbw %bx, %ax

  clc
  movq $0x1, %rbx
  movq $0x7F, %rax
  sbbw %bx, %ax

  stc
  movq $0x1, %rbx
  movq $0x7FFF, %rax
  sbbw %bx, %ax

  clc
  movq $0x1, %rbx
  movq $0xFFFF, %rax
  sbbw %bx, %ax

  stc
  movq $0x1, %rbx
  movq $0xFFFF, %rax
  sbbw %bx, %ax

  clc
  movq $0x8000, %rbx
  movq $0x8000, %rax
  sbbw %bx, %ax

  stc
  movq $0x8000, %rbx
  movq $0x7FFF, %rax
  sbbw %bx, %ax

  // SBBeax (i32 corners)
  clc
  movq $0x1, %rbx
  movq $0, %rax
  sbbl %ebx, %eax

  stc
  movq $0x1, %rbx
  movq $0, %rax
  sbbl %ebx, %eax

  clc
  movq $0x1, %rbx
  movq $1, %rax
  sbbl %ebx, %eax

  stc
  movq $0x1, %rbx
  movq $1, %rax
  sbbl %ebx, %eax

  clc
  movq $0x1, %rbx
  movq $0x7FFFFFFF, %rax
  sbbl %ebx, %eax

  stc
  movq $0x1, %rbx
  movq $0x7FFFFFFF, %rax
  sbbl %ebx, %eax

  clc
  movq $0x1, %rbx
  movq $0xFFFFFFFF, %rax
  sbbl %ebx, %eax

  stc
  movq $0x1, %rbx
  movq $0xFFFFFFFF, %rax
  sbbl %ebx, %eax

  clc
  movq $0x80000000, %rbx
  movq $0x80000000, %rax
  sbbl %ebx, %eax

  stc
  movq $0x80000000, %rbx
  movq $0x7FFFFFFF, %rax
  sbbl %ebx, %eax

  // SBBr64 (i64 corners)
  clc
  movq $1, %r8
  movq $0, %rax
  sbbq %r8, %rax

  stc
  movq $1, %r8
  movq $0, %rax
  sbbq %r8, %rax

  clc
  movq $1, %r8
  movq $1, %rax
  sbbq %r8, %rax

  stc
  movq $1, %r8
  movq $1, %rax
  sbbq %r8, %rax

  clc
  movq $1, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  stc
  movq $1, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  clc
  movq $1, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  stc
  movq $1, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  clc
  movq $0x8000000000000000, %r8
  movq $0x8000000000000000, %rax
  sbbq %r8, %rax

  stc
  movq $0x8000000000000000, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  // SBBr8u8_64 / SBBr8s8_64 (r8b low-byte forms)
  clc
  movq $0x7f, %rax
  movq $1, %r8
  sbbb %al, %r8b

  stc
  movq $0x7f, %rax
  movq $1, %r8
  sbbb %al, %r8b

  clc
  movq $0xff, %rax
  movq $0x7F, %r8
  sbbb %al, %r8b

  stc
  movq $0xff, %rax
  movq $0xFF, %r8
  sbbb %al, %r8b

  // SBBr16u8 / SBBr16i16 (r8w)
  clc
  movq $0x7f, %r8
  movq $0, %rax
  sbbw %r8w, %ax

  stc
  movq $0x7f, %r8
  movq $0, %rax
  sbbw %r8w, %ax

  clc
  movq $0xffff, %r8
  movq $0x7FFF, %rax
  sbbw %r8w, %ax

  stc
  movq $0xffff, %r8
  movq $0xFFFF, %rax
  sbbw %r8w, %ax

  // SBBr32u8 / SBBr32i32 (r8d)
  clc
  movq $0x7f, %r8
  movq $0, %rax
  sbbl %r8d, %eax

  stc
  movq $0x7f, %r8
  movq $0, %rax
  sbbl %r8d, %eax

  clc
  movq $0x7fffffff, %r8
  movq $0xFFFFFFFF, %rax
  sbbl %r8d, %eax

  stc
  movq $0x7fffffff, %r8
  movq $0xFFFFFFFF, %rax
  sbbl %r8d, %eax

  // SBBr64u8 / SBBr64s8 / SBBr64u32 / SBBr64s32 (full r8 with the
  // borrowed-in corners)
  clc
  movq $0x7f, %r8
  movq $0, %rax
  sbbq %r8, %rax

  stc
  movq $0xff, %r8
  movq $0, %rax
  sbbq %r8, %rax

  clc
  movq $0x7fffffff, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  stc
  movq $0xffffffff, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  sbbq %r8, %rax

  // SBBr8r8 / SBBr16r16 / SBBr32r32 / SBBr64r64 same-register forms
  // (the borrow-consume self-subtract corner)
  clc
  movq $0, %rbx
  movq $0, %rax
  sbbb %al, %bl

  stc
  movq $0, %rbx
  movq $0, %rax
  sbbb %al, %bl

  clc
  movq $1, %r8
  movq $0xFF, %rax
  sbbw %ax, %r8w

  stc
  movq $1, %r8
  movq $0xFF, %rax
  sbbw %ax, %r8w

  clc
  movq $0, %r8
  movq $0x10, %rax
  sbbl %eax, %r8d

  stc
  movq $0, %r8
  movq $0x10, %rax
  sbbl %eax, %r8d

  clc
  movq $0, %r8
  movq $0x7FFFFFFFFFFFFFFF, %rax
  sbbq %rax, %r8

  stc
  movq $0xFFFFFFFFFFFFFFFF, %r8
  movq $0xFFFFFFFFFFFFFFFF, %rax
  sbbq %rax, %r8

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
