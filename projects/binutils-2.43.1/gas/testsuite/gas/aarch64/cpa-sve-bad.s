/* General incorrect input tests for CPA+SVE instructions.  */

addpt   z0.b, p0/m, z0.b, z0.b
addpt   z0.d, p0/m, z1.d, z0.d
addpt   z0.d, p0/m, z0.d, z0.s
addpt   z0.h, p0/m, z0.h, z0.d
addpt   x1, p0/m, z0.d, z0.d
addpt   z0.d, x1, z0.d, z0.d
addpt   z0.d, p0/m, x1, z0.d
addpt   z0.d, p0/m, z0.d, x1

subpt   z0.b, p0/m, z0.b, z0.b
subpt   z0.d, p0/m, z1.d, z0.d
subpt   z0.d, p0/m, z0.d, z0.s
subpt   z0.h, p0/m, z0.h, z0.d
subpt   x1, p0/m, z0.d, z0.d
subpt   z0.d, x1, z0.d, z0.d
subpt   z0.d, p0/m, x1, z0.d
subpt   z0.d, p0/m, z0.d, x1

addpt   z0.b, z0.b, z0.b
addpt   z0.h, z0.h, z0.h
addpt   z0.s, z0.s, z0.s
addpt   x15, z0.d, z0.d
addpt   z0.d, x15, z0.d
addpt   z0.d, z0.d, x15

subpt   z0.b, z0.b, z0.b
subpt   z0.h, z0.h, z0.h
subpt   z0.s, z0.s, z0.s
subpt   x9, z0.d, z0.d
subpt   z0.d, x9, z0.d
subpt   z0.d, z0.d, x9

madpt   z0.d, p0/m, z0.d, z0.d
madpt   z10.b, z20.b, z30.b
madpt   z20.h, z20.h, z30.h
madpt   z20.s, z20.s, z30.s
madpt   w9, z0.d, z0.d
madpt   z0.d, w9, z0.d
madpt   z0.d, z0.d, w9

mlapt   z0.d, p3/m, z0.d, z0.d
mlapt   z10.b, z20.b, z30.b
mlapt   z20.h, z20.h, z30.h
mlapt   z20.s, z20.s, z30.s
mlapt   w7, z0.d, z0.d
mlapt   z0.d, w7, z0.d
mlapt   z0.d, z0.d, w7

/* MOVPRFX incorrect tests for CPA+SVE instructions.  */

movprfx z2, z10
addpt   z1.d, p0/m, z1.d, z2.d
movprfx z1.d, p4/m, z10.d
addpt   z1.d, p3/m, z1.d, z2.d

movprfx z2, z10
subpt   z1.d, p0/m, z1.d, z2.d
movprfx z1.d, p4/m, z10.d
subpt   z1.d, p3/m, z1.d, z2.d

movprfx z10.d, p4/m, z11.d
madpt   z10.d, z20.d, z30.d

movprfx z10.d, p4/m, z11.d
mlapt   z10.d, z20.d, z30.d
