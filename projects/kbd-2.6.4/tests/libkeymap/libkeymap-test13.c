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
	struct kmapinfo info;

	ctx = lk_init();
	lk_set_log_fn(ctx, NULL, NULL);

	if (lk_add_map(ctx, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map");

	lk_get_kmapinfo(ctx, &info);

	if (info.keymaps != 1)
		kbd_error(EXIT_FAILURE, 0, "Wrong keymap number");

	if (lk_add_map(ctx, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map");

	lk_get_kmapinfo(ctx, &info);

	if (info.keymaps != 1)
		kbd_error(EXIT_FAILURE, 0, "Wrong keymap number");

	if (lk_add_map(ctx, 1) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map");

	lk_get_kmapinfo(ctx, &info);

	if (info.keymaps != 2)
		kbd_error(EXIT_FAILURE, 0, "Wrong keymap number");

	if (lk_add_map(ctx, 2) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to define map");

	lk_get_kmapinfo(ctx, &info);

	if (info.keymaps != 3)
		kbd_error(EXIT_FAILURE, 0, "Wrong keymap number");

	lk_free(ctx);

	return EXIT_SUCCESS;
}
