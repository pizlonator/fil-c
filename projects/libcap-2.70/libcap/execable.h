/*
 * Copyright (c) 2021 Andrew G. Morgan <morgan@kernel.org>
 *
 * Some header magic to help make a shared object run-able as a stand
 * alone executable binary.
 *
 * This is a slightly more sophisticated implementation than the
 * answer I posted here:
 *
 *    https://stackoverflow.com/a/68339111/14760867
 *
 * Compile your shared library with:
 *
 *   -DSHARED_LOADER="\"ld-linux...\"" (loader for your target system)
 *   ...
 *   --entry=__so_start
 */
#include <stdlib.h>
#include <string.h>

#ifdef __EXECABLE_H
#error "only include execable.h once"
#endif
#define __EXECABLE_H

const char __execable_dl_loader[] __attribute((section(".interp"))) =
    SHARED_LOADER ;

static void __execable_parse_args(int *argc_p, char ***argv_p)
{
    int argc = 0;
    char **argv = NULL;
    FILE *f = fopen("/proc/self/cmdline", "rb");
    if (f != NULL) {
	char *mem = NULL, *p;
	size_t size = 32, offset;
	for (offset=0; ; size *= 2) {
	    char *new_mem = realloc(mem, size+1);
	    if (new_mem == NULL) {
		perror("unable to parse arguments");
		if (mem != NULL) {
		    free(mem);
		}
		exit(1);
	    }
	    mem = new_mem;
	    offset += fread(mem+offset, 1, size-offset, f);
	    if (offset < size) {
		size = offset;
		mem[size] = '\0';
		break;
	    }
	}
	fclose(f);
	for (argc=1, p=mem+size-2; p >= mem; p--) {
	    argc += (*p == '\0');
	}
	argv = calloc(argc+1, sizeof(char *));
	if (argv == NULL) {
	    perror("failed to allocate memory for argv");
	    free(mem);
	    exit(1);
	}
	for (p=mem, argc=0, offset=0; offset < size; argc++) {
	    argv[argc] = mem+offset;
	    offset += strlen(mem+offset)+1;
	}
    }
    *argc_p = argc;
    *argv_p = argv;
}

/*
 * Linux x86 ABI requires the stack be 16 byte aligned. Keep things
 * simple and just force it.
 */
#if defined(__i386__) || defined(__x86_64__)
#define __SO_FORCE_ARG_ALIGNMENT  __attribute__((force_align_arg_pointer))
#else
#define __SO_FORCE_ARG_ALIGNMENT
#endif /* def some x86 */

/*
 * Permit the compiler to override this one.
 */
#ifndef EXECABLE_INITIALIZE
#define EXECABLE_INITIALIZE do { } while(0)
#endif /* ndef EXECABLE_INITIALIZE */

/*
 * Note, to avoid any runtime confusion, SO_MAIN is a void static
 * function.
 */
#define SO_MAIN							\
static void __execable_main(int, char**);			\
__attribute__((visibility ("hidden")))                          \
void __so_start(void);					        \
__SO_FORCE_ARG_ALIGNMENT					\
void __so_start(void)						\
{								\
    int argc;							\
    char **argv;						\
    __execable_parse_args(&argc, &argv);			\
    EXECABLE_INITIALIZE;                                        \
    __execable_main(argc, argv);				\
    if (argc != 0) {						\
	free(argv[0]);						\
	free(argv);						\
    }								\
    exit(0);							\
}								\
static void __execable_main
