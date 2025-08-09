#!/usr/bin/env python
import sys
if(len(sys.argv)<2):
    print("please provide a file name")
    sys.exit(-1)
assert sys.version_info >= (3, 0)
filename = sys.argv[1]
counts=[0,0,0,0]#ascii, ascii+two, ascii+two_three, others
block_count=0
with open(filename, "rb") as file_content:
    array = file_content.read()
    maxv = max(array)
    minv = min(array)
    if(minv < 0): print("bug")
    if(maxv>=0b11110000):
        print("four bytes")
    elif(maxv>=0b11100000):
        print("three bytes")
    elif(maxv>=0b11000000):
        print("two bytes")
    else:
        print("ascii")
    counter = [0, 0, 0, 0]
    for x in array:
        if(x>=0b11110000):
          counter[3] += 1
        elif(x>=0b11100000):
          counter[2] += 1
        elif(x>=0b11000000):
          counter[1] += 1
        elif(x < 0b10000000):
          counter[0] += 1
        else:
          #we have a continuation byte
          pass
    print("ASCII: {}  2-Bytes: {}  3-Bytes: {} 4-Bytes: {}".format(*counter))
    l = sum(counter)
    counter = [c * 100.0 / l for c in counter]
    print("ASCII: {}%  2-Bytes: {}%  3-Bytes: {}% 4-Bytes: {}%".format(*counter))

