// Test DT_RELR with differently aligned relative relocs.

.text
.global _start
_start:
foo:

.data
.p2align 3
double_0:
.xword foo
.xword foo
.byte 0
double_1:
.xword foo
.xword foo
.byte 0
double_2:
.xword foo
.xword foo
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
single_0:
.xword foo
.byte 0
single_1:
.xword foo
.byte 0
single_2:
.xword foo
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
big:
.xword foo
.xword 1
.xword 2
.xword 3
.xword 4
.xword 5
.xword 6
.xword 7
.xword 8
.xword 9
.xword 10
.xword 11
.xword 12
.xword 13
.xword 14
.xword 15
.xword 16
.xword 17
.xword 18
.xword 19
.xword 20
.xword 21
.xword 22
.xword 23
.xword 24
.xword 25
.xword 26
.xword 27
.xword 28
.xword 29
.xword 30
.xword 31
.xword foo + 32
.xword 33
.xword 34
.xword 35
.xword 36
.xword 37
.xword 38
.xword 39
.xword 40
.xword 41
.xword 42
.xword 43
.xword 44
.xword 45
.xword 46
.xword 47
.xword 48
.xword 49
.xword 50
.xword 51
.xword 52
.xword 53
.xword 54
.xword 55
.xword 56
.xword 57
.xword 58
.xword 59
.xword 60
.xword 61
.xword 62
.xword foo + 63
.xword foo + 64
