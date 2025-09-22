/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_PWDUTILS_H
#define UTIL_LINUX_PWDUTILS_H

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

extern struct passwd *xgetpwnam(const char *username, char **pwdbuf);
extern struct group *xgetgrnam(const char *groupname, char **grpbuf);
extern struct passwd *xgetpwuid(uid_t uid, char **pwdbuf);
extern char *xgetlogin(void);

#endif /* UTIL_LINUX_PWDUTILS_H */

