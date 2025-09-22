#ifndef UTIL_LINUX_TERM_TTYMSG_H
#define UTIL_LINUX_TERM_TTYMSG_H

char *ttymsg(struct iovec *iov, size_t iovcnt, char *line, int tmout);

#endif /* UTIL_LINUX_TERM_TTYMSG_H */
