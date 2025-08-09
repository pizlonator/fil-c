## prefix_to_length_table
## We do
## const uint8_t prefix = (input[position] >> 5);
## so
## 00000000 becomes  000
## 10000000 becomes  100
## 11000000 becomes  110
## 11100000 becomes  111
prefix_to_length_table = []
for i in range(0b111 + 1):
    if(i < 0b100):
        # ascii
        prefix_to_length_table.append(1)
    elif(i<0b110):
        # continuation
        prefix_to_length_table.append(0)
    elif(i<0b111):
        prefix_to_length_table.append(2)
    else:
        prefix_to_length_table.append(3)
print("prefix_to_length_table")
print(prefix_to_length_table)


def decode(i):
    answer = []
    for j in range(8):
        rem = (i % 3) + 1
        answer.append(rem)
        i = i // 3
    answer.reverse()
    return answer

table_pattern1=[]
table_pattern2=[]

for i in range(3**8):
    x = decode(i)
    # pattern1 captures the second and third position
    # pattern2 captures the first position
    pattern1 = []
    pattern2 = []
    pos = 0
    for i in x:
        pattern2.append(pos+i-1)
        pattern2.append(0xFF)
        if(i == 1):
            pattern1.append(0xFF)
            pattern1.append(0xFF)
        elif(i == 2):
            pattern1.append(pos)
            pattern1.append(0xFF)
        elif(i==3):
            pattern1.append(pos +1)
            pattern1.append(pos)
        else:
            print("BUG")
        pos += i
    table_pattern1.append(pattern1)
    table_pattern2.append(pattern2)
    assert(len(pattern1) == 16)
    assert(len(pattern2) == 16)

print("const static uint8_t pattern1["+str(len(table_pattern1))+"][16]={")
for x in table_pattern1:
    assert(len(x) == 16)
    print('{%s},' % (', '.join(map(str, x))))
print("};")
print("const static uint8_t pattern2["+str(len(table_pattern1))+"][16]={")
for x in table_pattern2:
    assert(len(x) == 16)
    print('{%s},' % (', '.join(map(str, x))))
print("};")
