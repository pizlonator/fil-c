TOP=	../..
include ${TOP}/iconfig.mk

PROG=		eloop-bench
SRCS=		eloop-bench.c
SRCS+=		${TOP}/src/eloop.c

CFLAGS?=	-O2
CSTD?=		c99
CFLAGS+=	-std=${CSTD}

#CPPFLAGS+=	-DNO_CONFIG_H
#CPPFLAGS+=	-DQUEUE_H=../compat/queue.h
CPPFLAGS+=	-I${TOP} -I${TOP}/src

# Default is to let eloop decide
#CPPFLAGS+=	-DHAVE_KQUEUE
#CPPFLAGS+=	-DHAVE_POLLTS
#CPPFLAGS+=	-DHAVE_PSELECT
#CPPFLAGS+=	-DHAVE_EPOLL
#CPPFLAGS+=	-DHAVE_PPOLL
CPPFLAGS+=	-DWARN_SELECT

PCOMPAT_SRCS=   ${COMPAT_SRCS:compat/%=${TOP}/compat/%}
OBJS+=          ${SRCS:.c=.o} ${PCOMPAT_SRCS:.c=.o}

.c.o: Makefile
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

all: ${PROG}

clean:
	rm -f ${OBJS} ${PROG} ${PROG}.core ${CLEANFILES}

distclean: clean
	rm -f .depend
	rm -f *.diff *.patch *.orig *.rej

depend:

${PROG}: ${DEPEND} ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

test: ${PROG}
	./${PROG}
