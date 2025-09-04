#as: -march=armv8.5-a+fp8+sme2
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
[ ]*[0-9a-f]+:	c166e060 	bf1cvt	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c166e080 	bf1cvt	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c166e082 	bf1cvt	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c166e3fc 	bf1cvt	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c1e6e060 	bf2cvt	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c1e6e080 	bf2cvt	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c1e6e082 	bf2cvt	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c1e6e3fc 	bf2cvt	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c166e061 	bf1cvtl	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c166e081 	bf1cvtl	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c166e083 	bf1cvtl	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c166e3fd 	bf1cvtl	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c1e6e061 	bf2cvtl	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c1e6e081 	bf2cvtl	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c1e6e083 	bf2cvtl	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c1e6e3fd 	bf2cvtl	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c126e060 	f1cvt	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c126e080 	f1cvt	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c126e082 	f1cvt	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c126e3fc 	f1cvt	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c1a6e060 	f2cvt	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c1a6e080 	f2cvt	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c1a6e082 	f2cvt	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c1a6e3fc 	f2cvt	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c126e061 	f1cvtl	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c126e081 	f1cvtl	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c126e083 	f1cvtl	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c126e3fd 	f1cvtl	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c1a6e061 	f2cvtl	{z0.h-z1.h}, z3.b
[ ]*[0-9a-f]+:	c1a6e081 	f2cvtl	{z0.h-z1.h}, z4.b
[ ]*[0-9a-f]+:	c1a6e083 	f2cvtl	{z2.h-z3.h}, z4.b
[ ]*[0-9a-f]+:	c1a6e3fd 	f2cvtl	{z28.h-z29.h}, z31.b
[ ]*[0-9a-f]+:	c164e003 	bfcvt	z3.b, {z0.h-z1.h}
[ ]*[0-9a-f]+:	c164e004 	bfcvt	z4.b, {z0.h-z1.h}
[ ]*[0-9a-f]+:	c164e044 	bfcvt	z4.b, {z2.h-z3.h}
[ ]*[0-9a-f]+:	c164e39f 	bfcvt	z31.b, {z28.h-z29.h}
[ ]*[0-9a-f]+:	c124e003 	fcvt	z3.b, {z0.h-z1.h}
[ ]*[0-9a-f]+:	c124e004 	fcvt	z4.b, {z0.h-z1.h}
[ ]*[0-9a-f]+:	c124e044 	fcvt	z4.b, {z2.h-z3.h}
[ ]*[0-9a-f]+:	c124e39f 	fcvt	z31.b, {z28.h-z29.h}
[ ]*[0-9a-f]+:	c134e027 	fcvtn	z7.b, {z0.s-z3.s}
[ ]*[0-9a-f]+:	c134e028 	fcvtn	z8.b, {z0.s-z3.s}
[ ]*[0-9a-f]+:	c134e0a1 	fcvtn	z1.b, {z4.s-z7.s}
[ ]*[0-9a-f]+:	c134e0a2 	fcvtn	z2.b, {z4.s-z7.s}
[ ]*[0-9a-f]+:	c134e33f 	fcvtn	z31.b, {z24.s-z27.s}
[ ]*[0-9a-f]+:	c134e007 	fcvt	z7.b, {z0.s-z3.s}
[ ]*[0-9a-f]+:	c134e008 	fcvt	z8.b, {z0.s-z3.s}
[ ]*[0-9a-f]+:	c134e081 	fcvt	z1.b, {z4.s-z7.s}
[ ]*[0-9a-f]+:	c134e082 	fcvt	z2.b, {z4.s-z7.s}
[ ]*[0-9a-f]+:	c134e31f 	fcvt	z31.b, {z24.s-z27.s}
[ ]*[0-9a-f]+:	c162a180 	fscale	{z0.h-z1.h}, {z0.h-z1.h}, z2.h
[ ]*[0-9a-f]+:	c162a182 	fscale	{z2.h-z3.h}, {z2.h-z3.h}, z2.h
[ ]*[0-9a-f]+:	c164a180 	fscale	{z0.h-z1.h}, {z0.h-z1.h}, z4.h
[ ]*[0-9a-f]+:	c164a182 	fscale	{z2.h-z3.h}, {z2.h-z3.h}, z4.h
[ ]*[0-9a-f]+:	c16fa19e 	fscale	{z30.h-z31.h}, {z30.h-z31.h}, z15.h
[ ]*[0-9a-f]+:	c164a980 	fscale	{z0.h-z3.h}, {z0.h-z3.h}, z4.h
[ ]*[0-9a-f]+:	c164a984 	fscale	{z4.h-z7.h}, {z4.h-z7.h}, z4.h
[ ]*[0-9a-f]+:	c168a980 	fscale	{z0.h-z3.h}, {z0.h-z3.h}, z8.h
[ ]*[0-9a-f]+:	c168a984 	fscale	{z4.h-z7.h}, {z4.h-z7.h}, z8.h
[ ]*[0-9a-f]+:	c16fa99c 	fscale	{z28.h-z31.h}, {z28.h-z31.h}, z15.h
[ ]*[0-9a-f]+:	c1a2a180 	fscale	{z0.s-z1.s}, {z0.s-z1.s}, z2.s
[ ]*[0-9a-f]+:	c1a2a182 	fscale	{z2.s-z3.s}, {z2.s-z3.s}, z2.s
[ ]*[0-9a-f]+:	c1a4a180 	fscale	{z0.s-z1.s}, {z0.s-z1.s}, z4.s
[ ]*[0-9a-f]+:	c1a4a182 	fscale	{z2.s-z3.s}, {z2.s-z3.s}, z4.s
[ ]*[0-9a-f]+:	c1afa19e 	fscale	{z30.s-z31.s}, {z30.s-z31.s}, z15.s
[ ]*[0-9a-f]+:	c1a4a980 	fscale	{z0.s-z3.s}, {z0.s-z3.s}, z4.s
[ ]*[0-9a-f]+:	c1a4a984 	fscale	{z4.s-z7.s}, {z4.s-z7.s}, z4.s
[ ]*[0-9a-f]+:	c1a8a980 	fscale	{z0.s-z3.s}, {z0.s-z3.s}, z8.s
[ ]*[0-9a-f]+:	c1a8a984 	fscale	{z4.s-z7.s}, {z4.s-z7.s}, z8.s
[ ]*[0-9a-f]+:	c1afa99c 	fscale	{z28.s-z31.s}, {z28.s-z31.s}, z15.s
[ ]*[0-9a-f]+:	c1e2a180 	fscale	{z0.d-z1.d}, {z0.d-z1.d}, z2.d
[ ]*[0-9a-f]+:	c1e2a182 	fscale	{z2.d-z3.d}, {z2.d-z3.d}, z2.d
[ ]*[0-9a-f]+:	c1e4a180 	fscale	{z0.d-z1.d}, {z0.d-z1.d}, z4.d
[ ]*[0-9a-f]+:	c1e4a182 	fscale	{z2.d-z3.d}, {z2.d-z3.d}, z4.d
[ ]*[0-9a-f]+:	c1efa19e 	fscale	{z30.d-z31.d}, {z30.d-z31.d}, z15.d
[ ]*[0-9a-f]+:	c1e4a980 	fscale	{z0.d-z3.d}, {z0.d-z3.d}, z4.d
[ ]*[0-9a-f]+:	c1e4a984 	fscale	{z4.d-z7.d}, {z4.d-z7.d}, z4.d
[ ]*[0-9a-f]+:	c1e8a980 	fscale	{z0.d-z3.d}, {z0.d-z3.d}, z8.d
[ ]*[0-9a-f]+:	c1e8a984 	fscale	{z4.d-z7.d}, {z4.d-z7.d}, z8.d
[ ]*[0-9a-f]+:	c1efa99c 	fscale	{z28.d-z31.d}, {z28.d-z31.d}, z15.d
[ ]*[0-9a-f]+:	c162b180 	fscale	{z0.h-z1.h}, {z0.h-z1.h}, {z2.h-z3.h}
[ ]*[0-9a-f]+:	c162b182 	fscale	{z2.h-z3.h}, {z2.h-z3.h}, {z2.h-z3.h}
[ ]*[0-9a-f]+:	c164b180 	fscale	{z0.h-z1.h}, {z0.h-z1.h}, {z4.h-z5.h}
[ ]*[0-9a-f]+:	c164b182 	fscale	{z2.h-z3.h}, {z2.h-z3.h}, {z4.h-z5.h}
[ ]*[0-9a-f]+:	c17eb19e 	fscale	{z30.h-z31.h}, {z30.h-z31.h}, {z30.h-z31.h}
[ ]*[0-9a-f]+:	c164b980 	fscale	{z0.h-z3.h}, {z0.h-z3.h}, {z4.h-z7.h}
[ ]*[0-9a-f]+:	c164b984 	fscale	{z4.h-z7.h}, {z4.h-z7.h}, {z4.h-z7.h}
[ ]*[0-9a-f]+:	c168b980 	fscale	{z0.h-z3.h}, {z0.h-z3.h}, {z8.h-z11.h}
[ ]*[0-9a-f]+:	c168b984 	fscale	{z4.h-z7.h}, {z4.h-z7.h}, {z8.h-z11.h}
[ ]*[0-9a-f]+:	c178b99c 	fscale	{z28.h-z31.h}, {z28.h-z31.h}, {z24.h-z27.h}
[ ]*[0-9a-f]+:	c1a2b180 	fscale	{z0.s-z1.s}, {z0.s-z1.s}, {z2.s-z3.s}
[ ]*[0-9a-f]+:	c1a2b182 	fscale	{z2.s-z3.s}, {z2.s-z3.s}, {z2.s-z3.s}
[ ]*[0-9a-f]+:	c1a4b180 	fscale	{z0.s-z1.s}, {z0.s-z1.s}, {z4.s-z5.s}
[ ]*[0-9a-f]+:	c1a4b182 	fscale	{z2.s-z3.s}, {z2.s-z3.s}, {z4.s-z5.s}
[ ]*[0-9a-f]+:	c1beb19e 	fscale	{z30.s-z31.s}, {z30.s-z31.s}, {z30.s-z31.s}
[ ]*[0-9a-f]+:	c1a4b980 	fscale	{z0.s-z3.s}, {z0.s-z3.s}, {z4.s-z7.s}
[ ]*[0-9a-f]+:	c1a4b984 	fscale	{z4.s-z7.s}, {z4.s-z7.s}, {z4.s-z7.s}
[ ]*[0-9a-f]+:	c1a8b980 	fscale	{z0.s-z3.s}, {z0.s-z3.s}, {z8.s-z11.s}
[ ]*[0-9a-f]+:	c1a8b984 	fscale	{z4.s-z7.s}, {z4.s-z7.s}, {z8.s-z11.s}
[ ]*[0-9a-f]+:	c1b8b99c 	fscale	{z28.s-z31.s}, {z28.s-z31.s}, {z24.s-z27.s}
[ ]*[0-9a-f]+:	c1e2b180 	fscale	{z0.d-z1.d}, {z0.d-z1.d}, {z2.d-z3.d}
[ ]*[0-9a-f]+:	c1e2b182 	fscale	{z2.d-z3.d}, {z2.d-z3.d}, {z2.d-z3.d}
[ ]*[0-9a-f]+:	c1e4b180 	fscale	{z0.d-z1.d}, {z0.d-z1.d}, {z4.d-z5.d}
[ ]*[0-9a-f]+:	c1e4b182 	fscale	{z2.d-z3.d}, {z2.d-z3.d}, {z4.d-z5.d}
[ ]*[0-9a-f]+:	c1feb19e 	fscale	{z30.d-z31.d}, {z30.d-z31.d}, {z30.d-z31.d}
[ ]*[0-9a-f]+:	c1e4b980 	fscale	{z0.d-z3.d}, {z0.d-z3.d}, {z4.d-z7.d}
[ ]*[0-9a-f]+:	c1e4b984 	fscale	{z4.d-z7.d}, {z4.d-z7.d}, {z4.d-z7.d}
[ ]*[0-9a-f]+:	c1e8b980 	fscale	{z0.d-z3.d}, {z0.d-z3.d}, {z8.d-z11.d}
[ ]*[0-9a-f]+:	c1e8b984 	fscale	{z4.d-z7.d}, {z4.d-z7.d}, {z8.d-z11.d}
[ ]*[0-9a-f]+:	c1f8b99c 	fscale	{z28.d-z31.d}, {z28.d-z31.d}, {z24.d-z27.d}