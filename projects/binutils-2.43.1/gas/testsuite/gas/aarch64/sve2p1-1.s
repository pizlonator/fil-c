addqv v0.16b, p0, z0.b
addqv v31.16b, p0, z0.b
addqv v0.2d, p0, z0.d
addqv v0.16b, p7, z0.b
addqv v0.16b, p0, z31.b
addqv v31.2d, p7, z31.d
addqv v10.4s, p3, z20.s

andqv v0.16b, p0, z0.b
andqv v31.16b, p0, z0.b
andqv v0.2d, p0, z0.d
andqv v0.16b, p7, z0.b
andqv v0.16b, p0, z31.b
andqv v31.2d, p7, z31.d
andqv v10.4s, p3, z20.s

smaxqv v0.16b, p0, z0.b
smaxqv v31.16b, p0, z0.b
smaxqv v0.2d, p0, z0.d
smaxqv v0.16b, p7, z0.b
smaxqv v0.16b, p0, z31.b
smaxqv v31.2d, p7, z31.d
smaxqv v10.4s, p3, z20.s

umaxqv v0.16b, p0, z0.b
umaxqv v31.16b, p0, z0.b
umaxqv v0.2d, p0, z0.d
umaxqv v0.16b, p7, z0.b
umaxqv v0.16b, p0, z31.b
umaxqv v31.2d, p7, z31.d
umaxqv v10.4s, p3, z20.s

sminqv v0.16b, p0, z0.b
sminqv v31.16b, p0, z0.b
sminqv v0.2d, p0, z0.d
sminqv v0.16b, p7, z0.b
sminqv v0.16b, p0, z31.b
sminqv v31.2d, p7, z31.d
sminqv v10.4s, p3, z20.s

dupq z10.b, z20.b[0]
dupq z10.b, z20.b[15]
dupq z10.h, z20.h[0]
dupq z10.h, z20.h[7]
dupq z10.s, z20.s[0]
dupq z10.s, z20.s[3]
dupq z10.d, z20.d[0]
dupq z10.d, z20.d[1]

uminqv v0.16b, p0, z0.b
uminqv v31.16b, p0, z0.b
uminqv v0.2d, p0, z0.d
uminqv v0.16b, p7, z0.b
uminqv v0.16b, p0, z31.b
uminqv v31.2d, p7, z31.d
uminqv v10.4s, p3, z20.s

extq z0.b, z0.b, z0.b, #0
extq z31.b, z31.b, z0.b, #0
extq z0.b, z0.b, z31.b, #0
extq z0.b, z0.b, z0.b, #15
extq z31.b, z31.b, z31.b, #15
extq z15.b, z15.b, z31.b, #7

eorqv v0.16b, p0, z0.b
eorqv v31.16b, p0, z0.b
eorqv v0.2d, p0, z0.d
eorqv v0.16b, p7, z0.b
eorqv v0.16b, p0, z31.b
eorqv v31.2d, p7, z31.d
eorqv v10.4s, p3, z20.s

faddqv v0.8h, p0, z0.h
faddqv v31.8h, p0, z0.h
faddqv v0.2d, p0, z0.d
faddqv v0.8h, p7, z0.h
faddqv v0.8h, p0, z31.h
faddqv v31.2d, p7, z31.d
faddqv v10.4s, p3, z20.s

fmaxnmqv v0.8h, p0, z0.h
fmaxnmqv v31.8h, p0, z0.h
fmaxnmqv v0.2d, p0, z0.d
fmaxnmqv v0.8h, p7, z0.h
fmaxnmqv v0.8h, p0, z31.h
fmaxnmqv v31.2d, p7, z31.d
fmaxnmqv v10.4s, p3, z20.s

fmaxqv v0.8h, p0, z0.h
fmaxqv v31.8h, p0, z0.h
fmaxqv v0.2d, p0, z0.d
fmaxqv v0.8h, p7, z0.h
fmaxqv v0.8h, p0, z31.h
fmaxqv v31.2d, p7, z31.d
fmaxqv v10.4s, p3, z20.s

fminnmqv v0.8h, p0, z0.h
fminnmqv v31.8h, p0, z0.h
fminnmqv v0.2d, p0, z0.d
fminnmqv v0.8h, p7, z0.h
fminnmqv v0.8h, p0, z31.h
fminnmqv v31.2d, p7, z31.d
fminnmqv v10.4s, p3, z20.s

fminqv v0.8h, p0, z0.h
fminqv v31.8h, p0, z0.h
fminqv v0.2d, p0, z0.d
fminqv v0.8h, p7, z0.h
fminqv v0.8h, p0, z31.h
fminqv v31.2d, p7, z31.d
fminqv v10.4s, p3, z20.s
