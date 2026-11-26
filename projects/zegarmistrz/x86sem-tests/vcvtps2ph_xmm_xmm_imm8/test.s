 .text
.globl _start

_start:
  // ADCal  
  movq $0x0000, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x8000, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x8000, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3000, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3400, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3800, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3C00, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x4000, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x4400, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x4800, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3600, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3A00, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3E00, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x4200, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x4600, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3C01, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3267, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3668, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x3AFD, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x63CE, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x7BD0, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x0208, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x7BFF, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0xFBFF, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x0001, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $0x8001, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1

  // ADCal  
  movq $0x0, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2147483648, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1040187392, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1048576000, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1056964608, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1065353216, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1073741824, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1082130432, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1090519040, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1052770304, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1061158912, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1069547520, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1077936128, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1086324736, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1036831949, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1045220557, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1053609165, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1061997773, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1399379109, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1733542428, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2067830734, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2139095040, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $4286578688, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $3221225472, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1103626240, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2139095039, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $8388608, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1078530011, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1051372203, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1095106560, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1116225274, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1077936128, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $3225419776, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $1073741825, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $3221225473, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $4194304, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2151677953, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2139095039, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $4286578687, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $8388607, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $2155872255, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $8388608, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $12582912, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1
  
  movq $16777215, %rax
  movq %rax, %xmm0
  vcvtps2ph  $0, %xmm0, %xmm1
  vcvtps2ph  $1, %xmm0, %xmm1
  vcvtps2ph  $2, %xmm0, %xmm1
  vcvtps2ph  $3, %xmm0, %xmm1


  nop

/* --- zegarmistrz harness epilogue (not part of the original test) --- */
movq $60, %rax
xorq %rdi, %rdi
syscall
