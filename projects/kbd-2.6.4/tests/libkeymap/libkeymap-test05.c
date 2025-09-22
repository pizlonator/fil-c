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

	int c;
	FILE *f = NULL;
	struct kbdfile *fp = NULL;
	struct kbdfile_ctx *kbdfile_ctx;
	struct lk_ctx *ctx;

	kbdfile_ctx = kbdfile_context_new();
	if (!kbdfile_ctx)
		kbd_error(EXIT_FAILURE, 0, "Unable to create kbdfile context");

	fp = kbdfile_new(kbdfile_ctx);
	if (!fp)
		kbd_error(EXIT_FAILURE, 0, "Unable to create kbdfile");

	ctx = lk_init();
	lk_set_log_fn(ctx, NULL, NULL);

	kbdfile_set_pathname(fp, "keymap0.map");

	f = fopen(DATADIR "/data/libkeymap/keymap0.map", "r");
	if (!f)
		kbd_error(EXIT_FAILURE, 0, "Unable to open: " DATADIR "/data/libkeymap/keymap0.map: %s", strerror(errno));

	kbdfile_set_file(fp, f);

	if (lk_parse_keymap(ctx, fp) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to parse keymap");

	c = lk_get_key(ctx, 0, 16);
	if (KVAL(c) != 'q')
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode 16");

	c = lk_get_key(ctx, 0, 17);
	if (KVAL(c) != 'w')
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode 17");

	c = lk_get_key(ctx, 0, 18);
	if (KVAL(c) != 'e')
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode 18");

	c = lk_get_key(ctx, 0, 19);
	if (KVAL(c) != 'r')
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode 19");

	c = lk_get_key(ctx, 0, 20);
	if (KVAL(c) != 't')
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode 20");

	c = lk_get_key(ctx, 0, 21);
	if (KVAL(c) != 'y')
		kbd_error(EXIT_FAILURE, 0, "Unable to get keycode 21");

	kbdfile_free(fp);
	kbdfile_context_free(kbdfile_ctx);
	lk_free(ctx);

	return EXIT_SUCCESS;
}
