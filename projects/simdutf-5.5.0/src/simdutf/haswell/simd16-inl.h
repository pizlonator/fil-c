#ifdef __GNUC__
#if __GNUC__ < 8
#define _mm256_set_m128i(xmm1, xmm2) _mm256_permute2f128_si256(_mm256_castsi128_si256(xmm1), _mm256_castsi128_si256(xmm2), 2)
#define _mm256_setr_m128i(xmm2, xmm1)  _mm256_permute2f128_si256(_mm256_castsi128_si256(xmm1), _mm256_castsi128_si256(xmm2), 2)
#endif
#endif

template<typename T>
struct simd16;

template<typename T, typename Mask=simd16<bool>>
struct base16: base<simd16<T>> {
  using bitmask_type = uint32_t;

  simdutf_really_inline base16() : base<simd16<T>>() {}
  simdutf_really_inline base16(const __m256i _value) : base<simd16<T>>(_value) {}
  template <typename Pointer>
  simdutf_really_inline base16(const Pointer* ptr) : base16(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr))) {}
  friend simdutf_really_inline Mask operator==(const simd16<T> lhs, const simd16<T> rhs) { return _mm256_cmpeq_epi16(lhs, rhs); }

  /// the size of vector in bytes
  static const int SIZE = sizeof(base<simd16<T>>::value);

  /// the number of elements of type T a vector can hold
  static const int ELEMENTS = SIZE / sizeof(T);

  template<int N=1>
  simdutf_really_inline simd16<T> prev(const simd16<T> prev_chunk) const {
    return _mm256_alignr_epi8(*this, prev_chunk, 16 - N);
  }
};

// SIMD byte mask type (returned by things like eq and gt)
template<>
struct simd16<bool>: base16<bool> {
  static simdutf_really_inline simd16<bool> splat(bool _value) { return _mm256_set1_epi16(uint16_t(-(!!_value))); }

  simdutf_really_inline simd16() : base16() {}
  simdutf_really_inline simd16(const __m256i _value) : base16<bool>(_value) {}
  // Splat constructor
  simdutf_really_inline simd16(bool _value) : base16<bool>(splat(_value)) {}

  simdutf_really_inline bitmask_type to_bitmask() const { return _mm256_movemask_epi8(*this); }
  simdutf_really_inline bool any() const { return !_mm256_testz_si256(*this, *this); }
  simdutf_really_inline simd16<bool> operator~() const { return *this ^ true; }
};

template<typename T>
struct base16_numeric: base16<T> {
  static simdutf_really_inline simd16<T> splat(T _value) { return _mm256_set1_epi16(_value); }
  static simdutf_really_inline simd16<T> zero() { return _mm256_setzero_si256(); }
  static simdutf_really_inline simd16<T> load(const T values[8]) {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(values));
  }

  simdutf_really_inline base16_numeric() : base16<T>() {}
  simdutf_really_inline base16_numeric(const __m256i _value) : base16<T>(_value) {}

  // Store to array
  simdutf_really_inline void store(T dst[8]) const { return _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst), *this); }

  // Override to distinguish from bool version
  simdutf_really_inline simd16<T> operator~() const { return *this ^ 0xFFFFu; }

  // Addition/subtraction are the same for signed and unsigned
  simdutf_really_inline simd16<T> operator+(const simd16<T> other) const { return _mm256_add_epi16(*this, other); }
  simdutf_really_inline simd16<T> operator-(const simd16<T> other) const { return _mm256_sub_epi16(*this, other); }
  simdutf_really_inline simd16<T>& operator+=(const simd16<T> other) { *this = *this + other; return *static_cast<simd16<T>*>(this); }
  simdutf_really_inline simd16<T>& operator-=(const simd16<T> other) { *this = *this - other; return *static_cast<simd16<T>*>(this); }
};

// Signed code units
template<>
struct simd16<int16_t> : base16_numeric<int16_t> {
  simdutf_really_inline simd16() : base16_numeric<int16_t>() {}
  simdutf_really_inline simd16(const __m256i _value) : base16_numeric<int16_t>(_value) {}
  // Splat constructor
  simdutf_really_inline simd16(int16_t _value) : simd16(splat(_value)) {}
  // Array constructor
  simdutf_really_inline simd16(const int16_t* values) : simd16(load(values)) {}
  simdutf_really_inline simd16(const char16_t* values) : simd16(load(reinterpret_cast<const int16_t*>(values))) {}
  // Order-sensitive comparisons
  simdutf_really_inline simd16<int16_t> max_val(const simd16<int16_t> other) const { return _mm256_max_epi16(*this, other); }
  simdutf_really_inline simd16<int16_t> min_val(const simd16<int16_t> other) const { return _mm256_min_epi16(*this, other); }
  simdutf_really_inline simd16<bool> operator>(const simd16<int16_t> other) const { return _mm256_cmpgt_epi16(*this, other); }
  simdutf_really_inline simd16<bool> operator<(const simd16<int16_t> other) const { return _mm256_cmpgt_epi16(other, *this); }
};

// Unsigned code units
template<>
struct simd16<uint16_t>: base16_numeric<uint16_t>  {
  simdutf_really_inline simd16() : base16_numeric<uint16_t>() {}
  simdutf_really_inline simd16(const __m256i _value) : base16_numeric<uint16_t>(_value) {}

  // Splat constructor
  simdutf_really_inline simd16(uint16_t _value) : simd16(splat(_value)) {}
  // Array constructor
  simdutf_really_inline simd16(const uint16_t* values) : simd16(load(values)) {}
  simdutf_really_inline simd16(const char16_t* values) : simd16(load(reinterpret_cast<const uint16_t*>(values))) {}

  // Saturated math
  simdutf_really_inline simd16<uint16_t> saturating_add(const simd16<uint16_t> other) const { return _mm256_adds_epu16(*this, other); }
  simdutf_really_inline simd16<uint16_t> saturating_sub(const simd16<uint16_t> other) const { return _mm256_subs_epu16(*this, other); }

  // Order-specific operations
  simdutf_really_inline simd16<uint16_t> max_val(const simd16<uint16_t> other) const { return _mm256_max_epu16(*this, other); }
  simdutf_really_inline simd16<uint16_t> min_val(const simd16<uint16_t> other) const { return _mm256_min_epu16(*this, other); }
  // Same as >, but only guarantees true is nonzero (< guarantees true = -1)
  simdutf_really_inline simd16<uint16_t> gt_bits(const simd16<uint16_t> other) const { return this->saturating_sub(other); }
  // Same as <, but only guarantees true is nonzero (< guarantees true = -1)
  simdutf_really_inline simd16<uint16_t> lt_bits(const simd16<uint16_t> other) const { return other.saturating_sub(*this); }
  simdutf_really_inline simd16<bool> operator<=(const simd16<uint16_t> other) const { return other.max_val(*this) == other; }
  simdutf_really_inline simd16<bool> operator>=(const simd16<uint16_t> other) const { return other.min_val(*this) == other; }
  simdutf_really_inline simd16<bool> operator>(const simd16<uint16_t> other) const { return this->gt_bits(other).any_bits_set(); }
  simdutf_really_inline simd16<bool> operator<(const simd16<uint16_t> other) const { return this->gt_bits(other).any_bits_set(); }

  // Bit-specific operations
  simdutf_really_inline simd16<bool> bits_not_set() const { return *this == uint16_t(0); }
  simdutf_really_inline simd16<bool> bits_not_set(simd16<uint16_t> bits) const { return (*this & bits).bits_not_set(); }
  simdutf_really_inline simd16<bool> any_bits_set() const { return ~this->bits_not_set(); }
  simdutf_really_inline simd16<bool> any_bits_set(simd16<uint16_t> bits) const { return ~this->bits_not_set(bits); }

  simdutf_really_inline bool bits_not_set_anywhere() const { return _mm256_testz_si256(*this, *this); }
  simdutf_really_inline bool any_bits_set_anywhere() const { return !bits_not_set_anywhere(); }
  simdutf_really_inline bool bits_not_set_anywhere(simd16<uint16_t> bits) const { return _mm256_testz_si256(*this, bits); }
  simdutf_really_inline bool any_bits_set_anywhere(simd16<uint16_t> bits) const { return !bits_not_set_anywhere(bits); }
  template<int N>
  simdutf_really_inline simd16<uint16_t> shr() const { return simd16<uint16_t>(_mm256_srli_epi16(*this, N)); }
  template<int N>
  simdutf_really_inline simd16<uint16_t> shl() const { return simd16<uint16_t>(_mm256_slli_epi16(*this, N)); }
  // Get one of the bits and make a bitmask out of it.
  // e.g. value.get_bit<7>() gets the high bit
  template<int N>
  simdutf_really_inline int get_bit() const { return _mm256_movemask_epi8(_mm256_slli_epi16(*this, 15-N)); }

  // Change the endianness
  simdutf_really_inline simd16<uint16_t> swap_bytes() const {
    const __m256i swap = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                  17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
    return _mm256_shuffle_epi8(*this, swap);
  }

  // Pack with the unsigned saturation of two uint16_t code units into single uint8_t vector
  static simdutf_really_inline simd8<uint8_t> pack(const simd16<uint16_t>& v0, const simd16<uint16_t>& v1) {
    // Note: the AVX2 variant of pack operates on 128-bit lanes, thus
    //       we have to shuffle lanes in order to produce bytes in the
    //       correct order.

    // get the 0th lanes
    const __m128i lo_0 = _mm256_extracti128_si256(v0, 0);
    const __m128i lo_1 = _mm256_extracti128_si256(v1, 0);

    // get the 1st lanes
    const __m128i hi_0 = _mm256_extracti128_si256(v0, 1);
    const __m128i hi_1 = _mm256_extracti128_si256(v1, 1);

    // build new vectors (shuffle lanes)
    const __m256i t0 = _mm256_set_m128i(lo_1, lo_0);
    const __m256i t1 = _mm256_set_m128i(hi_1, hi_0);

    // pack code units in linear order from v0 and v1
    return _mm256_packus_epi16(t0, t1);
  }
};


  template<typename T>
  struct simd16x32 {
    static constexpr int NUM_CHUNKS = 64 / sizeof(simd16<T>);
    static_assert(NUM_CHUNKS == 2, "Haswell kernel should use two registers per 64-byte block.");
    simd16<T> chunks[NUM_CHUNKS];

    simd16x32(const simd16x32<T>& o) = delete; // no copy allowed
    simd16x32<T>& operator=(const simd16<T> other) = delete; // no assignment allowed
    simd16x32() = delete; // no default constructor allowed

    simdutf_really_inline simd16x32(const simd16<T> chunk0, const simd16<T> chunk1) : chunks{chunk0, chunk1} {}
    simdutf_really_inline simd16x32(const T* ptr) : chunks{simd16<T>::load(ptr), simd16<T>::load(ptr+sizeof(simd16<T>)/sizeof(T))} {}

    simdutf_really_inline void store(T* ptr) const {
      this->chunks[0].store(ptr+sizeof(simd16<T>)*0/sizeof(T));
      this->chunks[1].store(ptr+sizeof(simd16<T>)*1/sizeof(T));
    }

    simdutf_really_inline uint64_t to_bitmask() const {
      uint64_t r_lo = uint32_t(this->chunks[0].to_bitmask());
      uint64_t r_hi =                       this->chunks[1].to_bitmask();
      return r_lo | (r_hi << 32);
    }

    simdutf_really_inline simd16<T> reduce_or() const {
      return this->chunks[0] | this->chunks[1];
    }

    simdutf_really_inline bool is_ascii() const {
      return this->reduce_or().is_ascii();
    }

    simdutf_really_inline void store_ascii_as_utf16(char16_t * ptr) const {
      this->chunks[0].store_ascii_as_utf16(ptr+sizeof(simd16<T>)*0);
      this->chunks[1].store_ascii_as_utf16(ptr+sizeof(simd16<T>));
    }

    simdutf_really_inline simd16x32<T> bit_or(const T m) const {
      const simd16<T> mask = simd16<T>::splat(m);
      return simd16x32<T>(
        this->chunks[0] | mask,
        this->chunks[1] | mask
      );
    }

    simdutf_really_inline void swap_bytes() {
      this->chunks[0] = this->chunks[0].swap_bytes();
      this->chunks[1] = this->chunks[1].swap_bytes();
    }

    simdutf_really_inline uint64_t eq(const T m) const {
      const simd16<T> mask = simd16<T>::splat(m);
      return  simd16x32<bool>(
        this->chunks[0] == mask,
        this->chunks[1] == mask
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t eq(const simd16x32<uint16_t> &other) const {
      return  simd16x32<bool>(
        this->chunks[0] == other.chunks[0],
        this->chunks[1] == other.chunks[1]
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t lteq(const T m) const {
      const simd16<T> mask = simd16<T>::splat(m);
      return  simd16x32<bool>(
        this->chunks[0] <= mask,
        this->chunks[1] <= mask
      ).to_bitmask();
    }

    simdutf_really_inline uint64_t in_range(const T low, const T high) const {
      const simd16<T> mask_low = simd16<T>::splat(low);
      const simd16<T> mask_high = simd16<T>::splat(high);

      return  simd16x32<bool>(
        (this->chunks[0] <= mask_high) & (this->chunks[0] >= mask_low),
        (this->chunks[1] <= mask_high) & (this->chunks[1] >= mask_low)
      ).to_bitmask();
    }
    simdutf_really_inline uint64_t not_in_range(const T low, const T high) const {
      const simd16<T> mask_low = simd16<T>::splat(static_cast<T>(low-1));
      const simd16<T> mask_high = simd16<T>::splat(static_cast<T>(high+1));
      return simd16x32<bool>(
        (this->chunks[0] >= mask_high) | (this->chunks[0] <= mask_low),
        (this->chunks[1] >= mask_high) | (this->chunks[1] <= mask_low)
      ).to_bitmask();
    }
    simdutf_really_inline uint64_t lt(const T m) const {
      const simd16<T> mask = simd16<T>::splat(m);
      return  simd16x32<bool>(
        this->chunks[0] < mask,
        this->chunks[1] < mask
      ).to_bitmask();
    }
  }; // struct simd16x32<T>
