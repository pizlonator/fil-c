/*
 * sti.c: put text in some tty input buffer - aeb, 951009
 *
 * You may have to be root if the tty is not your controlling tty.
 */
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

int main(int argc, char **argv)
{
	int fd;
	char *s;

	if (argc != 3) {
		fprintf(stderr, "call: sti tty text\n");
		exit(1);
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		fprintf(stderr, "sti: could not open tty\n");
		exit(1);
	}
	s = argv[2];
	while (*s) {
		if (ioctl(fd, TIOCSTI, s)) {
			perror("TIOCSTI");
			fprintf(stderr, "sti: TIOCSTI ioctl failed\n");
			exit(1);
		}
		s++;
	}
	return 0;
}
