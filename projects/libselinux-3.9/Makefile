SUBDIRS = include src utils man

PKG_CONFIG ?= pkg-config
DISABLE_SETRANS ?= n
DISABLE_RPM ?= n
ANDROID_HOST ?= n
LABEL_BACKEND_ANDROID ?= n
ifeq ($(ANDROID_HOST),y)
	override DISABLE_SETRANS=y
	override DISABLE_BOOL=y
endif
ifeq ($(DISABLE_RPM),y)
	DISABLE_FLAGS+= -DDISABLE_RPM
endif
ifeq ($(DISABLE_SETRANS),y)
	DISABLE_FLAGS+= -DDISABLE_SETRANS
endif
ifeq ($(DISABLE_BOOL),y)
	DISABLE_FLAGS+= -DDISABLE_BOOL
endif
ifeq ($(DISABLE_X11),y)
	DISABLE_FLAGS+= -DNO_X_BACKEND
endif
export DISABLE_SETRANS DISABLE_RPM DISABLE_FLAGS ANDROID_HOST DISABLE_X11 LABEL_BACKEND_ANDROID

USE_PCRE2 ?= y
ifeq ($(USE_PCRE2),y)
	PCRE_MODULE := libpcre2-8
	PCRE_CFLAGS := -DUSE_PCRE2 -DPCRE2_CODE_UNIT_WIDTH=8
else
	PCRE_MODULE := libpcre
endif
PCRE_CFLAGS += $(shell $(PKG_CONFIG) --cflags $(PCRE_MODULE))
PCRE_LDLIBS := $(shell $(PKG_CONFIG) --libs $(PCRE_MODULE))
export PCRE_MODULE PCRE_CFLAGS PCRE_LDLIBS

USE_LFS ?= y
ifeq ($(USE_LFS),y)
	LFS_CFLAGS := -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif
export LFS_CFLAGS

OS := $(shell uname)
export OS

ifeq ($(shell $(CC) -v 2>&1 | grep "clang"),)
COMPILER := gcc
else
COMPILER := clang
endif
export COMPILER

all install relabel clean distclean indent:
	@for subdir in $(SUBDIRS); do \
		(cd $$subdir && $(MAKE) $@) || exit 1; \
	done

swigify: all
	$(MAKE) -C src $@

pywrap: 
	$(MAKE) -C src $@

rubywrap: 
	$(MAKE) -C src $@

install-pywrap: 
	$(MAKE) -C src $@

install-rubywrap: 
	$(MAKE) -C src $@

clean-pywrap:
	$(MAKE) -C src $@

clean-rubywrap:
	$(MAKE) -C src $@

test:
