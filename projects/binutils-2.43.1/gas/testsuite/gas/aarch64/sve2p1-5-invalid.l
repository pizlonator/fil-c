.*: Assembler messages:
.*: Error: operand mismatch -- `orqv v0.8h,p0,z0.b'
.*: Info:    did you mean this\?
.*: Info:    	orqv v0.16b, p0, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	orqv v0.8h, p0, z0.h
.*: Info:    	orqv v0.4s, p0, z0.s
.*: Info:    	orqv v0.2d, p0, z0.d
.*: Error: p0-p7 expected at operand 2 -- `orqv v31.16b,p8,z0.b'
.*: Error: operand mismatch -- `orqv v0.2d,p7,z0.b'
.*: Info:    did you mean this\?
.*: Info:    	orqv v0.16b, p7, z0.b
.*: Info:    other valid variant\(s\):
.*: Info:    	orqv v0.8h, p7, z0.h
.*: Info:    	orqv v0.4s, p7, z0.s
.*: Info:    	orqv v0.2d, p7, z0.d
.*: Error: bad vector arrangement type at operand 1 -- `orqv v0.2b,p7,z0.b'
.*: Error: indexed vector register expected at operand 1 -- `orqv v0.b,p0,z0.16b'
.*: Error: unexpected character `8' in element size at operand 3 -- `orqv v0.4h,p0,z0.8h'
.*: Error: unexpected character `4' in element size at operand 3 -- `orqv v0.4s,p8/m,z0.4s'
.*: Error: operand mismatch -- `orqv v0.2d,p0/z,z0.d'
.*: Info:    did you mean this\?
.*: Info:    	orqv v0.2d, p0, z0.d
.*: Info:    other valid variant\(s\):
.*: Info:    	orqv v0.16b, p0, z0.b
.*: Info:    	orqv v0.8h, p0, z0.h
.*: Info:    	orqv v0.4s, p0, z0.s
