/* A tiny utility for fuzzing bzip3 round-trip compression/decompression.
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
 * 2. Insert a test file to afl_in/
 * 
 * cp ./standard_test_files/63_byte_file.bin afl_in/
 * 
 * 3. Build binary (for fuzzing)
 * 
 * afl-clang-fast fuzz-round-trip.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 * 
 * 4. Run the fuzzer
 * 
 * AFL_SKIP_CPUFREQ=1 afl-fuzz -i afl_in -o afl_out -- ./fuzz @@
 *
 * 5. Need to go faster? Multithread.
 * 
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -M fuzzer01 -- ./fuzz @@; exec bash" &
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -S fuzzer02 -- ./fuzz @@; exec bash" &
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -S fuzzer03 -- ./fuzz @@; exec bash" &
 * alacritty -e bash -c "afl-fuzz -i afl_in -o afl_out -S fuzzer04 -- ./fuzz @@; exec bash" &
 * 
 * etc. Replace `alacritty` with your terminal.
 * 
 * 6. For ASAN testing:
 *
 * export AFL_USE_ASAN=1
 * afl-clang-fast fuzz-round-trip.c -I../include -o fuzz -g3 "-DVERSION=\"0.0.0\"" -O3 -march=native
 */

#include "../include/libbz3.h"
#include "../src/libbz3.c"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define KiB(x) ((x)*1024)
#define DEFAULT_BLOCK_SIZE KiB(65)

// Required for AFL++ persistent mode
#ifdef __AFL_HAVE_MANUAL_CONTROL
#include <unistd.h>
__AFL_FUZZ_INIT();
#endif

// Function to emulate a crash for diagnostic purposes
static void __attribute__((noreturn)) crash_with_message(const char* msg) {
    fprintf(stderr, "Emulating crash: %s\n", msg);
    // Use abort() to generate a crash that ASAN and other tools can catch
    abort();
}

// Returns 0 on success, crashes on failure
static int try_round_trip(const uint8_t *input_buf, size_t input_len) {
    if (input_len == 0) return 0;

    // Use the larger of DEFAULT_BLOCK_SIZE or input_len
    size_t block_size = input_len > DEFAULT_BLOCK_SIZE ? input_len : DEFAULT_BLOCK_SIZE;
    
    struct bz3_state *state = bz3_new(block_size);
    if (!state) {
        return -1; // allocation failures not tested.
    }

    // Allocate buffer for both compression and decompression
    // Using block_size to ensure we have enough space for both operations
    size_t comp_buf_len = bz3_bound(input_len);
    uint8_t *comp_buf = malloc(comp_buf_len);
    if (!comp_buf) {
        bz3_free(state);
        return -1; // allocation failures not tested.
    }

    // Step 0: Move input to compress buffer
    memmove(comp_buf, input_buf, input_len);

    // Step 1: Compress the input
    int32_t comp_size = bz3_encode_block(state, comp_buf, input_len);
    if (comp_size < 0) {
        bz3_free(state);
        free(comp_buf);
        crash_with_message("Compression failed");
    }

    // Step 2: Decompress
    int bzerr = bz3_decode_block(state, comp_buf, comp_buf_len, comp_size, input_len);
    if (bzerr < 0 || bzerr != input_len) {
        bz3_free(state);
        free(comp_buf);
        crash_with_message("Decompression failed");
    }

    // Step 3: Compare
    if (memcmp(input_buf, comp_buf, input_len) != 0) {
        bz3_free(state);
        free(comp_buf);
        crash_with_message("Round-trip data mismatch");
    }

    bz3_free(state);
    free(comp_buf);
    return 0;
}

static int test_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
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
        crash_with_message("Failed to allocate input buffer");
    }

    if (fread(buffer, 1, size, fp) != size) {
        fclose(fp);
        free(buffer);
        crash_with_message("Failed to read input file");
    }
    fclose(fp);

    int result = try_round_trip(buffer, size);
    free(buffer);
    return result;
}

int main(int argc, char **argv) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    
    while (__AFL_LOOP(1000)) {
        try_round_trip(__AFL_FUZZ_TESTCASE_BUF, __AFL_FUZZ_TESTCASE_LEN);
    }
#else
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    return test_file(argv[1]);
#endif

    return 0;
}