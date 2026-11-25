#include "stdio_impl.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <stdfil.h>

size_t __stdout_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct winsize wsz;
	int saved_errno = errno;
	f->write = __stdio_write;
	if (!(f->flags & F_SVB) && zsys_ioctl(f->fd, TIOCGWINSZ, &wsz))
		f->lbf = -1;
	errno = saved_errno;
	return __stdio_write(f, buf, len);
}
