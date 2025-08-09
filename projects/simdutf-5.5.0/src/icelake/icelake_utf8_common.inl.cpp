// Common procedures for both validating and non-validating conversions from UTF-8.
enum block_processing_mode { SIMDUTF_FULL, SIMDUTF_TAIL};

using utf8_to_utf16_result = std::pair<const char*, char16_t*>;
using utf8_to_utf32_result = std::pair<const char*, uint32_t*>;

/*
    process_block_utf8_to_utf16 converts up to 64 bytes from 'in' from UTF-8
    to UTF-16. When tail = SIMDUTF_FULL, then the full input buffer (64 bytes)
    might be used. When tail = SIMDUTF_TAIL, we take into account 'gap' which
    indicates how many input bytes are relevant.

    Returns true when the result is correct, otherwise it returns false.

    The provided in and out pointers are advanced according to how many input
    bytes have been processed, upon success.
*/
template <block_processing_mode tail, endianness big_endian>
simdutf_really_inline bool process_block_utf8_to_utf16(const char *&in, char16_t *&out, size_t gap) {
  // constants
  __m512i mask_identity = _mm512_set_epi8(63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
  __m512i mask_c0c0c0c0 = _mm512_set1_epi32(0xc0c0c0c0);
  __m512i mask_80808080 = _mm512_set1_epi32(0x80808080);
  __m512i mask_f0f0f0f0 = _mm512_set1_epi32(0xf0f0f0f0);
  __m512i mask_dfdfdfdf_tail = _mm512_set_epi64(0xffffdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf, 0xdfdfdfdfdfdfdfdf);
  __m512i mask_c2c2c2c2 = _mm512_set1_epi32(0xc2c2c2c2);
  __m512i mask_ffffffff = _mm512_set1_epi32(0xffffffff);
  __m512i mask_d7c0d7c0 = _mm512_set1_epi32(0xd7c0d7c0);
  __m512i mask_dc00dc00 = _mm512_set1_epi32(0xdc00dc00);
  __m512i byteflip = _mm512_setr_epi64(
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809
        );
  // Note that 'tail' is a compile-time constant !
  __mmask64 b = (tail == SIMDUTF_FULL) ? 0xFFFFFFFFFFFFFFFF : (uint64_t(1) << gap) - 1;
  __m512i input = (tail == SIMDUTF_FULL) ? _mm512_loadu_si512(in) : _mm512_maskz_loadu_epi8(b, in);
  __mmask64 m1 = (tail == SIMDUTF_FULL) ? _mm512_cmplt_epu8_mask(input, mask_80808080) : _mm512_mask_cmplt_epu8_mask(b, input, mask_80808080);
  if(_ktestc_mask64_u8(m1, b)) {// NOT(m1) AND b -- if all zeroes, then all ASCII
  // alternatively, we could do 'if (m1 == b) { '
    if (tail == SIMDUTF_FULL) {
      in += 64;          // consumed 64 bytes
      // we convert a full 64-byte block, writing 128 bytes.
      __m512i input1 = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(input));
      if(big_endian) { input1 = _mm512_shuffle_epi8(input1, byteflip); }
      _mm512_storeu_si512(out, input1);
      out += 32;
      __m512i input2 = _mm512_cvtepu8_epi16(_mm512_extracti64x4_epi64(input, 1));
      if(big_endian) { input2 = _mm512_shuffle_epi8(input2, byteflip); }
      _mm512_storeu_si512(out, input2);
      out += 32;
      return true; // we are done
    } else {
      in += gap;
      if (gap <= 32) {
        __m512i input1 = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(input));
        if(big_endian) { input1 = _mm512_shuffle_epi8(input1, byteflip); }
        _mm512_mask_storeu_epi16(out, __mmask32((uint64_t(1) << (gap)) - 1), input1);
        out += gap;
      } else {
        __m512i input1 = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(input));
        if(big_endian) { input1 = _mm512_shuffle_epi8(input1, byteflip); }
        _mm512_storeu_si512(out, input1);
        out += 32;
        __m512i input2 = _mm512_cvtepu8_epi16(_mm512_extracti64x4_epi64(input, 1));
        if(big_endian) { input2 = _mm512_shuffle_epi8(input2, byteflip); }
        _mm512_mask_storeu_epi16(out, __mmask32((uint32_t(1) << (gap - 32)) - 1), input2);
        out += gap - 32;
      }
      return true; // we are done
    }
  }
  // classify characters further
  __mmask64 m234 = _mm512_cmp_epu8_mask(mask_c0c0c0c0, input,
                                        _MM_CMPINT_LE); // 0xc0 <= input, 2, 3, or 4 leading byte
  __mmask64 m34 = _mm512_cmp_epu8_mask(mask_dfdfdfdf_tail, input,
                                       _MM_CMPINT_LT); // 0xdf < input,  3 or 4 leading byte

  __mmask64 milltwobytes = _mm512_mask_cmp_epu8_mask(m234, input, mask_c2c2c2c2,
                                                     _MM_CMPINT_LT); // 0xc0 <= input < 0xc2 (illegal two byte sequence)
                                                                     // Overlong 2-byte sequence
  if (_ktestz_mask64_u8(milltwobytes, milltwobytes) == 0) {
    // Overlong 2-byte sequence
    return false;
  }
  if (_ktestz_mask64_u8(m34, m34) == 0) {
    // We have a 3-byte sequence and/or a 2-byte sequence, or possibly even a 4-byte sequence!
    __mmask64 m4 = _mm512_cmp_epu8_mask(input, mask_f0f0f0f0,
                                        _MM_CMPINT_NLT); // 0xf0 <= zmm0 (4 byte start bytes)

    __mmask64 mask_not_ascii = (tail == SIMDUTF_FULL) ? _knot_mask64(m1) : _kand_mask64(_knot_mask64(m1), b);

    __mmask64 mp1 = _kshiftli_mask64(m234, 1);
    __mmask64 mp2 = _kshiftli_mask64(m34, 2);
    // We could do it as follows...
    // if (_kortestz_mask64_u8(m4,m4)) { // compute the bitwise OR of the 64-bit masks a and b and return 1 if all zeroes
    // but GCC generates better code when we do:
    if (m4 == 0) { // compute the bitwise OR of the 64-bit masks a and b and return 1 if all zeroes
      // Fast path with 1,2,3 bytes
      __mmask64 mc = _kor_mask64(mp1, mp2); // expected continuation bytes
      __mmask64 m1234 = _kor_mask64(m1, m234);
      // mismatched continuation bytes:
      if (tail == SIMDUTF_FULL) {
        __mmask64 xnormcm1234 = _kxnor_mask64(mc, m1234); // XNOR of mc and m1234 should be all zero if they differ
        // the presence of a 1 bit indicates that they overlap.
        // _kortestz_mask64_u8: compute the bitwise OR of 64-bit masksand return 1 if all zeroes.
        if (!_kortestz_mask64_u8(xnormcm1234, xnormcm1234)) { return false; }
      } else {
        __mmask64 bxorm1234 = _kxor_mask64(b, m1234);
        if (mc != bxorm1234) { return false; }
      }
      // mend: identifying the last bytes of each sequence to be decoded
      __mmask64 mend = _kshiftri_mask64(m1234, 1);
      if (tail != SIMDUTF_FULL) {
        mend = _kor_mask64(mend, (uint64_t(1) << (gap - 1)));
      }


      __m512i last_and_third = _mm512_maskz_compress_epi8(mend, mask_identity);
      __m512i last_and_thirdu16 = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(last_and_third));

      __m512i nonasciitags = _mm512_maskz_mov_epi8(mask_not_ascii, mask_c0c0c0c0); // ASCII: 00000000  other: 11000000
      __m512i clearedbytes = _mm512_andnot_si512(nonasciitags, input);             // high two bits cleared where not ASCII
      __m512i lastbytes = _mm512_maskz_permutexvar_epi8(0x5555555555555555, last_and_thirdu16,
                                                        clearedbytes); // the last byte of each character

      __mmask64 mask_before_non_ascii = _kshiftri_mask64(mask_not_ascii, 1);               // bytes that precede non-ASCII bytes
      __m512i indexofsecondlastbytes = _mm512_add_epi16(mask_ffffffff, last_and_thirdu16); // indices of the second last bytes
      __m512i beforeasciibytes = _mm512_maskz_mov_epi8(mask_before_non_ascii, clearedbytes);
      __m512i secondlastbytes = _mm512_maskz_permutexvar_epi8(0x5555555555555555, indexofsecondlastbytes,
                                                              beforeasciibytes); // the second last bytes (of two, three byte seq,
                                                                                 // surrogates)
      secondlastbytes = _mm512_slli_epi16(secondlastbytes, 6);                   // shifted into position

      __m512i indexofthirdlastbytes = _mm512_add_epi16(mask_ffffffff,
                                                       indexofsecondlastbytes); // indices of the second last bytes
      __m512i thirdlastbyte = _mm512_maskz_mov_epi8(m34,
                                                    clearedbytes); // only those that are the third last byte of a sequence
      __m512i thirdlastbytes = _mm512_maskz_permutexvar_epi8(0x5555555555555555, indexofthirdlastbytes,
                                                             thirdlastbyte); // the third last bytes (of three byte sequences, hi
                                                                             // surrogate)
      thirdlastbytes = _mm512_slli_epi16(thirdlastbytes, 12);                // shifted into position
      __m512i Wout = _mm512_ternarylogic_epi32(lastbytes, secondlastbytes, thirdlastbytes, 254);
      // the elements of Wout excluding the last element if it happens to be a high surrogate:

      __mmask64 mprocessed = (tail == SIMDUTF_FULL) ? _pdep_u64(0xFFFFFFFF, mend) : _pdep_u64(0xFFFFFFFF, _kand_mask64(mend, b)); // we adjust mend at the end of the output.


      // Encodings out of range...
      {
        // the location of 3-byte sequence start bytes in the input
        __mmask64 m3 = m34 & (b ^ m4);
        // code units in Wout corresponding to 3-byte sequences.
        __mmask32 M3 = __mmask32(_pext_u64(m3 << 2, mend));
        __m512i mask_08000800 = _mm512_set1_epi32(0x08000800);
        __mmask32 Msmall800 = _mm512_mask_cmplt_epu16_mask(M3, Wout, mask_08000800);
        __m512i mask_d800d800 = _mm512_set1_epi32(0xd800d800);
        __m512i Moutminusd800 = _mm512_sub_epi16(Wout, mask_d800d800);
        __mmask32 M3s = _mm512_mask_cmplt_epu16_mask(M3, Moutminusd800, mask_08000800);
        if (_kor_mask32(Msmall800, M3s)) { return false; }
      }
      int64_t nout = _mm_popcnt_u64(mprocessed);
      in +=  64 - _lzcnt_u64(mprocessed);
      if(big_endian) { Wout = _mm512_shuffle_epi8(Wout, byteflip); }
      _mm512_mask_storeu_epi16(out, __mmask32((uint64_t(1) << nout) - 1), Wout);
      out += nout;
      return true; // ok
    }
    //
    // We have a 4-byte sequence, this is the general case.
    // Slow!
    __mmask64 mp3 = _kshiftli_mask64(m4, 3);
    __mmask64 mc = _kor_mask64(_kor_mask64(mp1, mp2), mp3); // expected continuation bytes
    __mmask64 m1234 = _kor_mask64(m1, m234);

    // mend: identifying the last bytes of each sequence to be decoded
    __mmask64 mend = _kor_mask64(_kshiftri_mask64(_kor_mask64(mp3, m1234), 1), mp3);
    if (tail != SIMDUTF_FULL) {
      mend = _kor_mask64(mend, __mmask64(uint64_t(1) << (gap - 1)));
    }
    __m512i last_and_third = _mm512_maskz_compress_epi8(mend, mask_identity);
    __m512i last_and_thirdu16 = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(last_and_third));

    __m512i nonasciitags = _mm512_maskz_mov_epi8(mask_not_ascii, mask_c0c0c0c0); // ASCII: 00000000  other: 11000000
    __m512i clearedbytes = _mm512_andnot_si512(nonasciitags, input);             // high two bits cleared where not ASCII
    __m512i lastbytes = _mm512_maskz_permutexvar_epi8(0x5555555555555555, last_and_thirdu16,
                                                      clearedbytes); // the last byte of each character

    __mmask64 mask_before_non_ascii = _kshiftri_mask64(mask_not_ascii, 1);               // bytes that precede non-ASCII bytes
    __m512i indexofsecondlastbytes = _mm512_add_epi16(mask_ffffffff, last_and_thirdu16); // indices of the second last bytes
    __m512i beforeasciibytes = _mm512_maskz_mov_epi8(mask_before_non_ascii, clearedbytes);
    __m512i secondlastbytes = _mm512_maskz_permutexvar_epi8(0x5555555555555555, indexofsecondlastbytes,
                                                            beforeasciibytes); // the second last bytes (of two, three byte seq,
                                                                               // surrogates)
    secondlastbytes = _mm512_slli_epi16(secondlastbytes, 6);                   // shifted into position

    __m512i indexofthirdlastbytes = _mm512_add_epi16(mask_ffffffff,
                                                     indexofsecondlastbytes); // indices of the second last bytes
    __m512i thirdlastbyte = _mm512_maskz_mov_epi8(m34,
                                                  clearedbytes); // only those that are the third last byte of a sequence
    __m512i thirdlastbytes = _mm512_maskz_permutexvar_epi8(0x5555555555555555, indexofthirdlastbytes,
                                                           thirdlastbyte); // the third last bytes (of three byte sequences, hi
                                                                           // surrogate)
    thirdlastbytes = _mm512_slli_epi16(thirdlastbytes, 12);                // shifted into position
    __m512i thirdsecondandlastbytes = _mm512_ternarylogic_epi32(lastbytes, secondlastbytes, thirdlastbytes, 254);
    uint64_t Mlo_uint64 = _pext_u64(mp3, mend);
    __mmask32 Mlo = __mmask32(Mlo_uint64);
    __mmask32 Mhi = __mmask32(Mlo_uint64 >> 1);
    __m512i lo_surr_mask = _mm512_maskz_mov_epi16(Mlo,
                                                  mask_dc00dc00); // lo surr: 1101110000000000, other:  0000000000000000
    __m512i shifted4_thirdsecondandlastbytes = _mm512_srli_epi16(thirdsecondandlastbytes,
                                                                 4); // hi surr: 00000WVUTSRQPNML  vuts = WVUTS - 1
    __m512i tagged_lo_surrogates = _mm512_or_si512(thirdsecondandlastbytes,
                                                   lo_surr_mask); // lo surr: 110111KJHGFEDCBA, other:  unchanged
    __m512i Wout = _mm512_mask_add_epi16(tagged_lo_surrogates, Mhi, shifted4_thirdsecondandlastbytes,
                                         mask_d7c0d7c0); // hi sur: 110110vutsRQPNML, other:  unchanged
    // the elements of Wout excluding the last element if it happens to be a high surrogate:
    __mmask32 Mout = ~(Mhi & 0x80000000);
    __mmask64 mprocessed = (tail == SIMDUTF_FULL) ? _pdep_u64(Mout, mend) : _pdep_u64(Mout, _kand_mask64(mend, b)); // we adjust mend at the end of the output.


    // mismatched continuation bytes:
    if (tail == SIMDUTF_FULL) {
      __mmask64 xnormcm1234 = _kxnor_mask64(mc, m1234); // XNOR of mc and m1234 should be all zero if they differ
      // the presence of a 1 bit indicates that they overlap.
      // _kortestz_mask64_u8: compute the bitwise OR of 64-bit masksand return 1 if all zeroes.
      if (!_kortestz_mask64_u8(xnormcm1234, xnormcm1234)) { return false; }
    } else {
      __mmask64 bxorm1234 = _kxor_mask64(b, m1234);
      if (mc != bxorm1234) { return false; }
    }
    // Encodings out of range...
    {
      // the location of 3-byte sequence start bytes in the input
      __mmask64 m3 = m34 & (b ^ m4);
      // code units in Wout corresponding to 3-byte sequences.
      __mmask32 M3 = __mmask32(_pext_u64(m3 << 2, mend));
      __m512i mask_08000800 = _mm512_set1_epi32(0x08000800);
      __mmask32 Msmall800 = _mm512_mask_cmplt_epu16_mask(M3, Wout, mask_08000800);
      __m512i mask_d800d800 = _mm512_set1_epi32(0xd800d800);
      __m512i Moutminusd800 = _mm512_sub_epi16(Wout, mask_d800d800);
      __mmask32 M3s = _mm512_mask_cmplt_epu16_mask(M3, Moutminusd800, mask_08000800);
      __m512i mask_04000400 = _mm512_set1_epi32(0x04000400);
      __mmask32 M4s = _mm512_mask_cmpge_epu16_mask(Mhi, Moutminusd800, mask_04000400);
      if (!_kortestz_mask32_u8(M4s, _kor_mask32(Msmall800, M3s))) { return false; }
    }
    in += 64 - _lzcnt_u64(mprocessed);
    int64_t nout = _mm_popcnt_u64(mprocessed);
    if(big_endian) { Wout = _mm512_shuffle_epi8(Wout, byteflip); }
    _mm512_mask_storeu_epi16(out, __mmask32((uint64_t(1) << nout) - 1), Wout);
    out += nout;
    return true; // ok
  }
  // Fast path 2: all ASCII or 2 byte
  __mmask64 continuation_or_ascii = (tail == SIMDUTF_FULL) ? _knot_mask64(m234) : _kand_mask64(_knot_mask64(m234), b);
  // on top of -0xc0 we subtract -2 which we get back later of the
  // continuation byte tags
  __m512i leading2byte = _mm512_maskz_sub_epi8(m234, input, mask_c2c2c2c2);
  __mmask64 leading = tail == (tail == SIMDUTF_FULL) ? _kor_mask64(m1, m234) : _kand_mask64(_kor_mask64(m1, m234), b); // first bytes of each sequence
  if (tail == SIMDUTF_FULL) {
    __mmask64 xnor234leading = _kxnor_mask64(_kshiftli_mask64(m234, 1), leading);
    if (!_kortestz_mask64_u8(xnor234leading, xnor234leading)) { return false; }
  } else {
    __mmask64 bxorleading = _kxor_mask64(b, leading);
    if (_kshiftli_mask64(m234, 1) != bxorleading) { return false; }
  }
  //
  if (tail == SIMDUTF_FULL) {
    // In the two-byte/ASCII scenario, we are easily latency bound, so we want
    // to increment the input buffer as quickly as possible.
    // We process 32 bytes unless the byte at index 32 is a continuation byte,
    // in which case we include it as well for a total of 33 bytes.
    // Note that if x is an ASCII byte, then the following is false:
    // int8_t(x) <= int8_t(0xc0) under two's complement.
    in += 32;
    if(int8_t(*in) <= int8_t(0xc0)) in++;
    // The alternative is to do
    // in += 64 - _lzcnt_u64(_pdep_u64(0xFFFFFFFF, continuation_or_ascii));
    // but it requires loading the input, doing the mask computation, and converting
    // back the mask to a general register. It just takes too long, leaving the
    // processor likely to be idle.
  } else {
    in += 64 - _lzcnt_u64(_pdep_u64(0xFFFFFFFF, continuation_or_ascii));
  }
  __m512i lead = _mm512_maskz_compress_epi8(leading, leading2byte);          // will contain zero for ascii, and the data
  lead = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(lead));                 // ... zero extended into code units
  __m512i follow = _mm512_maskz_compress_epi8(continuation_or_ascii, input); // the last bytes of each sequence
  follow = _mm512_cvtepu8_epi16(_mm512_castsi512_si256(follow));             // ... zero extended into code units
  lead = _mm512_slli_epi16(lead, 6);                                         // shifted into position
  __m512i final = _mm512_add_epi16(follow, lead);                            // combining lead and follow

  if(big_endian) { final = _mm512_shuffle_epi8(final, byteflip); }
  if (tail == SIMDUTF_FULL) {
    // Next part is UTF-16 specific and can be generalized to UTF-32.
    int nout = _mm_popcnt_u32(uint32_t(leading));
    _mm512_mask_storeu_epi16(out, __mmask32((uint64_t(1) << nout) - 1), final);
    out += nout; // UTF-8 to UTF-16 is only expansionary in this case.
  } else {
    int nout = int(_mm_popcnt_u64(_pdep_u64(0xFFFFFFFF, leading)));
    _mm512_mask_storeu_epi16(out, __mmask32((uint64_t(1) << nout) - 1), final);
    out += nout; // UTF-8 to UTF-16 is only expansionary in this case.
  }

  return true; // we are fine.
}




/*
    utf32_to_utf16_masked converts `count` lower UTF-32 code units
    from input `utf32` into UTF-16. It differs from utf32_to_utf16
    in that it 'masks' the writes.

    Returns how many 16-bit code units were stored.

    byteflip is used for flipping 16-bit code units, and it should be
        __m512i byteflip = _mm512_setr_epi64(
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809
        );
    We pass it to the (always inlined) function to encourage the compiler to
    keep the value in a (constant) register.
*/
template <endianness big_endian>
simdutf_really_inline size_t utf32_to_utf16_masked(const __m512i byteflip, __m512i utf32, unsigned int count, char16_t* output) {

    const __mmask16 valid = uint16_t((1 << count) - 1);
    // 1. check if we have any surrogate pairs
    const __m512i v_0000_ffff = _mm512_set1_epi32(0x0000ffff);
    const __mmask16 sp_mask = _mm512_mask_cmpgt_epu32_mask(valid, utf32, v_0000_ffff);

    if (sp_mask == 0) {
        if(big_endian) {
          _mm256_mask_storeu_epi16((__m256i*)output, valid, _mm256_shuffle_epi8(_mm512_cvtepi32_epi16(utf32), _mm512_castsi512_si256(byteflip)));

        } else {
          _mm256_mask_storeu_epi16((__m256i*)output, valid, _mm512_cvtepi32_epi16(utf32));
        }
        return count;
    }

    {
        // build surrogate pair code units in 32-bit lanes

        //    t0 = 8 x [000000000000aaaa|aaaaaabbbbbbbbbb]
        const __m512i v_0001_0000 = _mm512_set1_epi32(0x00010000);
        const __m512i t0 = _mm512_sub_epi32(utf32, v_0001_0000);

        //    t1 = 8 x [000000aaaaaaaaaa|bbbbbbbbbb000000]
        const __m512i t1 = _mm512_slli_epi32(t0, 6);

        //    t2 = 8 x [000000aaaaaaaaaa|aaaaaabbbbbbbbbb] -- copy hi word from t1 to t0
        //         0xe4 = (t1 and v_ffff_0000) or (t0 and not v_ffff_0000)
        const __m512i v_ffff_0000 = _mm512_set1_epi32(0xffff0000);
        const __m512i t2 = _mm512_ternarylogic_epi32(t1, t0, v_ffff_0000, 0xe4);

        //    t2 = 8 x [110110aaaaaaaaaa|110111bbbbbbbbbb] -- copy hi word from t1 to t0
        //         0xba = (t2 and not v_fc00_fc000) or v_d800_dc00
        const __m512i v_fc00_fc00 = _mm512_set1_epi32(0xfc00fc00);
        const __m512i v_d800_dc00 = _mm512_set1_epi32(0xd800dc00);
        const __m512i t3 = _mm512_ternarylogic_epi32(t2, v_fc00_fc00, v_d800_dc00, 0xba);
        const __m512i t4 = _mm512_mask_blend_epi32(sp_mask, utf32, t3);
        __m512i t5 = _mm512_ror_epi32(t4, 16);
        // Here we want to trim all of the upper 16-bit code units from the 2-byte
        // characters represented as 4-byte values. We can compute it from
        // sp_mask or the following... It can be more optimized!
        const  __mmask32 nonzero = _kor_mask32(0xaaaaaaaa,_mm512_cmpneq_epi16_mask(t5, _mm512_setzero_si512()));
        const  __mmask32 nonzero_masked = _kand_mask32(nonzero, __mmask32((uint64_t(1) << (2*count)) - 1));
        if(big_endian) { t5 = _mm512_shuffle_epi8(t5, byteflip); }
        // we deliberately avoid _mm512_mask_compressstoreu_epi16 for portability (zen4)
        __m512i compressed = _mm512_maskz_compress_epi16(nonzero_masked, t5);
        _mm512_mask_storeu_epi16(output, (1<<(count + static_cast<unsigned int>(count_ones(sp_mask)))) - 1, compressed);
        //_mm512_mask_compressstoreu_epi16(output, nonzero_masked, t5);
    }

    return count + static_cast<unsigned int>(count_ones(sp_mask));
}

/*
    utf32_to_utf16 converts `count` lower UTF-32 code units
    from input `utf32` into UTF-16. It may overflow.

    Returns how many 16-bit code units were stored.

    byteflip is used for flipping 16-bit code units, and it should be
        __m512i byteflip = _mm512_setr_epi64(
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809
        );
    We pass it to the (always inlined) function to encourage the compiler to
    keep the value in a (constant) register.
*/
template <endianness big_endian>
simdutf_really_inline size_t utf32_to_utf16(const __m512i byteflip, __m512i utf32, unsigned int count, char16_t* output) {
    // check if we have any surrogate pairs
    const __m512i v_0000_ffff = _mm512_set1_epi32(0x0000ffff);
    const __mmask16 sp_mask = _mm512_cmpgt_epu32_mask(utf32, v_0000_ffff);

    if (sp_mask == 0) {
        // technically, it should be _mm256_storeu_epi16
        if(big_endian) {
          _mm256_storeu_si256((__m256i*)output, _mm256_shuffle_epi8(_mm512_cvtepi32_epi16(utf32),_mm512_castsi512_si256(byteflip)));
        } else {
          _mm256_storeu_si256((__m256i*)output, _mm512_cvtepi32_epi16(utf32));
        }
        return count;
    }

    {
        // build surrogate pair code units in 32-bit lanes

        //    t0 = 8 x [000000000000aaaa|aaaaaabbbbbbbbbb]
        const __m512i v_0001_0000 = _mm512_set1_epi32(0x00010000);
        const __m512i t0 = _mm512_sub_epi32(utf32, v_0001_0000);

        //    t1 = 8 x [000000aaaaaaaaaa|bbbbbbbbbb000000]
        const __m512i t1 = _mm512_slli_epi32(t0, 6);

        //    t2 = 8 x [000000aaaaaaaaaa|aaaaaabbbbbbbbbb] -- copy hi word from t1 to t0
        //         0xe4 = (t1 and v_ffff_0000) or (t0 and not v_ffff_0000)
        const __m512i v_ffff_0000 = _mm512_set1_epi32(0xffff0000);
        const __m512i t2 = _mm512_ternarylogic_epi32(t1, t0, v_ffff_0000, 0xe4);

        //    t2 = 8 x [110110aaaaaaaaaa|110111bbbbbbbbbb] -- copy hi word from t1 to t0
        //         0xba = (t2 and not v_fc00_fc000) or v_d800_dc00
        const __m512i v_fc00_fc00 = _mm512_set1_epi32(0xfc00fc00);
        const __m512i v_d800_dc00 = _mm512_set1_epi32(0xd800dc00);
        const __m512i t3 = _mm512_ternarylogic_epi32(t2, v_fc00_fc00, v_d800_dc00, 0xba);
        const __m512i t4 = _mm512_mask_blend_epi32(sp_mask, utf32, t3);
        __m512i t5 = _mm512_ror_epi32(t4, 16);
        const  __mmask32 nonzero = _kor_mask32(0xaaaaaaaa,_mm512_cmpneq_epi16_mask(t5, _mm512_setzero_si512()));
        if(big_endian) { t5 = _mm512_shuffle_epi8(t5, byteflip); }
        // we deliberately avoid _mm512_mask_compressstoreu_epi16 for portability (zen4)
        __m512i compressed = _mm512_maskz_compress_epi16(nonzero, t5);
        _mm512_mask_storeu_epi16(output, (1<<(count + static_cast<unsigned int>(count_ones(sp_mask)))) - 1, compressed);
        //_mm512_mask_compressstoreu_epi16(output, nonzero, t5);
    }

    return count + static_cast<unsigned int>(count_ones(sp_mask));
}

/**
 * Store the last N bytes of previous followed by 512-N bytes from input.
 */
template <int N>
__m512i prev(__m512i input, __m512i previous) {
    static_assert(N<=32, "N must be no larger than 32");
    const __m512i movemask = _mm512_setr_epi32(28,29,30,31,0,1,2,3,4,5,6,7,8,9,10,11);
    const __m512i rotated = _mm512_permutex2var_epi32(input, movemask, previous);
#if SIMDUTF_GCC8 || SIMDUTF_GCC9
    constexpr int shift = 16-N; // workaround for GCC8,9
    return _mm512_alignr_epi8(input, rotated, shift);
#else
    return _mm512_alignr_epi8(input, rotated, 16-N);
#endif // SIMDUTF_GCC8 || SIMDUTF_GCC9
}

template <unsigned idx0, unsigned idx1, unsigned idx2, unsigned idx3>
__m512i shuffle_epi128(__m512i v) {
    static_assert((idx0 >= 0 && idx0 <= 3), "idx0 must be in range 0..3");
    static_assert((idx1 >= 0 && idx1 <= 3), "idx1 must be in range 0..3");
    static_assert((idx2 >= 0 && idx2 <= 3), "idx2 must be in range 0..3");
    static_assert((idx3 >= 0 && idx3 <= 3), "idx3 must be in range 0..3");

    constexpr unsigned shuffle = idx0 | (idx1 << 2) | (idx2 << 4) | (idx3 << 6);
    return _mm512_shuffle_i32x4(v, v, shuffle);
}

template <unsigned idx>
constexpr __m512i broadcast_epi128(__m512i v) {
    return shuffle_epi128<idx, idx, idx, idx>(v);
}

/**
 * Current unused.
 */
template <int N>
__m512i rotate_by_N_epi8(const __m512i input) {

    // lanes order: 1, 2, 3, 0 => 0b00_11_10_01
    const __m512i permuted = _mm512_shuffle_i32x4(input, input, 0x39);

    return _mm512_alignr_epi8(permuted, input, N);
}

/*
    expanded_utf8_to_utf32 converts expanded UTF-8 characters (`utf8`)
    stored at separate 32-bit lanes.

    For each lane we have also a character class (`char_class), given in form
    0x8080800N, where N is 4 highest bits from the leading byte; 0x80 resets
    corresponding bytes during pshufb.
*/
simdutf_really_inline __m512i expanded_utf8_to_utf32(__m512i char_class, __m512i utf8) {
    /*
        Input:
        - utf8: bytes stored at separate 32-bit code units
        - valid: which code units have valid UTF-8 characters

        Bit layout of single word. We show 4 cases for each possible
        UTF-8 character encoding. The `?` denotes bits we must not
        assume their value.

        |10dd.dddd|10cc.cccc|10bb.bbbb|1111.0aaa| 4-byte char
        |????.????|10cc.cccc|10bb.bbbb|1110.aaaa| 3-byte char
        |????.????|????.????|10bb.bbbb|110a.aaaa| 2-byte char
        |????.????|????.????|????.????|0aaa.aaaa| ASCII char
          byte 3    byte 2    byte 1     byte 0
    */

    /* 1. Reset control bits of continuation bytes and the MSB
          of the leading byte; this makes all bytes unsigned (and
          does not alter ASCII char).

        |00dd.dddd|00cc.cccc|00bb.bbbb|0111.0aaa| 4-byte char
        |00??.????|00cc.cccc|00bb.bbbb|0110.aaaa| 3-byte char
        |00??.????|00??.????|00bb.bbbb|010a.aaaa| 2-byte char
        |00??.????|00??.????|00??.????|0aaa.aaaa| ASCII char
         ^^        ^^        ^^        ^
    */
    __m512i values;
    const __m512i v_3f3f_3f7f = _mm512_set1_epi32(0x3f3f3f7f);
    values = _mm512_and_si512(utf8, v_3f3f_3f7f);

    /* 2. Swap and join fields A-B and C-D

        |0000.cccc|ccdd.dddd|0001.110a|aabb.bbbb| 4-byte char
        |0000.cccc|cc??.????|0001.10aa|aabb.bbbb| 3-byte char
        |0000.????|????.????|0001.0aaa|aabb.bbbb| 2-byte char
        |0000.????|????.????|000a.aaaa|aa??.????| ASCII char */
    const __m512i v_0140_0140 = _mm512_set1_epi32(0x01400140);
    values = _mm512_maddubs_epi16(values, v_0140_0140);

    /* 3. Swap and join fields AB & CD

        |0000.0001|110a.aabb|bbbb.cccc|ccdd.dddd| 4-byte char
        |0000.0001|10aa.aabb|bbbb.cccc|cc??.????| 3-byte char
        |0000.0001|0aaa.aabb|bbbb.????|????.????| 2-byte char
        |0000.000a|aaaa.aa??|????.????|????.????| ASCII char */
    const __m512i v_0001_1000 = _mm512_set1_epi32(0x00011000);
    values = _mm512_madd_epi16(values, v_0001_1000);

    /* 4. Shift left the values by variable amounts to reset highest UTF-8 bits
        |aaab.bbbb|bccc.cccd|dddd.d000|0000.0000| 4-byte char -- by 11
        |aaaa.bbbb|bbcc.cccc|????.??00|0000.0000| 3-byte char -- by 10
        |aaaa.abbb|bbb?.????|????.???0|0000.0000| 2-byte char -- by 9
        |aaaa.aaa?|????.????|????.????|?000.0000| ASCII char -- by 7 */
    {
        /** pshufb

        continuation = 0
        ascii    = 7
        _2_bytes = 9
        _3_bytes = 10
        _4_bytes = 11

        shift_left_v3 = 4 * [
            ascii, # 0000
            ascii, # 0001
            ascii, # 0010
            ascii, # 0011
            ascii, # 0100
            ascii, # 0101
            ascii, # 0110
            ascii, # 0111
            continuation, # 1000
            continuation, # 1001
            continuation, # 1010
            continuation, # 1011
            _2_bytes, # 1100
            _2_bytes, # 1101
            _3_bytes, # 1110
            _4_bytes, # 1111
        ] */
        const __m512i shift_left_v3 = _mm512_setr_epi64(
            0x0707070707070707,
            0x0b0a090900000000,
            0x0707070707070707,
            0x0b0a090900000000,
            0x0707070707070707,
            0x0b0a090900000000,
            0x0707070707070707,
            0x0b0a090900000000
        );

        const __m512i shift = _mm512_shuffle_epi8(shift_left_v3, char_class);
        values = _mm512_sllv_epi32(values, shift);
    }

    /* 5. Shift right the values by variable amounts to reset lowest bits
        |0000.0000|000a.aabb|bbbb.cccc|ccdd.dddd| 4-byte char -- by 11
        |0000.0000|0000.0000|aaaa.bbbb|bbcc.cccc| 3-byte char -- by 16
        |0000.0000|0000.0000|0000.0aaa|aabb.bbbb| 2-byte char -- by 21
        |0000.0000|0000.0000|0000.0000|0aaa.aaaa| ASCII char -- by 25 */
    {
        // 4 * [25, 25, 25, 25, 25, 25, 25, 25, 0, 0, 0, 0, 21, 21, 16, 11]
        const __m512i shift_right = _mm512_setr_epi64(
            0x1919191919191919,
            0x0b10151500000000,
            0x1919191919191919,
            0x0b10151500000000,
            0x1919191919191919,
            0x0b10151500000000,
            0x1919191919191919,
            0x0b10151500000000
        );

        const __m512i shift = _mm512_shuffle_epi8(shift_right, char_class);
        values = _mm512_srlv_epi32(values, shift);
    }

    return values;
}


simdutf_really_inline __m512i expand_and_identify(__m512i lane0, __m512i lane1, int &count) {
    const __m512i merged = _mm512_mask_mov_epi32(lane0, 0x1000, lane1);
    const __m512i expand_ver2 = _mm512_setr_epi64(
                0x0403020103020100,
                0x0605040305040302,
                0x0807060507060504,
                0x0a09080709080706,
                0x0c0b0a090b0a0908,
                0x0e0d0c0b0d0c0b0a,
                0x000f0e0d0f0e0d0c,
                0x0201000f01000f0e
    );
    const __m512i input = _mm512_shuffle_epi8(merged, expand_ver2);
    const __m512i v_0000_00c0 = _mm512_set1_epi32(0xc0);
    const __m512i t0 = _mm512_and_si512(input, v_0000_00c0);
    const __m512i v_0000_0080 = _mm512_set1_epi32(0x80);
    const __mmask16 leading_bytes = _mm512_cmpneq_epu32_mask(t0, v_0000_0080);
    count = static_cast<int>(count_ones(leading_bytes));
    return  _mm512_mask_compress_epi32(_mm512_setzero_si512(), leading_bytes, input);
}

simdutf_really_inline __m512i expand_utf8_to_utf32(__m512i input) {
    __m512i char_class = _mm512_srli_epi32(input, 4);
    /*  char_class = ((input >> 4) & 0x0f) | 0x80808000 */
    const __m512i v_0000_000f = _mm512_set1_epi32(0x0f);
    const __m512i v_8080_8000 = _mm512_set1_epi32(0x80808000);
    char_class = _mm512_ternarylogic_epi32(char_class, v_0000_000f, v_8080_8000, 0xea);
    return expanded_utf8_to_utf32(char_class, input);
}
