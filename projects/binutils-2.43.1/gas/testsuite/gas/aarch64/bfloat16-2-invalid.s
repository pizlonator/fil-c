bfadd z0.s, p0/m, z0.h, z16.h
bfadd z0.h, p0/z, z0.h, z16.h
bfadd z0.h, p8/m, z0.h, z16.h
bfadd z31.h, p0/m, z0.h, z16.h
bfadd z0.h, p0/z, z0.s, z16.h
bfadd z0.h, p0/z, z0.h, z16.d
bfadd z31.d, p7/m, z31.d, z31.d

bfmax z0.s, p0/m, z0.h, z16.h
bfmax z0.h, p0/z, z0.h, z16.h
bfmax z0.h, p8/m, z0.h, z16.h
bfmax z31.h, p0/m, z0.h, z16.h
bfmax z0.h, p0/z, z0.s, z16.h
bfmax z0.h, p0/z, z0.h, z16.d
bfmax z31.d, p7/m, z31.d, z31.d

bfmaxnm z0.s, p0/m, z0.h, z16.h
bfmaxnm z0.h, p0/z, z0.h, z16.h
bfmaxnm z0.h, p8/m, z0.h, z16.h
bfmaxnm z31.h, p0/m, z0.h, z16.h
bfmaxnm z0.h, p0/z, z0.s, z16.h
bfmaxnm z0.h, p0/z, z0.h, z16.d
bfmaxnm z31.d, p7/m, z31.d, z31.d

bfmin z0.s, p0/m, z0.h, z16.h
bfmin z0.h, p0/z, z0.h, z16.h
bfmin z0.h, p8/m, z0.h, z16.h
bfmin z31.h, p0/m, z0.h, z16.h
bfmin z0.h, p0/z, z0.s, z16.h
bfmin z0.h, p0/z, z0.h, z16.d
bfmin z31.d, p7/m, z31.d, z31.d

bfminnm z0.s, p0/m, z0.h, z16.h
bfminnm z0.h, p0/z, z0.h, z16.h
bfminnm z0.h, p8/m, z0.h, z16.h
bfminnm z31.h, p0/m, z0.h, z16.h
bfminnm z0.h, p0/z, z0.s, z16.h
bfminnm z0.h, p0/z, z0.h, z16.d
bfminnm z31.d, p7/m, z31.d, z31.d

bfmla z0.s, p0/m, z0.h, z16.h
bfmla z0.h, p0/z, z0.h, z16.h
bfmla z0.h, p8/m, z0.h, z16.h
bfmla z31.h, p0/m, z0.h, z16.h
bfmla z0.h, p0/z, z0.s, z16.h
bfmla z0.h, p0/z, z0.h, z16.d
bfmla z31.d, p7/m, z31.d, z31.d

bfmls z0.s, p0/m, z0.h, z16.h
bfmls z0.h, p0/z, z0.h, z16.h
bfmls z0.h, p8/m, z0.h, z16.h
bfmls z31.h, p0/m, z0.h, z16.h
bfmls z0.h, p0/z, z0.s, z16.h
bfmls z0.h, p0/z, z0.h, z16.d
bfmls z31.d, p7/m, z31.d, z31.d

bfmul z0.s, p0/m, z0.h, z16.h
bfmul z0.h, p0/z, z0.h, z16.h
bfmul z0.h, p8/m, z0.h, z16.h
bfmul z31.h, p0/m, z0.h, z16.h
bfmul z0.h, p0/z, z0.s, z16.h
bfmul z0.h, p0/z, z0.h, z16.d
bfmul z31.d, p7/m, z31.d, z31.d

bfsub z0.s, p0/m, z0.h, z16.h
bfsub z0.h, p0/z, z0.h, z16.h
bfsub z0.h, p8/m, z0.h, z16.h
bfsub z31.h, p0/m, z0.h, z16.h
bfsub z0.h, p0/z, z0.s, z16.h
bfsub z0.h, p0/z, z0.h, z16.d
bfsub z31.d, p7/m, z31.d, z31.d

bfadd z0.b, z0.h, z0.h
bfadd z31.h, z0.h, z0.h
bfadd z0.s, z0.h, z0.h
bfadd z0.h, z0.d, z0.h
bfadd z0.h, z0.h, z0.b
bfadd z31.b, z31.s, z31.d
bfadd {z0.h}, z0.h, z0.h
bfadd {z0.h - z0.h}, z0.h
bfadd z0.h, z0.h

bfclamp z0.b, z0.h, z0.h
bfclamp z31.h, z0.h, z0.h
bfclamp z0.s, z0.h, z0.h
bfclamp z0.h, z0.d, z0.h
bfclamp z0.h, z0.h, z0.b
bfclamp z31.b, z31.s, z31.d
bfclamp {z0.h}, z0.h, z0.h
bfclamp {z0.h - z0.h}, z0.h
bfclamp z0.h, z0.h

bfmla z0.b, z0.h, z0.h[0]
bfmla z31.h, z0.h, z0.h[3]
bfmla z0.s, z0.h, z0.h[6]
bfmla z0.h, z0.d, z0.h[8]
bfmla z0.h, z0.h, z0.b[2]
bfmla z31.b, z31.s, z31.d[8]
bfmla {z0.h}, z0.h, z0.h[1]
bfmla {z0.h - z0.h}, z0.h[2]
bfmla z0.h, z0.h[3]

bfmls z0.b, z0.h, z0.h[0]
bfmls z31.h, z0.h, z0.h[3]
bfmls z0.s, z0.h, z0.h[6]
bfmls z0.h, z0.d, z0.h[8]
bfmls z0.h, z0.h, z0.b[2]
bfmls z31.b, z31.s, z31.d[8]
bfmls {z0.h}, z0.h, z0.h[1]
bfmls {z0.h - z0.h}, z0.h[2]
bfmls z0.h, z0.h[3]

bfmul z0.b, z0.h, z0.h[0]
bfmul z31.h, z0.h, z0.h[3]
bfmul z0.s, z0.h, z0.h[6]
bfmul z0.h, z0.d, z0.h[8]
bfmul z0.h, z0.h, z0.b[2]
bfmul z31.b, z31.s, z31.d[8]
bfmul {z0.h}, z0.h, z0.h[1]
bfmul {z0.h - z0.h}, z0.h[2]
bfmul z0.h, z0.h[3]

bfsub z0.b, z0.h, z0.h
bfsub z31.h, z0.h, z0.h
bfsub z0.s, z0.h, z0.h
bfsub z0.h, z0.d, z0.h
bfsub z0.h, z0.h, z0.b
bfsub z31.b, z31.s, z31.d
bfsub {z0.h}, z0.h, z0.h
bfsub {z0.h - z0.h}, z0.h
bfsub z0.h, z0.h

bfmla z0.h, p0/m, z4.h, z16.h
movprfx z3, z5
bfclamp z1.h, z3.h, z16.h

movprfx z3, z5
bfmla z10.h, z16.h, z3.h[7]

movprfx z3, z5
bfmls z1.h, z3.h, z3.h[7]

movprfx z2, z3
movprfx z4, z5
bfclamp z2.h, z3.h, z4.h
bfmla z4.h, z5.h, z6.h[7]
bfmls z3.h, z1.h, z4.h[7]
