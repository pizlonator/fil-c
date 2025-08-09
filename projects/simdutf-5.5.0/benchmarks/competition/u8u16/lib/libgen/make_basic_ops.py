# 
#  make_basic_ops.py
#
#  Copyright (C) 2007 Dan Lin, Robert D. Cameron
#  Licensed to International Characters Inc. and Simon Fraser University
#	              under the Academic Free License version 3.0
#  Licensed to the public under the Open Software License version 3.0.

# make_basic_ops.py generates inline definitions for
# idealized basic SIMD operations that are not included
# in the built-in functions.
ops = ["sub", "add", "sll", "srl", "sra", "pack", "mergeh", "mergel", "rotl"]

# 1 for "add" and "sub"
# 2 for "sll", "srl" and "sra"
# 3 for "pack"
# 4 for "mergel" and "mergeh"
# Note: For new operations added into this file,
#       if different fieldwidths are needed, add more to the following list
#       otherwise share with the operations above
fws = { 1: [4], 2: [32,16,8,4], 3: [8,4,2], 4: [4,2,1]}

ops_immediateshift= ["sll","srl"]
fws_immediateshift = [2,4,8]


# make the structure of a inline function

def make_inline(op,fw,body):
	operation = "simd_%s_%i" % (op,fw)
	return "#ifndef %s\ninline SIMD_type %s(SIMD_type a,SIMD_type b){\n\treturn %s;}\n#endif\n\n" % (operation,operation,body)

# this is the main function that generate simd operations in n bit field
# by using simd operations in 2n bit field
#
# Considering that simd_const_64() is not included in simd_built_in operations
# we use _mm_cvtsi32_si64() as the mask for shift operations in 32 bit field
# instead of masking the operand twice with simd_himask_32 and smid_const_32()
#
# Note: "rotl" has not been tested
#       "srl", "sll" and "sra" in 4 bit field might not be efficient
#       by using 8 bit field operations
def make_halfsize_defn(op,fw):
    template = "simd_if(simd_himask_"+str(fw*2)+", simd_"+op+"_%i(%s,%s)\n\t,"+"simd_"+op+"_%i(%s,%s))"
    if (op == "add") or (op == "sub"):
        return template % (fw*2, "simd_and(a,simd_himask_"+str(fw*2)+")",
    			  "simd_and(b,simd_himask_"+str(fw*2)+")",
			  fw*2,"simd_andc(a,simd_himask_"+str(fw*2)+")",
			  "simd_andc(b,simd_himask_"+str(fw*2)+")")
    elif op == "srl":
	common_part = template % (fw*2,"a","simd_and(simd_const_"+str(fw)+"("+str(fw-1)+"),simd_srli_"+str(fw*2)+"(b,"+str(fw)+"))",
				  fw*2,"simd_andc(a,simd_himask_"+str(fw*2)+")","simd_and(%s)")
	if fw == 32:
		return common_part % "_mm_cvtsi32_si64(31),b"
	else:
		return common_part % ("b,simd_const_"+str(fw*2)+"("+str(fw-1)+")")
    elif op == "sll":
	common_part = template % (fw*2, "simd_and(a,simd_himask_"+str(fw*2)+")",
				  "simd_and(simd_const_"+str(fw)+"("+str(fw-1)+"),simd_srli_"+str(fw*2)+"(b,"+str(fw)+"))",
				  fw*2,"a", "simd_and(%s)")
	if fw == 32:
		return common_part % "_mm_cvtsi32_si64(31),b"
	else:
		return common_part % ("b,simd_const_"+str(fw*2)+"("+str(fw-1)+")")
    elif op == "sra":
	if fw == 32:
		return """simd_if(simd_himask_64(),
			_mm_sra_pi32(a, simd_and(simd_const_32(31),simd_srli_64(b,32))),
			_mm_sra_pi32(a, simd_and(_mm_cvtsi32_si64(31), b)))"""
        else:		
		return template % (fw*2,"simd_and(a,simd_himask_"+str(fw*2)+")",
				  "simd_and(sisd_srli(b,"+str(fw)+"),simd_const_"+str(fw*2)+"("+str(fw-1)+"))",
				  fw*2,"simd_srai_"+str(fw*2)+"(sisd_slli(a,"+str(fw)+"),"+str(fw)+")",
				  "simd_and(b,simd_const_"+str(fw*2)+"("+str(fw-1)+"))")
    elif op == "rotl":
        return "simd_or(simd_sll_"+str(fw)+"(a,b),simd_srl_"+str(fw)+"(a,simd_sub_"+str(fw)+"(simd_const_"+str(fw)+"("+str(fw)+"),b)))"
    elif op == "pack":
	return "simd_pack_"+str(fw*2)+"(%s,\n\t%s)" % ("simd_if(simd_himask_"+str(fw)+",sisd_srli(a,"+str(fw/2)+"),a)",
    							"simd_if(simd_himask_"+str(fw)+",sisd_srli(b,"+str(fw/2)+"),b)")
    elif op == "mergeh" or op == "mergel":
	return "simd_"+op+"_"+str(fw*2)+"(%s,\n\t%s)" % ("simd_if(simd_himask_"+str(fw*2)+",a,sisd_srli(b,"+str(fw)+"))",
    							"simd_if(simd_himask_"+str(fw*2)+",sisd_slli(a,"+str(fw)+"),b)")
    else:
	raise Exception("Bad operator %s" % op)

def make_immediateshift_defn(op,fw):
    template = "inline SIMD_type simd_"+op+"i_"+str(fw)+"(SIMD_type r, int sh){\n\t return %s"
    if op== "sll":
        return template % "simd_and(sisd_"+op+"i(r,sh),simd_const_"+str(fw)+"(("+str(2**fw-1)+"<<sh)&"+str(2**fw-1)+"));}\n"
    else:
        return template % "simd_and(sisd_"+op+"i(r,sh),simd_const_"+str(fw)+"("+str(2**fw-1)+">>sh));}\n"

def make_all_half_fieldwidth_versions (ops, fws):
    defn_list = ''
    for op in ops_immediateshift:
        for fw in fws_immediateshift:
            defn_list += make_immediateshift_defn(op, fw)
    for op in ops:
	if (op=="add" or op=="sub"):		
            for fw in fws[1]:
                defn_list += make_inline(op, fw, make_halfsize_defn(op,fw))
   	elif (op=="sll" or op=="srl" or op=="sra" or op=="rotl"):		
            for fw in fws[2]:
                defn_list += make_inline(op, fw, make_halfsize_defn(op,fw))
	elif (op=="pack"):		
            for fw in fws[3]:
                defn_list += make_inline(op, fw, make_halfsize_defn(op,fw))
	elif (op=="mergeh" or op=="mergel"):		
            for fw in fws[4]:
                defn_list += make_inline(op, fw, make_halfsize_defn(op,fw))	
	else: raise Exception("Bad operator %s" % op)
    return defn_list

def generate_and_write_versions(filename, ops, fws):
    defn_list = make_all_half_fieldwidth_versions (ops, fws)
    file = open(filename, 'w')
    file.write(defn_list)
    file.close()
    
if __name__ == "__main__":
    generate_and_write_versions("mmx_simd_basic.h", ops, fws)   