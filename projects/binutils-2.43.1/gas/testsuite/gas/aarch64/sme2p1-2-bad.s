/* LUTI2 (two registers) strided.  */
luti2	{ z0.b , z7.b }, zt0, z0[0]
LUTI2	{ Z0.s , Z8.s }, ZT0, Z0[0]
luti2	{ z7.d , z15.d }, zt0, z0[0]
luti2	{ z16.b , z24.b }, zt0, z0
luti2	{ z23.b , z31.b }, zt0, z0[8]
luti2	{ z0.b , z8.b }, zt0, z31[15]
luti2	{ z0.b , z8.b }, z0[7]
luti2	{ z8.b , z24.b }, zt0, z31[0]
luti2	{ z24.b , z24.b }, zt0, z0[7]
luti2	{ z4.h , z16.h }, zt0, z20[4]
luti2	{ z20.h , z22.h }, zt0, z12[2]

/* LUTI2 (four registers) strided.  */
luti2	{ z0.b , z3.b , z18.b , z1.b }, zt0, z0[0]
LUTI2	{ Z0.s , Z4.s, Z8.s , Z12.s }, ZT0, Z0[0]
luti2	{ z3.d , z7.d, z11.d, z15.d }, zt0, z0[0]
luti2	{ z16.h , z20.h , z24.h , z28.h }, zt0, z0
luti2	{ z19.h , z23.h , z27.h , z31.h }, zt0, z0[5]
luti2	{ z10.b , z14.b , z18.b , z22.b }, zt0, z31[0]
luti2	{ z20.b , z24.b , z28.b , z30.b }, z0[3]
luti2	{ z4.b , z7.b , z11.b , z18.b }, zt0, z31[0]
luti2	{ z6.b , z0.s , z2.d , z28.h }, zt0, z0[7]

/* LUTI4 (two registers) strided.  */
luti4	{ z0.b , z7.b }, zt0, z0[0]
LUTI2	{ Z0.s , Z8.s }, ZT0, Z0[0]
luti4	{ z7.d , z15.d }, zt0, z0[0]
luti4	{ z16.b , z24.b }, zt0, z0
luti4	{ z23.b , z31.b }, zt0, z0[8]
luti4	{ z0.b , z8.b }, zt0, z31[15]
luti4	{ z0.b , z8.b }, z0[7]
luti4	{ z8.b , z24.b }, zt0, z31[0]
luti4	{ z24.b , z24.b }, zt0, z0[7]
luti4	{ z4.h , z16.h }, zt0, z20[4]
luti4	{ z20.h , z22.h }, zt0, z12[2]

/* LUTI4 (four registers) strided.  */
luti4	{ z0.s , z4.s , z8.s , z12.s }, zt0, z0[0]
luti4	{ z0.b , z3.b , z18.b , z1.b }, zt0, z0[0]
LUTI2	{ Z0.s , Z4.s, Z8.s , Z12.s }, ZT0, Z0[0]
luti4	{ z3.d , z7.d, z11.d, z15.d }, zt0, z0[0]
luti4	{ z16.h , z20.h , z24.h , z28.h }, zt0, z0
luti4	{ z19.h , z23.h , z27.h , z31.h }, zt0, z0[5]
luti4	{ z10.b , z14.b , z18.b , z22.b }, zt0, z31[0]
luti4	{ z20.b , z24.b , z28.b , z30.b }, z0[3]
luti4	{ z4.b , z7.b , z11.b , z18.b }, zt0, z31[0]
luti4	{ z6.b , z0.s , z2.d , z28.h }, zt0, z0[7]
