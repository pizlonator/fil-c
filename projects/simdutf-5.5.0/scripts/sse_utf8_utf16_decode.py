#!/usr/bin/env python3
from common import *


def is_bit_set(mask, i):
    return (mask & ( 1<<i )) == ( 1<<i )

# computes the location of the 0 bits (index starts at zero)
def compute_locations(mask):
    answer = []
    i = 0
    while( (mask >> i) > 0 ):
        if(is_bit_set(mask,i)):
            answer.append(i)
        i += 1
    return answer


# computes the gaps between the 1, assuming we had an initial 1
def compute_code_point_size(mask):
    positions = compute_locations(mask)
    answer = []
    oldx = -1
    for i in range(len(positions)):
        x = positions[i]
        answer.append(x-oldx)
        oldx = x
    return answer

## check that we have 6 1-2 byte (at least)
def easy_case12(code_point_size):
    if(len(code_point_size)<6):
        return False
    return max(code_point_size[:6])<=2

## check that we have 4 1-2-3 byte (at least)
def easy_case123(code_point_size):
    if(len(code_point_size)<4):
        return False
    return max(code_point_size[:4])<=3

## check that we have 4 1-2-3 byte (at least)
def easy_case1234(code_point_size):
    if(len(code_point_size)<3):
        return False
    return max(code_point_size[:3])<=4

def grab_easy_case12_code_point_size(code_point_size):
    return code_point_size[:6]

def grab_easy_case123_code_point_size(code_point_size):
    return code_point_size[:4]

def grab_easy_case1234_code_point_size(code_point_size):
    return code_point_size[:3]

def buildshuf12_twobytes(sizes):
    answer = [0 for i in range(16)]
    pos = 0
    for i in range(len(sizes)):
        if(sizes[i] == 1):
            answer[2*i] = pos
            answer[2*i+1] = 0xff
            pos += 1
        else:
            answer[2*i] = pos + 1
            answer[2*i+1] = pos
            pos += 2
    return answer

def buildshuf123_threebytes(sizes):
    answer = [0 for i in range(16)]
    pos = 0
    for i in range(len(sizes)): # 4 * 4 = 16
        if(sizes[i] == 1):
            answer[4*i] = pos
            answer[4*i+1] = 0xff
            answer[4*i+2] = 0xff
            answer[4*i+3] = 0xff
            pos += 1
        elif(sizes[i] == 2):
            answer[4*i] = pos + 1
            answer[4*i+1] = pos
            answer[4*i+2] = 0xff
            answer[4*i+3] = 0xff
            pos += 2
        else: # must be three
            answer[4*i] = pos + 2
            answer[4*i+1] = pos + 1
            answer[4*i+2] = pos
            answer[4*i+3] = 0xff
            pos += 3
    return answer

def buildshuf1234_fourbytes(sizes):
    answer = [0 for i in range(16)]
    pos = 0
    for i in range(len(sizes)): # 3 * 4 = 12
        if(sizes[i] == 1):
            answer[4*i] = pos
            answer[4*i+1] = 0xff
            answer[4*i+2] = 0xff
            answer[4*i+3] = 0xff
            pos += 1
        elif(sizes[i] == 2):
            answer[4*i] = pos + 1
            answer[4*i+1] = pos
            answer[4*i+2] = 0xff
            answer[4*i+3] = 0xff
            pos += 2
        elif(sizes[i] == 3):
            answer[4*i] = pos + 2
            answer[4*i+1] = pos + 1
            answer[4*i+2] = pos
            answer[4*i+3] = 0xff
            pos += 3
        else: # must be four
            answer[4*i] = pos + 3
            answer[4*i+1] = pos + 2
            answer[4*i+2] = pos + 1
            answer[4*i+3] = pos
            pos += 4
    return answer


def main():
  easycase12 = set()
  easycase123 = set()
  easycase1234 = set()
  for x in range(1<<12):
    sizes = compute_code_point_size(x)
    if(easy_case12(sizes)):
        z1 = grab_easy_case12_code_point_size(sizes)
        easycase12.add(tuple(z1))
    elif(easy_case123(sizes)):
        z1 = grab_easy_case123_code_point_size(sizes)
        easycase123.add(tuple(z1))
    elif(easy_case1234(sizes)):
        z1 = grab_easy_case1234_code_point_size(sizes)
        easycase1234.add(tuple(z1))
  easycase12sorted = [x for x in easycase12]
  easycase12sorted.sort()
  easycase123sorted = [x for x in easycase123]
  easycase123sorted.sort()
  easycase1234sorted = [x for x in easycase1234]
  easycase1234sorted.sort()

  print("#include <cstdint>")
  allshuf = [buildshuf12_twobytes(z) for z in  easycase12sorted] + [buildshuf123_threebytes(z) for z in  easycase123sorted] + [buildshuf1234_fourbytes(z) for z in  easycase1234sorted]
  print("const uint8_t shufutf8["+str(len(easycase12sorted+easycase123sorted+easycase1234sorted))+"][16] = ")
  print(cpp_arrayarray_initializer(allshuf), end=";\n")
  print("/* number of two bytes : "+ str(len(easycase12sorted))+ " */")
  print("/* number of two + three bytes : "+ str(len(easycase12sorted+easycase123sorted))+ " */")
  print("/* number of two + three + four bytes : "+ str(len(easycase12sorted+easycase123sorted+easycase1234sorted))+ " */")
  c = 0
  index = {}
  for t in easycase12sorted + easycase123sorted + easycase1234sorted:
      index[t] = c
      c = c + 1
  arrg=[]
  for x in range(1<<12):
    sizes = compute_code_point_size(x)
    if(easy_case12(sizes)):
        z1 = grab_easy_case12_code_point_size(sizes)
        idx = index[tuple(z1)]
        s = sum(z1)
        arrg.append((idx,s))
    elif(easy_case123(sizes)):
        z1 = grab_easy_case123_code_point_size(sizes)
        idx = index[tuple(z1)]
        s = sum(z1)
        arrg.append((idx,s))
    elif(easy_case1234(sizes)):
        z1 = grab_easy_case1234_code_point_size(sizes)
        idx = index[tuple(z1)]
        s = sum(z1)
        arrg.append((idx,s))
    else:
        # we are in error, use a bogus index
        arrg.append((209,12))
  print("const uint8_t utf8bigindex["+str(len(arrg))+"][2] = ")
  print(cpp_arrayarray_initializer(arrg), end=";\n")


if __name__ == '__main__':
    main()
