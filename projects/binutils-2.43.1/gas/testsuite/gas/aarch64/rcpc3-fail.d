#name: RCPC3 GPR load/store illegal
#source: rcpc3-fail.s
#as: -march=armv8.3-a+rcpc3 -mno-verbose-error
#error_output: rcpc3-fail.l
