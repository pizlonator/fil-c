
/*
 * BZip3 - A spiritual successor to BZip2.
 * Copyright (C) 2022-2024 Kamila Szewczyk
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBBZ3_H
#define LIBBZ3_H

#include <stddef.h>
#include <stdint.h>

/* Symbol visibility control. */
#ifndef BZIP3_VISIBLE
    #if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(__MINGW32__)
        #define BZIP3_VISIBLE __attribute__((visibility("default")))
    #else
        #define BZIP3_VISIBLE
    #endif
#endif

#if defined(BZIP3_DLL_EXPORT) && (BZIP3_DLL_EXPORT == 1)
    #define BZIP3_API __declspec(dllexport) BZIP3_VISIBLE
#elif defined(BZIP3_DLL_IMPORT) && (BZIP3_DLL_IMPORT == 1)
    #define BZIP3_API __declspec(dllimport) BZIP3_VISIBLE
#else
    #define BZIP3_API BZIP3_VISIBLE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BZ3_OK 0
#define BZ3_ERR_OUT_OF_BOUNDS -1
#define BZ3_ERR_BWT -2
#define BZ3_ERR_CRC -3
#define BZ3_ERR_MALFORMED_HEADER -4
#define BZ3_ERR_TRUNCATED_DATA -5
#define BZ3_ERR_DATA_TOO_BIG -6
#define BZ3_ERR_INIT -7
#define BZ3_ERR_DATA_SIZE_TOO_SMALL -8

struct bz3_state;

/**
 * @brief Get bzip3 version.
 */
BZIP3_API const char * bz3_version(void);

/**
 * @brief Get the last error number associated with a given state.
 */
BZIP3_API int8_t bz3_last_error(struct bz3_state * state);

/**
 * @brief Return a user-readable message explaining the cause of the last error.
 */
BZIP3_API const char * bz3_strerror(struct bz3_state * state);

/**
 * @brief Construct a new block encoder state, which will encode blocks as big as the given block size.
 * The decoder will be able to decode blocks at most as big as the given block size.
 * Returns NULL in case allocation fails or the block size is not between 65K and 511M
 */
BZIP3_API struct bz3_state * bz3_new(int32_t block_size);

/**
 * @brief Free the memory occupied by a block encoder state.
 */
BZIP3_API void bz3_free(struct bz3_state * state);

/**
 * @brief Return the recommended size of the output buffer for the compression functions.
 */
BZIP3_API size_t bz3_bound(size_t input_size);

/* ** HIGH LEVEL APIs ** */

/**
 * @brief Compress a frame. This function does not support parallelism
 * by itself, consider using the low level `bz3_encode_blocks()` function instead.
 * Using the low level API might provide better performance.
 * Returns a bzip3 error code; BZ3_OK when the operation is successful.
 * Make sure to set out_size to the size of the output buffer before the operation;
 * out_size must be at least equal to `bz3_bound(in_size)'.
 */
BZIP3_API int bz3_compress(uint32_t block_size, const uint8_t * in, uint8_t * out, size_t in_size, size_t * out_size);

/**
 * @brief Decompress a frame. This function does not support parallelism
 * by itself, consider using the low level `bz3_decode_blocks()` function instead.
 * Using the low level API might provide better performance.
 * Returns a bzip3 error code; BZ3_OK when the operation is successful.
 * Make sure to set out_size to the size of the output buffer before the operation.
 */
BZIP3_API int bz3_decompress(const uint8_t * in, uint8_t * out, size_t in_size, size_t * out_size);

/**
 * @brief Calculate the minimal memory required for compression with the given block size.
 * This includes all internal buffers and state structures. This calculates the amount of bytes
 * that will be allocated by a call to `bz3_new()`.
 * 
 * @details Memory allocation and usage patterns:
 * 
 * bz3_new():
 *    - Allocates all memory upfront:
 *      - Core state structure (sizeof(struct bz3_state))
 *      - Swap buffer (bz3_bound(block_size) bytes)
 *      - SAIS array (BWT_BOUND(block_size) * sizeof(int32_t) bytes)
 *      - LZP lookup table ((1 << LZP_DICTIONARY) * sizeof(int32_t) bytes)
 *      - Compression state (sizeof(state))
 *    - All memory remains allocated until bz3_free()
 * 
 * Additional memory may be used depending on API used from here.
 * 
 * # Low Level APIs
 * 
 * 1. bz3_encode_block() / bz3_decode_block():
 *    - Uses pre-allocated memory from bz3_new()
 *    - No additional memory allocation except for libsais (usually ~16KiB)
 *    - Peak memory usage of physical RAM varies with compression stages:
 *      - LZP: Uses LZP lookup table + swap buffer
 *      - BWT: Uses SAIS array + swap buffer
 *      - Entropy coding: Uses compression state (cm_state) + swap buffer
 * 
 * Using the higher level API, `bz3_compress`, expect an additional allocation
 * of `bz3_bound(block_size)`.
 * 
 * In the parallel version `bz3_encode_blocks`, each thread gets its own state,
 * so memory usage is `n_threads * bz3_compress_memory_needed()`.
 * 
 * # High Level APIs
 * 
 * 1. bz3_compress():
 *    - Allocates additional temporary compression buffer (bz3_bound(block_size) bytes)
 *      in addition to the memory amount returned by this method call and libsais.
 *    - Everything is freed after compression completes
 * 
 * 2. bz3_decompress():
 *    - Allocates additional temporary compression buffer (bz3_bound(block_size) bytes)
 *      in addition to the memory amount returned by this method call and libsais.
 *    - Everything is freed after compression completes
 * 
 * Memory remains constant during operation, with except of some small allocations from libsais during
 * BWT stage. That is not accounted by this function, though it usually amounts to ~16KiB, negligible.
 * The worst case of BWT is 2*block_size technically speaking.
 * 
 * No dynamic (re)allocation occurs outside of that.
 * 
 * @param block_size The block size to be used for compression
 * @return The total number of bytes required for compression, or 0 if block_size is invalid
 */
BZIP3_API size_t bz3_min_memory_needed(int32_t block_size);

/* ** LOW LEVEL APIs ** */

/**
 * @brief Encode a single block. Returns the amount of bytes written to `buffer'.
 * `buffer' must be able to hold at least `bz3_bound(size)' bytes. The size must not
 * exceed the block size associated with the state.
 */
BZIP3_API int32_t bz3_encode_block(struct bz3_state * state, uint8_t * buffer, int32_t size);

/**
 * @brief Decode a single block.
 * 
 * `buffer' must be able to hold at least `bz3_bound(orig_size)' bytes
 * in order to ensure decompression will succeed for all possible bzip3 blocks.
 * 
 * In most (but not all) cases, `orig_size` should usually be sufficient.
 * If it is not sufficient, you must allocate a buffer of size `bz3_bound(orig_size)` temporarily. 
 * 
 * If `buffer_size` is too small, `BZ3_ERR_DATA_SIZE_TOO_SMALL` will be returned.
 * The size must not exceed the block size associated with the state.
 * 
 * @param buffer_size The size of the buffer at 'buffer'
 * @param compressed_size The size of the compressed data in 'buffer'
 * @param orig_size The original size of the data before compression.
 */
BZIP3_API int32_t bz3_decode_block(struct bz3_state * state, uint8_t * buffer, size_t buffer_size, int32_t compressed_size, int32_t orig_size);

/**
 * @brief Encode `n' blocks, all in parallel.
 * All specifics of the `bz3_encode_block' still hold. The function will launch a thread for each block.
 * The compressed sizes are written to the `sizes' array. Every buffer is overwritten and none of them can overlap.
 * Precisely `n' states, buffers and sizes must be supplied.
 *
 * Expects `n' between 2 and 16.
 *
 * Present in the shared library only if -lpthread was present during building.
 */
BZIP3_API void bz3_encode_blocks(struct bz3_state * states[], uint8_t * buffers[], int32_t sizes[], int32_t n);

/**
 * @brief Decode `n' blocks, all in parallel.
 * Same specifics as `bz3_encode_blocks', but doesn't overwrite `sizes'.
 */
BZIP3_API void bz3_decode_blocks(struct bz3_state * states[], uint8_t * buffers[], size_t buffer_sizes[], int32_t sizes[],
                                 int32_t orig_sizes[], int32_t n);

/**
 * @brief Check if using original file size as buffer size is sufficient for decompressing
 * a block at `block` pointer.
 * 
 * @param block Pointer to the compressed block data
 * @param block_size Size of the block buffer in bytes (must be at least 13 bytes for header)
 * @param orig_size Size of the original uncompressed data 
 * @return 1 if original size is sufficient, 0 if insufficient, -1 on header error (insufficient buffer size)
 * 
 * @remarks
 * 
 *      This function is useful for external APIs using the low level block encoding API,
 *      `bz3_encode_block`. You would normally call this directly after `bz3_encode_block`
 *      on the block that has been output.
 *      
 *      The purpose of this function is to prevent encoding blocks that would require an additional
 *      malloc at decompress time.
 *      The goal is to prevent erroring with `BZ3_ERR_DATA_SIZE_TOO_SMALL`, thus
 *      in turn 
 */
BZIP3_API int bz3_orig_size_sufficient_for_decode(const uint8_t * block, size_t block_size, int32_t orig_size);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
