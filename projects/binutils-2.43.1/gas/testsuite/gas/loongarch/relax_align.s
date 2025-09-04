# If max < -0x80000000, max becomes a positive number because type conversion
# (bfd_signed_vma -> unsigned int).
  .text
.L1:
  ret
  .align 4
  ret
  .align 4, , 0
  ret
  .align 4, , 1
  ret
  .align 4, , 11
  ret
  .align 4, , 12
  ret
  .align 4, , -1
  ret
  .align 4, , -0x80000000
  ret
