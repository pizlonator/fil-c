# Test DT_RELR with DT_TEXTREL and R_LARCH_ALIGN.

.text
.p2align 5
.global _start
_start:
.global foo
.hidden foo
foo:
.quad foo
