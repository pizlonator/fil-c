extq z0.b, z0.b, z0.b, #0
extq z31.b, z31.b, z0.b, #0
extq z0.b, z0.b, z31.b, #0
extq z0.b, z0.b, z0.b, #15
extq z31.b, z31.b, z31.b, #15
extq z15.b, z15.b, z31.b, #7

movprfx z3, z5
extq z3.b, z3.b, z0.b, #0

movprfx z31, z10
extq z31.b, z31.b, z0.b, #15
