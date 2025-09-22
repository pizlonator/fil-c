/*
 * vcstime.c
 *
 * Show time in upper right hand corner of the console screen
 * aeb, 951202, following a suggestion by Miguel de Icaza.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

void fatal(char *s)
{
	perror(s);
	exit(1);
}

int number_of_columns()
{
	int fda;
	unsigned char rc[2]; /* unsigned: Ranty@soon.com */

	if ((fda = open("/dev/vcsa", O_RDONLY)) < 0 && (fda = open("/dev/vcsa0", O_RDONLY)) < 0)
		fatal("/dev/vcsa");
	if (read(fda, rc, 2) != 2)
		fatal("/dev/vcsa");
	close(fda);
	return rc[1];
}

int main()
{
	int fd;
	int cols = number_of_columns();
	time_t tid;
	struct tm *t;
	char tijd[10];

	if ((fd = open("/dev/vcs", O_WRONLY)) < 0 && (fd = open("/dev/vcs0", O_WRONLY)) < 0)
		fatal("/dev/vcs");

	while (1) {
		lseek(fd, cols - 10, 0);
		tid = time(0);
		t   = localtime(&tid);
		sprintf(tijd, " %02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
		write(fd, tijd, 9);
		usleep(500000L); /* or sleep(1); */
	}
}
