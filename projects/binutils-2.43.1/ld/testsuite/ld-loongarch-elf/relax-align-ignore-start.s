# If .align at the start of a section, do not add NOP instructions
# and do not emit R_LARCH_ALIGN relocations.
# Section alignment can ensure correct alignment.
.section "aaa", "ax"
ret

.section "bbb", "ax"
.align 3
ret

.section "ccc", "ax"
.align 4, ,4
ret
