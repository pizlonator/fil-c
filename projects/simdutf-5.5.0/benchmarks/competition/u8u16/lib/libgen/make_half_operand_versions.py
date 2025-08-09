# 
#  make_half_operand_versions.py
#
#  Copyright (C) 2007 Robert D. Cameron, Dan Lin
#  Licensed to International Characters Inc. and Simon Fraser University
#	              under the Academic Free License version 3.0
#  Licensed to the public under the Open Software License version 3.0.
#
# make_halfoperand_versions.py generates macro definitions for
# the half-operand versions of idealized SIMD operations.
#
# The half-operand versions of a (binary) operation on n-bit fields
# are named by # extending the operation name with an underscore and 
# two # operand modification letters for the two operands:
#   operand modifier "h" indicates that the high n/2 bits of fields
#      are taken as the operand values (shifted right by n/2).
#   operand modifier "l" indicates that the low n/2 bits of fields
#      masking off the high n/2 bits.
#   operand modifier "x" indicates that all n bits of the field are
#      used as the operand value without modification.
#
ops = ["sub", "add", "pack", "mergeh", "mergel", "sll", "srl", "sra", "rotl"]
mods = ["x", "l", "h"]
fws = [2, 4, 8, 16, 32]             

# The generic approach to defining these operations is to simply
# include code to modify each of the operands and then perform
# the desired operation.  However, this program generates more
# efficient code when certain optimizations may apply.
#
# 1.  Lo-Mask Optimization.
#     The masking for an "l" operand can be eliminated if
#     that operation ignores the high n/2 bits.  The
#     ignores_operand_hi_half property declares this.
# 2.  Hi-Mask Optimization
#     The shifting of an "h" operand can be optimized if the
#     operation ignores the high n/2 bits of the operand.  In this 
#     case, it is possible to replace a simulated shift for n bit
#     fields with any built-in shift for fields of a multiple of n.
# 3.  Double-High Shift Optimization
#     In some cases, the two shift operations for a pair of "h" operands 
#     may be eliminated in favor of a single shift operation after
#     the basic operation is perfomed (support_hh_postshift property).
# 4.  LH Add Optimization
#     If both operands are modified (either "h" or "l" modifiers), 
#     a simulated add on small field widths can be replaced by a
#     built-in add operation for fields of a multiple of n.
#
##########################################################################
#
# Operation Properties to Support Optimization
#
# 1.  Lo-Mask Optimization.
#
# An operation satisfies the ignores_operand_hi_half property
# for a particular operand if the high n/2 bits of that
# operand play no role in the operation applied to n bit
# fields.   This property applies for the second operand
# of all shifts and rotates, as well as for both operands
# of packs.
#
ignores_operand_hi_half = { 1: ["pack"], 2: ["sll", "srl", "sra", "rotl", "pack"]}
#
# 2.  Hi-Mask Optimization
#
# Shifts of 2, 4 or 8 bits are simulated on common architectures.  Replace
# if possible with  16-bit shifts, which are built-in on common architectures.
#  
simulated_shift_replacement = {2: 16, 4: 16, 8: 16}
#
# 3.  Double-High Shift Optimization
#
support_hh_postshift = ["mergeh", "mergel"]
#
# 4.  LH Add Optimization
#
# Adds of 2-bit or 4-bit fields are simulated on common architectures.  Replace
# if possible with 8-bit adds, which are built-in on common architectrues.
#
simulated_add_replacement = {2: 8, 4: 8}

#
# Generate a definition that won't override any hand-coded version.
#
defn_mode = "INLINE"

def define_macro_if_undefined(op_name, body):
    return "#ifndef %s\n#define %s(v1, v2) %s\n#endif\n\n" % (op_name, op_name, body)

def define_inline_function_if_undefined(op_name, body):
    prototype = "inline SIMD_type %s(SIMD_type v1, SIMD_type v2)" % (op_name)
    return "#ifndef %s\n%s {\n  return %s;\n}\n#endif\n\n" % (op_name, prototype, body)

def define_if_undefined(op_name, body):
    global defn_mode
    if defn_mode == "MACRO": return define_macro_if_undefined(op_name, body)
    else: return define_inline_function_if_undefined(op_name, body)

def operand_name(operand_no):
    return "v%i" % operand_no

def make_modified_operand(operation, fw, operand_no, modifier):
    operand = operand_name(operand_no)
    if modifier == "x": return operand
    elif operation in ignores_operand_hi_half[operand_no]:
	if modifier == "l": return operand
	elif modifier == "h":
	    if fw in simulated_shift_replacement.keys():
	      shft_op = "simd_srli_%i" % (simulated_shift_replacement[fw])
	    else: shft_op = "simd_srli_%i" % (fw)
	    return "%s(%s, %i)" % (shft_op, operand, fw/2)
	else: raise Exception("Bad modifier %s" % modifier)
    elif modifier == "h": return "simd_srli_%i(%s, %i)" % (fw, operand, fw/2)
    elif modifier == "l": return "simd_andc(%s, simd_himask_%i)" % (operand, fw)
    else: raise Exception("Bad modifier %s" % modifier)


def make_optimized_defn(op, fw, m1, m2):
    base_operation = "simd_%s_%i" % (op, fw)
    op_name = base_operation + "_" + m1 + m2
    operand1 = make_modified_operand(op, fw, 1, m1)
    operand2 = make_modified_operand(op, fw, 2, m2)
    if (m1 == "h") and (m2 == "h") and (op in support_hh_postshift):
        code = "%s(%s, %s)" % (base_operation, operand1, operand2)
	return define_if_undefined(op_name, code)
    if (op == "add") and (m1 != "x") and (m2 != "x") and (fw in simulated_add_replacement.keys()):
	base_operation = "simd_%s_%i" % (op, simulated_add_replacement[fw])
    return define_if_undefined(op_name, "%s(%s, %s)" % (base_operation, operand1, operand2))

def make_all_for_op_fw(op, fw):
    defn_list = ''
    for m1 in mods:
        for m2 in mods:
            defn_list += make_optimized_defn(op, fw, m1, m2)
    return defn_list

#
# Usage: make_all_half_operand_versions(ops, fws) to generate
# a complete file of all the "half-operand" modified versions
# of a set of simd operations for each operation op within
# the list ops and each field width fw in the list of field
# widths.
#
def make_all_half_operand_versions (ops, fws):
    defn_list = ''
    for op in ops:
        for fw in fws:
            defn_list += make_all_for_op_fw(op, fw)
    return defn_list

def generate_and_write_versions(filename, ops, fws):
    defn_list = make_all_half_operand_versions (ops, fws)
    file = open(filename, 'w')
    file.write(defn_list)
    file.close()
    
if __name__ == "__main__":
    generate_and_write_versions("mmx_simd_modified.h", ops, fws)   
