#name: Test that fpmr register is not supported by default
#source: fpmr.s
#as: -march=armv9.2-a
#error_output: fpmr-unsupported-by-default.l
