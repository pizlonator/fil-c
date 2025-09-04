fmlal za.h[w8, 0:1], z0.b, z16.b[0]
fmlal za.h[w8, 0:1, VGx2], z0.b, z0.b[0]
fmlal za.h[w8, 0:1], z0.b, z0.b[16]
fmlal za.h[w8, 0], z0.b, z0.b[0]
fmlal za.h[w8, 1:2], z0.b, z0.b[0]
fmlal za.h[w8, 16:17], z0.b, z0.b[0]
fmlal za.h[w8, 0:3], z0.b, z0.b[0]
fmlal za.h[w7, 0:1], z0.b, z0.b[0]
fmlal za.h[w12, 0:1], z0.b, z0.b[0]

fmlal za.h[w8, 0:1], { z0.b-z1.b }, z16.b[0]
fmlal za.h[w8, 0:1], { z1.b-z2.b }, z0.b[0]
fmlal za.h[w8, 0:1, VGx2], { z0.b-z3.b }, z0.b[0]
fmlal za.h[w8, 0:1], { z0.b-z1.b }, z0.b[16]
fmlal za.h[w8, 0], { z0.b-z1.b }, z0.b[0]
fmlal za.h[w8, 1:2], { z0.b-z1.b }, z0.b[0]
fmlal za.h[w8, 8:9], { z0.b-z1.b }, z0.b[0]
fmlal za.h[w8, 0:3], { z0.b-z1.b }, z0.b[0]
fmlal za.h[w7, 0:1], { z0.b-z1.b }, z0.b[0]
fmlal za.h[w12, 0:1], { z0.b-z1.b }, z0.b[0]

fmlal za.h[w8, 0:1], { z0.b-z3.b }, z16.b[0]
fmlal za.h[w8, 0:1], { z2.b-z5.b }, z0.b[0]
fmlal za.h[w8, 0:1, VGx4], { z0.b-z1.b }, z0.b[0]
fmlal za.h[w8, 0:1], { z0.b-z3.b }, z0.b[16]
fmlal za.h[w8, 0], { z0.b-z3.b }, z0.b[0]
fmlal za.h[w8, 1:2], { z0.b-z3.b }, z0.b[0]
fmlal za.h[w8, 8:9], { z0.b-z3.b }, z0.b[0]
fmlal za.h[w8, 0:3], { z0.b-z3.b }, z0.b[0]
fmlal za.h[w7, 0], { z0.b-z3.b }, z0.b[0]
fmlal za.h[w12, 0], { z0.b-z3.b }, z0.b[0]

fmlal za.h[w8, 0:1], z0.b, z16.b
fmlal za.h[w8, 0], z0.b, z0.b
fmlal za.h[w8, 1:2], z0.b, z0.b
fmlal za.h[w8, 16:17], z0.b, z0.b
fmlal za.h[w8, 0:3], z0.b, z0.b
fmlal za.h[w7, 0], z0.b, z0.b
fmlal za.h[w12, 0], z0.b, z0.b
fmlal za.h[w8, 0, VGx4], z0.b, z0.b

fmlal za.h[w8, 0:1], { z0.b-z1.b }, z16.b
fmlal za.h[w8, 0], { z0.b-z1.b }, z0.b
fmlal za.h[w8, 1:2], { z0.b-z1.b }, z0.b
fmlal za.h[w8, 8:9], { z0.b-z1.b }, z0.b
fmlal za.h[w8, 0:3], { z0.b-z1.b }, z0.b
fmlal za.h[w7, 0:1], { z0.b-z1.b }, z0.b
fmlal za.h[w12, 0:1], { z0.b-z1.b }, z0.b
fmlal za.h[w8, 0:1, VGx4], { z0.b-z1.b }, z0.b

fmlal za.h[w8, 0:1], { z0.b-z3.b }, z16.b
fmlal za.h[w8, 0], { z0.b-z3.b }, z0.b
fmlal za.h[w8, 1:2], { z0.b-z3.b }, z0.b
fmlal za.h[w8, 8:9], { z0.b-z3.b }, z0.b
fmlal za.h[w8, 0:3], { z0.b-z3.b }, z0.b
fmlal za.h[w7, 0:1], { z0.b-z3.b }, z0.b
fmlal za.h[w12, 0:1], { z0.b-z3.b }, z0.b
fmlal za.h[w8, 0:1, VGx2], { z0.b-z3.b }, z0.b

fmlal za.h[w8, 0:1], { z0.b-z1.b }, { z0.b-z3.b}
fmlal za.h[w8, 0:1], { z0.b-z1.b }, { z1.b-z2.b}
fmlal za.h[w8, 0:1], { z1.b-z2.b }, { z0.b-z1.b}
fmlal za.h[w7, 0:1], { z0.b-z1.b }, { z0.b-z1.b}
fmlal za.h[w12, 0:1], { z0.b-z1.b }, { z0.b-z1.b}
fmlal za.h[w8, 0], { z0.b-z1.b }, { z0.b-z1.b}
fmlal za.h[w8, 1:2], { z0.b-z1.b }, { z0.b-z1.b}
fmlal za.h[w8, 8:9], { z0.b-z1.b }, { z0.b-z1.b}
fmlal za.h[w8, 0:3], { z0.b-z1.b }, { z0.b-z1.b}

fmlal za.h[w8, 0:1], { z0.b-z3.b }, { z0.b-z1.b}
fmlal za.h[w8, 0:1], { z0.b-z3.b }, { z2.b-z5.b}
fmlal za.h[w8, 0:1], { z2.b-z5.b }, { z0.b-z3.b}
fmlal za.h[w7, 0:1], { z0.b-z3.b }, { z0.b-z3.b}
fmlal za.h[w12, 0:1], { z0.b-z3.b }, { z0.b-z3.b}
fmlal za.h[w8, 0], { z0.b-z3.b }, { z0.b-z3.b}
fmlal za.h[w8, 1:2], { z0.b-z3.b }, { z0.b-z3.b}
fmlal za.h[w8, 8:9], { z0.b-z3.b }, { z0.b-z3.b}
fmlal za.h[w8, 0:3], { z0.b-z3.b }, { z0.b-z3.b}
