# Makefile for toybox.
# Copyright 2006 Rob Landley <rob@landley.net>

# If people set these on the make command line, use 'em
# Note that CC defaults to "cc" so the one in configure doesn't get
# used when scripts/make.sh and such called through "make".

HOSTCC?=cc

export CROSS_COMPILE CFLAGS OPTIMIZE LDOPTIMIZE CC HOSTCC V STRIP ASAN

all: toybox

KCONFIG_CONFIG ?= .config

toybox generated/unstripped/toybox: $(KCONFIG_CONFIG) *.[ch] lib/*.[ch] toys/*/*.c scripts/*.sh Config.in
	scripts/make.sh

.PHONY: clean distclean baseline bloatcheck install install_flat \
	uninstall uninstall_flat tests help change \
	list list_example list_pending root run_root
.SUFFIXES: # Disable legacy behavior

include kconfig/Makefile
-include .singlemake

$(KCONFIG_CONFIG): $(KCONFIG_TOP)
	@if [ -e "$(KCONFIG_CONFIG)" ]; then $(MAKE) silentoldconfig; \
	else echo "Not configured (run '$(MAKE) defconfig' or '$(MAKE) menuconfig')";\
	exit 1; fi

$(KCONFIG_TOP): generated/Config.in generated/Config.probed
generated/Config.probed: generated/Config.in
generated/Config.in: toys/*/*.c scripts/genconfig.sh
	scripts/genconfig.sh

# Development targets
baseline: generated/unstripped/toybox
	@cp generated/unstripped/toybox generated/unstripped/toybox_old

bloatcheck: generated/unstripped/toybox_old generated/unstripped/toybox
	@scripts/probes/bloatcheck generated/unstripped/toybox_old generated/unstripped/toybox

install_flat: toybox
	scripts/install.sh --symlink --force

install_airlock: toybox
	scripts/install.sh --symlink --force --airlock

install: toybox
	scripts/install.sh --long --symlink --force

uninstall_flat:
	scripts/install.sh --uninstall

uninstall:
	scripts/install.sh --long --uninstall

change:
	scripts/change.sh

root_clean:
	@rm -rf root
	@echo root cleaned

clean::
	@chmod -fR 700 generated 2>/dev/null || true
	@rm -rf toybox generated change install .singleconfig*
	@echo cleaned

# If singlemake was in generated/ "make clean; make test_ls" wouldn't work.
distclean: clean root_clean
	@rm -f toybox* .config* .singlemake
	@echo removed .config

tests: ASAN=1
tests: toybox
	scripts/test.sh

root:
	mkroot/mkroot.sh $(MAKEFLAGS)

run_root:
	cd root/"$${CROSS:-host}" && ./run-qemu.sh

help::
	@cat scripts/help.txt
