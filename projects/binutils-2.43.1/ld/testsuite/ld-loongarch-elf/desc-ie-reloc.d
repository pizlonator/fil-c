#source: desc-ie.s
#as:
#ld: -shared -z norelro --hash-style=both
#readelf: -Wr

#failif
#...
.* +R_LARCH_NONE +.*
#...
