[^ :]+: Assembler messages:
[^ :]+:[0-9]+: Error: operand mismatch -- `luti2 z2\.b,\{z5\.h\},z7\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti2 z2\.b, \{z5\.b\}, z7\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti2 z2\.h,\{z5\.b\},z7\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti2 z2\.b, \{z5\.b\}, z7\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z2\.b,\{z5\.h\},z7\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z2\.b, \{z5\.b\}, z7\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z2\.h,\{z5\.b\},z7\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z2\.b, \{z5\.b\}, z7\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z2\.b,\{z5\.h,z6\.h\},z12\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z2\.b, \{z5\.b-z6\.b\}, z12\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z2\.h,\{z5\.b,z6\.b\},z12\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z2\.b, \{z5\.b-z6\.b\}, z12\[1\]
[^ :]+:[0-9]+: Error: type mismatch in vector register list at operand 2 -- `luti4 z2\.b,\{z5\.b,z6\.h\},z12\[1\]'
[^ :]+:[0-9]+: Error: type mismatch in vector register list at operand 2 -- `luti4 z2\.h,\{z5\.h,z6\.b\},z12\[1\]'
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti2 z5\.b,\{\},z7\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected register type at operand 2 -- `luti2 \{z5\.b\},z6\.b,z7\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a single-register list at operand 2 -- `luti2 z5\.b,\{z7\.b,z8\.b\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti2 z5\.h,\{\},z7\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected register type at operand 2 -- `luti2 \{z5\.h\},z6\.h,z7\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a single-register list at operand 2 -- `luti2 z5\.h,\{z7\.h,z8\.h\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti4 z5\.b,\{\},z7\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected register type at operand 2 -- `luti4 \{z5\.b\},z6\.b,z7\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a single-register list at operand 2 -- `luti4 z5\.b,\{z7\.b,z8\.b\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti4 z5\.h,\{\},z7\[1\]'
[^ :]+:[0-9]+: Error: unexpected register type at operand 2 -- `luti4 \{z5\.h\},z6\.h,z7\[1\]'
[^ :]+:[0-9]+: Error: the register list must have a stride of 1 at operand 2 -- `luti4 z5\.h,\{z7\.h,z9\.h\},z3\[3\]'
[^ :]+:[0-9]+: Error: operand mismatch -- `luti2 z5\.s,\{z7\.s\},z9\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti2 z5\.b, \{z7\.b\}, z9\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z5\.s,\{z7\.s\},z9\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z5\.b, \{z7\.b\}, z9\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z5\.s,\{z7\.s,z8\.s\},z9\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z5\.b, \{z7\.b-z8\.b\}, z9\[1\]
[^ :]+:[0-9]+: Error: comma expected between operands at operand 2 -- `luti2 z5\.b'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `luti2 z5\.b,\{z7\.b\}'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 3 -- `luti2 z5\.b,\{z7\.b\},z9\[1\],z11\.b'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: comma expected between operands at operand 2 -- `luti2 z5\.h'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `luti2 z5\.h,\{z7\.h\}'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 3 -- `luti2 z5\.h,\{z7\.h\},z9\[1\],z11\.h'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: comma expected between operands at operand 2 -- `luti4 z5\.b'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `luti4 z5\.b,\{z7\.b\}'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 3 -- `luti4 z5\.b,\{z7\.b\},z9\[1\],z11\.b'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: comma expected between operands at operand 2 -- `luti4 z5\.h'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `luti4 z5\.h,\{z7\.h\}'
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 3 -- `luti4 z5\.h,\{z7\.h\},z9\[1\],z11\.h'
[^ :]+:[0-9]+: Error: comma expected between operands at operand 3 -- `luti4 z5\.h,\{z7\.h,z8\.h\}'
[^ :]+:[0-9]+: Error: unexpected characters following instruction at operand 3 -- `luti4 z5\.h,\{z7\.h,z8\.h\},z9\[1\],z11\.h'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 1 -- `luti2 z5\.t,\{z7\.b},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 2 -- `luti2 z5\.b,\{z7\.t},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti2 z5\.b,\{z7\.b},z9\.b'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 1 -- `luti2 z5\.t,\{z7\.h},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 2 -- `luti2 z5\.h,\{z7\.t},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti2 z5\.h,\{z7\.h},z9\.h'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 1 -- `luti4 z5\.t,\{z7\.b},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 2 -- `luti4 z5\.b,\{z7\.t},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z5\.b,\{z7\.b},z9\.b'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 1 -- `luti4 z5\.t,\{z7\.h,z8\.h},z9\[1\]'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 2 -- `luti4 z5\.h,\{z7\.t,z8\.h},z9\[1\]'
[^ :]+:[0-9]+: Error: unexpected character `t' in element size at operand 2 -- `luti4 z5\.h,\{z7\.h,z8\.t},z9\[1\]'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z5\.h,\{z7\.h,z8\.h},z9\.h'
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti2 5.b,\{z7.b\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing type suffix at operand 2 -- `luti2 z5.b,\{z7\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti2 z5.b,\{7.b\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti2 z5.b,\{z7.b\},9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti2 5.h,\{z7.h\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing type suffix at operand 2 -- `luti2 z5.h,\{z7\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti2 z5.h,\{7.h\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti2 z5.h,\{z7.h\},9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti4 5\.b,\{z7.b},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing type suffix at operand 2 -- `luti4 z5\.b,\{z7\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti4 z5\.b,\{7\.b\},z9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti4 z5\.b,\{z7\.b\},9\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: type mismatch in vector register list at operand 2 -- `luti4 z5\.h,\{z7,z8\.h\},z9\[1\]'
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti4 5\.h,\{z7\.h,z8\.h},z9\[1\]'
[^ :]+:[0-9]+: Error: syntax error in register list at operand 2 -- `luti4 z5\.h,\{7\.h,z8\.h\},z9\[1\]'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti4 z5\.h,\{z7\.h,z8\.h\},9\[1\]'
[^ :]+:[0-9]+: Error: operand mismatch -- `luti2 z5\.b,\{z7\.b\},z9\.b\[2\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti2 z5\.b, \{z7\.b\}, z9\[2\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti2 z5\.h,\{z7\.h\},z9\.h\[2\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti2 z5\.h, \{z7\.h\}, z9\[2\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z5\.b,\{z7\.b\},z9\.b\[1\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z5\.b, \{z7\.b\}, z9\[1\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z5\.h,\{z7\.h\},z9\.h\[2\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z5\.h, \{z7\.h\}, z9\[2\]
[^ :]+:[0-9]+: Error: operand mismatch -- `luti4 z5\.h,\{z7\.h,z8\.h\},z9\.h\[2\]'
[^ :]+:[0-9]+: Info:    did you mean this\?
[^ :]+:[0-9]+: Info:    	luti4 z5\.h, \{z7\.h-z8\.h\}, z9\[2\]
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti2 z4\.b\[2\],\{z9\.b\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti2 z4\[2\],\{z9\.b\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: index not allowed inside register list at operand 2 -- `luti2 z4.b,\{z9.b\[2\]\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti2 z4\[2\],\{z9\[2\]\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti2 z4\.h\[2\],\{z9\.h\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti2 z4\[2\],\{z9\.h\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: index not allowed inside register list at operand 2 -- `luti2 z4.h,\{z9.h\[2\]\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti2 z4\[2\],\{z9\[2\]\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti4 z4\.b\[2\],\{z9\.b\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti4 z4\[2\],\{z9\.b\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: index not allowed inside register list at operand 2 -- `luti4 z4.b,\{z9.b\[2\]\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti4 z4\[2\],\{z9\[2\]\},z15'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti4 z4\.h\[2\],\{z9.h,z10.h\},z15'
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti4 z4\[2\],\{z9.h,z10.h\},z15'
[^ :]+:[0-9]+: Error: index not allowed inside register list at operand 2 -- `luti4 z4.h,\{z9.h,z10.h\[2\]\},z15'
[^ :]+:[0-9]+: Error: missing braces at operand 1 -- `luti4 z4\[2\],\{z9,z10\[2\]\},z15'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti2 z4.b,\{z9.b\},z15.b'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti2 z4.b,\{z9.b\},z15'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti2 z4.h,\{z9.h\},z15.h'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti2 z4.h,\{z9.h\},z15'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z4.b,\{z9.b\},z15.b'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z4.b,\{z9.b\},z15'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z4.h,\{z9.h\},z15.h'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z4.h,\{z9.h\},z15'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z4.h,\{z9.h,z10.h\},z15.h'
[^ :]+:[0-9]+: Error: operand 3 must be an indexed SVE vector register -- `luti4 z4.h,\{z9.h,z10.h\},z15'
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti2 z32.b,\{z9.b\},z15\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 2 -- `luti2 z4.b,\{z32.b\},z4\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti2 z4.b,\{z9.b\},z32\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: register element index out of range 0 to 3 at operand 3 -- `luti2 z2.b,\{z9.b\},z4\[4\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: register element index out of range 0 to 3 at operand 3 -- `luti2 z4.b,\{z9.b\},z15\[-1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti2 z32.h,\{z9.h\},z15\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 2 -- `luti2 z4.h,\{z32.h\},z4\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti2 z4.h,\{z9.h\},z32\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: register element index out of range 0 to 7 at operand 3 -- `luti2 z2.h,\{z9.h\},z4\[8\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: register element index out of range 0 to 7 at operand 3 -- `luti2 z4.h,\{z9.h\},z15\[-1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti4 z32.b,\{z9.b\},z15\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 2 -- `luti4 z4.b,\{z32.b\},z4\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti4 z4.b,\{z9.b\},z32\[1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: register element index out of range 0 to 1 at operand 3 -- `luti4 z2.b,\{z9.b\},z4\[2\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: register element index out of range 0 to 1 at operand 3 -- `luti4 z4.b,\{z9.b\},z15\[-1\]'
[^ :]+:[0-9]+:  Info: macro invoked from here
[^ :]+:[0-9]+: Error: expected a register or register list at operand 1 -- `luti4 z32.h,\{z9.h\},z4\[2\]'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 2 -- `luti4 z4.h,\{z32.h\},z15\[2\]'
[^ :]+:[0-9]+: Error: expected an SVE vector register at operand 3 -- `luti4 z4.h,\{z9.h\},z32\[2\]'
[^ :]+:[0-9]+: Error: register element index out of range 0 to 3 at operand 3 -- `luti4 z2.h,\{z9.h\},z15\[4\]'
[^ :]+:[0-9]+: Error: register element index out of range 0 to 3 at operand 3 -- `luti4 z4.h,\{z9.h\},z15\[-1\]'
