CC= g++ $(CFLAGS)
CFLAGS= -O3 -DBUFFER_PROFILING
OS = $(shell uname)
ICONV_FLAGS=
ifeq ($(OS),Darwin)
	ICONV_FLAGS=-liconv
endif

u8u16_64:	u8u16_p4.c config/p4_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h
	$(CC) -o u8u16_64 u8u16_p4.c -march=nocona -m64

u8u16_64_noopt:	u8u16_p4.c config/p4_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h
	$(CC) -o u8u16_64_noopt u8u16_p4.c -march=nocona -m64 -DNO_OPTIMIZATION

u8u16_p4:	u8u16_p4.c config/p4_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h
	$(CC) -o u8u16_p4 u8u16_p4.c -march=pentium4

u8u16_p4_noopt:	u8u16_p4.c config/p4_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h
	$(CC) -o u8u16_p4_noopt u8u16_p4.c -march=pentium4 -DNO_OPTIMIZATION

u8u16_ssse3:	u8u16_p4.c config/p4_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h
	$(CC) -o u8u16_ssse3 u8u16_ssse3.c -march=core2

x8x16_p4:	x8x16_p4.c config/p4_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h \
		src/x8x16.c src/xmldecl.h src/xmldecl.c src/bytelex.h src/multiliteral.h
	$(CC) -o x8x16_p4 x8x16_p4.c -march=pentium4

u8u16_p4_ideal:	u8u16_p4_ideal.c config/p4_ideal_config.h src/u8u16.c src/libu8u16.c lib/sse_simd.h
	$(CC) -o u8u16_p4_ideal u8u16_p4_ideal.c -march=pentium4

u8u16_g4:	u8u16_g4.c config/g4_config.h src/u8u16.c src/libu8u16.c lib/altivec_simd.h
	$(CC) -o u8u16_g4 u8u16_g4.c -maltivec -faltivec

u8u16_mmx:	u8u16_mmx.c config/mmx_config.h src/u8u16.c src/libu8u16.c \
                lib/mmx_simd.h lib/mmx_simd_basic.h lib/mmx_simd_built_in.h lib/mmx_simd_modified.h
	$(CC) -o u8u16_mmx u8u16_mmx.c -march=pentium-mmx

iconv_u8u16:	iconv_u8u16.c src/u8u16.c src/libu8u16.c
	$(CC) -o  iconv_u8u16 iconv_u8u16.c $(ICONV_FLAGS)
	
clean:
	rm -f u8u16_64 u8u16_p4 x8x16_p4 u8u16_p4_ideal u8u16_g4 u8u16_mmx iconv_u8u16


