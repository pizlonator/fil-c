import sys
delta_asso = [0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,0x00, 0x00, 0x00, 0x0F, 0x00, 0x0F]
check_asso = [0x0D, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x07, 0x0B, 0x0B, 0x0B, 0x0F]

delta_values =[(0x00), (0x00), (0x00), (0x13), (0x04), (0xBF), (0xBF), (0xB9), (0xB9), (0x00), (0x10), (0xC3), (0xBF), (0xBF), (0xB9), (0xB9)]
check_values = [(0x80), (0x80), (0x80), (0x80), (0xCF), (0xBF), (0xD5), (0xA6), (0xB5), (0x86), (0xD1), (0x80), (0xB1), (0x80), (0x91), (0x80)]


valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

def safechr(i):
    if i < 32:
        return '.'
    if i > 127:
        return '?'
    return chr(i)

def safehex(x):
    return "0x{0:2x}".format(x)

def to_signed(x):
    if(x >= 128):
        return x - 256
    return x

def to_unsigned(x):
    if(x < 0):
        return x + 256
    return x

def sat(x, y):
    x = to_signed(x)
    y = to_signed(y)
    z = x + y
    if(z > 127):
        return 127
    if(z < -128):
        return to_unsigned(-128)
    return to_unsigned(z)

def lookup(table, index):
    print("looking up ", hex(index))
    if(index >= 128):
        return 0
    return table[index&0xf]



def quietlookup(table, index):
    if(index >= 128):
        return 0
    return table[index&0xf]

def process(src):
    shifted = (src >> 3)%256
    delta_hash = (lookup(delta_asso,src) + shifted + 1) >> 1
    check_hash = (lookup(check_asso,src) + shifted + 1) >> 1
    out = sat(lookup(delta_values,delta_hash), src)
    chk = sat(lookup(check_values,check_hash), src)
    mask = chk & 0x80
    return (out, mask)

def processquiet(src):
    shifted = (src >> 3)%256
    delta_hash = (quietlookup(delta_asso,src) + shifted + 1) >> 1
    check_hash = (quietlookup(check_asso,src) + shifted + 1) >> 1
    out = sat(quietlookup(delta_values,delta_hash), src)
    chk = sat(quietlookup(check_values,check_hash), src)
    mask = chk & 0x80
    return (out, mask)

def is_ok(i):
    out, mask = processquiet(i)
    if mask == 0:
        return 1
    return 0

def computestring():
    s = ""
    for i in range(256):
        out, mask = processquiet(i)
        if(mask == 0):
            s +=  safechr(i)
    return s
print(computestring() + " " + str(len(computestring())))

def print_layout():
    t={}
    for i in range(256):
        src = i
        shifted = (src >> 3)%256
        check_hash = (quietlookup(check_asso,src) + shifted + 1) >> 1
        if check_hash not in t:
            t[check_hash] = []
        t[check_hash].append(i)
    for check_hash in range(16):
        if check_hash in t:
            off = quietlookup(check_values,check_hash)
            print(hex(check_hash), hex(off), end="")
            print("\t", " ".join(["   "+safechr(c) for c in t[check_hash]]))
        else:
            continue


def is_valid():
    t={}
    for i in range(256):
        src = i
        shifted = (src >> 3)%256
        check_hash = (quietlookup(check_asso,src) + shifted + 1) >> 1
        if check_hash not in t:
            t[check_hash] = []
        t[check_hash].append(i)
    for check_hash in t.keys():
        if check_hash in t:
            array = t[check_hash]
            i = 0
            while i < len(array) and valid.find(chr(array[i])) == -1:
                i += 1
            while i < len(array) and valid.find(chr(array[i])) != -1:
                i += 1
            while i < len(array) and array[i] >= 128:
                i += 1
            if i < len(array):
                return False
        else:
            continue
    return True

print_layout()
print(is_valid())


def is_base64(src):
    is_base64 = None
    for i in range(8):
        shifted = (src >> 3)%256
        shifted += i << 5
        #print("shifted ", hex(shifted))
        delta_hash = (quietlookup(delta_asso,src) + shifted + 1) >> 1
        #print("delta_hash ", hex(delta_hash))
        check_hash = (quietlookup(check_asso,src) + shifted + 1) >> 1
        #print("check_hash ", hex(check_hash))
        out = sat(quietlookup(delta_values,delta_hash), src)
        #print("out ", hex(out))
        chk = sat(quietlookup(check_values,check_hash), src)
        this_is_base64 = chk & 0x80 == 0
        if is_base64 is None:
            is_base64 = this_is_base64
        else:
            if is_base64 != this_is_base64:
                print("unexpected")
                return None
    return is_base64

for i in range(256):
    if is_base64(i):
        print(chr(i), end=",")
    elif is_base64(i) is None:
        print("None", hex(i), end=",")