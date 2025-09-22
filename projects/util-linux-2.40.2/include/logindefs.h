/*
 * Copyright (C) 2003, 2004, 2005 Thorsten Kukuk
 * Author: Thorsten Kukuk <kukuk@suse.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain any existing copyright
 *    notice, and this entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 *
 * 2. Redistributions in binary form must reproduce all prior and current
 *    copyright notices, this list of conditions, and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *   written permission.
 */
#ifndef UTIL_LINUX_LOGINDEFS_H
#define UTIL_LINUX_LOGINDEFS_H

extern void logindefs_load_file(const char *filename);
extern void logindefs_set_loader(void (*loader)(void *data), void *data);
extern int getlogindefs_bool(const char *name, int dflt);
extern unsigned long getlogindefs_num(const char *name, unsigned long dflt);
extern const char *getlogindefs_str(const char *name, const char *dflt);
extern void free_getlogindefs_data(void);
extern int logindefs_setenv(const char *name, const char *conf, const char *dflt);
extern int effective_access(const char *path, int mode);
extern int get_hushlogin_status(struct passwd *pwd, int force_check);

#endif /* UTIL_LINUX_LOGINDEFS_H */
