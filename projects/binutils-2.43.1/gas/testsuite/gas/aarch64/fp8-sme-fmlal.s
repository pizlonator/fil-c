fmlal za.h[w8, 0:1], z0.b, z0.b[0]
fmlal za.h[w11, 0:1], z0.b, z0.b[0]
fmlal za.h[w8, 14:15], z0.b, z0.b[0]
fmlal za.h[w8, 0:1], z31.b, z0.b[0]
fmlal za.h[w8, 0:1], z0.b, z15.b[0]
fmlal za.h[w8, 0:1], z0.b, z0.b[3]
fmlal za.h[w8, 0:1], z0.b, z0.b[15]

fmlal za.h[w8, 0:1], {z0.b-z1.b }, z0.b[0]
fmlal za.h[w11, 0:1, VGx2], { z0.b-z1.b}, z0.b[0]
fmlal za.h[w8, 6:7], {z0.b - z1.b}, z0.b[0]
fmlal za.h[w8, 0:1], { z30.b-z31.b}, z0.b[0]
fmlal za.h[w8, 0:1], { z0.b - z1.b}, z15.b[0]
fmlal za.h[w8, 0:1], { z0.b - z1.b }, z0.b[1]
fmlal za.h[w8, 0:1], { z0.b - z1.b }, z0.b[7]
fmlal za.h[w8, 0:1], { z0.b - z1.b }, z0.b[15]

fmlal za.h[w8, 0:1], {z0.b-z3.b }, z0.b[0]
fmlal za.h[w11, 0:1, VGx4], { z0.b-z3.b}, z0.b[0]
fmlal za.h[w8, 6:7], {z0.b - z3.b}, z0.b[0]
fmlal za.h[w8, 0:1], { z28.b-z31.b}, z0.b[0]
fmlal za.h[w8, 0:1], { z0.b - z3.b}, z15.b[0]
fmlal za.h[w8, 0:1], { z0.b - z3.b }, z0.b[1]
fmlal za.h[w8, 0:1], { z0.b - z3.b }, z0.b[7]
fmlal za.h[w8, 0:1], { z0.b - z3.b }, z0.b[15]

fmlal za.h[w8, 0:1], z0.b, z0.b
fmlal za.h[w11, 0:1], z0.b, z0.b
fmlal za.h[w8, 14:15], z0.b, z0.b
fmlal za.h[w8, 0:1], z31.b, z0.b
fmlal za.h[w8, 0:1], z0.b, z15.b

fmlal za.h[w8, 0:1], {z0.b -z1.b}, z0.b
fmlal za.h[w11, 0:1], {z0.b-z1.b}, z0.b
fmlal za.h[w8, 6:7], { z0.b - z1.b }, z0.b
fmlal za.h[w8, 0:1, VGx2], {z31.b - z0.b}, z0.b
fmlal za.h[w8, 0:1], {z0.b - z1.b}, z15.b

fmlal za.h[w8, 0:1], {z0.b -z3.b}, z0.b
fmlal za.h[w11, 0:1], {z0.b-z3.b}, z0.b
fmlal za.h[w8, 6:7], { z0.b - z3.b }, z0.b
fmlal za.h[w8, 0:1, VGx4], {z31.b - z2.b}, z0.b
fmlal za.h[w8, 0:1], {z0.b - z3.b}, z15.b

fmlal za.h[w8, 0:1], {z0.b -z1.b}, {z0.b-z1.b}
fmlal za.h[w11, 0:1], {z0.b-z1.b}, {z0.b - z1.b}
fmlal za.h[w8, 6:7], { z0.b - z1.b }, {z0.b -z1.b}
fmlal za.h[w8, 0:1, VGx2], {z30.b - z31.b}, {z0.b-z1.b}
fmlal za.h[w8, 0:1], {z0.b - z1.b}, {z30.b -z31.b}

fmlal za.h[w8, 0:1], {z0.b -z3.b}, {z0.b-z3.b}
fmlal za.h[w11, 0:1], {z0.b-z3.b}, {z0.b - z3.b}
fmlal za.h[w8, 6:7], { z0.b - z3.b }, {z0.b-z3.b}
fmlal za.h[w8, 0:1, VGx4], {z28.b - z31.b}, {z0.b-z3.b}
fmlal za.h[w8, 0:1], {z0.b - z3.b}, {z28.b-z31.b}

