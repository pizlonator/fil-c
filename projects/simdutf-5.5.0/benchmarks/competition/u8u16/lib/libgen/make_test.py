# 
#  make_simd_operation_test.py
#
#  Copyright (C) 2007 Dan Lin, Robert D. Cameron
#  Licensed to International Characters Inc. and Simon Fraser University
#	              under the Academic Free License version 3.0
#  Licensed to the public under the Open Software License version 3.0.

# make_simd_operation_test.py generates test file simd_operation_test.c
# which compares the result computed by simd operations and 
# result simulated by this test file

from random import randint

ops = ["rotl"]
mods = ["x","h","l"]
fws = [32]
merge_fws = fws[:-1]
bitBlock_size = 64
r1=0
r2=0

def modified_operand(N, n, operand, modifier):
    if modifier == "x": return operand
    elif modifier == "h":
        for i in range(0,N/n):
            operand[i] = operand[i] >> ((n+1)/2)
        return operand
    else:
        for i in range(0,N/n):
            operand[i] = operand[i] % (2** ((n+1)/2))
        return operand

def split_up(r,N,n):
    remainder = r
    result = []
    for i in range(0,N/n):
        quotient = remainder / (2**(N-n*(i+1)))
        remainder = remainder % (2**(N-n*(i+1)))
        result.append(quotient)
    return result

def join_up(r,N,n):
    result = 0
    for i in range(0,N/n):
        result = result + (r[i] * (2**(N-n*(i+1))))
    return result

def gen_const(c,N,n):
    return c*(2**N-1)/(2**n-1)
 
def get_mask(n):
    count = 1
    temp = n
    while (n/2 != 1):
        n = n/2
        count = count + 1
    return count

def simulate_add (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        r3_field.append((r1_field[i] + r2_field[i])%(2**n))
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_sub (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        r3_field.append((r1_field[i] - r2_field[i] + 2**n)%(2**n))
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_srl (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        r3_field.append(r1_field[i] >> (r2_field[i] % 2**get_mask(n)))
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_sll (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        r3_field.append((r1_field[i] << (r2_field[i] % 2**get_mask(n)))% (2**n))
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_sra (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        supplement = 0
        shift = r2_field[i] % (2**get_mask(n))
        for j in range(1, shift+1):
            supplement = supplement + (r1_field[i]/(2**(n-1)))*2**(n-j)
        r3_field.append((r1_field[i] >> shift) + supplement)
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_rotl (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        shift = r2_field[i] % 2**get_mask(n)
        r3_field.append((r1_field[i] << shift) % 2**n + ((r1_field[i] >> (n - shift))% (2**n)))
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_mergeh (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n/2):
        r3_field.append(r1_field[i])
        r3_field.append(r2_field[i])
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_mergel (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(N/n/2, N/n):
        r3_field.append(r1_field[i])
        r3_field.append(r2_field[i])
    r3 = join_up(r3_field, N, n)
    return r3

def simulate_pack (N, n, m1, m2, r1, r2):
    r1_field = modified_operand(N, n, split_up(r1,N,n), m1)
    r2_field = modified_operand(N, n, split_up(r2,N,n), m2)
    r3_field = []
    for i in range(0, N/n):
        r3_field.append(r1_field[i] % (2** (n/2)))
    for j in range(0, N/n):
        r3_field.append(r2_field[j] % (2** (n/2)))
    r3 = join_up(r3_field, N, n/2)
    return r3
 
def make_modified_operand(fw, operand, modifier):
    if modifier == "x": return operand
    elif modifier == "h": return "simd_srli_"+str(fw)+"(%s, %i)" % (operand, fw/2)
    else: return "simd_andc(%s, simd_himask_%i)" % (operand, fw)

def split_64to16(r):
    result = []
    result_hi =  r/2**32
    result_lo =  r%2**32
    result.append(result_hi/2**16)
    result.append(result_hi%2**16)
    result.append(result_lo/2**16)
    result.append(result_lo%2**16)
    return result

def reverse(n):
    mask = 255
    result = 0
    for i in range(0,8):
	    temp = (n & mask)>>(i*8)
	    result = (result | temp)<< 8
	    mask = mask << 8
    return result>>8
    
def make_test(op, fw, m1, m2, r1, r2):
    test_operation = "simd_%s_%i_%s%s (a, b)"
    template1 = "\ttest_rslt = %s;\n" % test_operation
    template1 += "\tif (!(simd_all_eq_8(test_rslt, simd_if(simd_himask_64,simd_if(simd_himask_32,simd_const_16(%s),simd_const_16(%s)),simd_if(simd_himask_32,simd_const_16(%s),simd_const_16(%s)))))) {\n"
    err_stmts = "\t\t\tprintf(\"error in %s \\n\");\n" % test_operation
    err_stmts += "\t\t\tprint_bit_block(\"Computed:\", test_rslt);\n"
    err_stmts += "\t\t\tprintf(\"\\tExpected: %s\\n\");\n"
    err_stmts += "\t\t}\n"
    template = template1 + err_stmts
    if op == "add":
	r =  simulate_add(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "sub":
	r =  simulate_sub(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "pack":
	r =  simulate_pack(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "mergel":
	r =  simulate_mergel(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "mergeh":
	r =  simulate_mergeh(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "srl":
	r =  simulate_srl(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "sll":
	r =  simulate_sll(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "sra":
	r =  simulate_sra(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    elif op == "rotl":
	r =  simulate_rotl(bitBlock_size,fw,m1,m2,r1,r2)
	result = split_64to16(r)
        return template % (op, fw, m1, m2,result[0],result[1],result[2],result[3],op,fw,m1,m2,hex(reverse(r)))
    else:
        return template % (op, fw, m1, m2, "0x0","0x0","0x0","0x0",op,fw,m1,m2,"0x0")


def make_all_for_fw(fw,r1,r2):
    test_list = ''
    for m1 in mods:
        for m2 in mods:
            for op in ops:
                test_list += make_test(op, fw, m1, m2, r1, r2)
    return test_list

def gen_operand_sequence(fw):
    if fw == 2:
	test_operand = [0x0, 0x1, 0x2, 0x3]
    elif fw == 4:
	test_operand = [0x0, 0x7, 0x8, 0xf]
	test_operand.append(randint(0x1, 0xe))
    elif fw == 8:
	test_operand = [0x0, 0x7f, 0x80, 0xff]
	test_operand.append(randint(0x1, 0xfe))
    elif fw == 16:
	test_operand = [0x0, 0x7fff, 0x8000, 0xffff]
	test_operand.append(randint(0x1, 0xfffe))
    elif fw == 32:
	test_operand = [0x0, 0x7fffffff, 0x80000000, 0xffffffff]
	test_operand.append(randint(0x1, 0xfffffffe))
    return test_operand


def generate_and_write_versions(filename, ops,fws):
    test_list = r"""#include <stdio.h>
#include "%s.h"
int main() {
        SIMD_type a, b,test_rslt;
"""
    test_list = test_list % filename
    for fw in fws:      
	for r1 in gen_operand_sequence(fw):
	      for r2 in  gen_operand_sequence(fw):
                  test_list +='\ta=simd_const_'+str(fw)+'('+hex(r1)+');\n\tb=simd_const_'+str(fw)+'('+hex(r2)+');\n'
                  test_list += make_all_for_fw (fw,gen_const(r1,64,fw),gen_const(r2,64,fw))
    test_list += '\treturn 0;\n}\n'
    file = open(filename+"_test.c", 'w')
    file.write(test_list)
    file.close()
    
import sys
if __name__ == "__main__":
    if len(sys.argv) < 2: library_under_test = "mmx_simd"
    else: library_under_test = sys.argv[1]
    generate_and_write_versions(library_under_test, ops, fws)
