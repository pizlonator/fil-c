#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <keymap.h>
#include "libcommon.h"

int
main(int __attribute__((unused)) argc, char **argv)
{
	set_progname(argv[0]);

	struct lk_ctx *ctx;

	ctx = lk_init();
	lk_set_log_fn(ctx, NULL, NULL);

	if (lk_add_map(ctx, MAX_NR_KEYMAPS) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map == MAX_NR_KEYMAPS");

	if (lk_add_map(ctx, MAX_NR_KEYMAPS * 2) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map == MAX_NR_KEYMAPS*2");

	if (lk_add_map(ctx, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map");

	if (lk_add_map(ctx, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map");

	lk_free(ctx);

	return EXIT_SUCCESS;
}
