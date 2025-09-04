#name: Test Bfloat16 instructions with wrong operand combinations
#as: -march=armv9.4-a+b16b16
#source: bfloat16-2-invalid.s
#error_output: bfloat16-2-invalid.l
#xfail: *-*-*
