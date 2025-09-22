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

	const char *stringvalues[30] = {
		/* F1 .. F20 */
		"\033[[A", "\033[[B", "\033[[C", "\033[[D", "\033[[E",
		"\033[17~", "\033[18~", "\033[19~", "\033[20~", "\033[21~",
		"\033[23~", "\033[24~", "\033[25~", "\033[26~",
		"\033[28~", "\033[29~",
		"\033[31~", "\033[32~", "\033[33~", "\033[34~",
		/* Find,    Insert,     Remove,     Select,     Prior */
		"\033[1~", "\033[2~", "\033[3~", "\033[4~", "\033[5~",
		/* Next,    Macro,      Help,       Do,         Pause */
		"\033[6~", 0, 0, 0, 0
	};
	unsigned int i;
	struct lk_ctx *ctx;

	ctx = lk_init();
	lk_set_log_fn(ctx, NULL, NULL);

	for (i = 0; i < 30; i++) {
		struct kbsentry ke;

		if (!(stringvalues[i]))
			continue;

		strncpy((char *)ke.kb_string, stringvalues[i],
		        sizeof(ke.kb_string));
		ke.kb_string[sizeof(ke.kb_string) - 1] = 0;
		ke.kb_func                             = (unsigned char) i;

		if (lk_add_func(ctx, &ke) == -1)
			kbd_error(EXIT_FAILURE, 0, "Unable to add function");
	}

	lk_free(ctx);

	return EXIT_SUCCESS;
}
