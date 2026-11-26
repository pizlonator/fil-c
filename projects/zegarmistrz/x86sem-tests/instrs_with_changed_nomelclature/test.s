 .text
.globl _start

_start:
  movq  $0x401147AE147AE148,  %rax
  pushq  %rax
  
  cvtsd2si (%rsp), %ecx
  cvtsd2si (%rsp), %rcx
  
  movq  $0x000000004089999a,  %rax
  pushq  %rax
  cvtss2si (%rsp), %ecx
  cvtss2si (%rsp), %rcx
  
  movq  $0x401147AE147AE148,  %rax
  pushq  %rax
  
  cvttsd2si (%rsp), %ecx
  cvttsd2si (%rsp), %rcx
  
  movq  $0x000000004089999a,  %rax
  pushq  %rax
  cvttss2si (%rsp), %ecx
  cvttss2si (%rsp), %rcx
  
  movq  $0x401147AE147AE148,  %rax
  pushq  %rax
  
  vcvtsd2si (%rsp), %ecx
  vcvtsd2si (%rsp), %rcx
  
  movq  $0x000000004089999a,  %rax
  pushq  %rax
  vcvtss2si (%rsp), %ecx
  vcvtss2si (%rsp), %rcx
  
  movq  $0x401147AE147AE148,  %rax
  pushq  %rax
  
  vcvttsd2si (%rsp), %ecx
  vcvttsd2si (%rsp), %rcx
  
  movq  $0x000000004089999a,  %rax
  pushq  %rax
  vcvttss2si (%rsp), %ecx
  vcvttss2si (%rsp), %rcx


  movq  $0x401147AE147AE148,  %rax
  movq  %rax, %xmm0
  cvtsd2si %xmm0, %ecx
  cvtsd2si %xmm0, %rcx
  
  movq  $0x000000004089999a,  %rax
  movq  %rax, %xmm0
  cvtss2si %xmm0, %ecx
  cvtss2si %xmm0, %rcx
  
  movq  $0x401147AE147AE148,  %rax
  movq  %rax, %xmm0
  cvttsd2si %xmm0, %ecx
  cvttsd2si %xmm0, %rcx
  
  movq  $0x000000004089999a,  %rax
  movq  %rax, %xmm0
  cvttss2si %xmm0, %ecx
  cvttss2si %xmm0, %rcx
  
  movq  $0x401147AE147AE148,  %rax
  movq  %rax, %xmm0
  vcvtsd2si %xmm0, %ecx
  vcvtsd2si %xmm0, %rcx
  
  movq  $0x000000004089999a,  %rax
  movq  %rax, %xmm0
  vcvtss2si %xmm0, %ecx
  vcvtss2si %xmm0, %rcx
  
  movq  $0x401147AE147AE148,  %rax
  movq  %rax, %xmm0
  vcvttsd2si %xmm0, %ecx
  vcvttsd2si %xmm0, %rcx
  
  movq  $0x000000004089999a,  %rax
  movq  %rax, %xmm0
  vcvttss2si %xmm0, %ecx
  vcvttss2si %xmm0, %rcx


  movq  $0x401147AE147AE148,  %rax
  push %rax
  movq  $0x000000004089999a,  %rax
  push %rax
  vbroadcasti128 (%rsp), %ymm1

  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
