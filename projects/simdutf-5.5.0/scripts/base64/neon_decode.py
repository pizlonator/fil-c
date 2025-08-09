t='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
spaces=' \t\n\r\f'
lut_lo = [0x3a, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x61, 0xe1, 0xb4, 0xe5, 0xe5, 0xf4, 0xb4]
lut_hi = [0x11, 0x20, 0x42, 0x80, 0x8,  0x4,  0x8,  0x4, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20]
roll = [0x0, 0x10, 0x13, 0x4, 0xbf, 0xbf, 0xb9, 0xb9, 0x0, 0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0]
def decode(s):
    low = s & 0xf
    high = s >> 4
    m = lut_lo[low] & lut_hi[high]
    if(m > 0x3):
        return (m, None)
    if s == 0x2f:
        off = roll[high - 1]
    else:
        off = roll[high]
    return (m,(s + off)&0xff)


for i in range(256):
    m,d = decode(i)
    if d is None:
        assert t.find(chr(i)) == -1
        assert spaces.find(chr(i)) == -1
        continue
    if m == 0:
        assert d >= 0
        # we must have a base64 element
        v = t.find(chr(i))
        assert v == d
    else:
        # we must have a space
        v = spaces.find(chr(i))
        assert v >= 0, (i, chr(i), v)
print("REGULAR")
print("lut_lo")
print(",".join([hex(c) for c in lut_lo]))
print("lut_hi")
print(",".join([hex(c) for c in lut_hi]))
print("roll")
print(",".join([hex(c) for c in roll]))


print("SWITCHING TO BASE64URL")
print()
t='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_'
spaces=' \t\n\r\f'

lut_lo = [0x3a, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x61, 0xe1, 0xb4, 0xf4, 0xe5, 0xf4, 0xb0]
lut_hi = [0x11, 0x20, 0x42, 0x80, 0x8,  0x4,  0x8,  0x4, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20]
roll = [0xe0, 0x11, 0x13, 0x4, 0xbf, 0xbf, 0xb9, 0xb9, 0x0, 0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0]
t='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_'
spaces=' \t\n\r\f' ## ['0x20', '0x9', '0xa', '0xd']


lut_lo = [0x0 for i in range(16)]
lut_hi = [0x0 for i in range(16)]
#roll = [0 for i in range(16)]

#0x00 are forbidden except for \t \n \r \f which go to one
lut_hi[0] = 0x11
for z in range(16):
    if '\t\n\r\f'.find(chr(z)) != -1:
        lut_lo[z & 0xf] = 0x1 # allowed
    else:
        lut_lo[z] = 0x10 # forbidden
#0x10 and 0x80 all forbidden
lut_hi[0x1] = 0x20
for z in range(0x8, 16):
    lut_hi[z] = 0x20
#lut_hi[0x8] = 0x20

for z in range(16):
    lut_lo[z] |= 0x20

#0x20 selective
lut_hi[0x2] = 0x42
for z in range(16):
    if z == 0:
        lut_lo[z] |= 0x2
    elif z != 0xd:
        lut_lo[z] |= 0x40


#0x30 numbers
lut_hi[0x3] = 0x80
for z in range(10,16):
    lut_lo[z] |= 0x80

#0x40, 0x60 letters
lut_hi[0x4] = 0x8
lut_hi[0x6] = 0x8
lut_lo[0] |= 0x8

#0x7 letters
#0x5 letters
lut_hi[0x5] |= 0x4
lut_hi[0x7] |= 0x4
for i in range(0xb,16):
    lut_lo[i] |= 0x4



def decodes(s):
    low = s & 0xf
    high = s >> 4
    m = lut_lo[low] & lut_hi[high]
    is_underscore = s == 0x5f
    if(is_underscore):
        m = 0
        high = 0
    if(m > 0x3):
        return (m, None)
    if s == 0x2d:
        off = roll[high - 1]
    else:
        off = roll[high]
    return (m,(s + off)&0xff)
print("lut_lo")
print(",".join([hex(c) for c in lut_lo]))
print("lut_hi")
print(",".join([hex(c) for c in lut_hi]))
print("roll")
print(",".join([hex(c) for c in roll]))


for i in range(256):
    m,d = decodes(i)
    if d is None:
        assert t.find(chr(i)) == -1
        assert spaces.find(chr(i)) == -1
        continue
    if m == 0:
        assert d >= 0
        # we must have a base64 element
        v = t.find(chr(i))
        if(v != d): 
            print(hex(i), chr(i), v, d)
        #assert v == d
    else:
        # we must have a space
        v = spaces.find(chr(i))
        assert v >= 0, (i, chr(i), v)

