#include <termios.h>
#include <sys/ioctl.h>
#include "syscall.h"

int tcsetwinsize(int fd, const struct winsize *wsz)
{
	return zsys_ioctl(fd, TIOCSWINSZ, wsz);
}
