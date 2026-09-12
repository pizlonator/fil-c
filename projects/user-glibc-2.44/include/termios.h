#ifndef _TERMIOS_H
#include <termios/termios.h>

#ifndef _ISOMAC
extern speed_t __cfgetospeed (const struct termios *__termios_p);
extern speed_t __cfgetispeed (const struct termios *__termios_p);
extern int __cfsetospeed (struct termios *__termios_p, speed_t __speed);
extern int __cfsetispeed (struct termios *__termios_p, speed_t __speed);
extern int __cfsetspeed (struct termios *__termios_p, speed_t __speed);

extern baud_t __cfgetobaud (const struct termios *__termios_p);
extern baud_t __cfgetibaud (const struct termios *__termios_p);
extern int __cfsetobaud (struct termios *__termios_p, baud_t __baud);
extern int __cfsetibaud (struct termios *__termios_p, baud_t __baud);
extern int __cfsetbaud (struct termios *__termios_p, baud_t __baud);

extern int __tcgetattr (int __fd, struct termios *__termios_p);
extern int __tcsetattr (int __fd, int __optional_actions,
			const struct termios *__termios_p);

extern int __libc_tcdrain (int __fd);

libc_hidden_proto (__tcgetattr)
libc_hidden_proto (__tcsetattr)
libc_hidden_proto (__cfgetispeed)
libc_hidden_proto (__cfgetospeed)
libc_hidden_proto (__cfsetispeed)
libc_hidden_proto (__cfsetospeed)
libc_hidden_proto (__cfsetspeed)
libc_hidden_proto (__cfgetibaud)
libc_hidden_proto (__cfgetobaud)
libc_hidden_proto (__cfsetibaud)
libc_hidden_proto (__cfsetobaud)
libc_hidden_proto (__cfsetbaud)

#endif
#endif
