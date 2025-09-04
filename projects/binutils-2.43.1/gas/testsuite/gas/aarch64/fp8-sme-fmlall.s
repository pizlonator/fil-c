fmlall za.s[w8, 0:3], z0.b, z0.b[0]
fmlall za.s[w11, 0:3], z0.b, z0.b[0]
fmlall za.s[w8, 12:15], z0.b, z0.b[0]
fmlall za.s[w8, 0:3], z31.b, z0.b[0]
fmlall za.s[w8, 0:3], z0.b, z15.b[0]
fmlall za.s[w8, 0:3], z0.b, z0.b[3]
fmlall za.s[w8, 0:3], z0.b, z0.b[15]

fmlall za.s[w8, 0:3], {z0.b-z1.b }, z0.b[0]
fmlall za.s[w11, 0:3, VGx2], { z0.b-z1.b}, z0.b[0]
fmlall za.s[w8, 4:7], {z0.b - z1.b}, z0.b[0]
fmlall za.s[w8, 0:3], { z30.b-z31.b}, z0.b[0]
fmlall za.s[w8, 0:3], { z0.b - z1.b}, z15.b[0]
fmlall za.s[w8, 0:3], { z0.b - z1.b }, z0.b[1]
fmlall za.s[w8, 0:3], { z0.b - z1.b }, z0.b[7]
fmlall za.s[w8, 0:3], { z0.b - z1.b }, z0.b[15]

fmlall za.s[w8, 0:3], {z0.b-z3.b }, z0.b[0]
fmlall za.s[w11, 0:3, VGx4], { z0.b-z3.b}, z0.b[0]
fmlall za.s[w8, 4:7], {z0.b - z3.b}, z0.b[0]
fmlall za.s[w8, 0:3], { z28.b-z31.b}, z0.b[0]
fmlall za.s[w8, 0:3], { z0.b - z3.b}, z15.b[0]
fmlall za.s[w8, 0:3], { z0.b - z3.b }, z0.b[1]
fmlall za.s[w8, 0:3], { z0.b - z3.b }, z0.b[7]
fmlall za.s[w8, 0:3], { z0.b - z3.b }, z0.b[15]

fmlall za.s[w8, 0:3], z0.b, z0.b
fmlall za.s[w11, 0:3], z0.b, z0.b
fmlall za.s[w8, 12:15], z0.b, z0.b
fmlall za.s[w8, 0:3], z31.b, z0.b
fmlall za.s[w8, 0:3], z0.b, z15.b

fmlall za.s[w8, 0:3], {z0.b -z1.b}, z0.b
fmlall za.s[w11, 0:3], {z0.b-z1.b}, z0.b
fmlall za.s[w8, 4:7], { z0.b - z1.b }, z0.b
fmlall za.s[w8, 0:3, VGx2], {z31.b - z0.b}, z0.b
fmlall za.s[w8, 0:3], {z0.b - z1.b}, z15.b

fmlall za.s[w8, 0:3], {z0.b -z3.b}, z0.b
fmlall za.s[w11, 0:3], {z0.b-z3.b}, z0.b
fmlall za.s[w8, 4:7], { z0.b - z3.b }, z0.b
fmlall za.s[w8, 0:3, VGx4], {z31.b - z2.b}, z0.b
fmlall za.s[w8, 0:3], {z0.b - z3.b}, z15.b

fmlall za.s[w8, 0:3], {z0.b -z1.b}, {z0.b-z1.b}
fmlall za.s[w11, 0:3], {z0.b-z1.b}, {z0.b - z1.b}
fmlall za.s[w8, 4:7], { z0.b - z1.b }, {z0.b -z1.b}
fmlall za.s[w8, 0:3, VGx2], {z30.b - z31.b}, {z0.b-z1.b}
fmlall za.s[w8, 0:3], {z0.b - z1.b}, {z30.b -z31.b}

fmlall za.s[w8, 0:3], {z0.b -z3.b}, {z0.b-z3.b}
fmlall za.s[w11, 0:3], {z0.b-z3.b}, {z0.b - z3.b}
fmlall za.s[w8, 4:7], { z0.b - z3.b }, {z0.b-z3.b}
fmlall za.s[w8, 0:3, VGx4], {z28.b - z31.b}, {z0.b-z3.b}
fmlall za.s[w8, 0:3], {z0.b - z3.b}, {z28.b-z31.b}

