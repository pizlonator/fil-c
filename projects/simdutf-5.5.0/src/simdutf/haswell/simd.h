#ifndef SIMDUTF_HASWELL_SIMD_H
#define SIMDUTF_HASWELL_SIMD_H


namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
namespace simd {

  // Forward-declared so they can be used by splat and friends.
  template<typename Child>
  struct base {
    __m256i value;

    // Zero constructor
    simdutf_really_inline base() : value{__m256i()} {}

    // Conversion from SIMD register
    simdutf_really_inline base(const __m256i _value) : value(_value) {}
    // Conversion to SIMD register
    simdutf_really_inline operator const __m256i&() const { return this->value; }
    simdutf_really_inline operator __m256i&() { return this->value; }
    template <endianness big_endian>
    simdutf_really_inline void store_ascii_as_utf16(char16_t * ptr) const {
      __m256i first = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(*this));
      __m256i second = _mm256_cvtepu8_epi16(_mm256_extractf128_si256(*this,1));
      if (big_endian) {
        const __m256i swap = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                  17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
        first = _mm256_shuffle_epi8(first, swap);
        second = _mm256_shuffle_epi8(second, swap);
      }
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr), first);
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr + 16), second);
    }
    simdutf_really_inline void store_ascii_as_utf32(char32_t * ptr) const {
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr), _mm256_cvtepu8_epi32(_mm256_castsi256_si128(*this)));
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr+8), _mm256_cvtepu8_epi32(_mm256_castsi256_si128(_mm256_srli_si256(*this,8))));
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr + 16), _mm256_cvtepu8_epi32(_mm256_extractf128_si256(*this,1)));
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr + 24), _mm256_cvtepu8_epi32(_mm_srli_si128(_mm256_extractf128_si256(*this,1),8)));
    }
    // Bit operations
    simdutf_really_inline Child operator|(const Child other) const { return _mm256_or_si256(*this, other); }
    simdutf_really_inline Child operator&(const Child other) const { return _mm256_and_si256(*this, other); }
    simdutf_really_inline Child operator^(const Child other) const { return _mm256_xor_si256(*this, other); }
    simdutf_really_inline Child bit_andnot(const Child other) const { return _mm256_andnot_si256(other, *this); }
    simdutf_really_inline Child& operator|=(const Child other) { auto this_cast = static_cast<Child*>(this); *this_cast = *this_cast | other; return *this_cast; }
    simdutf_really_inline Child& operator&=(const Child other) { auto this_cast = static_cast<Child*>(this); *this_cast = *this_cast & other; return *this_cast; }
    simdutf_really_inline Child& operator^=(const Child other) { auto this_cast = static_cast<Child*>(this); *this_cast = *this_cast ^ other; return *this_cast; }
  };

  // Forward-declared so they can be used by splat and friends.
  template<typename T>
  struct simd8;

  template<typename T, typename Mask=simd8<bool>>
  struct base8: base<simd8<T>> {
    typedef uint32_t bitmask_t;
    typedef uint64_t bitmask2_t;

    simdutf_really_inline base8() : base<simd8<T>>() {}
    simdutf_really_inline base8(const __m256i _value) : base<simd8<T>>(_value) {}
    simdutf_really_inline T first() const { return _mm256_extract_epi8(*this,0); }
    simdutf_really_inline T last() const { return _mm256_extract_epi8(*this,31); }
    friend simdutf_really_inline Mask operator==(const simd8<T> lhs, const simd8<T> rhs) { return _mm256_cmpeq_epi8(lhs, rhs); }

    static const int SIZE = sizeof(base<T>::value);

    template<int N=1>
    simdutf_really_inline simd8<T> prev(const simd8<T> prev_chunk) const {
      return _mm256_alignr_epi8(*this, _mm256_permute2x128_si256(prev_chunk, *this, 0x21), 16 - N);
    }
  };

  // SIMD byte mask type (returned by things like eq and gt)
  template<>
  struct simd8<bool>: base8<bool> {
    static simdutf_really_inline simd8<bool> splat(bool _value) { return _mm256_set1_epi8(uint8_t(-(!!_value))); }

    simdutf_really_inline simd8() : base8() {}
    simdutf_really_inline simd8(const __m256i _value) : base8<bool>(_value) {}
    // Splat constructor
    simdutf_really_inline simd8(bool _value) : base8<bool>(splat(_value)) {}

    simdutf_really_inline uint32_t to_bitmask() const { return uint32_t(_mm256_movemask_epi8(*this)); }
    simdutf_really_inline bool any() const { return !_mm256_testz_si256(*this, *this); }
    simdutf_really_inline bool none() const { return _mm256_testz_si256(*this, *this); }
    simdutf_really_inline bool all() const { return static_cast<uint32_t>(_mm256_movemask_epi8(*this)) == 0xFFFFFFFF; }
    simdutf_really_inline simd8<bool> operator~() const { return *this ^ true; }
  };

  template<typename T>
  struct base8_numeric: base8<T> {
    static simdutf_really_inline simd8<T> splat(T _value) { return _mm256_set1_epi8(_value); }
    static simdutf_really_inline simd8<T> zero() { return _mm256_setzero_si256(); }
    static simdutf_really_inline simd8<T> load(const T values[32]) {
      return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(values));
    }
    // Repeat 16 values as many times as necessary (usually for lookup tables)
    static simdutf_really_inline simd8<T> repeat_16(
      T v0,  T v1,  T v2,  T v3,  T v4,  T v5,  T v6,  T v7,
      T v8,  T v9,  T v10, T v11, T v12, T v13, T v14, T v15
    ) {
      return simd8<T>(
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15
      );
    }

    simdutf_really_inline base8_numeric() : base8<T>() {}
    simdutf_really_inline base8_numeric(const __m256i _value) : base8<T>(_value) {}

    // Store to array
    simdutf_really_inline void store(T dst[32]) const { return _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst), *this); }

    // Addition/subtraction are the same for signed and unsigned
    simdutf_really_inline simd8<T> operator+(const simd8<T> other) const { return _mm256_add_epi8(*this, other); }
    simdutf_really_inline simd8<T> operator-(const simd8<T> other) const { return _mm256_sub_epi8(*this, other); }
    simdutf_really_inline simd8<T>& operator+=(const simd8<T> other) { *this = *this + other; return *static_cast<simd8<T>*>(this); }
    simdutf_really_inline simd8<T>& operator-=(const simd8<T> other) { *this = *this - other; return *static_cast<simd8<T>*>(this); }

    // Override to distinguish from bool version
    simdutf_really_inline simd8<T> operator~() const { return *this ^ 0xFFu; }

    // Perform a lookup assuming the value is between 0 and 16 (undefined behavior for out of range values)
    template<typename L>
    simdutf_really_inline simd8<L> lookup_16(simd8<L> lookup_table) const {
      return _mm256_shuffle_epi8(lookup_table, *this);
    }

    template<typename L>
    simdutf_really_inline simd8<L> lookup_16(
        L replace0,  L replace1,  L replace2,  L replace3,
        L replace4,  L replace5,  L replace6,  L replace7,
        L replace8,  L replace9,  L replace10, L replace11,
        L replace12, L replace13, L replace14, L replace15) const {
      return lookup_16(simd8<L>::repeat_16(
        replace0,  replace1,  replace2,  replace3,
        replace4,  replace5,  replace6,  replace7,
        replace8,  replace9,  replace10, replace11,
        replace12, replace13, replace14, replace15
      ));
    }
  };


  // Signed bytes
  template<>
  struct simd8<int8_t> : base8_numeric<int8_t> {
    simdutf_really_inline simd8() : base8_numeric<int8_t>() {}
    simdutf_really_inline simd8(const __m256i _value) : base8_numeric<int8_t>(_value) {}

    // Splat constructor
    simdutf_really_inline simd8(int8_t _value) : simd8(splat(_value)) {}
    // Array constructor
    simdutf_really_inline simd8(const int8_t values[32]) : simd8(load(values)) {}
    simdutf_really_inline operator simd8<uint8_t>() const;
    // Member-by-member initialization
    simdutf_really_inline simd8(
      int8_t v0,  int8_t v1,  int8_t v2,  int8_t v3,  int8_t v4,  int8_t v5,  int8_t v6,  int8_t v7,
      int8_t v8,  int8_t v9,  int8_t v10, int8_t v11, int8_t v12, int8_t v13, int8_t v14, int8_t v15,
      int8_t v16, int8_t v17, int8_t v18, int8_t v19, int8_t v20, int8_t v21, int8_t v22, int8_t v23,
      int8_t v24, int8_t v25, int8_t v26, int8_t v27, int8_t v28, int8_t v29, int8_t v30, int8_t v31
    ) : simd8(_mm256_setr_epi8(
      v0, v1, v2, v3, v4, v5, v6, v7,
      v8, v9, v10,v11,v12,v13,v14,v15,
      v16,v17,v18,v19,v20,v21,v22,v23,
      v24,v25,v26,v27,v28,v29,v30,v31
    )) {}
    // Repeat 16 values as many times as necessary (usually for lookup tables)
    simdutf_really_inline static simd8<int8_t> repeat_16(
      int8_t v0,  int8_t v1,  int8_t v2,  int8_t v3,  int8_t v4,  int8_t v5,  int8_t v6,  int8_t v7,
      int8_t v8,  int8_t v9,  int8_t v10, int8_t v11, int8_t v12, int8_t v13, int8_t v14, int8_t v15
    ) {
      return simd8<int8_t>(
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15
      );
    }
    simdutf_really_inline bool is_ascii() const { return _mm256_movemask_epi8(*this) == 0; }
    // Order-sensitive comparisons
    simdutf_really_inline simd8<int8_t> max_val(const simd8<int8_t> other) const { return _mm256_max_epi8(*this, other); }
    simdutf_really_inline simd8<int8_t> min_val(const simd8<int8_t> other) const { return _mm256_min_epi8(*this, other); }
    simdutf_really_inline simd8<bool> operator>(const simd8<int8_t> other) const { return _mm256_cmpgt_epi8(*this, other); }
    simdutf_really_inline simd8<bool> operator<(const simd8<int8_t> other) const { return _mm256_cmpgt_epi8(other, *this); }
  };

  // Unsigned bytes
  template<>
  struct simd8<uint8_t>: base8_numeric<uint8_t> {
    simdutf_really_inline simd8() : base8_numeric<uint8_t>() {}
    simdutf_really_inline simd8(const __m256i _value) : base8_numeric<uint8_t>(_value) {}
    // Splat constructor
    simdutf_really_inline simd8(uint8_t _value) : simd8(splat(_value)) {}
    // Array constructor
    simdutf_really_inline simd8(const uint8_t values[32]) : simd8(load(values)) {}
    // Member-by-member initialization
    simdutf_really_inline simd8(
      uint8_t v0,  uint8_t v1,  uint8_t v2,  uint8_t v3,  uint8_t v4,  uint8_t v5,  uint8_t v6,  uint8_t v7,
      uint8_t v8,  uint8_t v9,  uint8_t v10, uint8_t v11, uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15,
      uint8_t v16, uint8_t v17, uint8_t v18, uint8_t v19, uint8_t v20, uint8_t v21, uint8_t v22, uint8_t v23,
      uint8_t v24, uint8_t v25, uint8_t v26, uint8_t v27, uint8_t v28, uint8_t v29, uint8_t v30, uint8_t v31
    ) : simd8(_mm256_setr_epi8(
      v0, v1, v2, v3, v4, v5, v6, v7,
      v8, v9, v10,v11,v12,v13,v14,v15,
      v16,v17,v18,v19,v20,v21,v22,v23,
      v24,v25,v26,v27,v28,v29,v30,v31
    )) {}
    // Repeat 16 values as many times as necessary (usually for lookup tables)
    simdutf_really_inline static simd8<uint8_t> repeat_16(
      uint8_t v0,  uint8_t v1,  uint8_t v2,  uint8_t v3,  uint8_t v4,  uint8_t v5,  uint8_t v6,  uint8_t v7,
      uint8_t v8,  uint8_t v9,  uint8_t v10, uint8_t v11, uint8_t v12, uint8_t v13, uint8_t v14, uint8_t v15
    ) {
      return simd8<uint8_t>(
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15,
        v0, v1, v2, v3, v4, v5, v6, v7,
        v8, v9, v10,v11,v12,v13,v14,v15
      );
    }


    // Saturated math
    simdutf_really_inline simd8<uint8_t> saturating_add(const simd8<uint8_t> other) const { return _mm256_adds_epu8(*this, other); }
    simdutf_really_inline simd8<uint8_t> saturating_sub(const simd8<uint8_t> other) const { return _mm256_subs_epu8(*this, other); }

    // Order-specific operations
    simdutf_really_inline simd8<uint8_t> max_val(const simd8<uint8_t> other) const { return _mm256_max_epu8(*this, other); }
    simdutf_really_inline simd8<uint8_t> min_val(const simd8<uint8_t> other) const { return _mm256_min_epu8(other, *this); }
    // Same as >, but only guarantees true is nonzero (< guarantees true = -1)
    simdutf_really_inline simd8<uint8_t> gt_bits(const simd8<uint8_t> other) const { return this->saturating_sub(other); }
    // Same as <, but only guarantees true is nonzero (< guarantees true = -1)
    simdutf_really_inline simd8<uint8_t> lt_bits(const simd8<uint8_t> other) const { return other.saturating_sub(*this); }
    simdutf_really_inline simd8<bool> operator<=(const simd8<uint8_t> other) const { return other.max_val(*this) == other; }
    simdutf_really_inline simd8<bool> operator>=(const simd8<uint8_t> other) const { return other.min_val(*this) == other; }
    simdutf_really_inline simd8<bool> operator>(const simd8<uint8_t> other) const { return this->gt_bits(other).any_bits_set(); }
    simdutf_really_inline simd8<bool> operator<(const simd8<uint8_t> other) const { return this->lt_bits(other).any_bits_set(); }

    // Bit-specific operations
    simdutf_really_inline simd8<bool> bits_not_set() const { return *this == uint8_t(0); }
    simdutf_really_inline simd8<bool> bits_not_set(simd8<uint8_t> bits) const { return (*this & bits).bits_not_set(); }
    simdutf_really_inline simd8<bool> any_bits_set() const { return ~this->bits_not_set(); }
    simdutf_really_inline simd8<bool> any_bits_set(simd8<uint8_t> bits) const { return ~this->bits_not_set(bits); }
    simdutf_really_inline bool is_ascii() const { return _mm256_movemask_epi8(*this) == 0; }
    simdutf_really_inline bool bits_not_set_anywhere() const { return _mm256_testz_si256(*this, *this); }
    simdutf_really_inline bool any_bits_set_anywhere() const { return !bits_not_set_anywhere(); }
    simdutf_really_inline bool bits_not_set_anywhere(simd8<uint8_t> bits) const { return _mm256_testz_si256(*this, bits); }
    simdutf_really_inline bool any_bits_set_anywhere(simd8<uint8_t> bits) const { return !bits_not_set_anywhere(bits); }
    template<int N>
    simdutf_really_inline simd8<uint8_t> shr() const { return simd8<uint8_t>(_mm256_srli_epi16(*this, N)) & uint8_t(0xFFu >> N); }
    template<int N>
    simdutf_really_inline simd8<uint8_t> shl() const { return simd8<uint8_t>(_mm256_slli_epi16(*this, N)) & uint8_t(0xFFu << N); }
    // Get one of the bits and make a bitmask out of it.
    // e.g. value.get_bit<7>() gets the high bit
    template<int N>
    simdutf_really_inline int get_bit() const { return _mm256_movemask_epi8(_mm256_slli_epi16(*this, 7-N)); }
  };
  simdutf_really_inline simd8<int8_t>::operator simd8<uint8_t>() const { return this->value; }


  template<typename T>
  struct simd8x64 {
    static constexpr int NUM_CHUNKS = 64 / sizeof(simd8<T>);
    static_assert(NUM_CHUNKS == 2, "Haswell kernel should use two registers per 64-byte block.");
    simd8<T> chunks[NUM_CHUNKS];

    simd8x64(const simd8x64<T>& o) = delete; // no copy allowed
    simd8x64<T>& operator=(const simd8<T> other) = delete; // no assignment allowed
    simd8x64() = delete; // no default constructor allowed

    simdutf_really_inline simd8x64(const simd8<T> chunk0, const simd8<T> chunk1) : chunks{chunk0, chunk1} {}
    simdutf_really_inline simd8x64(const T* ptr) : chunks{simd8<T>::load(ptr), simd8<T>::load(ptr+sizeof(simd8<T>)/sizeof(T))} {}

    simdutf_really_inline void store(T* ptr) const {
      this->chunks[0].store(ptr+sizeof(simd8<T>)*0/sizeof(T));
      this->chunks[1].store(ptr+sizeof(simd8<T>)*1/sizeof(T));
    }

    simdutf_really_inline uint64_t to_bitmask() const {
      uint64_t r_lo = uint32_t(this->chunks[0].to_bitmask());
      uint64_t r_hi =                       this->chunks[1].to_bitmask();
      return r_lo | (r_hi << 32);
    }

    simdutf_really_inline simd8x64<T>& operator|=(const simd8x64<T> &other) {
      this->chunks[0] |= other.chunks[0];
      this->chunks[1] |= other.chunks[1];
      return *this;
    }

    simdutf_really_inline simd8<T> reduce_or() const {
      return this->chunks[0] | this->chunks[1];
    }

    simdutf_really_inline bool is_ascii() const {
      return this->reduce_or().is_ascii();
    }

    template <endianness endian>
    simdutf_really_inline void store_ascii_as_utf16(char16_t * ptr) const {
      this->chunks[0].template store_ascii_as_utf16<endian>(ptr+sizeof(simd8<T>)*0);
      this->chunks[1].template store_ascii_as_utf16<endian>(ptr+sizeof(simd8<T>)*1);
    }

    simdutf_really_inline void store_ascii_as_utf32(char32_t * ptr) const {
      this->chunks[0].store_ascii_as_utf32(ptr+sizeof(simd8<T>)*0);
      this->chunks[1].store_ascii_as_utf32(ptr+sizeof(simd8<T>)*1);
    }

    simdutf_really_inline simd8x64<T> bit_or(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return simd8x64<T>(
        this->chunks[0] | mask,
        this->chunks[1] | mask
      );
    }

    simdutf_really_inline uint64_t eq(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return  simd8x64<bool>(
        this->chunks[0] == mask,
        this->chunks[1] == mask
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t eq(const simd8x64<uint8_t> &other) const {
      return  simd8x64<bool>(
        this->chunks[0] == other.chunks[0],
        this->chunks[1] == other.chunks[1]
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t lteq(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return  simd8x64<bool>(
        this->chunks[0] <= mask,
        this->chunks[1] <= mask
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t in_range(const T low, const T high) const {
      const simd8<T> mask_low = simd8<T>::splat(low);
      const simd8<T> mask_high = simd8<T>::splat(high);

      return  simd8x64<bool>(
        (this->chunks[0] <= mask_high) & (this->chunks[0] >= mask_low),
        (this->chunks[1] <= mask_high) & (this->chunks[1] >= mask_low)
      ).to_bitmask();
    }
    simdutf_really_inline uint64_t not_in_range(const T low, const T high) const {
      const simd8<T> mask_low = simd8<T>::splat(low);
      const simd8<T> mask_high = simd8<T>::splat(high);
      return  simd8x64<bool>(
        (this->chunks[0] > mask_high) | (this->chunks[0] < mask_low),
        (this->chunks[1] > mask_high) | (this->chunks[1] < mask_low)
      ).to_bitmask();
    }
    simdutf_really_inline uint64_t lt(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return  simd8x64<bool>(
        this->chunks[0] < mask,
        this->chunks[1] < mask
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t gt(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return  simd8x64<bool>(
        this->chunks[0] > mask,
        this->chunks[1] > mask
      ).to_bitmask();
    }
    simdutf_really_inline uint64_t gteq(const T m) const {
      const simd8<T> mask = simd8<T>::splat(m);
      return  simd8x64<bool>(
        this->chunks[0] >= mask,
        this->chunks[1] >= mask
      ).to_bitmask();
    }
    simdutf_really_inline uint64_t gteq_unsigned(const uint8_t m) const {
      const simd8<uint8_t> mask = simd8<uint8_t>::splat(m);
      return  simd8x64<bool>(
        (simd8<uint8_t>(__m256i(this->chunks[0])) >= mask),
        (simd8<uint8_t>(__m256i(this->chunks[1])) >= mask)
      ).to_bitmask();
    }
  }; // struct simd8x64<T>

  #include "simdutf/haswell/simd16-inl.h"

} // namespace simd

} // unnamed namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#endif // SIMDUTF_HASWELL_SIMD_H
