#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "getfd.h"

#include <linux/kd.h>
#include <linux/keyboard.h>

int main(void)
{
	struct kbentry ke;

	int i, j, fd;
	char *console = NULL;

	/* get console */
	fd = getfd(console);

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		for (j = 0; j < NR_KEYS; j++) {
			ke.kb_index = j;
			ke.kb_table = i;
			ke.kb_value = K_HOLE;

			if (ioctl(fd, KDSKBENT, (unsigned long)&ke)) {
				perror("KDSKBENT");
				fprintf(stderr, "Error: cannot deallocate or clear keymap %d key %d\n", i, j);
				return EXIT_FAILURE;
			}
		}

		/* deallocate keymap */
		ke.kb_index = 0;
		ke.kb_table = i;
		ke.kb_value = K_NOSUCHMAP;

		if (ioctl(fd, KDSKBENT, (unsigned long)&ke)) {
			perror("KDSKBENT");
			fprintf(stderr, "Error: could not deallocate keymap %d\n", i);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
