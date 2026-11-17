/*
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

/* How's this for useless?  Slowly dribble the contents of files through the
 * title bar.  Apparently can be backgrounded. */

#define DEFAULT_WIDTH 80
#define DEFAULT_DELAY 70000

static void
my_usleep(long delay)
{
	struct timeval tv;
	tv.tv_sec = delay / 1000000;
	tv.tv_usec = delay % 1000000;
	select(0, NULL, NULL, NULL, &tv);
}

int
main(int argc, char **argv)
{
	long length = DEFAULT_WIDTH, delay = DEFAULT_DELAY, fd, i, j;
	int c;
	struct stat st;
	char *buffer, *outbuf;

	while ((c = getopt(argc, argv, "d:w:")) != -1) {
		switch (c) {
		case 'd':
			delay = atol(optarg);
			if (delay == 0) {
				delay = DEFAULT_DELAY;
			}
			break;
		case 'w':
			length = atol(optarg);
			if (length == 0) {
				length = DEFAULT_WIDTH;
			}
			break;
		default:
			g_print("Usage: xticker [-d delay] [-w width] file [...]\n");
			return 1;
			break;
		}
	}

	outbuf = g_malloc(length + 5);

	for (i = optind; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd != -1) {
			if (fstat(fd, &st) != -1) {
				buffer = g_malloc(st.st_size);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
				read(fd, buffer, st.st_size);
#pragma GCC diagnostic pop
				for (j = 0; j < st.st_size; j++) {
					switch (buffer[j]) {
					case '\r':
					case '\n':
					case '\t':
					case '\b':
					case '\0':
						buffer[j] = ' ';
						break;
					default:
						break;
					}
					if (j > 0) {
						if ((buffer[j] == ' ') &&
						    (buffer[j - 1] == ' ')) {
							memmove(buffer + j - 1,
								buffer + j,
								st.st_size - j);
							st.st_size--;
							j--;
						}
					}
				}
				close(fd);
				for (j = 0; j < st.st_size - length; j++) {
					outbuf[0] = '\033';
					outbuf[1] = ']';
					outbuf[2] = '0';
					outbuf[3] = ';';
					memcpy(outbuf + 4,
					       buffer + j,
					       length);
					outbuf[length + 4] = '\007';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
					write(STDERR_FILENO,
					      outbuf,
					      length + 5);
#pragma GCC diagnostic pop
					my_usleep(delay);
					if ((j == 0) ||
					    (j == st.st_size - length - 1)) {
						my_usleep(1000000);
					}
				}
				g_free(buffer);
			} else {
				close(fd);
			}
		} else {
			char *errbuf;
			errbuf = g_strdup_printf("\033]0;Error opening %s: %s."
						 "\007",
						 argv[i],
						 strerror(errno));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
			write(STDERR_FILENO, errbuf, strlen(errbuf));
#pragma GCC diagnostic pop
			g_free(errbuf);
			my_usleep(1000000);
		}
	}

	g_free(outbuf);

	return 0;
}
