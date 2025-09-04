# line 11 test the first two entries of .got.plt in relro region
# relro segment size is .dynamic size + .got size + 0x10
# line 13 test .got .got.plt before .got
# line 15 test .got in relro segment
#as:
#ld: -shared -z relro --hash-style=both
#readelf: -l --wide
#skip: loongarch32-*-*

#...
  GNU_RELRO      0x003c10 0x0000000000007c10 0x0000000000007c10 0x0003f0 0x0003f0 R   0x1
#...
   01     .dynamic .got .got.plt .data 
#...
   03     .dynamic .got 
#pass
