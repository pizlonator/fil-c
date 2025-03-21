// The Computer Language Benchmarks Game
// https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
//
// Contributed by Ilya Kurdyukov
// Based on "fannkuch-redux C++ g++ #6",
// contributed by Andrei Simion (with patch from Vincent Yu)
// which in turn was based on the C++ program by Dave Compton,
// which in turn was based on the C program by Jeremy Zerfasm
// which in turn was based on the Ada program by Jonathan Parker and 
// Georg Bauhaus which in turn was based on code by Dave Fladebo, 
// Eckehard Berns, Heiner Marxen, Hongwei Xi, and The Anh Tran and 
// also the Java program by Oleg Mazurov.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <smmintrin.h>  /* SSE 4.1 */

#define MAX_N 16
#define MAX_BLOCKS 24
#define ALIGN(n) __attribute__((aligned(n)))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define RAMP16 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15

static __m128i masks_shift[16] ALIGN(16);
static uint64_t factorials[MAX_N + 1];

struct fannkuch_data {
  uint64_t block_start, block_size, block_end;
  long long checksum;
  unsigned max_flips, n, mutex;
};

static void* fannkuch_func(void* param) {
  struct fannkuch_data *data = (struct fannkuch_data*)param;
  long long checksum = 0;
  unsigned max_flips = 0;

  int n = data->n;
  uint64_t block_size = data->block_size;
  uint64_t block_end = data->block_end;

  if (n < 1 || n > MAX_N) __builtin_unreachable();

  // iterate over each block.
  for (;;) {
    uint64_t block_start = __sync_fetch_and_add(&data->block_start, block_size);
    if (block_start >= block_end) break;

    __m128i ramp = _mm_setr_epi8(RAMP16), current = ramp;
	  __m128i c0 = _mm_setzero_si128();
		__m128i count_vec = c0;
    unsigned i = n;
    {  
	    uint64_t j = block_start;
      __m128i v0, v1, v2, mask, c1 = _mm_set1_epi8(1);
      mask = _mm_sub_epi8(ramp, _mm_set1_epi8(i));
      while (i--) {
        uint64_t d = j / factorials[i];
        j -= d * factorials[i];
        v2 = _mm_set1_epi8(d);
				count_vec = _mm_alignr_epi8(count_vec, v2, 15);
        v1 = _mm_add_epi8(ramp, v2);
        v0 = _mm_add_epi8(mask, v2);  // ramp - i + d
        v0 = _mm_blendv_epi8(v0, v1, v0);
        v2 = _mm_shuffle_epi8(current, v0);
        current = _mm_blendv_epi8(current, v2, mask);
        mask = _mm_add_epi8(mask, c1);
      }
    }

    // iterate over each permutation in the block.
    uint64_t block_left = block_size;

    do {
			__m128i v0, v1, v2, v3;	unsigned i, first;
#define X(op) \
			v2 = current; \
			first = _mm_cvtsi128_si32(current); \
			v0 = _mm_sub_epi8(count_vec, ramp); \
			i = __builtin_ctz(_mm_movemask_epi8(v0)); \
			v0 = _mm_set1_epi8(i); \
			v1 = _mm_andnot_si128(_mm_cmpgt_epi8(v0, ramp), count_vec); \
			count_vec = _mm_sub_epi8(v1, _mm_cmpeq_epi8(v0, ramp)); \
      current = _mm_shuffle_epi8(current, masks_shift[i]); \
      if (LIKELY(first & 0xff)) { \
        unsigned flips = 0; \
				v3 = _mm_shuffle_epi8(v2, c0); \
        do { \
   				v0 = _mm_sub_epi8(v3, ramp); \
					v3 = _mm_shuffle_epi8(v2, v3); \
   				v0 = _mm_blendv_epi8(v0, ramp, v0); \
          v2 = _mm_shuffle_epi8(v2, v0); \
          first = _mm_cvtsi128_si32(v3); \
          flips++; \
        } while (UNLIKELY(first & 0xff)); \
        checksum op flips; \
        if (flips > max_flips) max_flips = flips; \
      }
			X(+=) if (UNLIKELY(block_left == 1)) break; X(-=)
    } while (LIKELY(block_left -= 2));
  }

  __sync_add_and_fetch(&data->checksum, checksum);
  while (__sync_lock_test_and_set(&data->mutex, 1));
  if (data->max_flips < max_flips) data->max_flips = max_flips;
  __sync_lock_release(&data->mutex);
  return NULL;
}

#define MAX_THREADS 64

int main(int argc, char **argv) {   
  int i, n, nthreads = 4; uint64_t tmp = 1;
  __m128i ramp = _mm_setr_epi8(RAMP16);
  __m128i c1 = _mm_set1_epi8(1), v0, v1, v2;
  __m128i ramp1 = _mm_bsrli_si128(ramp, 1), old = ramp;
  factorials[0] = 1;
  v0 = _mm_sub_epi8(_mm_setzero_si128(), ramp);
  for (i = 0; i < MAX_N; v0 = _mm_add_epi8(v0, c1)) {
    v2 = _mm_blendv_epi8(v0, ramp, v0);
		v1 = _mm_blendv_epi8(ramp1, v2, _mm_sub_epi8(v0, c1));
		old = _mm_shuffle_epi8(old, v1);
    masks_shift[i] = old;
		tmp *= ++i;
		factorials[i] = tmp;
  }

  if (argc > 2 && !strcmp(argv[1], "-t"))
    argc -= 2, argv += 2, nthreads = atoi(*argv);
  if (nthreads < 1) nthreads = 1;
  if (nthreads > MAX_THREADS) nthreads = MAX_THREADS;

  while (argc-- > 1) {
    struct fannkuch_data data = { 0 };
    uint64_t block_end;
    pthread_t buf[MAX_THREADS];

    data.n = n = atoi(*++argv);
    if (n < 1 || n > MAX_N) return 1;

    block_end = factorials[n];
    data.block_size = block_end / (block_end > MAX_BLOCKS ? MAX_BLOCKS : 1);
    data.block_end = block_end;

    for (i = 1; i < nthreads; i++)
      pthread_create(buf + i, NULL, fannkuch_func, &data);
    fannkuch_func(&data);
    for (i = 1; i < nthreads; i++) pthread_join(buf[i], NULL);
    printf("%lld\nPfannkuchen(%u) = %u\n", data.checksum, n, data.max_flips);
  }
}
