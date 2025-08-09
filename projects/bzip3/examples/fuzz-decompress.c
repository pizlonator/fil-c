/* A tiny utility for fuzzing bzip3 frame decompression.
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
 * afl-clang fuzz-decompress.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 * 
 * 3. Make a fuzzer input file.
 * 
 * With `your_file` being an arbitrary input to test, use this utility
 * to generate a compressed test frame:
 * 
 * ./fuzz hl-api.c hl-api.c.bz3 8
 * mv hl-api.c.bz3 afl_in/
 * 
 * 4. Build binary (for fuzzing).
 * 
 * afl-clang-fast fuzz-decompress.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
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
 *      clang fuzz-decompress.c -g3 -O3 -march=native -o fuzz_asan -I../include "-DVERSION=\"0.0.0\"" -fsanitize=undefined -fsanitize=address
 *
 * And run fuzz_asan on the crashing test case (you can find it in one of the `afl_out/crashes/` folders).
 * Attach the test case /and/ the output of fuzz_asan to the bug report.
 * 
 * If no error occurs, it could be that there was a memory corruption `between` the runs.
 * In which case, you want to run AFL with address sanitizer. Use `export AFL_USE_ASAN=1` to enable
 * addres sanitizer; then run AFL.
 * 
 * export AFL_USE_ASAN=1
 * afl-clang-fast fuzz-decompress.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 */


/*
This hex editor template can be used to help debug a breaking file.
Would provide for ImHex, but ImHex terminates if template is borked.

//------------------------------------------------
//--- 010 Editor v15.0.1 Binary Template
//
//      File: bzip3-fuzz-decompress.bt
//   Authors: Sewer56
//   Version: 1.0.0
//   Purpose: Parse bzip3 fuzzer data
//------------------------------------------------

// Colors for different sections
#define COLOR_HEADER     0xA0FFA0 // Frame header
#define COLOR_BLOCKHEAD  0xFFB0B0 // Block headers
#define COLOR_DATA       0xB0B0FF // Compressed data

local uint32 currentBlockSize; // Store block size globally

// Frame header structure
typedef struct {
    char signature[5];     // "BZ3v1"
    uint32 blockSize;      // Maximum block size
    uint32 block_count;
} FRAME_HEADER <bgcolor=COLOR_HEADER>;

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

// Main block structure
typedef struct {
    uint32 compressedSize;  // Size of compressed block
    uint32 origSize;        // Original uncompressed size
    
    currentBlockSize = compressedSize; // Store for use in SMALL_BLOCK
    
    if(origSize < 64) {
        SMALL_BLOCK content;
    } else {
        BLOCK_HEADER header;
        uchar data[compressedSize - (Popcount(header.model) * 4 + 9)];
    }
} BLOCK <bgcolor=COLOR_DATA>;

// Helper function for bit counting (used for header size calculation)
int Popcount(byte b) {
    local int count = 0;
    while(b) {
        count += b & 1;
        b >>= 1;
    }
    return count;
}

// Main parsing structure
uint32 orig_size;
FRAME_HEADER frameHeader;

// Read blocks until end of file
while(!FEof()) {
    BLOCK block;
}

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

// Maximum allowed size to prevent excessive memory allocation
#define MAX_SIZE 0x10000000 // 256MB

// Returns 0 on success, negative on input validation errors, positive on bzip3 errors
static int try_decompress(const uint8_t *input_buf, size_t input_len) {
    if (input_len < 8) { // invalid, does not contain orig_size
        return -1;
    }

    size_t orig_size = *(const uint32_t *)input_buf;
    uint8_t *outbuf = malloc(orig_size);
    if (!outbuf) {
        return -3;
    }

    // We read orig_size from the input as we also want to fuzz it.
    int bzerr = bz3_decompress(
        input_buf + sizeof(uint32_t),
        outbuf,
        input_len - sizeof(uint32_t),
        &orig_size
    );

    if (bzerr != BZ3_OK) {
        printf("bz3_decompress() failed with error code %d\n", bzerr);
    } else {
        printf("OK, %d => %d\n", (int)input_len, (int)orig_size);
    }

    free(outbuf);
    return bzerr;
}

static int compress_file(const char *infile, const char *outfile, uint32_t block_size) {
    block_size = block_size <= KiB(65) ? KiB(65) : block_size;
    
    // Read the data into `inbuf`
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

    // Make buffer for output.
    size_t outsize = bz3_bound(insize);
    uint8_t *outbuf = malloc(outsize + sizeof(uint32_t));
    if (!outbuf) {
        free(inbuf);
        return 1;
    }

    // Store original size at the start
    // This is important, the `try_decompress` will read this field during fuzzing.
    // And pass it as a parameter to `bz3_decompress`. 
    *(uint32_t *)outbuf = insize;

    int bzerr = bz3_compress(block_size, inbuf, outbuf + sizeof(uint32_t), insize, &outsize);
    if (bzerr != BZ3_OK) {
        printf("bz3_compress() failed with error code %d\n", bzerr);
        free(inbuf);
        free(outbuf);
        return bzerr;
    }

    FILE *fp_out = fopen(outfile, "wb");
    if (!fp_out) {
        perror("Failed to open output file");
        free(inbuf);
        free(outbuf);
        return 1;
    }

    fwrite(outbuf, 1, outsize + sizeof(uint32_t), fp_out);
    fclose(fp_out);

    printf("Compressed %s (%zu bytes) to %s (%zu bytes)\n", 
           infile, insize, outfile, outsize + sizeof(uint32_t));

    free(inbuf);
    free(outbuf);
    return 0;
}

int main(int argc, char **argv) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    
    while (__AFL_LOOP(1000)) {
        try_decompress(__AFL_FUZZ_TESTCASE_BUF, __AFL_FUZZ_TESTCASE_LEN);
    }
#else
    if (argc == 4) {
        // Compression mode: input_file output_file block_size
        return compress_file(argv[1], argv[2], atoi(argv[3]));
    }
    
    if (argc != 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  Decompress: %s <input_file>\n", argv[0]);
        fprintf(stderr, "  Compress:   %s <input_file> <output_file> <block_size>\n", argv[0]);
        return 1;
    }

    // Decompression mode
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Failed to open input file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 64) {
        fclose(fp);
        return 0;
    }

    uint8_t *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return 1;
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    int result = try_decompress(buffer, size);
    free(buffer);
    return result > 0 ? result : 0; // Return bzip3 errors but treat validation errors as success
#endif

    return 0;
}