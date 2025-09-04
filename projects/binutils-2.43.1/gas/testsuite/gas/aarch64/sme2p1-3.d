#name: Test of SME2.1 movaz array to vector instructions.
#as: -march=armv9.4-a+sme2p1
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	c0060a00 	movaz	{z0.d-z1.d}, za.d\[w8, 0, vgx2\]
.*:	c0060a1e 	movaz	{z30.d-z31.d}, za.d\[w8, 0, vgx2\]
.*:	c0066a00 	movaz	{z0.d-z1.d}, za.d\[w11, 0, vgx2\]
.*:	c0060ae0 	movaz	{z0.d-z1.d}, za.d\[w8, 7, vgx2\]
.*:	c0066afe 	movaz	{z30.d-z31.d}, za.d\[w11, 7, vgx2\]
.*:	c0062a8e 	movaz	{z14.d-z15.d}, za.d\[w9, 4, vgx2\]
.*:	c0064a66 	movaz	{z6.d-z7.d}, za.d\[w10, 3, vgx2\]
.*:	c0064ac2 	movaz	{z2.d-z3.d}, za.d\[w10, 6, vgx2\]
.*:	c0060e00 	movaz	{z0.d-z3.d}, za.d\[w8, 0, vgx4\]
.*:	c0060e1c 	movaz	{z28.d-z31.d}, za.d\[w8, 0, vgx4\]
.*:	c0066e00 	movaz	{z0.d-z3.d}, za.d\[w11, 0, vgx4\]
.*:	c0060ee0 	movaz	{z0.d-z3.d}, za.d\[w8, 7, vgx4\]
.*:	c0066efc 	movaz	{z28.d-z31.d}, za.d\[w11, 7, vgx4\]
.*:	c0062e8c 	movaz	{z12.d-z15.d}, za.d\[w9, 4, vgx4\]
.*:	c0064e64 	movaz	{z4.d-z7.d}, za.d\[w10, 3, vgx4\]
.*:	c0064ec0 	movaz	{z0.d-z3.d}, za.d\[w10, 6, vgx4\]
