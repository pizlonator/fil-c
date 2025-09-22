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

	int i = MAX_DIACR + 10;
	struct lk_ctx *ctx;
	struct lk_kbdiacr ptr;

	ptr.diacr  = 0;
	ptr.base   = 0;
	ptr.result = 0;

	ctx = lk_init();
	lk_set_log_fn(ctx, NULL, NULL);

	while (i > 0) {
		if (lk_append_diacr(ctx, &ptr) != 0)
			kbd_error(EXIT_FAILURE, 0, "Unable to add diacr");
		i--;
	}

	lk_free(ctx);

	return EXIT_SUCCESS;
}
