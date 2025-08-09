#ifdef MEMCPY_READ_NT
    #define MEMCPY_READ_FUNC _mm_stream_load_si128  //SSE 4.1
    #define MEMCPY_READ_SUFFIX nt
#else
    #define MEMCPY_READ_FUNC _mm_loadu_si128
    #define MEMCPY_READ_SUFFIX t
#endif

#ifdef MEMCPY_WRITE_NT
    #define MEMCPY_WRITE_FUNC _mm_stream_si128
    #define MEMCPY_WRITE_SUFFIX nt
#else
    #define MEMCPY_WRITE_FUNC _mm_storeu_si128
    #define MEMCPY_WRITE_SUFFIX t
#endif

#define MEMCPY_CONCAT(a, b, c) a##_##b##_##c
#define MEMCPY_JOIN(a, b, c) MEMCPY_CONCAT(a, b, c)

inline void MEMCPY_JOIN(CustomMemcpy, MEMCPY_WRITE_SUFFIX, MEMCPY_READ_SUFFIX)(char *dst, const char *src, size_t num) {
#ifdef MEMCPY_READ_NT
    while (num && (size_t(src) & 0xF)) {
        *dst++ = *src++;
        num--;
    }
#endif
#ifdef MEMCPY_WRITE_NT
    while (num && (size_t(dst) & 0xF)) {
        *dst++ = *src++;
        num--;
    }
#endif
    while (num >= 128) {
        #define MEMCPY_READ_X(k) __m128i reg##k = MEMCPY_READ_FUNC((__m128i*)(src + 16*k))
        #define MEMCPY_WRITE_X(k) MEMCPY_WRITE_FUNC((__m128i*)(dst + 16*k), reg##k)
        MEMCPY_READ_X(0);
        MEMCPY_READ_X(1);
        MEMCPY_READ_X(2);
        MEMCPY_READ_X(3);
        MEMCPY_WRITE_X(0);
        MEMCPY_WRITE_X(1);
        MEMCPY_WRITE_X(2);
        MEMCPY_WRITE_X(3);
        MEMCPY_READ_X(4);
        MEMCPY_READ_X(5);
        MEMCPY_READ_X(6);
        MEMCPY_READ_X(7);
        MEMCPY_WRITE_X(4);
        MEMCPY_WRITE_X(5);
        MEMCPY_WRITE_X(6);
        MEMCPY_WRITE_X(7);
        dst += 128;
        src += 128;
        num -= 128;
        #undef MEMCPY_READ_X
        #undef MEMCPY_WRITE_X
    }
#ifdef MEMCPY_WRITE_NT
    _mm_sfence();
#endif
    memcpy(dst, src, num);
}

#undef MEMCPY_READ_FUNC
#undef MEMCPY_READ_SUFFIX
#undef MEMCPY_WRITE_FUNC
#undef MEMCPY_WRITE_SUFFIX
#undef MEMCPY_CONCAT
#undef MEMCPY_JOIN
#ifdef MEMCPY_READ_NT
    #undef MEMCPY_READ_NT
#endif
#ifdef MEMCPY_WRITE_NT
    #undef MEMCPY_WRITE_NT
#endif
