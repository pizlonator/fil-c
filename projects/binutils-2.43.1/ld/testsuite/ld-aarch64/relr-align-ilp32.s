// Test DT_RELR with differently aligned relative relocs.

.text
.global _start
_start:
foo:

.data
.p2align 3
double_0:
.word foo
.word foo
.byte 0
double_1:
.word foo
.word foo
.byte 0
double_2:
.word foo
.word foo
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
single_0:
.word foo
.byte 0
single_1:
.word foo
.byte 0
single_2:
.word foo
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
.byte 0
big:
.word foo
.word 1
.word 2
.word 3
.word 4
.word 5
.word 6
.word 7
.word 8
.word 9
.word 10
.word 11
.word 12
.word 13
.word 14
.word 15
.word 16
.word 17
.word 18
.word 19
.word 20
.word 21
.word 22
.word 23
.word 24
.word 25
.word 26
.word 27
.word 28
.word 29
.word 30
.word 31
.word foo + 32
.word 33
.word 34
.word 35
.word 36
.word 37
.word 38
.word 39
.word 40
.word 41
.word 42
.word 43
.word 44
.word 45
.word 46
.word 47
.word 48
.word 49
.word 50
.word 51
.word 52
.word 53
.word 54
.word 55
.word 56
.word 57
.word 58
.word 59
.word 60
.word 61
.word 62
.word foo + 63
.word foo + 64
