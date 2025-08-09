// file included directly
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

struct block64 {
  __m512i chunks[1];
};

template <bool base64_url>
size_t encode_base64(char *dst, const char *src, size_t srclen,
                     base64_options options) {
  // credit: Wojciech Muła
  const uint8_t *input = (const uint8_t *)src;

  uint8_t *out = (uint8_t *)dst;
  static const char *lookup_tbl =
      base64_url
          ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
          : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  const __m512i shuffle_input = _mm512_setr_epi32(
      0x01020001, 0x04050304, 0x07080607, 0x0a0b090a, 0x0d0e0c0d, 0x10110f10,
      0x13141213, 0x16171516, 0x191a1819, 0x1c1d1b1c, 0x1f201e1f, 0x22232122,
      0x25262425, 0x28292728, 0x2b2c2a2b, 0x2e2f2d2e);
  const __m512i lookup =
      _mm512_loadu_si512(reinterpret_cast<const __m512i *>(lookup_tbl));
  const __m512i multi_shifts = _mm512_set1_epi64(UINT64_C(0x3036242a1016040a));
  size_t i = 0;
  for (; i + 64 <= srclen; i += 48) {
    const __m512i v =
        _mm512_loadu_si512(reinterpret_cast<const __m512i *>(input + i));
    const __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
    const __m512i indices = _mm512_multishift_epi64_epi8(multi_shifts, in);
    const __m512i result = _mm512_permutexvar_epi8(indices, lookup);
    _mm512_storeu_si512(reinterpret_cast<__m512i *>(out), result);
    out += 64;
  }
  return i / 3 * 4 + scalar::base64::tail_encode_base64((char *)out, src + i,
                                                        srclen - i, options);
}

template <bool base64_url>
static inline uint64_t to_base64_mask(block64 *b, bool *error) {
  __m512i input = b->chunks[0];
  const __m512i ascii_space_tbl = _mm512_set_epi8(
      0, 0, 13, 12, 0, 10, 9, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 13, 12, 0, 10, 9,
      0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 13, 12, 0, 10, 9, 0, 0, 0, 0, 0, 0, 0, 0,
      32, 0, 0, 13, 12, 0, 10, 9, 0, 0, 0, 0, 0, 0, 0, 0, 32);
  __m512i lookup0;
  if (base64_url) {
    lookup0 = _mm512_set_epi8(
        -128, -128, -128, -128, -128, -128, 61, 60, 59, 58, 57, 56, 55, 54, 53,
        52, -128, -128, 62, -128, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -128, -1, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -1,
        -128, -128, -1, -1, -128, -128, -128, -128, -128, -128, -128, -128, -1);
  } else {
    lookup0 = _mm512_set_epi8(
        -128, -128, -128, -128, -128, -128, 61, 60, 59, 58, 57, 56, 55, 54, 53,
        52, 63, -128, -128, -128, 62, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -1, -128, -128, -128, -128, -128, -128, -128, -128,
        -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -1, -128,
        -128, -1, -1, -128, -128, -128, -128, -128, -128, -128, -128, -128);
  }
  __m512i lookup1;
  if (base64_url) {
    lookup1 = _mm512_set_epi8(
        -128, -128, -128, -128, -128, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,
        41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, -128,
        63, -128, -128, -128, -128, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
        14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -128);
  } else {
    lookup1 = _mm512_set_epi8(
        -128, -128, -128, -128, -128, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,
        41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, -128,
        -128, -128, -128, -128, -128, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -128);
  }

  const __m512i translated = _mm512_permutex2var_epi8(lookup0, input, lookup1);
  const __m512i combined = _mm512_or_si512(translated, input);
  const __mmask64 mask = _mm512_movepi8_mask(combined);
  if (mask) {
    const __mmask64 spaces = _mm512_cmpeq_epi8_mask(
        _mm512_shuffle_epi8(ascii_space_tbl, input), input);
    *error |= (mask != spaces);
  }
  b->chunks[0] = translated;

  return mask;
}

static inline void copy_block(block64 *b, char *output) {
  _mm512_storeu_si512(reinterpret_cast<__m512i *>(output), b->chunks[0]);
}

static inline uint64_t compress_block(block64 *b, uint64_t mask, char *output) {
  uint64_t nmask = ~mask;
  __m512i c = _mm512_maskz_compress_epi8(nmask, b->chunks[0]);
  _mm512_storeu_si512(reinterpret_cast<__m512i *>(output), c);
  return _mm_popcnt_u64(nmask);
}

// The caller of this function is responsible to ensure that there are 64 bytes available
// from reading at src. The data is read into a block64 structure.
static inline void load_block(block64 *b, const char *src) {
  b->chunks[0] = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(src));
}

// The caller of this function is responsible to ensure that there are 128 bytes available
// from reading at src. The data is read into a block64 structure.
static inline void load_block(block64 *b, const char16_t *src) {
  __m512i m1 = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(src));
  __m512i m2 = _mm512_loadu_si512(reinterpret_cast<const __m512i *>(src + 32));
  __m512i p = _mm512_packus_epi16(m1, m2);
  b->chunks[0] =
      _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), p);
}

static inline void base64_decode(char *out, __m512i str) {
  const __m512i merge_ab_and_bc =
      _mm512_maddubs_epi16(str, _mm512_set1_epi32(0x01400140));
  const __m512i merged =
      _mm512_madd_epi16(merge_ab_and_bc, _mm512_set1_epi32(0x00011000));
  const __m512i pack = _mm512_set_epi8(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 61, 62, 56, 57, 58,
      52, 53, 54, 48, 49, 50, 44, 45, 46, 40, 41, 42, 36, 37, 38, 32, 33, 34,
      28, 29, 30, 24, 25, 26, 20, 21, 22, 16, 17, 18, 12, 13, 14, 8, 9, 10, 4,
      5, 6, 0, 1, 2);
  const __m512i shuffled = _mm512_permutexvar_epi8(pack, merged);
  _mm512_mask_storeu_epi8(
      (__m512i *)out, 0xffffffffffff,
      shuffled); // mask would be 0xffffffffffff since we write 48 bytes.
}
// decode 64 bytes and output 48 bytes
static inline void base64_decode_block(char *out, const char *src) {
  base64_decode(out,
                _mm512_loadu_si512(reinterpret_cast<const __m512i *>(src)));
}
static inline void base64_decode_block(char *out, block64 *b) {
  base64_decode(out, b->chunks[0]);
}

template <bool base64_url, typename chartype>
result compress_decode_base64(char *dst, const chartype *src, size_t srclen,
                              base64_options options) {
  const uint8_t *to_base64 = base64_url ? tables::base64::to_base64_url_value
                                        : tables::base64::to_base64_value;
  size_t equallocation = srclen; // location of the first padding character if any
  size_t equalsigns = 0;
  // skip trailing spaces
  while (srclen > 0 && scalar::base64::is_eight_byte(src[srclen - 1]) && to_base64[uint8_t(src[srclen - 1])] == 64) {
    srclen--;
  }
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
  const chartype *const srcinit = src;
  const char *const dstinit = dst;
  const chartype *const srcend = src + srclen;

  // figure out why block_size == 2 is sometimes best???
  constexpr size_t block_size = 6;
  char buffer[block_size * 64];
  char *bufferptr = buffer;
  if (srclen >= 64) {
    const chartype *const srcend64 = src + srclen - 64;
    while (src <= srcend64) {
      block64 b;
      load_block(&b, src);
      src += 64;
      bool error = false;
      uint64_t badcharmask = to_base64_mask<base64_url>(&b, &error);
      if (error) {
        src -= 64;
        while (src < srcend && scalar::base64::is_eight_byte(*src) && to_base64[uint8_t(*src)] <= 64) {
          src++;
        }
        return {error_code::INVALID_BASE64_CHARACTER, size_t(src - srcinit)};
      }
      if (badcharmask != 0) {
        // optimization opportunity: check for simple masks like those made of
        // continuous 1s followed by continuous 0s. And masks containing a
        // single bad character.
        bufferptr += compress_block(&b, badcharmask, bufferptr);
      } else if (bufferptr != buffer) {
        copy_block(&b, bufferptr);
        bufferptr += 64;
      } else {
        base64_decode_block(dst, &b);
        dst += 48;
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