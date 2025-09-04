# Test DT_RELR with differently aligned relative relocs.

.text
.global _start
_start:
foo:

.data
.p2align 3
double_0:
.quad foo
.quad foo
.byte 0
double_1:
.quad foo
.quad foo
.byte 0
double_2:
.quad foo
.quad foo
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
single_0:
.quad foo
.byte 0
single_1:
.quad foo
.byte 0
single_2:
.quad foo
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
big:
.quad foo
.quad 1
.quad 2
.quad 3
.quad 4
.quad 5
.quad 6
.quad 7
.quad 8
.quad 9
.quad 10
.quad 11
.quad 12
.quad 13
.quad 14
.quad 15
.quad 16
.quad 17
.quad 18
.quad 19
.quad 20
.quad 21
.quad 22
.quad 23
.quad 24
.quad 25
.quad 26
.quad 27
.quad 28
.quad 29
.quad 30
.quad 31
.quad foo + 32
.quad 33
.quad 34
.quad 35
.quad 36
.quad 37
.quad 38
.quad 39
.quad 40
.quad 41
.quad 42
.quad 43
.quad 44
.quad 45
.quad 46
.quad 47
.quad 48
.quad 49
.quad 50
.quad 51
.quad 52
.quad 53
.quad 54
.quad 55
.quad 56
.quad 57
.quad 58
.quad 59
.quad 60
.quad 61
.quad 62
.quad foo + 63
.quad foo + 64
