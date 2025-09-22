/*
 * Copyright © 2010 William Ahern
 * Copyright © 2012-2013 Guillem Jover <guillem@hadrons.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SETPROCTITLE_H
#define SETPROCTITLE_H

#ifndef __printflike
#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
#define	__printflike(a, b) __attribute__((format(printf, a, b)))
#else
#define	__printflike(a, b)
#endif
#endif /* !__printflike */

/* WEXITSTATUS is defined in stdlib.h which defines free() */
#ifdef WEXITSTATUS
static inline const char *
getprogname(void)
{
	return "dhcpcd";
}
static inline void
setprogname(char *name)
{
	free(name);
}
#endif

void setproctitle_init(int, char *[], char *[]);
__printflike(1, 2) void setproctitle(const char *, ...);
void setproctitle_fini(void);

#define libbsd_symver_default(alias, symbol, version) \
    extern __typeof(symbol) alias __attribute__((__alias__(#symbol)))

#define libbsd_symver_variant(alias, symbol, version)
#endif
