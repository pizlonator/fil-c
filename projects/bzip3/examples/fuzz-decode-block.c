/* A tiny utility for fuzzing bzip3 block decompression.
 *
 * Prerequisites:
 * 
 * - AFL https://github.com/AFLplusplus/AFLplusplus
 * - clang (part of LLVM)
 * 
 * On Arch this is `pacman -S afl++ clang`
 *
 * # Instructions:
 * 
 * 1. Prepare fuzzer directories
 * 
 * mkdir -p afl_in && mkdir -p afl_out
 * 
 * 2. Build binary (to compress test data).
 * 
 * afl-clang fuzz-decode-block.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 * 
 * 3. Make a fuzzer input file.
 * 
 * With `your_file` being an arbitrary input to test, use this utility
 * to generate a compressed test block:
 * 
 * ./fuzz standard_test_files/63_byte_file.bin 63_byte_file.bin.bz3b 8
 * ./fuzz standard_test_files/65_byte_file.bin 65_byte_file.bin.bz3b 8
 * mv 63_byte_file.bin.bz3b afl_in/
 * mv 65_byte_file.bin.bz3b afl_in/
 * 
 * For this test, it is recommended to make 2 files, one that's <64 bytes and one that's >64 bytes.
 * 
 * 4. Build binary (for fuzzing).
 * 
 * afl-clang-fast fuzz-decode-block.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 * 
 * 5. Run the fuzzer.
 * 
 * AFL_SKIP_CPUFREQ=1 afl-fuzz -i afl_in -o afl_out -- ./fuzz @@
 *
 * 6. Wanna go faster? Multithread.
 * 
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -M fuzzer01 -- ./fuzz @@; exec bash" &
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -S fuzzer02 -- ./fuzz @@; exec bash" &
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -S fuzzer03 -- ./fuzz @@; exec bash" &
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -S fuzzer04 -- ./fuzz @@; exec bash" &
 * 
 * etc. Replace `alacritty` with your terminal.
 * 
 * And check progress with `afl-whatsup afl_out` (updates periodically).
 * 
 * 7. Found a crash?
 * 
 * If you find a crash, consider also doing the following:
 * 
 *      clang fuzz-decode-block.c -g3 -O3 -march=native -o fuzz_asan -I../include "-DVERSION=\"0.0.0\"" -fsanitize=undefined -fsanitize=address
 *
 * And run fuzz_asan on the crashing test case (you can find it in one of the `afl_out/crashes/` folders).
 * Attach the test case /and/ the output of fuzz_asan to the bug report.
 * 
 * If no error occurs, it could be that there was a memory corruption `between` the runs.
 * In which case, you want to run AFL with address sanitizer. Use `export AFL_USE_ASAN=1` to enable
 * addres sanitizer; then run AFL.
 * 
 * export AFL_USE_ASAN=1
 * afl-clang-fast fuzz-decode-block.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 */

/*

This hex editor template can be used to help debug a breaking file.
Would provide for ImHex, but ImHex terminates if template is borked.


//------------------------------------------------
//--- 010 Editor v15.0.1 Binary Template
//
//      File: bzip3block.bt
//   Authors: Sewer56
//   Version: 1.0.0
//   Purpose: Parse bzip3 fuzzer block data
//  Category: Archive
// File Mask: *.bz3b
//------------------------------------------------

// Colors for different sections
#define COLOR_HEADER     0xA0FFA0 // Block metadata
#define COLOR_BLOCKHEAD  0xFFB0B0 // Block headers
#define COLOR_DATA       0xB0B0FF // Compressed data

local uint32 currentBlockSize; // Store block size globally

// Block metadata structure
typedef struct {
    uint32 orig_size;      // Original uncompressed size
    uint32 comp_size;      // Compressed size
    uint32 buffer_size;    // Size of decompression buffer
} BLOCK_META <bgcolor=COLOR_HEADER>;

// Regular block header (for blocks >= 64 bytes)
typedef struct {
    uint32 crc32;         // CRC32 checksum of uncompressed data
    uint32 bwtIndex;      // Burrows-Wheeler transform index
    uint8  model;         // Compression model flags:
                         // bit 1 (0x02): LZP was used
                         // bit 2 (0x04): RLE was used
    
    // Optional size fields based on compression flags
    if(model & 0x02)     
        uint32 lzpSize;   // Size after LZP compression
    if(model & 0x04)     
        uint32 rleSize;   // Size after RLE compression
} BLOCK_HEADER <bgcolor=COLOR_BLOCKHEAD>;

// Small block header (for blocks < 64 bytes)
typedef struct {
    uint32 crc32;        // CRC32 checksum
    uint32 literal;      // Always 0xFFFFFFFF for small blocks
    uint8 data[currentBlockSize - 8]; // Uncompressed data
} SMALL_BLOCK <bgcolor=COLOR_BLOCKHEAD>;

// Block content structure
typedef struct {
    currentBlockSize = meta.comp_size;
    
    if(meta.orig_size < 64) {
        SMALL_BLOCK content;
    } else {
        BLOCK_HEADER header;
        uchar data[meta.comp_size - (Popcount(header.model) * 4 + 9)];
    }
} BLOCK_CONTENT <bgcolor=COLOR_DATA>;

// Helper function for bit counting (used for header size calculation)
int Popcount(byte b) {
    local int count = 0;
    while(b) {
        count += b & 1;
        b >>= 1;
    }
    return count;
}

// Main block structure
typedef struct {
    BLOCK_META meta;
    BLOCK_CONTENT content;
} BLOCK;

// Main parsing structure
BLOCK block;
*/

#include "../include/libbz3.h"
#include "../src/libbz3.c"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define KiB(x) ((x)*1024)

// Required for AFL++ persistent mode
#ifdef __AFL_HAVE_MANUAL_CONTROL
#include <unistd.h>
__AFL_FUZZ_INIT();
#endif

size_t min_size_t(size_t a, size_t b) {
    return (a < b) ? a : b;
}

// Returns 0 on success, positive on bzip3 errors
static int try_decode_block(const uint8_t *input_buf, size_t input_len) {
    // Read whatever metadata we can get
    uint32_t orig_size = 0;
    uint32_t comp_size = 0;
    uint32_t buffer_size = 0;
    
    if (input_len >= 4) orig_size = *(const uint32_t *)input_buf;
    if (input_len >= 8) comp_size = *(const uint32_t *)(input_buf + 4);
    if (input_len >= 12) buffer_size = *(const uint32_t *)(input_buf + 8);
    
    // Initialize state with minimum block size
    struct bz3_state *state = bz3_new(KiB(65));
    if (!state) return 0; // not under test

    // Allocate buffer with fuzzer-provided size
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        bz3_free(state);
        return 0; // not under test
    }

    // Copy whatever compressed data we can get
    size_t data_len = input_len > 12 ? input_len - 12 : 0;
    if (data_len > 0) {
        memcpy(buffer, input_buf + 12, min_size_t(data_len, (size_t)buffer_size));
    }

    // Attempt decompression with potentially invalid parameters
    int bzerr = bz3_decode_block(state, buffer, buffer_size, comp_size, orig_size);
    // and pray we don't crash :p

    free(buffer);
    bz3_free(state);
    return bzerr;
}

static int encode_block(const char *infile, const char *outfile, uint32_t block_size) {
    block_size = block_size <= KiB(65) ? KiB(65) : block_size;
    
    // Read input file
    FILE *fp_in = fopen(infile, "rb");
    if (!fp_in) {
        perror("Failed to open input file");
        return 1;
    }

    fseek(fp_in, 0, SEEK_END);
    size_t insize = ftell(fp_in);
    fseek(fp_in, 0, SEEK_SET);

    uint8_t *inbuf = malloc(insize);
    if (!inbuf) {
        fclose(fp_in);
        return 1;
    }

    fread(inbuf, 1, insize, fp_in);
    fclose(fp_in);

    // Initialize compression state
    struct bz3_state *state = bz3_new(block_size);
    if (!state) {
        free(inbuf);
        return 1;
    }

    // Make output buffer
    size_t outsize = bz3_bound(insize);
    uint8_t *outbuf = malloc(outsize + 12); // +12 for metadata
    if (!outbuf) {
        bz3_free(state);
        free(inbuf);
        return 1;
    }

    // Store metadata
    *(uint32_t *)outbuf = insize;        // Original size
    *(uint32_t *)(outbuf + 8) = outsize; // Buffer size needed for decompression
    
    // Compress the block
    int32_t comp_size = bz3_encode_block(state, outbuf + 12, insize);
    if (comp_size < 0) {
        printf("bz3_encode_block() failed with error code %d\n", comp_size);
        bz3_free(state);
        free(inbuf);
        free(outbuf);
        return comp_size;
    }

    // Store compressed size
    *(uint32_t *)(outbuf + 4) = comp_size;

    FILE *fp_out = fopen(outfile, "wb");
    if (!fp_out) {
        perror("Failed to open output file");
        bz3_free(state);
        free(inbuf);
        free(outbuf);
        return 1;
    }

    fwrite(outbuf, 1, comp_size + 12, fp_out);
    fclose(fp_out);

    printf("Encoded block from %s (%zu bytes) to %s (%d bytes)\n", 
           infile, insize, outfile, comp_size + 12);

    bz3_free(state);
    free(inbuf);
    free(outbuf);
    return 0;
}

int main(int argc, char **argv) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    
    while (__AFL_LOOP(1000)) {
        try_decode_block(__AFL_FUZZ_TESTCASE_BUF, __AFL_FUZZ_TESTCASE_LEN);
    }
#else
    if (argc == 4) {
        // Compression mode: input_file output_file block_size
        return encode_block(argv[1], argv[2], atoi(argv[3]));
    }
    
    if (argc != 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  Decode: %s <input_file>\n", argv[0]);
        fprintf(stderr, "  Encode: %s <input_file> <output_file> <block_size>\n", argv[0]);
        return 1;
    }

    // Decode mode
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Failed to open input file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return 1;
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    int result = try_decode_block(buffer, size);
    free(buffer);
    return result > 0 ? result : 0; // Return bzip3 errors but treat validation errors as success
#endif

    return 0;
}