topdir=$(shell pwd)/../..
include ../../Make.Rules

# Always build sources this way:
CFLAGS += -fPIC $(CAPSO_DEBUG)

all: bind

bind: bind.c capso.so
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ bind.c capso.so -L../../libcap -lcap

../../libcap/loader.txt:
	$(MAKE) -C ../../libcap loader.txt

capso.o: capso.c capso.h ../../libcap/execable.h ../../libcap/loader.txt
	$(CC) $(CFLAGS) $(CPPFLAGS) -DLIBCAP_VERSION=\"libcap-$(VERSION).$(MINOR)\" -DSHARED_LOADER=\"$(shell cat ../../libcap/loader.txt)\" -c capso.c -o $@

capso.so: capso.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBCAPLIB) -ldl -Wl,-e,__so_start
	sudo setcap cap_net_bind_service=p $@

clean:
	rm -f bind capso.o capso.so *~
