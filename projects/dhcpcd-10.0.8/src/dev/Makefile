TOP=	../../
include ${TOP}/Makefile.inc
include ${TOP}/config.mk

CFLAGS?=	-O2
CSTD?=		c99
CFLAGS+=	-std=${CSTD}
CPPFLAGS+=	-I${TOP} -I${TOP}/src

DEVDIR=		${LIBDIR}/dhcpcd/dev
DSRC=		${DEV_PLUGINS:=.c}
DOBJ=		${DSRC:.c=.o}
DSOBJ=		${DOBJ:.o=.So}
DPLUGS=		${DEV_PLUGINS:=.so}

CLEANFILES+=	${DSOBJ} ${DPLUGS}

.SUFFIXES:	.So .so

.c.So:
	${CC} ${PICFLAG} -DPIC ${CPPFLAGS} ${CFLAGS} -c $< -o $@

.So.so:
	${CC} ${LDFLAGS} -shared -Wl,-x -o $@ -Wl,-soname,$@ \
	    $< ${LIBS}

all: ${DPLUGS}

udev.So:
CFLAGS+=	${LIBUDEV_CFLAGS}
CPPFLAGS+=	${LIBUDEV_CPPFLAGS}

udev.so:
LIBS+=		${LIBUDEV_LIBS}

proginstall: ${DPLUGS}
	${INSTALL} -d ${DESTDIR}${DEVDIR}
	${INSTALL} -m ${BINMODE} ${PROG} ${DPLUGS} ${DESTDIR}${DEVDIR}

eginstall:

install: proginstall

clean:
	rm -f ${CLEANFILES}
