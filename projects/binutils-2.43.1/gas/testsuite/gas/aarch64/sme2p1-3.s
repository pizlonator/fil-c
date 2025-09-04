/* MOVAZ (array to vector, two registers).  */
movaz {z0.d - z1.d} , za.d[w8, 0, vgx2]
movaz {z30.d - z31.d} , za.d[w8, 0, vgx2]
movaz {z0.d - z1.d} , za.d[w11, 0, vgx2]
movaz {z0.d - z1.d} , za.d[w8, 7, vgx2]
movaz {z30.d - z31.d} , za.d[w11, 7, vgx2]
movaz {z14.d - z15.d} , za.d[w9, 4, vgx2]
movaz {z6.d - z7.d} , za.d[w10, 3, vgx2]
movaz {z2.d - z3.d} , za.d[w10, 6]

/* MOVAZ (array to vector, four registers).  */
movaz {z0.d - z3.d} , za.d[w8, 0, vgx4]
movaz {z28.d - z31.d} , za.d[w8, 0, vgx4]
movaz {z0.d - z3.d} , za.d[w11, 0, vgx4]
movaz {z0.d - z3.d} , za.d[w8, 7, vgx4]
movaz {z28.d - z31.d} , za.d[w11, 7, vgx4]
movaz {z12.d - z15.d} , za.d[w9, 4, vgx4]
movaz {z4.d - z7.d} , za.d[w10, 3, vgx4]
movaz {z0.d - z3.d} , za.d[w10, 6]
