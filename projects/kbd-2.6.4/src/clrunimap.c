/*
 * clrunimap.c
 *
 * Note: nowadays this kills kernel console output!
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/kd.h>

#include "libcommon.h"
#include "kfont.h"

int main(int argc, char *argv[])
{
	int fd;
	char *console = NULL;

	set_progname(argv[0]);
	setuplocale();

	if (argc >= 3 && !strcmp(argv[1], "-C"))
		console = argv[2];

	if ((fd = getfd(console)) < 0)
		kbd_error(EXIT_FAILURE, 0, _("Couldn't get a file descriptor referring to the console."));

	int ret;
	struct kfont_context *ctx;

	if ((ret = kfont_init(get_progname(), &ctx)) < 0)
		return -ret;

	return kfont_put_unicodemap(ctx, fd, NULL, NULL);
}
