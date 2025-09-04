# ld -z separate-code let .text and .rodata in two segment,
# need to consider segment alignment
.text
  # first two la.local relax in the second trip, and result in the third
  # la.local (relax to pcaddi) overflow
  la.local $r12, a
  la.local $r12, b
  .fill 0x3ff8
  # relax in the first trip
  la.local $r12, c
  .fill 0x1fbfec
a:
  .fill 8
b:
  .fill 0x1000
d:
  .fill 0x1

.section .rodata
c:
  .8byte 0x1
