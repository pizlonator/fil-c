#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <keymap.h>
#include <kbdfile.h>

#include "libcommon.h"

int main(int __attribute__((unused)) argc, char **argv)
{
	set_progname(argv[0]);

	lk_table_shape table;
	char numeric;
	struct lk_ctx *ctx;
	struct kbdfile_ctx *kbdfile_ctx;
	struct kbdfile *fp;

	if (argc == 1) {
		printf("Usage: %s <keymap> <table_shape> <numeric>\n", argv[0]);
		return 1;
	}

	if ((kbdfile_ctx = kbdfile_context_new()) == NULL) {
		perror("nomem");
		exit(1);
	}

	if ((fp = kbdfile_new(kbdfile_ctx)) == NULL) {
		perror("nomem");
		exit(1);
	}

	if (!strcasecmp(argv[2], "FULL_TABLE"))
		table = LK_SHAPE_FULL_TABLE;
	else if (!strcasecmp(argv[2], "SEPARATE_LINES"))
		table = LK_SHAPE_SEPARATE_LINES;
	else if (!strcasecmp(argv[2], "UNTIL_HOLE"))
		table = LK_SHAPE_UNTIL_HOLE;
	else
		table = LK_SHAPE_DEFAULT;

	numeric = (!strcasecmp(argv[3], "TRUE")) ? 1 : 0;

	ctx = lk_init();
	lk_set_parser_flags(ctx, LK_FLAG_PREFER_UNICODE);

	kbdfile_set_pathname(fp, argv[1]);
	kbdfile_set_file(fp, fopen(argv[1], "r"));

	lk_parse_keymap(ctx, fp);
	lk_dump_keymap(ctx, stdout, table, numeric);
	lk_dump_diacs(ctx, stdout);

	kbdfile_free(fp);
	kbdfile_context_free(kbdfile_ctx);
	lk_free(ctx);
	return 0;
}
