/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2022 Roger Knecht <rknecht@pm.me>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config TREE
//config:	bool "tree (2.5 kb)"
//config:	default y
//config:	help
//config:	List files and directories in a tree structure.

//applet:IF_TREE(APPLET(tree, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TREE) += tree.o

//usage:#define tree_trivial_usage NOUSAGE_STR
//usage:#define tree_full_usage ""

#include "libbb.h"
#include "common_bufsiz.h"
#include "unicode.h"

#define prefix_buf bb_common_bufsiz1

static void tree_print(unsigned count[2], const char* directory_name, char* prefix_pos)
{
	struct dirent **entries;
	int index, size;
	const char *bar = "|   ";
	const char *mid = "|-- ";
	const char *end = "`-- ";

#if ENABLE_UNICODE_SUPPORT
	if (unicode_status == UNICODE_ON) {
		bar = "│   ";
		mid = "├── ";
		end = "└── ";
	}
#endif

	// read directory entries
	size = scandir(directory_name, &entries, NULL, alphasort);

	if (size < 0) {
		fputs_stdout(directory_name);
		puts(" [error opening dir]");
		return;
	}

	// print directory name
	puts(directory_name);

	// switch to sub directory
	xchdir(directory_name);

	// print all directory entries
	for (index = 0; index < size;) {
		struct dirent *dirent = entries[index++];

		// filter hidden files and directories
		if (dirent->d_name[0] != '.') {
			int status;
			struct stat statBuf;

//TODO: when -l is implemented, use stat, not lstat, if -l
			status = lstat(dirent->d_name, &statBuf);

			if (index == size) {
				strcpy(prefix_pos, end);
			} else {
				strcpy(prefix_pos, mid);
			}
			fputs_stdout(prefix_buf);

			if (status == 0 && S_ISLNK(statBuf.st_mode)) {
				// handle symlink
				char* symlink_path = xmalloc_readlink(dirent->d_name);
				printf("%s -> %s\n", dirent->d_name, symlink_path);
				free(symlink_path);
				count[1]++;
			} else if (status == 0 && S_ISDIR(statBuf.st_mode)
			 && (prefix_pos - prefix_buf) < (COMMON_BUFSIZE - 16)
			) {
				// handle directory
				char* pos;
				if (index == size) {
					pos = stpcpy(prefix_pos, "    ");
				} else {
					pos = stpcpy(prefix_pos, bar);
				}
				tree_print(count, dirent->d_name, pos);
				count[0]++;
			} else {
				// handle file
				puts(dirent->d_name);
				count[1]++;
			}
		}

		// release directory entry
		free(dirent);
	}

	// release directory array
	free(entries);

	// switch to parent directory
	xchdir("..");
}

int tree_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tree_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned count[2] = { 0, 0 };

	setup_common_bufsiz();
	init_unicode();

	if (!argv[1])
		*argv-- = (char*)".";

	// list directories given as command line arguments
	while (*(++argv))
		tree_print(count, *argv, prefix_buf);

	// print statistic
	printf("\n%u directories, %u files\n", count[0], count[1]);

	return EXIT_SUCCESS;
}
