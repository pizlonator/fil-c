# At relax pass 0, offset is 0x120204000-0x12000bff8=0x1f8008 < 0x200000
# At relax pass 1, delete 0x7ff8 bytes NOP,
# offset is 0x120204000-0x120004000=0x200000 >= 0x200000, overflow
.text
.align 14 # delete at relax pass 1
.fill 0x4000
.align 14 # delete at relax pass 1
la.local $t2, a # relax to pcaddi at relax pass 0

.section ".text1", "ax"
  .fill 0x4000
a:	     # 0x120204000
  ret
