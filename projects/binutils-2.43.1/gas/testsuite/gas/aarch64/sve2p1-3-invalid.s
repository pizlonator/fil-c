extq z0.b, z0.h, z0.b, #0
extq z31.b, z15.b, z0.b, #0
extq z0.b, z0.b, z31.h, #0
extq z0.b, z0.b, z0.b, #16
extq z0.h, z0.h, z0.h, #15
movprfx z1, z5
extq z3.b, z3.b, z0.b, #0

movprfx z31, z10
extq z31.b, z2.b, z0.b, #15

movprfx z31.b, p1/m, z10.b
extq z31.b, z31.b, z0.b, #15

movprfx z0, z2
extq z0.b, z0.b, z0.b, #0
