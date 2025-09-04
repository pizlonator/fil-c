#name: Illegal test of SVE2.1 instructions.
#as: -march=armv9.4-a+nosve2
#source: sve2p1-nosve2.s
#error_output: sve2p1-2-bad.l
