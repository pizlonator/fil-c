# Use the section name symbol for R_LARCH_ALIGN to avoid discard section problem
.section ".another.text", "ax"
.cfi_startproc
break 0
.cfi_def_cfa_offset 16
.p2align 5
break 1
.cfi_endproc

.text
.cfi_startproc
break 0
.cfi_def_cfa_offset 16
.p2align 5
break 1
.cfi_endproc

