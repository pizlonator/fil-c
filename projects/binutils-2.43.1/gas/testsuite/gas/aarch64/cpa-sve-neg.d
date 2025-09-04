#name: Negative tests for CPA+SVE instructions.
#as: -march=armv8-a
#as: -march=armv8-a+sve
#as: -march=armv8-a+cpa
#as: -march=armv9-a
#source: cpa-sve-neg.s
#error_output: cpa-sve-neg.l
