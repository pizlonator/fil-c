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

	if (lk_add_key(ctx, 0, 0, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to add keycode = 0");

	if (lk_add_key(ctx, 0, 0, 16) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to add keycode = 16");

	if (lk_add_key(ctx, 1, 1, K_HOLE) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to add keycode = K_HOLE");

	lk_free(ctx);

	return EXIT_SUCCESS;
}
