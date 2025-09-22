# GNU Make does not automagically include .depend
# Luckily it does read GNUmakefile over Makefile so we can work around it

include Makefile
ifneq ($(wildcard .depend), )
include .depend
endif
