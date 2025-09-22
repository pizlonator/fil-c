/* splitfont: extract characters from font */
/* this is for iso fonts, no psf header, just 256 characters */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void dosplit(int from, int to, char *fontbuf, int size, char *fontfile)
{
	int itemsize = size / 256;
	int i, fd;
	char *p, *q, s;
	char filename[4096];

	if (from < 0 || from > 255 || to < 0 || to > 255) {
		fprintf(stderr, "splitfont: bad argument %s,%s\n",
		        from, to);
		exit(1);
	}
	if (strlen(fontfile) >= sizeof(filename) - 4) {
		fprintf(stderr, "splitfont: ridiculously long name\n");
		exit(1);
	}
	while (from <= to) {
		sprintf(filename, "%s.%02x", fontfile, from);
		if ((fd = open(filename, O_WRONLY | O_CREAT, 0666)) < 0) {
			perror("splitfont");
			fprintf(stderr, "cannot open %s for writing\n", filename);
		}
		p = &fontbuf[from * itemsize];
		if (write(fd, p, itemsize) != itemsize) {
			perror("splitfont");
			fprintf(stderr, "error writing %s\n", filename);
		}
		close(fd);
		from++;
	}
}

int main(int argc, char **argv)
{
	struct stat statbuf;
	char fontbuf[4096];
	int fd;
	char *p, *q;
	int from, to;

	if (argc != 3) {
		fprintf(stderr, "call: splitfont fontfile 17,23-30,...\n");
		exit(1);
	}
	if (stat(argv[1], &statbuf)) {
		perror("splitfont");
		fprintf(stderr, "cannot stat fontfile %s", argv[1]);
		exit(1);
	}
	if (statbuf.st_size > 4096) {
		fprintf(stderr, "splitfont: file unexpectedly large\n");
		exit(1);
	}
	if (statbuf.st_size % 256) {
		fprintf(stderr, "splitfont: file size not a multiple of 256\n");
		exit(1);
	}
	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		perror("splitfont");
		fprintf(stderr, "cannot open fontfile %s", argv[1]);
		exit(1);
	}
	if (read(fd, fontbuf, statbuf.st_size) != statbuf.st_size) {
		perror("splitfont");
		fprintf(stderr, "error reading fontfile %s", argv[1]);
		exit(1);
	}

	p = argv[2];
	while (1) {
		to = from = strtoul(p, &q, 0);
		if (*q == '-') {
			p  = q + 1;
			to = strtoul(p, &q, 0);
		}
		if (*q && *q != ',') {
			fprintf(stderr, "splitfont: garbage in %s\n", p);
			exit(1);
		}
		dosplit(from, to, fontbuf, statbuf.st_size, argv[1]);
		if (!*q)
			break;
		p = q + 1;
	}
	return 0;
}
