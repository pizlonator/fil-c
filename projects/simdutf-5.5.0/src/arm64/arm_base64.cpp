/**
 * References and further reading:
 *
 * Wojciech Muła, Daniel Lemire, Base64 encoding and decoding at almost the
 * speed of a memory copy, Software: Practice and Experience 50 (2), 2020.
 * https://arxiv.org/abs/1910.05109
 *
 * Wojciech Muła, Daniel Lemire, Faster Base64 Encoding and Decoding using AVX2
 * Instructions, ACM Transactions on the Web 12 (3), 2018.
 * https://arxiv.org/abs/1704.00605
 *
 * Simon Josefsson. 2006. The Base16, Base32, and Base64 Data Encodings.
 * https://tools.ietf.org/html/rfc4648. (2006). Internet Engineering Task Force,
 * Request for Comments: 4648.
 *
 * Alfred Klomp. 2014a. Fast Base64 encoding/decoding with SSE vectorization.
 * http://www.alfredklomp.com/programming/sse-base64/. (2014).
 *
 * Alfred Klomp. 2014b. Fast Base64 stream encoder/decoder in C99, with SIMD
 * acceleration. https://github.com/aklomp/base64. (2014).
 *
 * Hanson Char. 2014. A Fast and Correct Base 64 Codec. (2014).
 * https://aws.amazon.com/blogs/developer/a-fast-and-correct-base-64-codec/
 *
 * Nick Kopp. 2013. Base64 Encoding on a GPU.
 * https://www.codeproject.com/Articles/276993/Base-Encoding-on-a-GPU. (2013).
 */

size_t encode_base64(char *dst, const char *src, size_t srclen,
                     base64_options options) {
  // credit: Wojciech Muła
  uint8_t *out = (uint8_t *)dst;
  constexpr static uint8_t source_table[64] = {
      'A', 'Q', 'g', 'w', 'B', 'R', 'h', 'x', 'C', 'S', 'i', 'y', 'D',
      'T', 'j', 'z', 'E', 'U', 'k', '0', 'F', 'V', 'l', '1', 'G', 'W',
      'm', '2', 'H', 'X', 'n', '3', 'I', 'Y', 'o', '4', 'J', 'Z', 'p',
      '5', 'K', 'a', 'q', '6', 'L', 'b', 'r', '7', 'M', 'c', 's', '8',
      'N', 'd', 't', '9', 'O', 'e', 'u', '+', 'P', 'f', 'v', '/',
  };
  constexpr static uint8_t source_table_url[64] = {
      'A', 'Q', 'g', 'w', 'B', 'R', 'h', 'x', 'C', 'S', 'i', 'y', 'D',
      'T', 'j', 'z', 'E', 'U', 'k', '0', 'F', 'V', 'l', '1', 'G', 'W',
      'm', '2', 'H', 'X', 'n', '3', 'I', 'Y', 'o', '4', 'J', 'Z', 'p',
      '5', 'K', 'a', 'q', '6', 'L', 'b', 'r', '7', 'M', 'c', 's', '8',
      'N', 'd', 't', '9', 'O', 'e', 'u', '-', 'P', 'f', 'v', '_',
  };
  const uint8x16_t v3f = vdupq_n_u8(0x3f);
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  // When trying to load a uint8_t array, Visual Studio might
  // error with: error C2664: '__n128x4 neon_ld4m_q8(const char *)':
  // cannot convert argument 1 from 'const uint8_t [64]' to 'const char *
  const uint8x16x4_t table =
      vld4q_u8((reinterpret_cast<const char *>(
        options & base64_url) ? source_table_url : source_table));
#else
  const uint8x16x4_t table =
      vld4q_u8((options & base64_url) ? source_table_url : source_table);
#endif
  size_t i = 0;
  for (; i + 16 * 3 <= srclen; i += 16 * 3) {
    const uint8x16x3_t in = vld3q_u8((const uint8_t *)src + i);
    uint8x16x4_t result;
    result.val[0] = vshrq_n_u8(in.val[0], 2);
    result.val[1] =
        vandq_u8(vsliq_n_u8(vshrq_n_u8(in.val[1], 4), in.val[0], 4), v3f);
    result.val[2] =
        vandq_u8(vsliq_n_u8(vshrq_n_u8(in.val[2], 6), in.val[1], 2), v3f);
    result.val[3] = vandq_u8(in.val[2], v3f);
    result.val[0] = vqtbl4q_u8(table, result.val[0]);
    result.val[1] = vqtbl4q_u8(table, result.val[1]);
    result.val[2] = vqtbl4q_u8(table, result.val[2]);
    result.val[3] = vqtbl4q_u8(table, result.val[3]);
    vst4q_u8(out, result);
    out += 64;
  }
  out += scalar::base64::tail_encode_base64((char *)out, src + i, srclen - i,
                                            options);

  return size_t((char *)out - dst);
}

static inline void compress(uint8x16_t data, uint16_t mask, char *output) {
  if (mask == 0) {
    vst1q_u8((uint8_t *)output, data);
    return;
  }
  uint8_t mask1 = uint8_t(mask);      // least significant 8 bits
  uint8_t mask2 = uint8_t(mask >> 8); // most significant 8 bits
  uint64x2_t compactmasku64 = {tables::base64::thintable_epi8[mask1],
                               tables::base64::thintable_epi8[mask2]};
  uint8x16_t compactmask = vreinterpretq_u8_u64(compactmasku64);
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  const uint8x16_t off =
      simdutf_make_uint8x16_t(0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8);
#else
  const uint8x16_t off = {0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8};
#endif

  compactmask = vaddq_u8(compactmask, off);
  uint8x16_t pruned = vqtbl1q_u8(data, compactmask);

  int pop1 = tables::base64::BitsSetTable256mul2[mask1];
  // then load the corresponding mask, what it does is to write
  // only the first pop1 bytes from the first 8 bytes, and then
  // it fills in with the bytes from the second 8 bytes + some filling
  // at the end.
  compactmask = vld1q_u8(tables::base64::pshufb_combine_table + pop1 * 8);
  uint8x16_t answer = vqtbl1q_u8(pruned, compactmask);
  vst1q_u8((uint8_t *)output, answer);
}

struct block64 {
  uint8x16_t chunks[4];
};

static_assert(sizeof(block64) == 64, "block64 is not 64 bytes");
template <bool base64_url> uint64_t to_base64_mask(block64 *b, bool *error) {
  uint8x16_t v0f = vdupq_n_u8(0xf);

  uint8x16_t underscore0, underscore1, underscore2, underscore3;
  if (base64_url) {
    underscore0 = vceqq_u8(b->chunks[0], vdupq_n_u8(0x5f));
    underscore1 = vceqq_u8(b->chunks[1], vdupq_n_u8(0x5f));
    underscore2 = vceqq_u8(b->chunks[2], vdupq_n_u8(0x5f));
    underscore3 = vceqq_u8(b->chunks[3], vdupq_n_u8(0x5f));
  } else {
    (void)underscore0;
    (void)underscore1;
    (void)underscore2;
    (void)underscore3;
  }

  uint8x16_t lo_nibbles0 = vandq_u8(b->chunks[0], v0f);
  uint8x16_t lo_nibbles1 = vandq_u8(b->chunks[1], v0f);
  uint8x16_t lo_nibbles2 = vandq_u8(b->chunks[2], v0f);
  uint8x16_t lo_nibbles3 = vandq_u8(b->chunks[3], v0f);

  // Needed by the decoding step.
  uint8x16_t hi_nibbles0 = vshrq_n_u8(b->chunks[0], 4);
  uint8x16_t hi_nibbles1 = vshrq_n_u8(b->chunks[1], 4);
  uint8x16_t hi_nibbles2 = vshrq_n_u8(b->chunks[2], 4);
  uint8x16_t hi_nibbles3 = vshrq_n_u8(b->chunks[3], 4);
  uint8x16_t lut_lo;
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  if (base64_url) {
    lut_lo =
        simdutf_make_uint8x16_t(0x3a,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x61,0xe1,0xf4,0xe5,0xa5,0xf4,0xf4);
  } else {
    lut_lo =
        simdutf_make_uint8x16_t(0x3a,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x61,0xe1,0xb4,0xe5,0xe5,0xf4,0xb4);
  }
#else
  if (base64_url) {
    lut_lo = uint8x16_t{0x3a,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x61,0xe1,0xf4,0xe5,0xa5,0xf4,0xf4};
  } else {
    lut_lo = uint8x16_t{0x3a,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x61,0xe1,0xb4,0xe5,0xe5,0xf4,0xb4};
  }
#endif
  uint8x16_t lo0 = vqtbl1q_u8(lut_lo, lo_nibbles0);
  uint8x16_t lo1 = vqtbl1q_u8(lut_lo, lo_nibbles1);
  uint8x16_t lo2 = vqtbl1q_u8(lut_lo, lo_nibbles2);
  uint8x16_t lo3 = vqtbl1q_u8(lut_lo, lo_nibbles3);
  uint8x16_t lut_hi;
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  if (base64_url) {
    lut_hi =
        simdutf_make_uint8x16_t(0x11, 0x20, 0x42, 0x80, 0x8, 0x4, 0x8, 0x4,
                                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20);
  } else {
    lut_hi =
        simdutf_make_uint8x16_t(0x11, 0x20, 0x42, 0x80, 0x8, 0x4, 0x8, 0x4,
                                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20);
  }
#else
  if (base64_url) {
    lut_hi = uint8x16_t{0x11, 0x20, 0x42, 0x80, 0x8,  0x4,  0x8,  0x4,
              0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
  } else {
    lut_hi = uint8x16_t{0x11, 0x20, 0x42, 0x80, 0x8,  0x4,  0x8,  0x4,
              0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
  }
#endif
  uint8x16_t hi0 = vqtbl1q_u8(lut_hi, hi_nibbles0);
  uint8x16_t hi1 = vqtbl1q_u8(lut_hi, hi_nibbles1);
  uint8x16_t hi2 = vqtbl1q_u8(lut_hi, hi_nibbles2);
  uint8x16_t hi3 = vqtbl1q_u8(lut_hi, hi_nibbles3);

  if (base64_url) {
    hi0 = vbicq_u8(hi0, underscore0);
    hi1 = vbicq_u8(hi1, underscore1);
    hi2 = vbicq_u8(hi2, underscore2);
    hi3 = vbicq_u8(hi3, underscore3);
  }

  uint8_t checks =
      vmaxvq_u8(vorrq_u8(vorrq_u8(vandq_u8(lo0, hi0), vandq_u8(lo1, hi1)),
                         vorrq_u8(vandq_u8(lo2, hi2), vandq_u8(lo3, hi3))));
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  const uint8x16_t bit_mask =
      simdutf_make_uint8x16_t(0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                              0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80);
#else
  const uint8x16_t bit_mask = {0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                               0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
#endif
  uint64_t badcharmask = 0;
  *error = checks > 0x3;
  if (checks) {
    // Add each of the elements next to each other, successively, to stuff each
    // 8 byte mask into one.
    uint8x16_t test0 = vtstq_u8(lo0, hi0);
    uint8x16_t test1 = vtstq_u8(lo1, hi1);
    uint8x16_t test2 = vtstq_u8(lo2, hi2);
    uint8x16_t test3 = vtstq_u8(lo3, hi3);
    uint8x16_t sum0 =
        vpaddq_u8(vandq_u8(test0, bit_mask), vandq_u8(test1, bit_mask));
    uint8x16_t sum1 =
        vpaddq_u8(vandq_u8(test2, bit_mask), vandq_u8(test3, bit_mask));
    sum0 = vpaddq_u8(sum0, sum1);
    sum0 = vpaddq_u8(sum0, sum0);
    badcharmask = vgetq_lane_u64(vreinterpretq_u64_u8(sum0), 0);
  }
  // This is the transformation step that can be done while we are waiting for
  // sum0
  uint8x16_t roll_lut;
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  if (base64_url) {
    roll_lut =
        simdutf_make_uint8x16_t(0xe0, 0x11, 0x13, 0x4, 0xbf, 0xbf, 0xb9, 0xb9,
                                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);
  } else {
    roll_lut =
        simdutf_make_uint8x16_t(0x0, 0x10, 0x13, 0x4, 0xbf, 0xbf, 0xb9, 0xb9,
                                0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);
  }
#else
  if (base64_url) {
    roll_lut = uint8x16_t{0xe0, 0x11, 0x13, 0x4, 0xbf, 0xbf, 0xb9, 0xb9,
                0x0,  0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0};
  } else {
    roll_lut = uint8x16_t{0x0, 0x10, 0x13, 0x4, 0xbf, 0xbf, 0xb9, 0xb9,
                0x0, 0x0,  0x0,  0x0, 0x0,  0x0,  0x0,  0x0};
  }
#endif
  uint8x16_t vsecond_last = base64_url ? vdupq_n_u8(0x2d) : vdupq_n_u8(0x2f);
  if (base64_url) {
    hi_nibbles0 = vbicq_u8(hi_nibbles0, underscore0);
    hi_nibbles1 = vbicq_u8(hi_nibbles1, underscore1);
    hi_nibbles2 = vbicq_u8(hi_nibbles2, underscore2);
    hi_nibbles3 = vbicq_u8(hi_nibbles3, underscore3);
  }
  uint8x16_t roll0 = vqtbl1q_u8(
      roll_lut, vaddq_u8(vceqq_u8(b->chunks[0], vsecond_last), hi_nibbles0));
  uint8x16_t roll1 = vqtbl1q_u8(
      roll_lut, vaddq_u8(vceqq_u8(b->chunks[1], vsecond_last), hi_nibbles1));
  uint8x16_t roll2 = vqtbl1q_u8(
      roll_lut, vaddq_u8(vceqq_u8(b->chunks[2], vsecond_last), hi_nibbles2));
  uint8x16_t roll3 = vqtbl1q_u8(
      roll_lut, vaddq_u8(vceqq_u8(b->chunks[3], vsecond_last), hi_nibbles3));
  b->chunks[0] = vaddq_u8(b->chunks[0], roll0);
  b->chunks[1] = vaddq_u8(b->chunks[1], roll1);
  b->chunks[2] = vaddq_u8(b->chunks[2], roll2);
  b->chunks[3] = vaddq_u8(b->chunks[3], roll3);
  return badcharmask;
}

void copy_block(block64 *b, char *output) {
  vst1q_u8((uint8_t *)output, b->chunks[0]);
  vst1q_u8((uint8_t *)output + 16, b->chunks[1]);
  vst1q_u8((uint8_t *)output + 32, b->chunks[2]);
  vst1q_u8((uint8_t *)output + 48, b->chunks[3]);
}

uint64_t compress_block(block64 *b, uint64_t mask, char *output) {
  uint64_t popcounts =
      vget_lane_u64(vreinterpret_u64_u8(vcnt_u8(vcreate_u8(~mask))), 0);
  uint64_t offsets = popcounts * 0x0101010101010101;
  compress(b->chunks[0], uint16_t(mask), output);
  compress(b->chunks[1], uint16_t(mask >> 16), &output[(offsets >> 8) & 0xFF]);
  compress(b->chunks[2], uint16_t(mask >> 32), &output[(offsets >> 24) & 0xFF]);
  compress(b->chunks[3], uint16_t(mask >> 48), &output[(offsets >> 40) & 0xFF]);
  return offsets >> 56;
}

// The caller of this function is responsible to ensure that there are 64 bytes available
// from reading at src. The data is read into a block64 structure.
void load_block(block64 *b, const char *src) {
  b->chunks[0] = vld1q_u8(reinterpret_cast<const uint8_t *>(src));
  b->chunks[1] = vld1q_u8(reinterpret_cast<const uint8_t *>(src) + 16);
  b->chunks[2] = vld1q_u8(reinterpret_cast<const uint8_t *>(src) + 32);
  b->chunks[3] = vld1q_u8(reinterpret_cast<const uint8_t *>(src) + 48);
}

// The caller of this function is responsible to ensure that there are 32 bytes available
// from reading at data. It returns a 16-byte value, narrowing with saturation the 16-bit words.
inline uint8x16_t load_satured(const uint16_t *data) {
  uint16x8_t in1 = vld1q_u16(data);
  uint16x8_t in2 = vld1q_u16(data + 8);
  return vqmovn_high_u16(vqmovn_u16(in1), in2);
}

// The caller of this function is responsible to ensure that there are 128 bytes available
// from reading at src. The data is read into a block64 structure.
void load_block(block64 *b, const char16_t *src) {
  b->chunks[0] = load_satured(reinterpret_cast<const uint16_t *>(src));
  b->chunks[1] = load_satured(reinterpret_cast<const uint16_t *>(src) + 16);
  b->chunks[2] = load_satured(reinterpret_cast<const uint16_t *>(src) + 32);
  b->chunks[3] = load_satured(reinterpret_cast<const uint16_t *>(src) + 48);
}

// decode 64 bytes and output 48 bytes
void base64_decode_block(char *out, const char *src) {
  uint8x16x4_t str = vld4q_u8((uint8_t *)src);
  uint8x16x3_t outvec;
  outvec.val[0] =
      vorrq_u8(vshlq_n_u8(str.val[0], 2), vshrq_n_u8(str.val[1], 4));
  outvec.val[1] =
      vorrq_u8(vshlq_n_u8(str.val[1], 4), vshrq_n_u8(str.val[2], 2));
  outvec.val[2] = vorrq_u8(vshlq_n_u8(str.val[2], 6), str.val[3]);
  vst3q_u8((uint8_t *)out, outvec);
}

template <bool base64_url, typename char_type>
result compress_decode_base64(char *dst, const char_type *src, size_t srclen,
                              base64_options options) {
  const uint8_t *to_base64 = base64_url ? tables::base64::to_base64_url_value
                                        : tables::base64::to_base64_value;
  size_t equallocation = srclen; // location of the first padding character if any
  // skip trailing spaces
  while (srclen > 0 && scalar::base64::is_eight_byte(src[srclen - 1]) && to_base64[uint8_t(src[srclen - 1])] == 64) {
    srclen--;
  }
  size_t equalsigns = 0;
  if (srclen > 0 && src[srclen - 1] == '=') {
    equallocation = srclen - 1;
    srclen--;
    equalsigns = 1;
    // skip trailing spaces
    while (srclen > 0 && scalar::base64::is_eight_byte(src[srclen - 1]) && to_base64[uint8_t(src[srclen - 1])] == 64) {
      srclen--;
    }
    if (srclen > 0 && src[srclen - 1] == '=') {
      equallocation = srclen - 1;
      srclen--;
      equalsigns = 2;
    }
  }
  const char_type *const srcinit = src;
  const char *const dstinit = dst;
  const char_type *const srcend = src + srclen;

  constexpr size_t block_size = 10;
  char buffer[block_size * 64];
  char *bufferptr = buffer;
  if (srclen >= 64) {
    const char_type *const srcend64 = src + srclen - 64;
    while (src <= srcend64) {
      block64 b;
      load_block(&b, src);
      src += 64;
      bool error = false;
      uint64_t badcharmask = to_base64_mask<base64_url>(&b, &error);
      if(badcharmask){
        if (error) {
          src -= 64;
          while (src < srcend && scalar::base64::is_eight_byte(*src) && to_base64[uint8_t(*src)] <= 64) {
            src++;
          }
          if(src < srcend){
            // should never happen
          }
          return {error_code::INVALID_BASE64_CHARACTER, size_t(src - srcinit)};
        }
      }

      if (badcharmask != 0) {
        // optimization opportunity: check for simple masks like those made of
        // continuous 1s followed by continuous 0s. And masks containing a
        // single bad character.
        bufferptr += compress_block(&b, badcharmask, bufferptr);
      } else {
        // optimization opportunity: if bufferptr == buffer and mask == 0, we
        // can avoid the call to compress_block and decode directly.
        copy_block(&b, bufferptr);
        bufferptr += 64;
      }
      if (bufferptr >= (block_size - 1) * 64 + buffer) {
        for (size_t i = 0; i < (block_size - 1); i++) {
          base64_decode_block(dst, buffer + i * 64);
          dst += 48;
        }
        std::memcpy(buffer, buffer + (block_size - 1) * 64,
                    64); // 64 might be too much
        bufferptr -= (block_size - 1) * 64;
      }
    }
  }
  char *buffer_start = buffer;
  // Optimization note: if this is almost full, then it is worth our
  // time, otherwise, we should just decode directly.
  int last_block = (int)((bufferptr - buffer_start) % 64);
  if (last_block != 0 && srcend - src + last_block >= 64) {
    while ((bufferptr - buffer_start) % 64 != 0 && src < srcend) {
      uint8_t val = to_base64[uint8_t(*src)];
      *bufferptr = char(val);
      if (!scalar::base64::is_eight_byte(*src) || val > 64) {
        return {error_code::INVALID_BASE64_CHARACTER, size_t(src - srcinit)};
      }
      bufferptr += (val <= 63);
      src++;
    }
  }

  for (; buffer_start + 64 <= bufferptr; buffer_start += 64) {
    base64_decode_block(dst, buffer_start);
    dst += 48;
  }
  if ((bufferptr - buffer_start) % 64 != 0) {
    while (buffer_start + 4 < bufferptr) {
      uint32_t triple = ((uint32_t(uint8_t(buffer_start[0])) << 3 * 6) +
                         (uint32_t(uint8_t(buffer_start[1])) << 2 * 6) +
                         (uint32_t(uint8_t(buffer_start[2])) << 1 * 6) +
                         (uint32_t(uint8_t(buffer_start[3])) << 0 * 6))
                        << 8;
      triple = scalar::utf32::swap_bytes(triple);
      std::memcpy(dst, &triple, 4);

      dst += 3;
      buffer_start += 4;
    }
    if (buffer_start + 4 <= bufferptr) {
      uint32_t triple = ((uint32_t(uint8_t(buffer_start[0])) << 3 * 6) +
                         (uint32_t(uint8_t(buffer_start[1])) << 2 * 6) +
                         (uint32_t(uint8_t(buffer_start[2])) << 1 * 6) +
                         (uint32_t(uint8_t(buffer_start[3])) << 0 * 6))
                        << 8;
      triple = scalar::utf32::swap_bytes(triple);
      std::memcpy(dst, &triple, 3);

      dst += 3;
      buffer_start += 4;
    }
    // we may have 1, 2 or 3 bytes left and we need to decode them so let us
    // bring in src content
    int leftover = int(bufferptr - buffer_start);
    if (leftover > 0) {
      while (leftover < 4 && src < srcend) {
        uint8_t val = to_base64[uint8_t(*src)];
        if (!scalar::base64::is_eight_byte(*src) || val > 64) {
          return {error_code::INVALID_BASE64_CHARACTER, size_t(src - srcinit)};
        }
        buffer_start[leftover] = char(val);
        leftover += (val <= 63);
        src++;
      }

      if (leftover == 1) {
        return {BASE64_INPUT_REMAINDER, size_t(dst - dstinit)};
      }
      if (leftover == 2) {
        uint32_t triple = (uint32_t(buffer_start[0]) << 3 * 6) +
                          (uint32_t(buffer_start[1]) << 2 * 6);
        triple = scalar::utf32::swap_bytes(triple);
        triple >>= 8;
        std::memcpy(dst, &triple, 1);
        dst += 1;
      } else if (leftover == 3) {
        uint32_t triple = (uint32_t(buffer_start[0]) << 3 * 6) +
                          (uint32_t(buffer_start[1]) << 2 * 6) +
                          (uint32_t(buffer_start[2]) << 1 * 6);
        triple = scalar::utf32::swap_bytes(triple);
        triple >>= 8;

        std::memcpy(dst, &triple, 2);
        dst += 2;
      } else {
        uint32_t triple = ((uint32_t(uint8_t(buffer_start[0])) << 3 * 6) +
                           (uint32_t(uint8_t(buffer_start[1])) << 2 * 6) +
                           (uint32_t(uint8_t(buffer_start[2])) << 1 * 6) +
                           (uint32_t(uint8_t(buffer_start[3])) << 0 * 6))
                          << 8;
        triple = scalar::utf32::swap_bytes(triple);
        std::memcpy(dst, &triple, 3);
        dst += 3;
      }
    }
  }
  if (src < srcend + equalsigns) {
    result r =
        scalar::base64::base64_tail_decode(dst, src, srcend - src, options);
    if (r.error == error_code::INVALID_BASE64_CHARACTER) {
      r.count += size_t(src - srcinit);
      return r;
    } else {
      r.count += size_t(dst - dstinit);
    }
    if(r.error == error_code::SUCCESS && equalsigns > 0) {
      // additional checks
      if((r.count % 3 == 0) || ((r.count % 3) + 1 + equalsigns != 4)) {
        r.error = error_code::INVALID_BASE64_CHARACTER;
        r.count = equallocation;
      }
    }
    return r;
  }
  if(equalsigns > 0) {
    if((size_t(dst - dstinit) % 3 == 0) || ((size_t(dst - dstinit) % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
  }
  return {SUCCESS, size_t(dst - dstinit)};
}
