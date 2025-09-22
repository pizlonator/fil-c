#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <keymap.h>

#include "libcommon.h"
#include "contextP.h"

int main(int __attribute__((unused)) argc, char **argv)
{
	set_progname(argv[0]);

	unsigned int i;
	struct lk_ctx *ctx;
	struct kbdfile *fp;
	struct kbdfile_ctx *kbdfile_ctx;

	if ((kbdfile_ctx = kbdfile_context_new()) == NULL) {
		perror("nomem");
		exit(1);
	}

	if ((fp = kbdfile_new(kbdfile_ctx)) == NULL) {
		perror("nomem");
		exit(1);
	}

	ctx = lk_init();

	kbdfile_set_pathname(fp, argv[1]);
	kbdfile_set_file(fp, fopen(argv[1], "r"));

	lk_parse_keymap(ctx, fp);

	for (i = 0; i < ctx->keymap->total; i++) {
		if (!lk_map_exists(ctx, i))
			continue;
		printf("keymap %03d\n", i);
	}

	kbdfile_free(fp);
	kbdfile_context_free(kbdfile_ctx);
	lk_free(ctx);
	return 0;
}
