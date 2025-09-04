#name: Negative test for CPA instructions ((M)ADDPT and (M)SUBPT).
#as: -march=armv8-a
#as: -march=armv9-a
#source: cpa-addsub.s
#error_output: cpa-addsub-neg.l
