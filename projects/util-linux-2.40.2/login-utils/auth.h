/*
 *   auth.h -- PAM authorization code, common between chsh and chfn
 *   (c) 2012 by Cody Maloney <cmaloney@theoreticalchaos.com>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 */
#ifndef UTIL_LINUX_LOGIN_AUTH_H
#define UTIL_LINUX_LOGIN_AUTH_H

#include <sys/types.h>

extern int auth_pam(const char *service_name, uid_t uid, const char *username);

#endif /* UTIL_LINUX_LOGIN_AUTH_H */
