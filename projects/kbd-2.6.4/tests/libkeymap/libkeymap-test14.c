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

	if (lk_add_key(ctx, 0, NR_KEYS + 1, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to use index > NR_KEYS");

	if (lk_add_key(ctx, MAX_NR_KEYMAPS + 1, 0, 0) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to use table > MAX_NR_KEYMAPS");

	lk_free(ctx);

	return EXIT_SUCCESS;
}
