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

	const char *s;
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

	kbdfile_set_pathname(fp, "null");

	f = fopen("/dev/null", "r");
	if (!f)
		kbd_error(EXIT_FAILURE, 0, "Unable to open: /dev/null: %s", strerror(errno));

	kbdfile_set_file(fp, f);

	ctx = lk_init();
	lk_set_log_fn(ctx, NULL, NULL);

	if (lk_parse_keymap(ctx, fp) != 0)
		kbd_error(EXIT_FAILURE, 0, "Unable to parse keymap");

	s = lk_get_charset(ctx);

	if (s == NULL)
		kbd_error(EXIT_FAILURE, 0, "Charset not found");

	if (strcmp(s, "iso-8859-1"))
		kbd_error(EXIT_FAILURE, 0, "Unable to parse charset");

	kbdfile_free(fp);
	kbdfile_context_free(kbdfile_ctx);
	lk_free(ctx);

	return EXIT_SUCCESS;
}
