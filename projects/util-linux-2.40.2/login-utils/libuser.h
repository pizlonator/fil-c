/*
 *   libuser.h -- Utilize libuser to set a user attribute
 *   (c) 2012 by Cody Maloney <cmaloney@theoreticalchaos.com>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 */
#ifndef UTIL_LINUX_LOGIN_LIBUSER_H
#define UTIL_LINUX_LOGIN_LIBUSER_H

#include <sys/types.h>

extern int set_value_libuser(const char *service_name, const char *username,
			uid_t uid, const char *attr, const char *val);

#endif /* UTIL_LINUX_LOGIN_LIBUSER_H */
