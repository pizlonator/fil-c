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
	lk_set_parser_flags(ctx, LK_KEYWORD_ALTISMETA);

	if (lk_add_key(ctx, 0, 0, 16) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to add keycode");

	if (lk_get_key(ctx, 0, 0) != 16)
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode");

	lk_free(ctx);

	return EXIT_SUCCESS;
}
