#name: Test of SME2.1 ZERO instructions.
#as: -march=armv9.4-a+sme2p1
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	c00c0000 	zero	za.d\[w8, 0, vgx2\]
.*:	c00c6000 	zero	za.d\[w11, 0, vgx2\]
.*:	c00c0007 	zero	za.d\[w8, 7, vgx2\]
.*:	c00c6007 	zero	za.d\[w11, 7, vgx2\]
.*:	c00c2004 	zero	za.d\[w9, 4, vgx2\]
.*:	c00c4003 	zero	za.d\[w10, 3, vgx2\]
.*:	c00e0000 	zero	za.d\[w8, 0, vgx4\]
.*:	c00e6000 	zero	za.d\[w11, 0, vgx4\]
.*:	c00e0007 	zero	za.d\[w8, 7, vgx4\]
.*:	c00e6007 	zero	za.d\[w11, 7, vgx4\]
.*:	c00e2004 	zero	za.d\[w9, 4, vgx4\]
.*:	c00e4003 	zero	za.d\[w10, 3, vgx4\]
.*:	c00c8000 	zero	za.d\[w8, 0:1\]
.*:	c00ce000 	zero	za.d\[w11, 0:1\]
.*:	c00c8007 	zero	za.d\[w8, 14:15\]
.*:	c00ce007 	zero	za.d\[w11, 14:15\]
.*:	c00ca001 	zero	za.d\[w9, 2:3\]
.*:	c00cc003 	zero	za.d\[w10, 6:7\]
.*:	c00d0000 	zero	za.d\[w8, 0:1, vgx2\]
.*:	c00d6000 	zero	za.d\[w11, 0:1, vgx2\]
.*:	c00d0003 	zero	za.d\[w8, 6:7, vgx2\]
.*:	c00d2001 	zero	za.d\[w9, 2:3, vgx2\]
.*:	c00d8000 	zero	za.d\[w8, 0:1, vgx4\]
.*:	c00de000 	zero	za.d\[w11, 0:1, vgx4\]
.*:	c00d8003 	zero	za.d\[w8, 6:7, vgx4\]
.*:	c00da001 	zero	za.d\[w9, 2:3, vgx4\]
.*:	c00e8000 	zero	za.d\[w8, 0:3\]
.*:	c00ee000 	zero	za.d\[w11, 0:3\]
.*:	c00e8002 	zero	za.d\[w8, 8:11\]
.*:	c00ee002 	zero	za.d\[w11, 8:11\]
.*:	c00ea001 	zero	za.d\[w9, 4:7\]
.*:	c00ec000 	zero	za.d\[w10, 0:3\]
.*:	c00f0000 	zero	za.d\[w8, 0:3, vgx2\]
.*:	c00f6000 	zero	za.d\[w11, 0:3, vgx2\]
.*:	c00f0001 	zero	za.d\[w8, 4:7, vgx2\]
.*:	c00f6001 	zero	za.d\[w11, 4:7, vgx2\]
.*:	c00f2000 	zero	za.d\[w9, 0:3, vgx2\]
.*:	c00f4001 	zero	za.d\[w10, 4:7, vgx2\]
.*:	c00f8000 	zero	za.d\[w8, 0:3, vgx4\]
.*:	c00fe000 	zero	za.d\[w11, 0:3, vgx4\]
.*:	c00f8001 	zero	za.d\[w8, 4:7, vgx4\]
.*:	c00fe001 	zero	za.d\[w11, 4:7, vgx4\]
.*:	c00fa000 	zero	za.d\[w9, 0:3, vgx4\]
.*:	c00fc001 	zero	za.d\[w10, 4:7, vgx4\]
