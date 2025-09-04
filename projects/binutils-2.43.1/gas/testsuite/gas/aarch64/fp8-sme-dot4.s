fdot za.s[w8, 0], { z0.b-z1.b }, z0.b[0]
fdot za.s[w8, 0], { z0.b - z1.b }, z15.b[0]
fdot za.s[w8, 0], { z30.b-z31.b }, z0.b[0]
fdot za.s[w8, 0, VGx2 ], { z0.b-z1.b }, z0.b[3]
fdot za.s[w8, 7], { z0.b-z1.b }, z0.b[0]
fdot za.s[w11, 0], { z0.b-z1.b }, z0.b[0]

fdot za.s[w8, 0], { z0.b-z3.b }, z0.b[0]
fdot za.s[w8, 0, VGx4], { z0.b - z3.b }, z15.b[0]
fdot za.s[w8, 0], { z28.b-z31.b }, z0.b[0]
fdot za.s[w8, 0], { z0.b-z3.b }, z0.b[3]
fdot za.s[w8, 7], { z0.b-z3.b }, z0.b[0]
fdot za.s[w11, 0], { z0.b-z3.b }, z0.b[0]

fdot za.s[w8, 0], { z0.b-z1.b }, z0.b
fdot za.s[w8, 0], { z0.b - z1.b }, z15.b
fdot za.s[w8, 0, VGx2], { z31.b-z0.b }, z0.b
fdot za.s[w8, 7], { z0.b-z1.b }, z0.b
fdot za.s[w11, 0], { z0.b-z1.b }, z0.b

fdot za.s[w8, 0], { z0.b-z3.b }, z0.b
fdot za.s[w8, 0, VGx4], { z0.b - z3.b }, z15.b
fdot za.s[w8, 0], { z31.b-z2.b }, z0.b
fdot za.s[w8, 7], { z0.b-z3.b }, z0.b
fdot za.s[w11, 0], { z0.b-z3.b }, z0.b

fdot za.s[w8, 0], { z0.b-z1.b }, {z0.b-z1.b }
fdot za.s[w8, 0], { z0.b - z1.b }, {z30.b - z31.b}
fdot za.s[w8, 0], { z30.b-z31.b }, {z0.b-z1.b}
fdot za.s[w8, 7], { z0.b-z1.b },{ z0.b-z1.b}
fdot za.s[w11, 0, VGx2], { z0.b-z1.b }, {z0.b-z1.b}

fdot za.s[w8, 0], { z0.b-z3.b }, {z0.b-z3.b }
fdot za.s[w8, 0], { z0.b - z3.b }, {z28.b - z31.b}
fdot za.s[w8, 0], { z28.b-z31.b }, {z0.b-z3.b}
fdot za.s[w8, 7], { z0.b-z3.b }, {z0.b-z3.b}
fdot za.s[w11, 0, VGx4], { z0.b-z3.b }, {z0.b-z3.b}

fvdotb za.s[w8, 0, VGx4], { z0.b-z1.b }, z0.b[0]
fvdotb za.s[w11, 0, VGx4], { z0.b-z1.b }, z0.b[0]
fvdotb za.s[w8, 7, VGx4], { z0.b-z1.b }, z0.b[0]
fvdotb za.s[w8, 0, VGx4], { z30.b-z31.b }, z0.b[0]
fvdotb za.s[w8, 0, VGx4], { z0.b-z1.b }, z15.b[0]
fvdotb za.s[w8, 0, VGx4], { z0.b-z1.b }, z0.b[1]
fvdotb za.s[w8, 0, VGx4], { z0.b-z1.b }, z0.b[3]

fvdott za.s[w8, 0, VGx4], { z0.b-z1.b }, z0.b[0]
fvdott za.s[w11, 0, VGx4], { z0.b-z1.b }, z0.b[0]
fvdott za.s[w8, 7, VGx4], { z0.b-z1.b }, z0.b[0]
fvdott za.s[w8, 0, VGx4], { z30.b-z31.b }, z0.b[0]
fvdott za.s[w8, 0, VGx4], { z0.b-z1.b }, z15.b[0]
fvdott za.s[w8, 0, VGx4], { z0.b-z1.b }, z0.b[1]
fvdott za.s[w8, 0, VGx4], { z0.b-z1.b }, z0.b[3]
