#source: undefweak-1a.s
#source: undefweak-1b.s
#as:
#ld: -r
#readelf: -sW

#failif
#...
.* bar
#pass
