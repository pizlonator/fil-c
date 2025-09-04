#objdump: -dr
#as: -march=armv9.5-a+sve2
#as: -march=armv8-a+faminmax+sve2

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
   0:	654e8000 	famax	z0.h, p0/m, z0.h, z0.h
   4:	654e8800 	famax	z0.h, p2/m, z0.h, z0.h
   8:	654e9c00 	famax	z0.h, p7/m, z0.h, z0.h
   c:	654e801f 	famax	z31.h, p0/m, z31.h, z0.h
  10:	654e881f 	famax	z31.h, p2/m, z31.h, z0.h
  14:	654e9c1f 	famax	z31.h, p7/m, z31.h, z0.h
  18:	654e83e0 	famax	z0.h, p0/m, z0.h, z31.h
  1c:	654e8be0 	famax	z0.h, p2/m, z0.h, z31.h
  20:	654e9fe0 	famax	z0.h, p7/m, z0.h, z31.h
  24:	654e80e5 	famax	z5.h, p0/m, z5.h, z7.h
  28:	654e88e5 	famax	z5.h, p2/m, z5.h, z7.h
  2c:	654e9ce5 	famax	z5.h, p7/m, z5.h, z7.h
  30:	658e8000 	famax	z0.s, p0/m, z0.s, z0.s
  34:	658e8800 	famax	z0.s, p2/m, z0.s, z0.s
  38:	658e9c00 	famax	z0.s, p7/m, z0.s, z0.s
  3c:	658e801f 	famax	z31.s, p0/m, z31.s, z0.s
  40:	658e881f 	famax	z31.s, p2/m, z31.s, z0.s
  44:	658e9c1f 	famax	z31.s, p7/m, z31.s, z0.s
  48:	658e83e0 	famax	z0.s, p0/m, z0.s, z31.s
  4c:	658e8be0 	famax	z0.s, p2/m, z0.s, z31.s
  50:	658e9fe0 	famax	z0.s, p7/m, z0.s, z31.s
  54:	658e80e5 	famax	z5.s, p0/m, z5.s, z7.s
  58:	658e88e5 	famax	z5.s, p2/m, z5.s, z7.s
  5c:	658e9ce5 	famax	z5.s, p7/m, z5.s, z7.s
  60:	65ce8000 	famax	z0.d, p0/m, z0.d, z0.d
  64:	65ce8800 	famax	z0.d, p2/m, z0.d, z0.d
  68:	65ce9c00 	famax	z0.d, p7/m, z0.d, z0.d
  6c:	65ce801f 	famax	z31.d, p0/m, z31.d, z0.d
  70:	65ce881f 	famax	z31.d, p2/m, z31.d, z0.d
  74:	65ce9c1f 	famax	z31.d, p7/m, z31.d, z0.d
  78:	65ce83e0 	famax	z0.d, p0/m, z0.d, z31.d
  7c:	65ce8be0 	famax	z0.d, p2/m, z0.d, z31.d
  80:	65ce9fe0 	famax	z0.d, p7/m, z0.d, z31.d
  84:	65ce80e5 	famax	z5.d, p0/m, z5.d, z7.d
  88:	65ce88e5 	famax	z5.d, p2/m, z5.d, z7.d
  8c:	65ce9ce5 	famax	z5.d, p7/m, z5.d, z7.d
  90:	0420bd25 	movprfx	z5, z9
  94:	654e9ce5 	famax	z5.h, p7/m, z5.h, z7.h
  98:	04d13d05 	movprfx	z5.d, p7/m, z8.d
  9c:	65ce9d25 	famax	z5.d, p7/m, z5.d, z9.d
  a0:	650e9d25 	.inst	0x650e9d25 ; undefined
  a4:	654f8000 	famin	z0.h, p0/m, z0.h, z0.h
  a8:	654f8800 	famin	z0.h, p2/m, z0.h, z0.h
  ac:	654f9c00 	famin	z0.h, p7/m, z0.h, z0.h
  b0:	654f801f 	famin	z31.h, p0/m, z31.h, z0.h
  b4:	654f881f 	famin	z31.h, p2/m, z31.h, z0.h
  b8:	654f9c1f 	famin	z31.h, p7/m, z31.h, z0.h
  bc:	654f83e0 	famin	z0.h, p0/m, z0.h, z31.h
  c0:	654f8be0 	famin	z0.h, p2/m, z0.h, z31.h
  c4:	654f9fe0 	famin	z0.h, p7/m, z0.h, z31.h
  c8:	654f80e5 	famin	z5.h, p0/m, z5.h, z7.h
  cc:	654f88e5 	famin	z5.h, p2/m, z5.h, z7.h
  d0:	654f9ce5 	famin	z5.h, p7/m, z5.h, z7.h
  d4:	658f8000 	famin	z0.s, p0/m, z0.s, z0.s
  d8:	658f8800 	famin	z0.s, p2/m, z0.s, z0.s
  dc:	658f9c00 	famin	z0.s, p7/m, z0.s, z0.s
  e0:	658f801f 	famin	z31.s, p0/m, z31.s, z0.s
  e4:	658f881f 	famin	z31.s, p2/m, z31.s, z0.s
  e8:	658f9c1f 	famin	z31.s, p7/m, z31.s, z0.s
  ec:	658f83e0 	famin	z0.s, p0/m, z0.s, z31.s
  f0:	658f8be0 	famin	z0.s, p2/m, z0.s, z31.s
  f4:	658f9fe0 	famin	z0.s, p7/m, z0.s, z31.s
  f8:	658f80e5 	famin	z5.s, p0/m, z5.s, z7.s
  fc:	658f88e5 	famin	z5.s, p2/m, z5.s, z7.s
 100:	658f9ce5 	famin	z5.s, p7/m, z5.s, z7.s
 104:	65cf8000 	famin	z0.d, p0/m, z0.d, z0.d
 108:	65cf8800 	famin	z0.d, p2/m, z0.d, z0.d
 10c:	65cf9c00 	famin	z0.d, p7/m, z0.d, z0.d
 110:	65cf801f 	famin	z31.d, p0/m, z31.d, z0.d
 114:	65cf881f 	famin	z31.d, p2/m, z31.d, z0.d
 118:	65cf9c1f 	famin	z31.d, p7/m, z31.d, z0.d
 11c:	65cf83e0 	famin	z0.d, p0/m, z0.d, z31.d
 120:	65cf8be0 	famin	z0.d, p2/m, z0.d, z31.d
 124:	65cf9fe0 	famin	z0.d, p7/m, z0.d, z31.d
 128:	65cf80e5 	famin	z5.d, p0/m, z5.d, z7.d
 12c:	65cf88e5 	famin	z5.d, p2/m, z5.d, z7.d
 130:	65cf9ce5 	famin	z5.d, p7/m, z5.d, z7.d
 134:	0420bd25 	movprfx	z5, z9
 138:	654f9ce5 	famin	z5.h, p7/m, z5.h, z7.h
 13c:	04d13d05 	movprfx	z5.d, p7/m, z8.d
 140:	65cf9d25 	famin	z5.d, p7/m, z5.d, z9.d
 144:	650f9d25 	.inst	0x650f9d25 ; undefined
