SUBDIRS=	src hooks

PACKAGE=	dhcpcd
VERSION!=	sed -n 's/\#define VERSION[[:space:]]*"\(.*\)".*/\1/p' src/defs.h

DIST!=		if test -d .git; then echo "dist-git"; \
		else echo "dist-inst"; fi
FOSSILID?=	current
GITREF?=	HEAD

DISTSUFFIX=
DISTPREFIX?=	${PACKAGE}-${VERSION}${DISTSUFFIX}
DISTFILE?=	${DISTPREFIX}.tar.xz
DISTINFO=	${DISTFILE}.distinfo
DISTINFOMD=	${DISTINFO}.md
DISTSIGN=	${DISTFILE}.asc

CLEANFILES+=	*.tar.xz

.PHONY:		hooks import import-bsd tests

.SUFFIXES:	.in

all: config.h
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

depend: config.h
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

tests:
	cd $@; ${MAKE} $@

test: tests

hooks:
	cd $@; ${MAKE}

eginstall:
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

install:
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

proginstall:
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

clean:
	rm -rf cov-int dhcpcd.xz
	for x in ${SUBDIRS} tests; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

distclean: clean
	rm -f config.h config.mk config.log \
		${DISTFILE} ${DISTINFO} ${DISTINFOMD} ${DISTSIGN}
	rm -f *.diff *.patch *.orig *.rej
	for x in ${SUBDIRS} tests; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

dist-git:
	git archive --prefix=${DISTPREFIX}/ v${VERSION} | xz >${DISTFILE}

dist-inst:
	mkdir /tmp/${DISTPREFIX}
	cp -RPp * /tmp/${DISTPREFIX}
	(cd /tmp/${DISTPREFIX}; make clean)
	tar -cvJpf ${DISTFILE} -C /tmp ${DISTPREFIX}
	rm -rf /tmp/${DISTPREFIX}

dist: ${DIST}

distinfo: dist
	rm -f ${DISTINFO} ${DISTSIGN}
	${SHA256} ${DISTFILE} >${DISTINFO}
	wc -c <${DISTFILE} \
		| xargs printf 'Size   (${DISTFILE}) = %s\n' >>${DISTINFO}
	${PGP} --armour --detach-sign ${DISTFILE}
	chmod 644 ${DISTSIGN}
	ls -l ${DISTFILE} ${DISTINFO} ${DISTSIGN}

${DISTINFOMD}: ${DISTINFO}
	echo '```' >${DISTINFOMD}
	cat ${DISTINFO} >>${DISTINFOMD}
	echo '```' >>${DISTINFOMD}

release: distinfo ${DISTINFOMD}
	gh release create v${VERSION} \
		--title "${PACKAGE} ${VERSION}" --draft --generate-notes \
		--notes-file ${DISTINFOMD} \
		${DISTFILE} ${DISTSIGN}

snapshot:
	rm -rf /tmp/${DISTPREFIX}
	${INSTALL} -d /tmp/${DISTPREFIX}
	cp -RPp * /tmp/${DISTPREFIX}
	${MAKE} -C /tmp/${DISTPREFIX} distclean
	tar cf - -C /tmp ${DISTPREFIX} | xz >${DISTFILE}
	ls -l ${DISTFILE}

_import: dist
	rm -rf ${DESTDIR}/*
	${INSTALL} -d ${DESTDIR}
	tar xvpf ${DISTFILE} -C ${DESTDIR} --strip 1
	@${ECHO}
	@${ECHO} "============================================================="
	@${ECHO} "${PACKAGE}-${VERSION} imported to ${DESTDIR}"

import:
	${MAKE} _import DESTDIR=`if [ -n "${DESTDIR}" ]; then echo "${DESTDIR}"; else  echo /tmp/${DISTPREFIX}; fi`


_import-src: clean
	rm -rf ${DESTDIR}/*
	${INSTALL} -d ${DESTDIR}
	cp LICENSE README.md ${DESTDIR};
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} DESTDIR=${DESTDIR} $@ || exit $$?; cd ..; done
	@${ECHO}
	@${ECHO} "============================================================="
	@${ECHO} "${PACKAGE}-${VERSION} imported to ${DESTDIR}"

import-src:
	${MAKE} _import-src DESTDIR=`if [ -n "${DESTDIR}" ]; then echo "${DESTDIR}"; else  echo /tmp/${DISTPREFIX}; fi`

include Makefile.inc
