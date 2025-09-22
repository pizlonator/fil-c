#ifndef CAPSO_H
#define CAPSO_H

/*
 * bind80 returns a socket filedescriptor that is bound to port 80 of
 * the provided service address.
 *
 * Example:
 *
 *   int fd = bind80("localhost");
 *
 * fd < 0 in the case of error.
 */
extern int bind80(const char *hostname);

#endif /* ndef CAPSO_H */
