#include <termios.h>
#include <sys/ioctl.h>
#include "syscall.h"

int tcgetwinsize(int fd, struct winsize *wsz)
{
	return zsys_ioctl(fd, TIOCGWINSZ, wsz);
}
