#!/usr/bin/env python3

import sys


def format_array(array):
    result = []
    for value in array:
        if value < 0 or value == 0x80:
            result.append('0x80')
        else:
            result.append(str(value))

    return ', '.join(result)


def assure_array_length(array, size, value = 0x80):
    while len(array) < size:
        array.append(value)


CPP_1_2 = """
  // 1 byte for length, 16 bytes for mask
  const uint8_t pack_1_2_utf8_bytes[256][17] = {
%(rows)s
  };
"""


# For all patterns the 0th element of shuffle is 0.
# We may reuse that entry to store length, but it would
# require some changes in C++ code.
def shuffle_for_conversion_1_or_2_utf8_bytes(file):
  rows = []
  indent = (' ' * 4)
  for shuffle, size in shuffle_for_conversion_1_or_2_utf8_bytes_aux():
    array_str = []
    for value in [size] + shuffle:
      if value == 0x80:
        array_str.append('0x80')
      else:
        array_str.append(str(value))

    array = ','.join(array_str)
    rows.append(f'{indent}{{{array}}}')

  file.write(CPP_1_2 % {'rows': ',\n'.join(rows)})


def shuffle_for_conversion_1_or_2_utf8_bytes_aux():
  # We process 8 x 16-bit word
  # a bit one indices a word having values 0x00..0x7f (produces a single UTF-8 byte)
  # a bit zero indices a word having values 0x0080..0x7ff (produces two UTF-8 bytes)

  # Our input is a 16-bit word in form hhggffeeddccbbaa -- the bits are doubled
  # (h - MSB, a - LSB). In a C++ code we transform it using the following formula:
  #
  # in = hhggffeeddccbbaa
  # t0 = in & 0x5555       // t0 = 0h0g0f0e0d0c0b0a
  # t1 = t0 >> 7           // t1 = 00000000h0g0f0e0
  # t2 = (t0 | t1) & 0xff  // t2 =         hdgcfbea

  for mask in range(256):
    def getbit(k):
      return (mask & (1 << k) != 0)

    a = getbit(0)
    b = getbit(2)
    c = getbit(4)
    d = getbit(6)
    e = getbit(1)
    f = getbit(3)
    g = getbit(5)
    h = getbit(7)

    shuffle = []
    for word_index, bit in enumerate([a, b, c, d, e, f, g, h]):
      if bit: # 1 byte
        shuffle.append(word_index * 2)
      else: # 2 bytes
        shuffle.append(word_index * 2 + 1)
        shuffle.append(word_index * 2)

    output_bytes = len(shuffle)
    while (len(shuffle) < 16):
      shuffle.append(0x80)

    yield (shuffle, output_bytes)


CPP_1_2_3 = """
  // 1 byte for length, 16 bytes for mask
  const uint8_t pack_1_2_3_utf8_bytes[256][17] = {
%(rows)s
  };
"""

def shuffle_for_conversion_1_2_3_utf8_bytes(file):
  rows = []
  indent = (' ' * 4)
  for shuffle, size in shuffle_for_conversion_1_2_3_utf8_bytes_aux():
    array_str = []
    for value in [size] + shuffle:
      if value == 0x80:
        array_str.append('0x80')
      else:
        array_str.append(str(value))

    array = ','.join(array_str)
    rows.append(f'{indent}{{{array}}}')

  file.write(CPP_1_2_3 % {'rows': ',\n'.join(rows)})


def shuffle_for_conversion_1_2_3_utf8_bytes_aux():
  # There are two 8-bit bitmask telling how many bytes each word produces (1, 2 or 3).
  # mask1 = ddccbbaa -- output exactly one byte (d - MSB, a - LSB)
  # mask2 = hhggffee -- output one or two bytes

  # Please note that each bit is duplicated. In final form these bits are interleaved:
  # mask  = (mask1 & 0x5555) | (mask2 & 0xaaaa)
  #       = hdgcfbea

  # Each two-bit subword decides how many bytes will be copied from a 32-bit word of register:
  # | e | a | ea |
  # +---+---+----+-------
  # | 0 | 0 |  0 |  3 bytes
  # | 0 | 1 |  1 |  -- such combination will never come from C++ code, it has no sense
  # | 1 | 0 |  2 |  2 bytes
  # | 1 | 1 |  3 |  1 byte

  for mask in range(256):
    empty = 0x80
    shuffle = []
    for i in range(4):
      subword = mask & 0b11
      mask >>= 2

      if subword == 0:
        shuffle.append(i*4 + 2)
        shuffle.append(i*4 + 3)
        shuffle.append(i*4 + 1)
      elif subword == 3:
        shuffle.append(i*4 + 0)
      elif subword == 2:
        shuffle.append(i*4 + 3)
        shuffle.append(i*4 + 1)

    output_bytes = len(shuffle)
    while (len(shuffle) < 16):
      shuffle.append(empty)

    yield (shuffle, output_bytes)


CPP_EXPAND_SURROGATES = """
  // 2x16 bytes for masks, dwords_consumed
  const uint8_t expand_surrogates[256][33] = {
%(rows)s
  };
"""


def shuffle_for_expanding_surrogate_pairs(file):
  rows = []
  indent = (' ' * 4)
  for shuffle, dwords_consumed in shuffle_for_expanding_surrogate_pairs_aux():

    # If we consume, say 6 dwords of 8, then anyway the C++ conversion
    # routing convert 2 extra dwords (zeroed) into 2 UTF-8 bytes. Thus
    # we have to subtract this zero_dwords from saved bytes, to get
    # the real number of output bytes.
    zero_dwords = 8 - dwords_consumed;

    assert len(shuffle) == 32
    rows.append('%s{%s}' % (indent, format_array(shuffle + [zero_dwords])))

  file.write(CPP_EXPAND_SURROGATES % {'rows': ',\n'.join(rows)})


# Our input 8-bit bitmask informs which word contains a surrogate (low or high one).
# At this point we do not need to know which is which, as we assume that word
# expansion is done after validation. (Let's assume L - low surrogate, H - high
# surrogate, V - any valid non-surrogate word).
#
# Example 1: bitmask 1001'1110 describes a sequence V-L-H-L-H-V-V-? -- the last
# surrogate word might be either L or H, we'll ignore it. Two adjacent bits
# are expected to contain low & high surrogates
#
# Example 2: bitmask 0011'0110 describes a sequence V-L-K-V-L-H-V-V.
#
# Example 3: bitmask 0000'0001 is not valid --- sole surrogate word must not start
# a chunk of string, and C++ takes care not to pass such wrong input.
#
# Example 4: bitmask 0000'1110 is not valid too
#
# We expand all words into  32-bit lanes, spanning two SSE registers.
def shuffle_for_expanding_surrogate_pairs_aux():

  def shuffle_mask(mask):
    result = []
    prev = 'V'
    dwords_consumed = 0
    for i in range(8):
      bit = bool(mask & (1 << i))
      if bit:
        if prev == 'V':
          curr = 'L'
        elif prev == 'L':
          curr = 'H'
        elif prev == 'H':
          curr = 'L'

        result.append(2*i + 0)
        result.append(2*i + 1)

        if curr == 'L':
          dwords_consumed += 1

      else:
        if prev == 'V':
          curr = 'V'
        elif prev == 'L':
          raise ValueError('invalid sequence')
        elif prev == 'H':
          curr = 'V'

        result.append(2*i + 0)
        result.append(2*i + 1)
        result.append(-1)
        result.append(-1)

        dwords_consumed += 1

      prev = curr
    #for

    if curr == 'L': # a sole low surrogate word at the end, discard it (C++ code deals with this case)
        del result[-1]
        del result[-1]
        dwords_consumed -= 1

    while len(result) < 32:
        result.append(-1)

    return result, dwords_consumed

  invalid = 0

  # our input is in form: hdgcfbea
  # we need bits in seq:  hgfedcba
  def as_mask(x):
    def bit(k):
      return int(bool((1 << k) & x))

    return bit(0) \
         | (bit(2) << 1) \
         | (bit(4) << 2) \
         | (bit(6) << 3) \
         | (bit(1) << 4) \
         | (bit(3) << 5) \
         | (bit(5) << 6) \
         | (bit(7) << 7)


  if False:
      print('{:08b}'.format(as_mask(0x85)))
      shuffle_mask(as_mask(0x85))
      sys.exit(1)

  for x in range(256):
    mask = as_mask(x)

    try:
      yield shuffle_mask(mask)
    except ValueError:
      yield (([-1] * 32), 0)


CPP_UCS4_TO_UTF8 = """
  struct UCS4_to_UTF8 {
    uint8_t shuffle[16];
    uint8_t const_bits_mask[16];
    uint8_t output_bytes;
  };

  static_assert(sizeof(UCS4_to_UTF8) == 33, "Structure must be packed");

  const UCS4_to_UTF8 ucs4_to_utf8[256] = {
%(rows)s
  };
"""

"""
The input is 8-bit mask: geca'hfdb. Two-bit words: ab, cd, ef, gh
encodes how many UTF-8 bytes are store in each dword of an SSE
register:
- 00 - 1 byte
- 01 - 2 bytes
- 10 - 3 bytes
- 11 - 4 bytes

We output 3 values:
- a shuffle mask to extract UTF-8 bytes,
- mask to complete UTF-8 format,
- the total number of UTF-8 bytes.
"""
def ucs4_to_utf8(file):
    rows = []
    indent = (' ' * 4)
    for shuffle, const_bits_mask, output_bytes in ucs4_to_utf8_aux():
        #print(output_bytes)
        rows.append('%s{{%s}, {%s}, %d}' % (indent,
                                            format_array(shuffle),
                                            format_array(const_bits_mask),
                                            output_bytes))

    file.write(CPP_UCS4_TO_UTF8 % {'rows': ',\n'.join(rows)})


def ucs4_to_utf8_aux():
    for x in range(256):
        shuffle     = []
        utf8bits    = []
        output_bytes = 0

        def bit(k):
            return int(bool((1 << k) & x))

        def code(bit1, bit0):
            return 2*bit1 + bit0

        ab = code(bit(1), bit(0))
        cd = code(bit(5), bit(4))
        ef = code(bit(3), bit(2))
        gh = code(bit(7), bit(6))

        for i, count in enumerate([ab, cd, ef, gh]):
            if count == 0:
                shuffle.append(4*i + 0)

                utf8bits.append(0x00)
                utf8bits.append(0x00)
                utf8bits.append(0x00)
                utf8bits.append(0x00)

                output_bytes += 1
            elif count == 1:
                shuffle.append(4*i + 1)
                shuffle.append(4*i + 0)

                utf8bits.append(0b10000000)
                utf8bits.append(0b11000000)
                utf8bits.append(0x00)
                utf8bits.append(0x00)

                output_bytes += 2
            elif count == 2:
                shuffle.append(4*i + 2)
                shuffle.append(4*i + 1)
                shuffle.append(4*i + 0)

                utf8bits.append(0b10000000)
                utf8bits.append(0b10000000)
                utf8bits.append(0b11100000)
                utf8bits.append(0x00)

                output_bytes += 3
            elif count == 3:
                shuffle.append(4*i + 3)
                shuffle.append(4*i + 2)
                shuffle.append(4*i + 1)
                shuffle.append(4*i + 0)

                utf8bits.append(0b10000000)
                utf8bits.append(0b10000000)
                utf8bits.append(0b10000000)
                utf8bits.append(0b11110000)

                output_bytes += 4
            else:
                assert False

        assure_array_length(shuffle, 16, 0x80)
        assert len(utf8bits) == 16
        assert len(shuffle) == 16

        yield (shuffle, utf8bits, output_bytes)



CPP_HEADER = """// file generated by scripts/sse_convert_utf16_to_utf8.py
#ifndef SIMDUTF_UTF16_TO_UTF8_TABLES_H
#define SIMDUTF_UTF16_TO_UTF8_TABLES_H

namespace simdutf {
namespace {
namespace tables {
namespace utf16_to_utf8 {
"""

CPP_FOOTER = """} // utf16_to_utf8 namespace
} // tables namespace
} // unnamed namespace
} // namespace simdutf

#endif // SIMDUTF_UTF16_TO_UTF8_TABLES_H
"""

def main():
  with open('utf16_to_utf8_tables.h', 'wt') as f:
    f.write(CPP_HEADER)
    shuffle_for_conversion_1_or_2_utf8_bytes(f)
    shuffle_for_conversion_1_2_3_utf8_bytes(f)
    shuffle_for_expanding_surrogate_pairs(f)
    ucs4_to_utf8(f)
    f.write(CPP_FOOTER)


if __name__ == '__main__':
    main()

