/* ZERO (single-vector).  */
zero za.s[w8, 0, vgx2]
zero za.b[w14, 0, vgx2]
zero za.d[w2, 7, vgx2]
zero za.d[w11, 17, vgx2]
zero za.d[w9, 4, vgx3]
zero za.h[w10, 3]

zero za.s[w18, 0, vgx4]
zero za.b[w1, 0, vgx4]
zero za.q[w8, 17, vgx2]
zero za.h[w11, 7, vgx3]
zero za.s[w9, vg]
zero za.b[w10, 3]

/* ZERO (double-vector).  */
zero za.s[w18, 0:1]
zero za.s[w1, 0:1]
zero za.b[w8, 4:5]
zero za.b[w11, 1:5]
zero za.h[w9, 2:13]
zero za.h[w10, 16:7]

zero za.s[w18, 0:3, vgx2]
zero za.b[w1, 0:1, vgx4]
zero za.h[w8, 6:7, vg]
zero za.q[w9, 12:13, vgx3]

zero za.s[w18, 0:1, vgx4]
zero za.h[w1, 0:1, vgx3]
zero za.b[w8, 16:17, vgx4]
zero za.q[w9, 12:13]

/* ZERO (quad-vector).  */
zero za.s[w18, 0:3,]
zero za.s[w1, 0:3]
zero za.b[w8, 8:11]
zero za.b[w11, 18:1, vgx3]
zero za.h[w9, 4]
zero za.h[w10, 10:13]

zero za.s[w18, 0:3, vgx2]
zero za.s[w1, 0:3, vgx2]
zero za.b[w8, 14:17, vgx2]
zero za.b[w11, 4:7, vg]
zero za.h[w9, 0:3]
zero za.q[w10, vgx2]

zero za.s[w18, 0:3, vgx4]
zero za.s[w1, 0:3, vgx4]
zero za.b[w8, 14:17, vgx4]
zero za.b[w11, 4:7, vg]
zero za.h[w9, 0:3, vgx3]
zero za.q[w10, 4]
