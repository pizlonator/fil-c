#!/usr/bin/python

# u8u16.py
#
# Python prototype implementation
# Robert D. Cameron
# revised August 5, 2009 - use generated UTF-8 definitions
#
#
#
#----------------------------------------------------------------------------
# 
# We use python's unlimited precision integers for unbounded bit streams.
# This permits simple logical operations on the entire stream.
# Assumption: bitstreams are little-endian (e.g., as on x86).
#
#----------------------------------------------------------------------------
# 

# Utility functions for demo purposes.  Slowwwww, but simple.


#
def readfile(filename):
	f = open(filename)
	contents = f.read()
	f.close()
	return contents


def count_leading_zeroes(strm):
	zeroes = 0
	while (strm & 0xFFFFFFFF) == 0: 
		zeroes += 32
		strm >>= 32
	while (strm & 1) == 0:
		zeroes += 1
		strm >>= 1
	return zeroes
	
#
def transpose_streams(s):
	
	b = []
	
	mask = 128
	index = 0
	while index < 8:
		current = 0
		cursor = 1
		for c in s:
			
			if isinstance(c,str):
				val = ord(c)
			else:
				val = c
			if  (val & mask != 0):
				current += cursor
			cursor <<= 1
			
		index+=1
		mask>>=1
		b.append(current)
	return b

def inverse_transpose(bitset, len):
	bytestream=""
	cursor = 1
	for i in range(0, len):
		byteval = 0
	       	for j in range(0,8):
       			if bitset[j] & cursor != 0:
       				byteval += 128 >> j
	       	bytestream += chr(byteval)
		cursor += cursor
	return bytestream
			
def filter_bytes(bytestream, delmask):
	newstream=""
	cursor = 1
	for c in bytestream:
		if delmask & cursor == 0:
			newstream += c
		cursor += cursor
	return newstream
			
def merge_bytes(stream1, stream2):
	s = ""
	for i in range(len(stream1)):
		s += stream1[i]
		s += stream2[i]
	return s

def bitstream2string(stream, lgth):
	str = ""
	for i in range(lgth):
		if stream & 1 == 1: str += '1'
		else: str += '_'
		stream >>= 1
	return str
#
# Advance all cursors by one position.
def Advance(stream):
	return stream + stream

def ShiftBack(stream):
	return stream >> 1


def u8_streams(u8bit):
	u8 = {}
	u8['unibyte'] = ~u8bit[0]
	u8['prefix'] = u8bit[0] & u8bit[1]
	u8['suffix'] = u8bit[0] & ~u8bit[1]
	u8prefix3or4 = u8['prefix'] & u8bit[2]
	u8['prefix2'] = u8['prefix'] & ~u8bit[2]
	u8['prefix3'] = u8prefix3or4 & ~u8bit[3]
	u8['prefix4'] = u8prefix3or4 & u8bit[3]
	u8['scope22'] = Advance(u8['prefix2'])
	u8['scope32'] = Advance(u8['prefix3'])
	u8['scope42'] = Advance(u8['prefix4'])
	u8['scope33'] = Advance(u8['scope32'])
	u8['scope43'] = Advance(u8['scope42'])
	u8['scope44'] = Advance(u8['scope43'])
	
	u8lastscope = u8['scope22'] | u8['scope33'] | u8['scope44']
	u8anyscope = u8lastscope | u8['scope32'] | u8['scope42'] | u8['scope43']
	
	# C0-C1 are illegal
	error_mask = u8['prefix2'] &~ (u8bit[3] | u8bit[4] | u8bit[5] | u8bit[6]) 
	
	prefix_E0ED = u8['prefix3'] &~ ((u8bit[6] | (u8bit[4] ^ u8bit[7])) | (u8bit[4] ^ u8bit[5]))
	E0ED_constraint = Advance(u8bit[5]) ^ u8bit[2]
	error_mask |= Advance(prefix_E0ED) &~ E0ED_constraint
	
	prefix_F5FF = u8['prefix4'] & (u8bit[4] | (u8bit[5] & (u8bit[6] | u8bit[7])))
	error_mask |= prefix_F5FF

	prefix_F0F4 = u8['prefix4'] &~ (u8bit[4] | u8bit[6] | u8bit[7])
	F0F4_constraint = Advance(u8bit[5]) ^ (u8bit[2] | u8bit[3])
	error_mask |= Advance(prefix_F0F4) &~ F0F4_constraint
	
	error_mask |= u8anyscope ^ u8['suffix']
	u8['error'] = error_mask
	return u8


#
# Generated version using chardeflist2py(DefinitionSet['UTF8'])
#
def u8_streams_generated(bit):  
  unibyte = (~bit[0]);
  prefix = (bit[0] & bit[1]);
  prefix2 = (prefix &~ bit[2]);
  temp1 = (bit[2] &~ bit[3]);
  prefix3 = (prefix & temp1);
  temp2 = (bit[2] & bit[3]);
  prefix4 = (prefix & temp2);
  suffix = (bit[0] &~ bit[1]);
  temp3 = (bit[2] | bit[3]);
  temp4 = (prefix &~ temp3);
  temp5 = (bit[4] | bit[5]);
  temp6 = (temp5 | bit[6]);
  temp7 = (temp4 &~ temp6);
  temp8 = (bit[6] | bit[7]);
  temp9 = (bit[5] & temp8);
  temp10 = (bit[4] | temp9);
  temp11 = (prefix4 & temp10);
  badprefix = (temp7 | temp11);
  temp12 = (temp5 | temp8);
  xE0 = (prefix3 &~ temp12);
  temp13 = (bit[4] & bit[5]);
  temp14 = (bit[7] &~ bit[6]);
  temp15 = (temp13 & temp14);
  xED = (prefix3 & temp15);
  xF0 = (prefix4 &~ temp12);
  temp16 = (bit[5] &~ bit[4]);
  temp17 = (temp16 &~ temp8);
  xF4 = (prefix4 & temp17);
  xA0_xBF = (suffix & bit[2]);
  x80_x9F = (suffix &~ bit[2]);
  x90_xBF = (suffix & temp3);
  x80_x8F = (suffix &~ temp3);

#
# End of generated code
  u8 = {}
  u8['unibyte'] = unibyte
  u8['prefix'] = prefix
  u8['suffix'] = suffix
  u8['prefix2'] = prefix2
  u8['prefix3'] = prefix3
  u8['prefix4'] = prefix4
  u8['scope22'] = Advance(u8['prefix2'])
  u8['scope32'] = Advance(u8['prefix3'])
  u8['scope42'] = Advance(u8['prefix4'])
  u8['scope33'] = Advance(u8['scope32'])
  u8['scope43'] = Advance(u8['scope42'])
  u8['scope44'] = Advance(u8['scope43'])
	
  u8lastscope = u8['scope22'] | u8['scope33'] | u8['scope44']
  u8anyscope = u8lastscope | u8['scope32'] | u8['scope42'] | u8['scope43']
	
  # C0-C1 and F5-FF are illegal
  error_mask = badprefix
	
  error_mask |= Advance(xE0) & x80_x9F
  error_mask |= Advance(xED) & xA0_xBF
  error_mask |= Advance(xF0) & x80_x8F
  error_mask |= Advance(xF4) & x90_xBF
	
  error_mask |= u8anyscope ^ u8['suffix']
  u8['error'] = error_mask
  return u8


#
# The following calculation of UTF-16 bit streams is consistent
# with the original u8u16, calculating streams at u8scope42 and
# u8scope44 positions.
#

def u16_streams(u8, u8bit):
	u16hi = [0,0,0,0,0,0,0,0]
	u16lo = [0,0,0,0,0,0,0,0]
	
	u8lastscope = u8['scope22'] | u8['scope33'] | u8['scope44']
	u8lastbyte = u8['unibyte'] | u8lastscope
	u16lo[2] = u8lastbyte & u8bit[2]
	u16lo[3] = u8lastbyte & u8bit[3]
	u16lo[4] = u8lastbyte & u8bit[4]
	u16lo[5] = u8lastbyte & u8bit[5]
	u16lo[6] = u8lastbyte & u8bit[6]
	u16lo[7] = u8lastbyte & u8bit[7]
	u16lo[1] = (u8['unibyte'] & u8bit[1]) | (u8lastscope & Advance(u8bit[7]))
	u16lo[0] = u8lastscope & Advance(u8bit[6])
	
	u16hi[5] = u8lastscope & Advance(u8bit[3])
	u16hi[6] = u8lastscope & Advance(u8bit[4])
	u16hi[7] = u8lastscope & Advance(u8bit[5])
	u16hi[0] = u8['scope33'] & Advance(Advance(u8bit[4]))
	u16hi[1] = u8['scope33'] & Advance(Advance(u8bit[5]))
	u16hi[2] = u8['scope33'] & Advance(Advance(u8bit[6]))
	u16hi[3] = u8['scope33'] & Advance(Advance(u8bit[7]))
	u16hi[4] = u8['scope33'] & Advance(u8bit[2])

	u8surrogate = u8['scope42'] | u8['scope44']
	u16hi[0] = u16hi[0] | u8surrogate	
	u16hi[1] = u16hi[1] | u8surrogate	
	u16hi[3] = u16hi[3] | u8surrogate	
	u16hi[4] = u16hi[4] | u8surrogate	
	u16hi[5] = u16hi[5] | u8['scope44']

	s42lo1 = ~u8bit[3] # subtract 1 
	u16lo[1] = u16lo[1] | (u8['scope42'] & s42lo1)
	s42lo0 = u8bit[2] ^ s42lo1 # borrow *
	u16lo[0] = u16lo[0] | (u8['scope42'] & s42lo0)
	borrow1 = s42lo1 & ~u8bit[2]
	s42hi7 = Advance(u8bit[7]) ^ borrow1
	u16hi[7]= u16hi[7] | (u8['scope42'] & s42hi7)
	borrow2 = borrow1 & ~Advance(u8bit[7])
	s42hi6 = Advance(u8bit[6]) ^ borrow2
	u16hi[6] = u16hi[6] | (u8['scope42'] & s42hi6)

	u16lo[2] = u16lo[2] | (u8['scope42'] & u8bit[4])
	u16lo[3] = u16lo[3] | (u8['scope42'] & u8bit[5])
	u16lo[4] = u16lo[4] | (u8['scope42'] & u8bit[6])
	u16lo[5] = u16lo[5] | (u8['scope42'] & u8bit[7])
	u16lo[6] = u16lo[6] | (u8['scope42'] & ShiftBack(u8bit[2]))
	u16lo[7] = u16lo[7] | (u8['scope42'] & ShiftBack(u8bit[3]))

	delmask = u8['prefix'] | u8['scope32'] | u8['scope43']
	return (u16hi, u16lo, delmask)




#
# The following calculation of UTF-16 bit streams uses the 
# u8scope43 position rather than the u8scope42 position for
# the bits of the first UTF-16 code unit of a surrogate pair.
# This requires more shifting than with the use of u8scope42,
# but has the advantage that all shifts are in the forward
# direction only and can hence be implemented using addition
# on little-endian architecture.
#

def u16_streams_fwdonly(u8, u8bit):
	u16hi = [0,0,0,0,0,0,0,0]
	u16lo = [0,0,0,0,0,0,0,0]
	
	u8lastscope = u8['scope22'] | u8['scope33'] | u8['scope44']
	u8lastbyte = u8['unibyte'] | u8lastscope
	u16lo[2] = u8lastbyte & u8bit[2]
	u16lo[3] = u8lastbyte & u8bit[3]
	u16lo[4] = u8lastbyte & u8bit[4]
	u16lo[5] = u8lastbyte & u8bit[5]
	u16lo[6] = u8lastbyte & u8bit[6]
	u16lo[7] = u8lastbyte & u8bit[7]
	u16lo[1] = (u8['unibyte'] & u8bit[1]) | (u8lastscope & Advance(u8bit[7]))
	u16lo[0] = u8lastscope & Advance(u8bit[6])
	
	u16hi[5] = u8lastscope & Advance(u8bit[3])
	u16hi[6] = u8lastscope & Advance(u8bit[4])
	u16hi[7] = u8lastscope & Advance(u8bit[5])
	u16hi[0] = u8['scope33'] & Advance(Advance(u8bit[4]))
	u16hi[1] = u8['scope33'] & Advance(Advance(u8bit[5]))
	u16hi[2] = u8['scope33'] & Advance(Advance(u8bit[6]))
	u16hi[3] = u8['scope33'] & Advance(Advance(u8bit[7]))
	u16hi[4] = u8['scope33'] & Advance(u8bit[2])

	u8surrogate = u8['scope43'] | u8['scope44']
	u16hi[0] = u16hi[0] | u8surrogate	
	u16hi[1] = u16hi[1] | u8surrogate	
	u16hi[3] = u16hi[3] | u8surrogate	
	u16hi[4] = u16hi[4] | u8surrogate	
	u16hi[5] = u16hi[5] | u8['scope44']


	s42lo1 = ~u8bit[3] # subtract 1 
	u16lo[1] = u16lo[1] | (u8['scope43'] & Advance(s42lo1))
	s42lo0 = u8bit[2] ^ s42lo1 # borrow *
	u16lo[0] = u16lo[0] | (u8['scope43'] & Advance(s42lo0))
	borrow1 = s42lo1 & ~u8bit[2]
	advance_bit7 = Advance(u8bit[7])
	s42hi7 = advance_bit7 ^ borrow1
	u16hi[7]= u16hi[7] | (u8['scope43'] & Advance(s42hi7))
	borrow2 = borrow1 & ~advance_bit7
	s42hi6 = Advance(u8bit[6]) ^ borrow2
	u16hi[6] = u16hi[6] | (u8['scope43'] & Advance(s42hi6))

	u16lo[2] = u16lo[2] | (u8['scope43'] & Advance(u8bit[4]))
	u16lo[3] = u16lo[3] | (u8['scope43'] & Advance(u8bit[5]))
	u16lo[4] = u16lo[4] | (u8['scope43'] & Advance(u8bit[6]))
	u16lo[5] = u16lo[5] | (u8['scope43'] & Advance(u8bit[7]))
	u16lo[6] = u16lo[6] | (u8['scope43'] & u8bit[2])
	u16lo[7] = u16lo[7] | (u8['scope43'] & u8bit[3])

	delmask = u8['prefix'] | u8['scope32'] | u8['scope42']
	return (u16hi, u16lo, delmask)



# 
# Messages to duplicate u8u16 error reporting.
#
def IllegalSequenceMessage(pos):
	return "Illegal UTF-8 sequence at position %i in source.\n" % pos

def IncompleteSequenceMessage(pos):
	return "EOF with incomplete UTF-8 sequence at position %i in source.\n" % pos


import sys
def main():

	if len(sys.argv) < 2:
		sys.stderr.write("Usage: u8u16.py u8file [u16file]\n")
		exit
	if len(sys.argv) == 3:
		outfile = open(sys.argv[2],"w")
	else: outfile = sys.stdout
	u8data = readfile(sys.argv[1])
	u8len = len(u8data)
	u8bit = transpose_streams(u8data)
#	u8 = u8_streams(u8bit)
	u8 = u8_streams_generated(u8bit)
	if u8['error'] != 0:
		err_pos = count_leading_zeroes(u8['error'])
		at_EOF = err_pos == len(u8data)
		if (err_pos >= 1) and ord(u8data[err_pos-1]) >= 0xC0: err_pos -= 1
		elif err_pos >= 2 and ord(u8data[err_pos-2]) >= 0xE0: err_pos -= 2
		elif err_pos >= 3 and ord(u8data[err_pos-3]) >= 0xF0: err_pos -= 3	
		if at_EOF:
			sys.stderr.write(IncompleteSequenceMessage(err_pos))
		else:
			sys.stderr.write(IllegalSequenceMessage(err_pos))
		u8len = err_pos
# 	Originally, we used the u16_streams version.
#	(u16hi, u16lo, delmask) = u16_streams(u8, u8bit)
	(u16hi, u16lo, delmask) = u16_streams_fwdonly(u8, u8bit)
	U16H = filter_bytes(inverse_transpose(u16hi, u8len), delmask)
	U16L = filter_bytes(inverse_transpose(u16lo, u8len), delmask)
	U16final = merge_bytes(U16H, U16L)
	outfile.write(U16final)
	outfile.close()
		
if __name__ == "__main__": main()


