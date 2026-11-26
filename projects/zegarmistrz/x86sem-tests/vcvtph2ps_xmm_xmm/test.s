 .text
.globl _start

_start:

  // ADCal  
  movq $0x0000, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x8000, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x8000, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3000, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3400, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3800, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3C00, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x4000, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x4400, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x4800, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3600, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3A00, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3E00, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x4200, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x4600, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3C01, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3267, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3668, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x3AFD, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x63CE, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x7BD0, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x0208, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x7BFF, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0xFBFF, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x0001, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1
  
  movq $0x8001, %rax
  movq %rax, %xmm0
  vcvtph2ps  %xmm0, %xmm1


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
