topdir=$(shell pwd)/..
include ../Make.Rules

test: exit
	rm -f interactive
	./test-kernel.sh

shell: exit
	touch interactive
	./test-kernel.sh

exit: exit.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@ --static

all:
	@echo cd to kdebug to test a kernel build

install:

clean:
	$(LOCALCLEAN)
	rm -f fs.conf initramfs.img exit interactive
