/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef UTIL_LINUX_SHELLS_H
#define UTIL_LINUX_SHELLS_H

extern void print_shells(FILE *out, const char *format);
extern int is_known_shell(const char *shell_name);

#endif /* UTIL_LINUX_SHELLS_H */
