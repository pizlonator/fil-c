// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#include "atoi/a2i.h"


extern inline int a2sh_c(short *restrict n, const char *s,
    const char **restrict endp, int base, short min, short max);
extern inline int a2si_c(int *restrict n, const char *s,
    const char **restrict endp, int base, int min, int max);
extern inline int a2sl_c(long *restrict n, const char *s,
    const char **restrict endp, int base, long min, long max);
extern inline int a2sll_c(long long *restrict n, const char *s,
    const char **restrict endp, int base, long long min, long long max);
extern inline int a2uh_c(unsigned short *restrict n, const char *s,
    const char **restrict endp, int base, unsigned short min,
    unsigned short max);
extern inline int a2ui_c(unsigned int *restrict n, const char *s,
    const char **restrict endp, int base, unsigned int min, unsigned int max);
extern inline int a2ul_c(unsigned long *restrict n, const char *s,
    const char **restrict endp, int base, unsigned long min, unsigned long max);
extern inline int a2ull_c(unsigned long long *restrict n, const char *s,
    const char **restrict endp, int base, unsigned long long min,
    unsigned long long max);


extern inline int a2sh_nc(short *restrict n, char *s,
    char **restrict endp, int base, short min, short max);
extern inline int a2si_nc(int *restrict n, char *s,
    char **restrict endp, int base, int min, int max);
extern inline int a2sl_nc(long *restrict n, char *s,
    char **restrict endp, int base, long min, long max);
extern inline int a2sll_nc(long long *restrict n, char *s,
    char **restrict endp, int base, long long min, long long max);
extern inline int a2uh_nc(unsigned short *restrict n, char *s,
    char **restrict endp, int base, unsigned short min, unsigned short max);
extern inline int a2ui_nc(unsigned int *restrict n, char *s,
    char **restrict endp, int base, unsigned int min, unsigned int max);
extern inline int a2ul_nc(unsigned long *restrict n, char *s,
    char **restrict endp, int base, unsigned long min, unsigned long max);
extern inline int a2ull_nc(unsigned long long *restrict n, char *s,
    char **restrict endp, int base, unsigned long long min,
    unsigned long long max);
