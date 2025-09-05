/*      $OpenBSD: log.h,v 1.6 2021/03/02 13:06:50 lum Exp $   */

/* This file is in the public domain. */
#include <stdarg.h>

/*
 * Specifically for mg logging functionality.
 *
 */
int	 mglog(PF, void *);
int	 mgloginit(void);
int	 mglog_execbuf(	const char* const,
			const char* const,
			const char* const,
			const char* const,
	     		const int,
			const int,
			const char* const,
			const char* const,
			const char* const
			);

int	 mglog_isvar(	const char* const,
			const char* const,
			const int
			);
int	 mglog_misc(	const char *, ...);

extern const char	*mglogpath_lines;
extern const char	*mglogpath_undo;
extern const char	*mglogpath_window;
extern const char	*mglogpath_key;
extern const char	*mglogpath_interpreter;
extern const char	*mglogpath_misc;
