// Valid instructions
famax      z0.h, p0/m, z0.h, z0.h
famax      z0.h, p2/m, z0.h, z0.h
famax      z0.h, p7/m, z0.h, z0.h
famax      z31.h, p0/m, z31.h, z0.h
famax      z31.h, p2/m, z31.h, z0.h
famax      z31.h, p7/m, z31.h, z0.h
famax      z0.h, p0/m, z0.h, z31.h
famax      z0.h, p2/m, z0.h, z31.h
famax      z0.h, p7/m, z0.h, z31.h
famax      z5.h, p0/m, z5.h, z7.h
famax      z5.h, p2/m, z5.h, z7.h
famax      z5.h, p7/m, z5.h, z7.h

famax      z0.s, p0/m, z0.s, z0.s
famax      z0.s, p2/m, z0.s, z0.s
famax      z0.s, p7/m, z0.s, z0.s
famax      z31.s, p0/m, z31.s, z0.s
famax      z31.s, p2/m, z31.s, z0.s
famax      z31.s, p7/m, z31.s, z0.s
famax      z0.s, p0/m, z0.s, z31.s
famax      z0.s, p2/m, z0.s, z31.s
famax      z0.s, p7/m, z0.s, z31.s
famax      z5.s, p0/m, z5.s, z7.s
famax      z5.s, p2/m, z5.s, z7.s
famax      z5.s, p7/m, z5.s, z7.s

famax      z0.d, p0/m, z0.d, z0.d
famax      z0.d, p2/m, z0.d, z0.d
famax      z0.d, p7/m, z0.d, z0.d
famax      z31.d, p0/m, z31.d, z0.d
famax      z31.d, p2/m, z31.d, z0.d
famax      z31.d, p7/m, z31.d, z0.d
famax      z0.d, p0/m, z0.d, z31.d
famax      z0.d, p2/m, z0.d, z31.d
famax      z0.d, p7/m, z0.d, z31.d
famax      z5.d, p0/m, z5.d, z7.d
famax      z5.d, p2/m, z5.d, z7.d
famax      z5.d, p7/m, z5.d, z7.d

movprfx    z5, z9
famax	   z5.h, p7/m, z5.h, z7.h
movprfx	   z5.d, p7/m, z8.d
famax	   z5.d, p7/m, z5.d, z9.d

// Invalid instruction because sz is 00 which is reserved
.inst	0x650e9d25

// Valid instructions
famin      z0.h, p0/m, z0.h, z0.h
famin      z0.h, p2/m, z0.h, z0.h
famin      z0.h, p7/m, z0.h, z0.h
famin      z31.h, p0/m, z31.h, z0.h
famin      z31.h, p2/m, z31.h, z0.h
famin      z31.h, p7/m, z31.h, z0.h
famin      z0.h, p0/m, z0.h, z31.h
famin      z0.h, p2/m, z0.h, z31.h
famin      z0.h, p7/m, z0.h, z31.h
famin      z5.h, p0/m, z5.h, z7.h
famin      z5.h, p2/m, z5.h, z7.h
famin      z5.h, p7/m, z5.h, z7.h

famin      z0.s, p0/m, z0.s, z0.s
famin      z0.s, p2/m, z0.s, z0.s
famin      z0.s, p7/m, z0.s, z0.s
famin      z31.s, p0/m, z31.s, z0.s
famin      z31.s, p2/m, z31.s, z0.s
famin      z31.s, p7/m, z31.s, z0.s
famin      z0.s, p0/m, z0.s, z31.s
famin      z0.s, p2/m, z0.s, z31.s
famin      z0.s, p7/m, z0.s, z31.s
famin      z5.s, p0/m, z5.s, z7.s
famin      z5.s, p2/m, z5.s, z7.s
famin      z5.s, p7/m, z5.s, z7.s

famin      z0.d, p0/m, z0.d, z0.d
famin      z0.d, p2/m, z0.d, z0.d
famin      z0.d, p7/m, z0.d, z0.d
famin      z31.d, p0/m, z31.d, z0.d
famin      z31.d, p2/m, z31.d, z0.d
famin      z31.d, p7/m, z31.d, z0.d
famin      z0.d, p0/m, z0.d, z31.d
famin      z0.d, p2/m, z0.d, z31.d
famin      z0.d, p7/m, z0.d, z31.d
famin      z5.d, p0/m, z5.d, z7.d
famin      z5.d, p2/m, z5.d, z7.d
famin      z5.d, p7/m, z5.d, z7.d

movprfx    z5, z9
famin	   z5.h, p7/m, z5.h, z7.h
movprfx	   z5.d, p7/m, z8.d
famin	   z5.d, p7/m, z5.d, z9.d

// Invalid instruction because sz is 00 which is reserved
.inst	0x650f9d25
