/* General tests for CPA+SVE instructions.  */

addpt   z0.d, p0/m, z0.d, z0.d
addpt   z31.d, p0/m, z31.d, z0.d
addpt   z0.d, p7/m, z0.d, z0.d
addpt   z0.d, p0/m, z0.d, z31.d
addpt   z31.d, p7/m, z31.d, z31.d
addpt   z9.d, p4/m, z9.d, z17.d

subpt   z0.d, p0/m, z0.d, z0.d
subpt   z31.d, p0/m, z31.d, z0.d
subpt   z0.d, p7/m, z0.d, z0.d
subpt   z0.d, p0/m, z0.d, z31.d
subpt   z31.d, p7/m, z31.d, z31.d
subpt   z28.d, p3/m, z28.d, z2.d

addpt   z0.d, z0.d, z0.d
addpt   z31.d, z0.d, z0.d
addpt   z0.d, z31.d, z0.d
addpt   z0.d, z0.d, z31.d
addpt   z31.d, z31.d, z31.d
addpt   z13.d, z22.d, z9.d

subpt   z0.d, z0.d, z0.d
subpt   z31.d, z0.d, z0.d
subpt   z0.d, z31.d, z0.d
subpt   z0.d, z0.d, z31.d
subpt   z31.d, z31.d, z31.d
subpt   z30.d, z9.d, z16.d

madpt   z0.d, z0.d, z0.d
madpt   z31.d, z0.d, z0.d
madpt   z0.d, z31.d, z0.d
madpt   z0.d, z0.d, z31.d
madpt   z31.d, z31.d, z31.d
madpt   z4.d, z11.d, z26.d

mlapt   z0.d, z0.d, z0.d
mlapt   z31.d, z0.d, z0.d
mlapt   z0.d, z31.d, z0.d
mlapt   z0.d, z0.d, z31.d
mlapt   z31.d, z31.d, z31.d
mlapt   z10.d, z19.d, z6.d

/* MOVPRFX tests for CPA+SVE instructions.  */

movprfx z1, z10
addpt   z1.d, p0/m, z1.d, z2.d
movprfx z1.d, p4/m, z10.d
addpt   z1.d, p4/m, z1.d, z2.d

movprfx z1, z0
subpt   z1.d, p0/m, z1.d, z2.d
movprfx z1.d, p4/m, z10.d
subpt   z1.d, p4/m, z1.d, z2.d

movprfx z1, z0
madpt   z1.d, z2.d, z3.d
movprfx z1, z0
mlapt   z1.d, z2.d, z3.d
