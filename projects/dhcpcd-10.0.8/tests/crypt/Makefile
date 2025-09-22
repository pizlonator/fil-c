TOP=	../..
include ${TOP}/iconfig.mk

PROG=		run-test
SRCS=		run-test.c
SRCS+=		test_hmac_md5.c test_sha256.c

CFLAGS?=	-O2
CSTD?=		c99
CFLAGS+=	-std=${CSTD}

CPPFLAGS+=	-I${TOP} -I${TOP}/src

PCRYPT_SRCS=	${CRYPT_SRCS:compat/%=${TOP}/compat/%}
OBJS+=		${SRCS:.c=.o} ${PCRYPT_SRCS:.c=.o}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

all: ${PROG}

clean:
	rm -f ${OBJS} ${PROG} ${PROG}.core ${CLEANFILES}

distclean: clean
	rm -f .depend
	rm -f *.diff *.patch *.orig *.rej

.depend: ${SRCS} ${PCRYPT_SRCS}
	${CC} ${CPPFLAGS} -MM ${SRCS} ${PCRYPT_SRCS}

${PROG}: ${DEPEND} ${OBJS}
	${CC} ${LDFLAGS} -o $@ ${OBJS} ${LDADD}

test: ${PROG}
	./${PROG}
