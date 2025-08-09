
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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined __MSVCRT__
    #include <fcntl.h>
    #include <io.h>
#endif

#include "common.h"
#include "libbz3.h"
#include "yarg.h"

#define MODE_DECODE 0
#define MODE_ENCODE 1
#define MODE_TEST 2
#define MODE_RECOVER 3

static void version() {
    fprintf(stdout, "bzip3 " VERSION
                    "\n"
                    "Copyright (C) by Kamila Szewczyk, 2022-2023.\n"
                    "License: GNU Lesser GPL version 3 <https://www.gnu.org/licenses/lgpl-3.0.en.html>\n");
}

static void help() {
    fprintf(stdout,
            "bzip3 - better and stronger spiritual successor to bzip2.\n"
            "Usage: bzip3 [-e/-z/-d/-t/-c/-h/-V] [-b block_size] [-j jobs] files...\n"
            "Operations:\n"
            "  -e/-z, --encode   compress data (default)\n"
            "  -d, --decode      decompress data\n"
            "  -r, --recover     attempt at recovering corrupted data\n"
            "  -t, --test        verify validity of compressed data\n"
            "  -h, --help        display an usage overview\n"
            "  -f, --force       force overwriting output if it already exists\n"
            "      --rm          remove input files after successful (de)compression\n"
            "  -k, --keep        keep (don't delete) input files (default)\n"
            "  -v, --verbose     verbose mode (display more information)\n"
            "  -V, --version     display version information\n"
            "Extra flags:\n"
            "  -c, --stdout      force writing to standard output\n"
            "  -b N, --block=N   set block size in MiB {16}\n"
            "  -B, --batch       process all files specified as inputs\n"
#ifdef PTHREAD
            "  -j N, --jobs=N    set the amount of parallel threads\n"
#endif
            "\n"
            "Report bugs to: https://github.com/kspalaiologos/bzip3\n");
}

static void xwrite(const void * data, size_t size, size_t len, FILE * des) {
    if (len == 0 || size == 0) return;
    if (fwrite(data, size, len, des) != len) {
        fprintf(stderr, "Write error: %s\n", strerror(errno));
        exit(1);
    }
}

/* Read any amount of items (from 0 to len) as long as there is no error */
static size_t xread(void * data, size_t size, size_t len, FILE * des) {
    size_t written = fread(data, size, len, des);
    if (ferror(des)) {
        fprintf(stderr, "Read error: %s\n", strerror(errno));
        exit(1);
    }
    return written;
}

/* Either read 0 (due to eof) items or exactly len items */
static size_t xread_eofcheck(void * data, size_t size, size_t len, FILE * des) {
    size_t written = xread(data, size, len, des);
    /* feof will be true */
    if (!written) return 0;
    if (feof(des)) {
        fprintf(stderr, "Error: Corrupt file\n");
        exit(1);
    }
    return written;
}

/* Always read len items */
static void xread_noeof(void * data, size_t size, size_t len, FILE * des) {
    if (!xread_eofcheck(data, size, len, des)) {
        fprintf(stderr, "Error: Corrupt file\n");
        exit(1);
    }
}

static void close_out_file(FILE * des) {
    if (des) {
        int outfd = fileno(des);

        if (fflush(des)) {
            fprintf(stderr, "Error: Failed on fflush: %s\n", strerror(errno));
            exit(1);
        }

#ifdef __linux__
        while (1) {
            int status = fsync(outfd);
            if (status == -1) {
                if (errno == EINVAL) break;
                if (errno == EINTR) continue;
                fprintf(stderr, "Error: Failed on fsync: %s\n", strerror(errno));
                exit(1);
            }
            break;
        }
#endif

        if (des != stdout && fclose(des)) {
            fprintf(stderr, "Error: Failed on fclose: %s\n", strerror(errno));
            exit(1);
        }
    }
}

static void remove_in_file(char * file_name, FILE * output_des) {
    if (file_name == NULL) {
        return;
    }
    if (output_des == stdout) {
        return;
    }
    if (remove(file_name)) {
        fprintf(stderr, "Error: failed to remove input file `%s': %s\n", file_name, strerror(errno));
        exit(1);
    }
}

static int process(FILE * input_des, FILE * output_des, int mode, int block_size, int workers, int verbose,
                   char * file_name) {
    uint64_t bytes_read = 0, bytes_written = 0;

    if ((mode == MODE_ENCODE && isatty(fileno(output_des))) ||
        ((mode == MODE_DECODE || mode == MODE_TEST || mode == MODE_RECOVER) && isatty(fileno(input_des)))) {
        fprintf(stderr, "Refusing to read/write binary data from/to the terminal.\n");
        return 1;
    }

    // Reset errno after the isatty() call.
    errno = 0;

    u8 byteswap_buf[4];

    switch (mode) {
        case MODE_ENCODE:
            xwrite("BZ3v1", 5, 1, output_des);

            write_neutral_s32(byteswap_buf, block_size);
            xwrite(byteswap_buf, 4, 1, output_des);

            bytes_written += 9;
            break;
        case MODE_RECOVER:
        case MODE_DECODE:
        case MODE_TEST: {
            char signature[5];

            if (xread(signature, 5, 1, input_des) != 1 || strncmp(signature, "BZ3v1", 5) != 0) {
                fprintf(stderr, "Invalid signature.\n");
                return 1;
            }

            xread_noeof(byteswap_buf, 4, 1, input_des);

            block_size = read_neutral_s32(byteswap_buf);

            if (block_size < KiB(65) || block_size > MiB(511)) {
                fprintf(stderr,
                        "The input file is corrupted. Reason: Invalid block "
                        "size in the header.\n");
                if (mode == MODE_RECOVER) {
                    fprintf(stderr, "Recovery mode: Proceeding.\n");
                    block_size = MiB(511);
                } else {
                    return 1;
                }
            }

            bytes_read += 9;
            break;
        }
    }

#ifdef PTHREAD
    if (workers > 64 || workers < 0) {
        fprintf(stderr, "Number of workers must be between 0 and 64.\n");
        return 1;
    }

    if (workers <= 1) {
#endif
        struct bz3_state * state = bz3_new(block_size);

        if (state == NULL) {
            fprintf(stderr, "Failed to create a block encoder state.\n");
            return 1;
        }

        size_t buffer_size = bz3_bound(block_size);
        u8 * buffer = malloc(buffer_size);

        if (!buffer) {
            fprintf(stderr, "Failed to allocate memory.\n");
            return 1;
        }

        if (mode == MODE_ENCODE) {
            s32 read_count;
            while (!feof(input_des)) {
                read_count = xread(buffer, 1, block_size, input_des);
                bytes_read += read_count;

                if (read_count == 0) break;

                s32 new_size = bz3_encode_block(state, buffer, read_count);
                if (new_size == -1) {
                    fprintf(stderr, "Failed to encode a block: %s\n", bz3_strerror(state));
                    return 1;
                }

                write_neutral_s32(byteswap_buf, new_size);
                xwrite(byteswap_buf, 4, 1, output_des);
                write_neutral_s32(byteswap_buf, read_count);
                xwrite(byteswap_buf, 4, 1, output_des);
                xwrite(buffer, new_size, 1, output_des);
                bytes_written += 8 + new_size;
            }
            fflush(output_des);
        } else if (mode == MODE_DECODE) {
            s32 new_size, old_size;
            while (!feof(input_des)) {
                if (!xread_eofcheck(&byteswap_buf, 1, 4, input_des)) continue;

                new_size = read_neutral_s32(byteswap_buf);
                xread_noeof(&byteswap_buf, 1, 4, input_des);
                old_size = read_neutral_s32(byteswap_buf);
                if (old_size > bz3_bound(block_size) || new_size > bz3_bound(block_size)) {
                    fprintf(stderr, "Failed to decode a block: Inconsistent headers.\n");
                    return 1;
                }
                xread_noeof(buffer, 1, new_size, input_des);
                bytes_read += 8 + new_size;
                if (bz3_decode_block(state, buffer, buffer_size, new_size, old_size) == -1) {
                    fprintf(stderr, "Failed to decode a block: %s\n", bz3_strerror(state));
                    return 1;
                }
                xwrite(buffer, old_size, 1, output_des);
                bytes_written += old_size;
            }
            fflush(output_des);
        } else if (mode == MODE_RECOVER) {
            s32 new_size, old_size;
            while (!feof(input_des)) {
                if (!xread_eofcheck(&byteswap_buf, 1, 4, input_des)) continue;

                new_size = read_neutral_s32(byteswap_buf);
                xread_noeof(&byteswap_buf, 1, 4, input_des);
                old_size = read_neutral_s32(byteswap_buf);
                if (old_size > bz3_bound(block_size) || new_size > bz3_bound(block_size)) {
                    fprintf(stderr, "Failed to decode a block: Inconsistent headers.\n");
                    return 1;
                }
                xread_noeof(buffer, 1, new_size, input_des);
                bytes_read += 8 + new_size;
                if (bz3_decode_block(state, buffer, buffer_size, new_size, old_size) == -1) {
                    fprintf(stderr, "Writing invalid block: %s\n", bz3_strerror(state));
                }
                xwrite(buffer, old_size, 1, output_des);
                bytes_written += old_size;
            }
            fflush(output_des);
        } else if (mode == MODE_TEST) {
            s32 new_size, old_size;
            while (!feof(input_des)) {
                if (!xread_eofcheck(&byteswap_buf, 1, 4, input_des)) continue;
                new_size = read_neutral_s32(byteswap_buf);
                xread_noeof(&byteswap_buf, 1, 4, input_des);
                old_size = read_neutral_s32(byteswap_buf);
                if (old_size > bz3_bound(block_size) || new_size > bz3_bound(block_size)) {
                    fprintf(stderr, "Failed to decode a block: Inconsistent headers.\n");
                    return 1;
                }
                xread_noeof(buffer, 1, new_size, input_des);
                bytes_read += 8 + new_size;
                bytes_written += old_size;
                if (bz3_decode_block(state, buffer, buffer_size, new_size, old_size) == -1) {
                    fprintf(stderr, "Failed to decode a block: %s\n", bz3_strerror(state));
                    return 1;
                }
            }
        }

        if (bz3_last_error(state) != BZ3_OK && mode != MODE_RECOVER) {
            fprintf(stderr, "Failed to read data: %s\n", bz3_strerror(state));
            return 1;
        }

        free(buffer);

        bz3_free(state);
#ifdef PTHREAD
    } else {
        struct bz3_state * states[workers];
        u8 * buffers[workers];
        s32 sizes[workers];
        size_t buffer_sizes[workers];
        s32 old_sizes[workers];
        for (s32 i = 0; i < workers; i++) {
            states[i] = bz3_new(block_size);
            if (states[i] == NULL) {
                fprintf(stderr, "Failed to create a block encoder state.\n");
                return 1;
            }
            size_t buffer_size = bz3_bound(block_size);
            buffer_sizes[i] = buffer_size;
            buffers[i] = malloc(buffer_size);
            if (!buffers[i]) {
                fprintf(stderr, "Failed to allocate memory.\n");
                return 1;
            }
        }

        if (mode == MODE_ENCODE) {
            while (!feof(input_des)) {
                s32 i = 0;
                for (; i < workers; i++) {
                    size_t read_count = xread(buffers[i], 1, block_size, input_des);
                    bytes_read += read_count;
                    sizes[i] = old_sizes[i] = read_count;
                    if (read_count < block_size) {
                        i++;
                        break;
                    }
                }
                bz3_encode_blocks(states, buffers, sizes, i);
                for (s32 j = 0; j < i; j++) {
                    if (bz3_last_error(states[j]) != BZ3_OK) {
                        fprintf(stderr, "Failed to encode data: %s\n", bz3_strerror(states[j]));
                        return 1;
                    }
                }
                for (s32 j = 0; j < i; j++) {
                    write_neutral_s32(byteswap_buf, sizes[j]);
                    xwrite(byteswap_buf, 4, 1, output_des);
                    write_neutral_s32(byteswap_buf, old_sizes[j]);
                    xwrite(byteswap_buf, 4, 1, output_des);
                    xwrite(buffers[j], sizes[j], 1, output_des);
                    bytes_written += 8 + sizes[j];
                }
            }
            fflush(output_des);
        } else if (mode == MODE_DECODE) {
            while (!feof(input_des)) {
                s32 i = 0;
                for (; i < workers; i++) {
                    if (!xread_eofcheck(&byteswap_buf, 1, 4, input_des)) break;
                    sizes[i] = read_neutral_s32(byteswap_buf);
                    xread_noeof(&byteswap_buf, 1, 4, input_des);
                    old_sizes[i] = read_neutral_s32(byteswap_buf);
                    if (old_sizes[i] > bz3_bound(block_size) || sizes[i] > bz3_bound(block_size)) {
                        fprintf(stderr, "Failed to decode a block: Inconsistent headers.\n");
                        return 1;
                    }
                    xread_noeof(buffers[i], 1, sizes[i], input_des);
                    bytes_read += 8 + sizes[i];
                }
                bz3_decode_blocks(states, buffers, buffer_sizes, sizes, old_sizes, i);
                for (s32 j = 0; j < i; j++) {
                    if (bz3_last_error(states[j]) != BZ3_OK) {
                        fprintf(stderr, "Failed to decode data: %s\n", bz3_strerror(states[j]));
                        return 1;
                    }
                }
                for (s32 j = 0; j < i; j++) {
                    xwrite(buffers[j], old_sizes[j], 1, output_des);
                    bytes_written += old_sizes[j];
                }
            }
            fflush(output_des);
        } else if (mode == MODE_RECOVER) {
            while (!feof(input_des)) {
                s32 i = 0;
                for (; i < workers; i++) {
                    if (!xread_eofcheck(&byteswap_buf, 1, 4, input_des)) break;
                    sizes[i] = read_neutral_s32(byteswap_buf);
                    xread_noeof(&byteswap_buf, 1, 4, input_des);
                    old_sizes[i] = read_neutral_s32(byteswap_buf);
                    if (old_sizes[i] > bz3_bound(block_size) || sizes[i] > bz3_bound(block_size)) {
                        fprintf(stderr, "Failed to decode a block: Inconsistent headers.\n");
                        return 1;
                    }
                    xread_noeof(buffers[i], 1, sizes[i], input_des);
                    bytes_read += 8 + sizes[i];
                }
                bz3_decode_blocks(states, buffers, buffer_sizes, sizes, old_sizes, i);
                for (s32 j = 0; j < i; j++) {
                    if (bz3_last_error(states[j]) != BZ3_OK) {
                        fprintf(stderr, "Writing invalid block: %s\n", bz3_strerror(states[j]));
                    }
                }
                for (s32 j = 0; j < i; j++) {
                    xwrite(buffers[j], old_sizes[j], 1, output_des);
                    bytes_written += old_sizes[j];
                }
            }
            fflush(output_des);
        } else if (mode == MODE_TEST) {
            while (!feof(input_des)) {
                s32 i = 0;
                for (; i < workers; i++) {
                    if (!xread_eofcheck(&byteswap_buf, 1, 4, input_des)) break;
                    sizes[i] = read_neutral_s32(byteswap_buf);
                    xread_noeof(&byteswap_buf, 1, 4, input_des);
                    old_sizes[i] = read_neutral_s32(byteswap_buf);
                    if (old_sizes[i] > bz3_bound(block_size) || sizes[i] > bz3_bound(block_size)) {
                        fprintf(stderr, "Failed to decode a block: Inconsistent headers.\n");
                        return 1;
                    }
                    xread_noeof(buffers[i], 1, sizes[i], input_des);
                    bytes_read += 8 + sizes[i];
                    bytes_written += old_sizes[i];
                }
                bz3_decode_blocks(states, buffers, buffer_sizes, sizes, old_sizes, i);
                for (s32 j = 0; j < i; j++) {
                    if (bz3_last_error(states[j]) != BZ3_OK) {
                        fprintf(stderr, "Failed to decode data: %s\n", bz3_strerror(states[j]));
                        return 1;
                    }
                }
            }
        }

        for (s32 i = 0; i < workers; i++) {
            free(buffers[i]);
            bz3_free(states[i]);
        }
    }
#endif

    if (verbose) {
        if (file_name) fprintf(stderr, " %s:", file_name);
        if (mode == MODE_ENCODE)
            fprintf(stderr, "\t%" PRIu64 " -> %" PRIu64 " bytes, %.2f%%, %.2f bpb\n", bytes_read, bytes_written,
                    (double)bytes_written * 100.0 / bytes_read, (double)bytes_written * 8.0 / bytes_read);
        else if (mode == MODE_DECODE)
            fprintf(stderr, "\t%" PRIu64 " -> %" PRIu64 " bytes, %.2f%%, %.2f bpb\n", bytes_read, bytes_written,
                    (double)bytes_read * 100.0 / bytes_written, (double)bytes_read * 8.0 / bytes_written);
        else
            fprintf(stderr, "\tOK, %" PRIu64 " -> %" PRIu64 " bytes, %.2f%%, %.2f bpb\n", bytes_read, bytes_written,
                    (double)bytes_read * 100.0 / bytes_written, (double)bytes_read * 8.0 / bytes_written);
    }

    return 0;
}

static int is_dir(const char * path) {
    struct stat sb;
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) return 1;
    return 0;
}

static int is_numeric(const char * str) {
    for (; *str; str++)
        if (!isdigit(*str)) return 0;
    return 1;
}

static FILE * open_output(char * output, int force) {
    FILE * output_des = NULL;

    if (output != NULL) {
        if (is_dir(output)) {
            fprintf(stderr, "Error: output file `%s' is a directory.\n", output);
            exit(1);
        }

        if (access(output, F_OK) == 0) {
            if (!force) {
                fprintf(stderr, "Error: output file `%s' already exists. Use -f to force overwrite.\n", output);
                exit(1);
            }
        }

        output_des = fopen(output, "wb");
        if (output_des == NULL) {
            fprintf(stderr, "Error: failed to open output file `%s': %s\n", output, strerror(errno));
            exit(1);
        }
    } else {
        output_des = stdout;
    }

    return output_des;
}

static FILE * open_input(char * input) {
    FILE * input_des = NULL;

    if (input != NULL) {
        if (is_dir(input)) {
            fprintf(stderr, "Error: input `%s' is a directory.\n", input);
            exit(1);
        }

        input_des = fopen(input, "rb");
        if (input_des == NULL) {
            fprintf(stderr, "Error: failed to open input file `%s': %s\n", input, strerror(errno));
            exit(1);
        }
    } else {
        input_des = stdin;
    }

    return input_des;
}

int main(int argc, char * argv[]) {
    int mode = MODE_ENCODE;

    // input and output file names
    char *input = NULL, *output = NULL;
    char *f1 = NULL, *f2 = NULL;
    int force = 0;

    // command line arguments
    int force_stdstreams = 0, workers = 0, batch = 0, verbose = 0, remove_input_file = 0;

    // the block size
    u32 block_size = MiB(16);

    enum { RM_OPTION = CHAR_MAX + 1 };

    yarg_options opt[] = {
        {       'e', no_argument,       "encode" },
        {       'z', no_argument,       "encode" }, /* alias */
        {       'd', no_argument,       "decode" },
        {       't', no_argument,       "test" },
        {       'c', no_argument,       "stdout" },
        {       'f', no_argument,       "force" },
        {       'r', no_argument,       "recover" },
        {       'h', no_argument,       "help" },
        { RM_OPTION, no_argument,       "rm" },
        {       'k', no_argument,       "keep" },
        {       'V', no_argument,       "version" },
        {       'v', no_argument,       "verbose" },
        {       'b', required_argument, "block" },
        {       'B', no_argument,       "batch" },
#ifdef PTHREAD
        {       'j', required_argument, "jobs" },
#endif
        {         0, no_argument,       NULL }
    };
    yarg_settings settings = {
        .dash_dash = true,
        .style = YARG_STYLE_UNIX,
    };
    yarg_result * res = yarg_parse(argc, argv, opt, settings);
    if (!res) {
        fprintf(stderr, "bzip3: out of memory.\n");
        return 1;
    }
    if (res->error) {
        fputs(res->error, stderr);
        fputs("Try 'bzip3 --help' for more information.\n", stderr);
        return 1;
    }
    // `res' is not freed later on as it has the approximate lifetime
    // equal to the lifetime of the program overall.
    for (int i = 0; i < res->argc; i++) {
        switch(res->args[i].opt) {
            case 'e': case 'z': mode = MODE_ENCODE; break;
            case 'd': mode = MODE_DECODE; break;
            case 'r': mode = MODE_RECOVER; break;
            case 't': mode = MODE_TEST; break;
            case 'c': force_stdstreams = 1; break;
            case 'f': force = 1; break;
            case RM_OPTION: remove_input_file = 1; break;
            case 'k': break;
            case 'h': help(); return 0;
            case 'V': version(); return 0;
            case 'B': batch = 1; break;
            case 'v': verbose = 1; break;
            case 'b':
                if (!is_numeric(res->args[i].arg)) {
                    fprintf(stderr, "bzip3: invalid block size: %s\n", res->args[i].arg);
                    return 1;
                }
                block_size = MiB(atoi(res->args[i].arg));
                break;
#ifdef PTHREAD
            case 'j':
                if (!is_numeric(res->args[i].arg)) {
                    fprintf(stderr, "bzip3: invalid amount of jobs: %s\n", res->args[i].arg);
                    return 1;
                }
                workers = atoi(res->args[i].arg);
                break;
#endif
        }
    }

#if defined(__MSVCRT__)
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
#endif

    if (block_size < KiB(65) || block_size > MiB(511)) {
        fprintf(stderr, "Block size must be between 65 KiB and 511 MiB.\n");
        return 1;
    }

    if (batch && res->pos_argc) {
        switch (mode) {
            case MODE_ENCODE:
                /* Encode each of the files. */
                for (int i = 0; i < res->pos_argc; i++) {
                    char * arg = res->pos_args[i];

                    FILE * input_des = open_input(arg);
                    char * output_name;
                    if (force_stdstreams)
                        output_name = NULL;
                    else {
                        output_name = malloc(strlen(arg) + 5);
                        strcpy(output_name, arg);
                        strcat(output_name, ".bz3");
                    }

                    FILE * output_des = open_output(output_name, force);
                    process(input_des, output_des, mode, block_size, workers, verbose, arg);

                    fclose(input_des);
                    close_out_file(output_des);
                    if (!force_stdstreams) free(output_name);
                    if (remove_input_file) {
                        remove_in_file(arg, output_des);
                    }
                }
                break;
            case MODE_RECOVER:
            case MODE_DECODE:
                /* Decode each of the files. */
                for (int i = 0; i < res->pos_argc; i++) {
                    char * arg = res->pos_args[i];

                    FILE * input_des = open_input(arg);
                    char * output_name;
                    if (force_stdstreams)
                        output_name = NULL;
                    else {
                        output_name = malloc(strlen(arg) + 1);
                        strcpy(output_name, arg);
                        if (strlen(output_name) > 4 && !strcmp(output_name + strlen(output_name) - 4, ".bz3"))
                            output_name[strlen(output_name) - 4] = 0;
                        else {
                            fprintf(stderr, "Warning: file %s has an unknown extension, skipping.\n", arg);
                            return 1;
                        }
                    }

                    FILE * output_des = open_output(output_name, force);
                    process(input_des, output_des, mode, block_size, workers, verbose, arg);

                    fclose(input_des);
                    close_out_file(output_des);
                    if (!force_stdstreams) free(output_name);
                    if (remove_input_file) {
                        remove_in_file(arg, output_des);
                    }
                }
                break;
            case MODE_TEST:
                /* Test each of the files. */
                for (int i = 0; i < res->pos_argc; i++) {
                    char * arg = res->pos_args[i];

                    FILE * input_des = open_input(arg);
                    process(input_des, NULL, mode, block_size, workers, verbose, arg);
                    fclose(input_des);
                }
                break;
        }

        if (fclose(stdout)) {
            fprintf(stderr, "Error: Failed on fclose(stdout): %s\n", strerror(errno));
            return 1;
        }

        return 0;
    }

    for (int i = 0; i < res->pos_argc; i++) {
        char * arg = res->pos_args[i];

        if (f1 != NULL && f2 != NULL) {
            fprintf(stderr, "Error: too many files specified.\n");
            return 1;
        }

        if (f1 == NULL)
            f1 = arg;
        else
            f2 = arg;
    }

    if (f1 == NULL && f2 == NULL)
        input = NULL, output = NULL;
    else if (mode == MODE_TEST)
        input = f1;
    else {
        if (mode == MODE_ENCODE) {
            if (f2 == NULL) {
                // encode from f1?
                input = f1;
                if (force_stdstreams)
                    output = NULL;
                else {
                    output = malloc(strlen(f1) + 5);
                    strcpy(output, f1);
                    strcat(output, ".bz3");
                }
            } else {
                // encode from f1 to f2.
                input = f1;
                output = f2;
            }
        } else if (mode == MODE_DECODE || mode == MODE_RECOVER) {
            if (f2 == NULL) {
                // decode from f1 to stdout.
                input = f1;
                if (force_stdstreams)
                    output = NULL;
                else {
                    output = malloc(strlen(f1) + 1);
                    strcpy(output, f1);
                    if (strlen(output) > 4 && !strcmp(output + strlen(output) - 4, ".bz3"))
                        output[strlen(output) - 4] = 0;
                    else {
                        fprintf(stderr, "Warning: file %s has an unknown extension, skipping.\n", f1);
                        return 1;
                    }
                }
            } else {
                // decode from f1 to f2.
                input = f1;
                output = f2;
            }
        }
    }

    FILE *input_des = NULL, *output_des = NULL;

    input_des = open_input(input);
    output_des = mode != MODE_TEST ? open_output(output, force) : NULL;

    if (output != f2) free(output);

    int r = process(input_des, output_des, mode, block_size, workers, verbose, input);

    fclose(input_des);
    close_out_file(output_des);
    if (fclose(stdout)) {
        fprintf(stderr, "Error: Failed on fclose(stdout): %s\n", strerror(errno));
        return 1;
    }
    if (remove_input_file) {
        remove_in_file(input, output_des);
    }
    return r;
}