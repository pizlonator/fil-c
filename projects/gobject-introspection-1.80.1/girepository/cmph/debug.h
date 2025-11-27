#ifdef DEBUGP
#undef DEBUGP
#endif

#ifdef __cplusplus
#include <cstdio>
#ifdef WIN32
#include <cstring>
#endif
#else
#include <stdio.h>
#ifdef WIN32
#include <string.h>
#endif
#endif

#ifndef __GNUC__
#ifndef __DEBUG_H__
#define __DEBUG_H__
#include <stdarg.h>
static void debugprintf(const char *format, ...)
{
    va_list ap;
	char *f = NULL;
	const char *p="%s:%d ";
	size_t plen = strlen(p);
    va_start(ap, format);
	f = (char *)malloc(plen + strlen(format) + 1);
	if (!f) return;
	memcpy(f, p, plen);
	memcpy(f + plen, format, strlen(format) + 1);
    vfprintf(stderr, f, ap);
    va_end(ap);
	free(f);
}
static void dummyprintf(const char *format, ...)
{}
#endif
#endif

#ifdef DEBUG
#ifndef __GNUC__
#define DEBUGP debugprintf
#else
#define DEBUGP(args...) do { fprintf(stderr, "%s:%d ", __FILE__, __LINE__); fprintf(stderr, ## args); } while(0)
#endif
#else
#ifndef __GNUC__
#define DEBUGP dummyprintf
#else
#define DEBUGP(args...)
#endif
#endif
