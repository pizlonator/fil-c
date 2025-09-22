SUBDIRS=	crypt eloop-bench

all: 
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

install:

proginstall:

clean:
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

distclean: clean
	rm -f *.diff *.patch *.orig *.rej
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

test:
	for x in ${SUBDIRS}; do cd $$x; ${MAKE} $@ || exit $$?; cd ..; done

tests: test
