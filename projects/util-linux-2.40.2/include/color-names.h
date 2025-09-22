/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Copyright (C) 2012-2015 Karel Zak <kzak@redhat.com>
 */
#ifndef UTIL_LINUX_COLOR_NAMES_H
#define UTIL_LINUX_COLOR_NAMES_H

#define UL_COLOR_RESET		"\033[0m"
#define UL_COLOR_BOLD		"\033[1m"
#define UL_COLOR_HALFBRIGHT	"\033[2m"
#define UL_COLOR_UNDERSCORE	"\033[4m"
#define UL_COLOR_BLINK		"\033[5m"
#define UL_COLOR_REVERSE	"\033[7m"

/* Standard colors */
#define UL_COLOR_BLACK		"\033[30m"
#define UL_COLOR_RED		"\033[31m"
#define UL_COLOR_GREEN		"\033[32m"
#define UL_COLOR_BROWN		"\033[33m"	/* well, brown */
#define UL_COLOR_BLUE		"\033[34m"
#define UL_COLOR_MAGENTA	"\033[35m"
#define UL_COLOR_CYAN		"\033[36m"
#define UL_COLOR_GRAY		"\033[37m"

/* Bold variants */
#define UL_COLOR_DARK_GRAY	"\033[1;30m"
#define UL_COLOR_BOLD_RED	"\033[1;31m"
#define UL_COLOR_BOLD_GREEN	"\033[1;32m"
#define UL_COLOR_BOLD_YELLOW	"\033[1;33m"
#define UL_COLOR_BOLD_BLUE	"\033[1;34m"
#define UL_COLOR_BOLD_MAGENTA	"\033[1;35m"
#define UL_COLOR_BOLD_CYAN	"\033[1;36m"

#define UL_COLOR_WHITE		"\033[1;37m"


/* maximal length of human readable name of ESC seq. */
#define UL_COLORNAME_MAXSZ	32

extern const char *color_sequence_from_colorname(const char *str);

extern int color_is_sequence(const char *color);
extern char *color_get_sequence(const char *color);

#endif /* UTIL_LINUX_COLOR_NAMES_H */
