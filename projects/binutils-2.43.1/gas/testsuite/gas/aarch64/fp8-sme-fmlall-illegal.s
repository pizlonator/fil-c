fmlall za.s[w8, 0:3], z0.b, z16.b[0]
fmlall za.s[w8, 0:3, VGx2], z0.b, z0.b[0]
fmlall za.s[w8, 0:3], z0.b, z0.b[16]
fmlall za.s[w8, 0], z0.b, z0.b[0]
fmlall za.s[w8, 2:5], z0.b, z0.b[0]
fmlall za.s[w8, 16:19], z0.b, z0.b[0]
fmlall za.s[w8, 0:1], z0.b, z0.b[0]
fmlall za.s[w7, 0:3], z0.b, z0.b[0]
fmlall za.s[w12, 0:3], z0.b, z0.b[0]

fmlall za.s[w8, 0:3], { z0.b-z1.b }, z16.b[0]
fmlall za.s[w8, 0:3], { z1.b-z2.b }, z0.b[0]
fmlall za.s[w8, 0:3, VGx2], { z0.b-z3.b }, z0.b[0]
fmlall za.s[w8, 0:3], { z0.b-z1.b }, z0.b[16]
fmlall za.s[w8, 0], { z0.b-z1.b }, z0.b[0]
fmlall za.s[w8, 2:5], { z0.b-z1.b }, z0.b[0]
fmlall za.s[w8, 8:11], { z0.b-z1.b }, z0.b[0]
fmlall za.s[w8, 0:1], { z0.b-z1.b }, z0.b[0]
fmlall za.s[w7, 0:3], { z0.b-z1.b }, z0.b[0]
fmlall za.s[w12, 0:3], { z0.b-z1.b }, z0.b[0]

fmlall za.s[w8, 0:3], { z0.b-z3.b }, z16.b[0]
fmlall za.s[w8, 0:3], { z2.b-z5.b }, z0.b[0]
fmlall za.s[w8, 0:3, VGx4], { z0.b-z1.b }, z0.b[0]
fmlall za.s[w8, 0:3], { z0.b-z3.b }, z0.b[16]
fmlall za.s[w8, 0], { z0.b-z3.b }, z0.b[0]
fmlall za.s[w8, 2:5], { z0.b-z3.b }, z0.b[0]
fmlall za.s[w8, 8:11], { z0.b-z3.b }, z0.b[0]
fmlall za.s[w8, 0:1], { z0.b-z3.b }, z0.b[0]
fmlall za.s[w7, 0], { z0.b-z3.b }, z0.b[0]
fmlall za.s[w12, 0], { z0.b-z3.b }, z0.b[0]

fmlall za.s[w8, 0:3], z0.b, z16.b
fmlall za.s[w8, 0], z0.b, z0.b
fmlall za.s[w8, 2:5], z0.b, z0.b
fmlall za.s[w8, 16:19], z0.b, z0.b
fmlall za.s[w8, 0:1], z0.b, z0.b
fmlall za.s[w7, 0], z0.b, z0.b
fmlall za.s[w12, 0], z0.b, z0.b
fmlall za.s[w8, 0, VGx4], z0.b, z0.b

fmlall za.s[w8, 0:3], { z0.b-z1.b }, z16.b
fmlall za.s[w8, 0], { z0.b-z1.b }, z0.b
fmlall za.s[w8, 2:5], { z0.b-z1.b }, z0.b
fmlall za.s[w8, 8:11], { z0.b-z1.b }, z0.b
fmlall za.s[w8, 0:1], { z0.b-z1.b }, z0.b
fmlall za.s[w7, 0:3], { z0.b-z1.b }, z0.b
fmlall za.s[w12, 0:3], { z0.b-z1.b }, z0.b
fmlall za.s[w8, 0:3, VGx4], { z0.b-z1.b }, z0.b

fmlall za.s[w8, 0:3], { z0.b-z3.b }, z16.b
fmlall za.s[w8, 0], { z0.b-z3.b }, z0.b
fmlall za.s[w8, 2:5], { z0.b-z3.b }, z0.b
fmlall za.s[w8, 8:11], { z0.b-z3.b }, z0.b
fmlall za.s[w8, 0:1], { z0.b-z3.b }, z0.b
fmlall za.s[w7, 0:3], { z0.b-z3.b }, z0.b
fmlall za.s[w12, 0:3], { z0.b-z3.b }, z0.b
fmlall za.s[w8, 0:3, VGx2], { z0.b-z3.b }, z0.b

fmlall za.s[w8, 0:3], { z0.b-z1.b }, { z0.b-z3.b}
fmlall za.s[w8, 0:3], { z0.b-z1.b }, { z1.b-z2.b}
fmlall za.s[w8, 0:3], { z1.b-z2.b }, { z0.b-z1.b}
fmlall za.s[w7, 0:3], { z0.b-z1.b }, { z0.b-z1.b}
fmlall za.s[w12, 0:3], { z0.b-z1.b }, { z0.b-z1.b}
fmlall za.s[w8, 0], { z0.b-z1.b }, { z0.b-z1.b}
fmlall za.s[w8, 2:5], { z0.b-z1.b }, { z0.b-z1.b}
fmlall za.s[w8, 8:11], { z0.b-z1.b }, { z0.b-z1.b}
fmlall za.s[w8, 0:1], { z0.b-z1.b }, { z0.b-z1.b}

fmlall za.s[w8, 0:3], { z0.b-z3.b }, { z0.b-z1.b}
fmlall za.s[w8, 0:3], { z0.b-z3.b }, { z2.b-z5.b}
fmlall za.s[w8, 0:3], { z2.b-z5.b }, { z0.b-z3.b}
fmlall za.s[w7, 0:3], { z0.b-z3.b }, { z0.b-z3.b}
fmlall za.s[w12, 0:3], { z0.b-z3.b }, { z0.b-z3.b}
fmlall za.s[w8, 0], { z0.b-z3.b }, { z0.b-z3.b}
fmlall za.s[w8, 2:5], { z0.b-z3.b }, { z0.b-z3.b}
fmlall za.s[w8, 8:11], { z0.b-z3.b }, { z0.b-z3.b}
fmlall za.s[w8, 0:1], { z0.b-z3.b }, { z0.b-z3.b}
