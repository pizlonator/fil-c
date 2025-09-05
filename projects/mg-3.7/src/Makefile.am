# Written by Joachim Wiberg <troglobit@gmail.com> and put in the public domain.
AUTOMAKE_OPTIONS = subdir-objects
if DOCS
doc_DATA         = README
endif

bin_PROGRAMS     = mg
mg_SOURCES       = basic.c bell.c buffer.c cinfo.c dir.c display.c echo.c 	\
		   extend.c file.c fileio.c funmap.c help.c interpreter.c	\
		   kbd.c keymap.c line.c macro.c main.c match.c modes.c		\
		   paragraph.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c	\
		   ttydef.h undo.c util.c version.c window.c word.c yank.c	\
		   chrdef.h def.h funmap.h kbd.h key.h macro.h pathnames.h
mg_SOURCES      += queue.h tree.h
mg_SOURCES      += extensions.c

mg_CFLAGS        = -W -Wall -Wextra -Wno-unused-parameter
mg_CFLAGS       += -D_GNU_SOURCE -DDATADIR=\"$(pkgdatadir)\" -DDOCDIR=\"$(docdir)\"
if TINY
mg_CFLAGS       += -fomit-frame-pointer -fdata-sections -ffunction-sections	\
		   -fno-stack-protector -fno-asynchronous-unwind-tables -flto	\
		   -fmerge-all-constants -fno-ident -finline-small-functions	\
		   -Wl,--gc-sections -Wl,-z,norelro -Wl,--hash-style=gnu	\
		   -Wl,--build-id=none
endif
mg_LDADD         = $(LIBS) $(LIBOBJS)

#
# More or less standalone extensions.
#
if AUTOEXEC
mg_SOURCES     += autoexec.c
endif
if CMODE
mg_SOURCES     += cmode.c
endif
if COMPILE
mg_SOURCES     += grep.c
endif
if CSCOPE
mg_SOURCES     += cscope.c
endif
if CTAGS
mg_SOURCES     += tags.c
endif
if DIRED
mg_SOURCES     += dired.c
endif
if REGEX
mg_SOURCES     += re_search.c
endif
if MGLOG
mg_SOURCES     += log.c log.h
endif

#
# Building w/o terminfo/termcap/curses/ncurses
#
if NOCURSES
mg_SOURCES     += ansi.c ansi.h
endif
