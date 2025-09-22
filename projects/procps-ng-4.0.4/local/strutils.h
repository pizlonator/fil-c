/*
 * This header was copied from util-linux at fall 2011.
 */

#ifndef PROCPS_NG_STRUTILS
#define PROCPS_NG_STRUTILS

extern long strtol_or_err(const char *str, const char *errmesg);
extern double strtod_or_err(const char *str, const char *errmesg);
double strtod_nol_or_err(char *str, const char *errmesg);

#endif
