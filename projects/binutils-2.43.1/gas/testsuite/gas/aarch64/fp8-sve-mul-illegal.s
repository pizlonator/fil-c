fdot z0.s, z0.b, z8.b[0]
fdot z0.s, z0.b, z0.b[4]

fdot z0.h, z0.b, z8.b[0]
fdot z0.h, z0.b, z0.b[8]

fmlalb z0.h, z0.b, z8.b[0]
fmlalb z0.h, z0.b, z0.b[16]

fmlallbb z0.s, z0.b, z8.b[0]
fmlallbb z0.s, z0.b, z0.b[16]

fmlallbt z0.s, z0.b, z8.b[0]
fmlallbt z0.s, z0.b, z0.b[16]

fmlalltb z0.s, z0.b, z8.b[0]
fmlalltb z0.s, z0.b, z0.b[16]

fmlalltt z0.s, z0.b, z8.b[0]
fmlalltt z0.s, z0.b, z0.b[16]

fmlalt z0.h, z0.b, z8.b[0]
fmlalt z0.h, z0.b, z0.b[16]

movprfx z25.s, p0/m, z31.s
fdot z25.s, z0.b, z0.b[0]

movprfx z25.h, p0/z, z31.h
fdot z25.h, z0.b, z0.b[0]

movprfx z25.h, p0/z, z31.h
fmlalb z25.h, z0.b, z0.b[0]

movprfx z25.s, p0/m, z31.s
fmlallbb z25.s, z0.b, z0.b[0]
