.*: Assembler messages:
.*: Error: operand mismatch -- `movaz {z0.s-z1.s},za.d\[w8,0,vgx2\]'
.*: Info:    did you mean this\?
.*: Info:    	movaz {z0.d-z1.d}, za.d\[w8, 0, vgx2\]
.*: Error: operand mismatch -- `movaz {z30.h-z31.h},za.d\[w8,0,vgx2\]'
.*: Info:    did you mean this\?
.*: Info:    	movaz {z30.d-z31.d}, za.d\[w8, 0, vgx2\]
.*: Error: operand mismatch -- `movaz {z0.b-z1.b},za.b\[w11,0,vgx2\]'
.*: Info:    did you mean this\?
.*: Info:    	movaz {z0.d-z1.d}, za.d\[w11, 0, vgx2\]
.*: Error: expected a selection register in the range w8-w11 at operand 2 -- `movaz {z0.d-z1.d},za.d\[w13,7,vgx2\]'
.*: Error: immediate offset out of range 0 to 7 at operand 2 -- `movaz {z30.d-z31.d},za.d\[w11,15,vgx2\]'
.*: Error: invalid vector group size at operand 2 -- `movaz {z14.d-z15.d},za.d\[w9,4,vgx3\]'
.*: Error: missing immediate offset at operand 2 -- `movaz {z6.d-z7.d},za.d\[w10\]'
.*: Error: missing immediate offset at operand 2 -- `movaz {z2.d-z4.d},za.d\[w10 6\]'
.*: Error: operand mismatch -- `movaz {z0.s-z3.s},za.d\[w8,0,vgx4\]'
.*: Info:    did you mean this\?
.*: Info:    	movaz {z0.d-z3.d}, za.d\[w8, 0, vgx4\]
.*: Error: operand mismatch -- `movaz {z28.h-z31.h},za.d\[w8,0,vgx4\]'
.*: Info:    did you mean this\?
.*: Info:    	movaz {z28.d-z31.d}, za.d\[w8, 0, vgx4\]
.*: Error: operand mismatch -- `movaz {z0.b-z3.b},za.b\[w11,0,vgx4\]'
.*: Info:    did you mean this\?
.*: Info:    	movaz {z0.d-z3.d}, za.d\[w11, 0, vgx4\]
.*: Error: expected a selection register in the range w8-w11 at operand 2 -- `movaz {z0.d-z3.d},za.d\[w14,7,vgx4\]'
.*: Error: invalid vector group size at operand 2 -- `movaz {z28.d-z31.d},za.d\[w11,11,vgx3\]'
.*: Error: start register out of range at operand 1 -- `movaz {z14.d-z17.d},za.d\[w9,4,vgx4\]'
.*: Error: too many registers in vector register list at operand 1 -- `movaz {z4.d-z8.d},za.d\[w10,3,vgx4\]'
.*: Error: missing immediate offset at operand 2 -- `movaz {z0.d,z3.d},za.d\[w10\]'
.*: Error: the register list must have a stride of 1 at operand 1 -- `movaz {z1.d,z4.d},za.d\[w10,20\]'
