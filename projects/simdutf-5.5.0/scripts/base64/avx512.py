lookup_0 = [0 for i in range(64)]
lookup_1 = [0 for i in range(64)]
for i in range(64):
    lookup_0[i] = 0x80
    lookup_1[i] = 0x80
lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
for i in range(64):
    val = ord(lookup[i])
    bit6 = val & 0x40
    bits05 = val & 0x3f
    if bit6:
        lookup_1[bits05] = i
    else:
        lookup_0[bits05] = i
allowed = "\t\r\n "
for z in allowed:
    lookup_0[ord(z)] = 0xff
def sign8(x):
    if x >= 128:
        return x - 256
    return x
lookup_0.reverse()
lookup_1.reverse()
print("lookup0:")
print(", ".join([str(sign8(i)) for i in lookup_0]))
print("lookup1:")
print(", ".join([str(sign8(i)) for i in lookup_1]))
lookupn = [0 for i in range(64)]
output = 0
for ifrom in range(16):
    lookupn[ifrom*4 + 0] = output + 3
    lookupn[ifrom*4 + 1] = output + 2
    lookupn[ifrom*4 + 2] = output + 1
    lookupn[ifrom*4 + 3] = output + 0
    output += 4
lookupn.reverse()
print("reverse:")
print(", ".join([str(i) for i in lookupn]))

print("====")

lookup_0 = [0 for i in range(64)]
lookup_1 = [0 for i in range(64)]
for i in range(64):
    lookup_0[i] = 0x80
    lookup_1[i] = 0x80
lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
for i in range(64):
    val = ord(lookup[i])
    bit6 = val & 0x40
    bits05 = val & 0x3f
    if bit6:
        lookup_1[bits05] = i
    else:
        lookup_0[bits05] = i
allowed = "\0\t\r\n "
for z in allowed:
    lookup_0[ord(z)] = 0xff

lookup_0.reverse()
lookup_1.reverse()
print("lookup0:")
print(", ".join([str(sign8(i)) for i in lookup_0]))
print("lookup1:")
print(", ".join([str(sign8(i)) for i in lookup_1]))
lookupn = [0 for i in range(64)]
output = 0
for ifrom in range(16):
    lookupn[ifrom*4 + 0] = output + 3
    lookupn[ifrom*4 + 1] = output + 2
    lookupn[ifrom*4 + 2] = output + 1
    lookupn[ifrom*4 + 3] = output + 0
    output += 4
lookupn.reverse()
print("reverse:")
print(", ".join([str(i) for i in lookupn]))