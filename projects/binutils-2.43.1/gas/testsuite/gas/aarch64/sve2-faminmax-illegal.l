[^ :]+: Assembler messages:
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5\.h,p0/m,z5\.h,z7\.s'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.h, p0/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.s, p0/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famax z5\.d, p0/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5\.s,p2/m,z5\.s,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    	famax z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5\.h,p0/m,z5\.h,z7\.s'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.h, p0/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.s, p0/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famin z5\.d, p0/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5\.s,p2/m,z5\.s,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    	famin z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand 3 must be the same register as operand 1 -- `famax z5\.h,p0/m,z7\.h,z9\.h'
[^ :]+:[0-9]+: Error: operand 3 must be the same register as operand 1 -- `famin z5\.h,p0/m,z7\.h,z9\.h'
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5\.b,p2/m,z5\.b,z5\.b'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.h, p2/m, z5\.h, z5\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.s, p2/m, z5\.s, z5\.s
[^ :]+:[0-9]+: Info:    	famax z5\.d, p2/m, z5\.d, z5\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5\.b,p2/m,z5\.b,z5\.b'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.h, p2/m, z5\.h, z5\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.s, p2/m, z5\.s, z5\.s
[^ :]+:[0-9]+: Info:    	famin z5\.d, p2/m, z5\.d, z5\.d
[^ :]+:[0-9]+: Error: comma expected between operands at operand 2 -- `famax z5\.h'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `famax z5\.h,p2/m'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 4 -- `famax z5\.h,p2/m,z5\.h'
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 4 -- `famax z5\.h,p2/m,z5\.h,z6\.h,z6\.h'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 2 -- `famin z5\.h'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `famin z5\.h,p2/m'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 4 -- `famin z5\.h,p2/m,z5\.h'
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 4 -- `famin z5\.h,p2/m,z5\.h,z6\.h,z6\.h'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 1 -- `famax z5\.t,p2/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 3 -- `famax z5\.s,p2/m,z5\.t,z7\.s'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 4 -- `famax z5\.s,p2/m,z5\.s,z7\.t'
[^ :]+:[0-9]+: Error: expected an SVE predicate register at operand 2 -- `famax z5\.s,j2/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 1 -- `famin z5\.t,p2/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 3 -- `famin z5\.s,p2/m,z5\.t,z7\.s'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 4 -- `famin z5\.s,p2/m,z5\.s,z7\.t'
[^ :]+:[0-9]+: Error: expected an SVE predicate register at operand 2 -- `famin z5\.s,j2/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `famax 5\.h,p2/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `famax z5\.h,p2/m,5\.h,z7\.h'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 4 -- `famax z5\.h,p2/m,z5\.h,7\.h'
[^ :]+:[0-9]+: Error: expected an SVE predicate register at operand 2 -- `famax z5\.h,2/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5,p2/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famax z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5\.h,p2/m,z5,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famax z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5\.h,p2/m,z5\.h,z7'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famax z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famax z5\.h,p2,z5\.h,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famax z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famax z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famax z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `famin 5\.h,p2/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `famin z5\.h,p2/m,5\.h,z7\.h'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 4 -- `famin z5\.h,p2/m,z5\.h,7\.h'
[^ :]+:[0-9]+: Error: expected an SVE predicate register at operand 2 -- `famin z5\.h,2/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5,p2/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famin z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5\.h,p2/m,z5,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famin z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5\.h,p2/m,z5\.h,z7'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famin z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: operand mismatch -- `famin z5\.h,p2,z5\.h,z7\.h'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	famin z5\.h, p2/m, z5\.h, z7\.h
[^ :]+:[0-9]+: Info:    other valid variant\(s\):
[^ :]+:[0-9]+: Info:    	famin z5\.s, p2/m, z5\.s, z7\.s
[^ :]+:[0-9]+: Info:    	famin z5\.d, p2/m, z5\.d, z7\.d
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `famax z35\.s,p2/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `famax z5\.s,p2/m,z35\.s,z7\.s'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 4 -- `famax z5\.s,p2/m,z5\.s,z35\.s'
[^ :]+:[0-9]+: Error: p0-p7 expected at operand 2 -- `famax z5\.s,p9/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `famin z35\.s,p2/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `famin z5\.s,p2/m,z35\.s,z7\.s'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 4 -- `famin z5\.s,p2/m,z5\.s,z35\.s'
[^ :]+:[0-9]+: Error: p0-p7 expected at operand 2 -- `famin z5\.s,p9/m,z5\.s,z7\.s'
[^ :]+:[0-9]+: Warning: output register of preceding `movprfx' not used in current instruction at operand 1 -- `famax z5\.h,p7/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Warning: predicate register differs from that in preceding `movprfx' at operand 2 -- `famax z5\.d,p7/m,z5\.d,z9\.d'
[^ :]+:[0-9]+: Warning: output register of preceding `movprfx' not used in current instruction at operand 1 -- `famin z5\.h,p7/m,z5\.h,z7\.h'
[^ :]+:[0-9]+: Warning: predicate register differs from that in preceding `movprfx' at operand 2 -- `famin z5\.d,p7/m,z5\.d,z9\.d'
