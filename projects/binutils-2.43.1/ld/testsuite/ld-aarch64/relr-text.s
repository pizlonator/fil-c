// Test DT_RELR with DT_TEXTREL.

.text
.p2align 3
.global _start
_start:
.global foo
.hidden foo
foo:
.xword foo
