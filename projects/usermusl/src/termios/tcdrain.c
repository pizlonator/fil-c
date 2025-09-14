#include <termios.h>
#include <sys/ioctl.h>
#include "syscall.h"

int tcdrain(int fd)
{
	return zsys_ioctl(fd, TCSBRK, 1);
}
