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

valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
print("----")
def fun_adjust():
    for zz in range(256):
        check_asso[ord('-')&0xf] = zz
        for yy in range(256):
            check_asso[ord('_')&0xf] = yy
            if(is_valid()):
                print("----")
                print_layout()
                print(is_valid())
                print("found")
                return
fun_adjust()
            #sys.exit(0)

def adjust(array, start, end, check_hash):
    for j in range(256):
        is_ok = True
        for i in range(len(array)):
            valid = (sat(j,array[i])&0x80 == 0) # sat(quietlookup(check_values,check_hash), src)
            should_be_valid = (i>=start and i < end)
            is_ok = is_ok and (valid == should_be_valid)
        if(is_ok):
            check_values[check_hash&0xf] = j
            return
    raise "unexpected"


    chk = sat(quietlookup(check_values,check_hash), src)
    mask = chk & 0x80

def process_explicit():
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
            if i < len(array) and valid.find(chr(array[i])) != -1:
                start = i
                while i < len(array) and valid.find(chr(array[i])) != -1:
                    i += 1
                end = i
                adjust(array, start, end, check_hash)
        else:
            continue
    return True
print("process")
process_explicit()
print("string")
print(computestring()+ " "+str(len(computestring())))

for c in valid:
    print(c,processquiet(ord(c)))

def examine():
    t={}
    for i in valid:
        src = ord(i)
        shifted = (src >> 3)%256
        check_hash = (quietlookup(delta_asso,src) + shifted + 1) >> 1
        if check_hash not in t:
            t[check_hash] = []
        t[check_hash].append(i)
    for check_hash in t.keys():
        print(check_hash, t[check_hash])
    return True
examine()

delta_values[10] += 1 

delta_values[13] += 33 

for c in valid:
    print(c,processquiet(ord(c)))



def casthex(v):
    if(v >= 0x80):
        return "uint8_t("+"0x{:X}".format(v)+")"
    return "0x{:X}".format(v)
def printme(c):
    print(",".join([casthex(i) for i in c]))
print("delta_asso")
printme(delta_asso)
print("check_asso")
printme(check_asso)
print("delta_values")
printme(delta_values)
print("check_values")
printme(check_values)

def processverbose(src):
    print("processing ", hex(src))
    shifted = (src >> 3)%256
    print("shifted ", hex(shifted))
    delta_hash = (lookup(delta_asso,src) + shifted + 1) >> 1
    print("delta_hash ", hex(delta_hash))
    check_hash = (lookup(check_asso,src) + shifted + 1) >> 1
    print("check_hash ", hex(check_hash))
    out = sat(lookup(delta_values,delta_hash), src)
    print("out ", hex(out))
    chk = sat(lookup(check_values,check_hash), src)
    print("chk ", hex(chk))

    mask = chk & 0x80
    return (out, mask)
processverbose(ord('-'))

print(computestring()+ " "+str(len(computestring())))
