topdir=$(shell pwd)/../..
include ../../Make.Rules

# This line is here to link against the in-tree copy of libcap.so
LINKEXTRA=-Wl,-rpath,$(topdir)/libcap
DEPS=../../libcap/libcap.so

all: su

su: su.c $(DEPS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -DPAM_APP_NAME=\"sucap\" $< -o $@ $(LINKEXTRA) -lpam -lpam_misc $(LIBCAPLIB)
	# to permit all ambient capabilities, this needs all permitted.
	#   sudo setcap =p ./su
	# to permit all inheritable, as CAP_PURE1E needs, we don't need as much
	sudo setcap cap_chown,cap_setgid,cap_setuid,cap_dac_read_search,cap_setpcap=p ./su

clean:
	rm -f su su.o *~
