## Process this file with automake to produce Makefile.in -*-Makefile-*-
## Configure input file for elfutils.
##
## Copyright (C) 2004, 2005, 2008, 2009, 2011, 2015, 2016 Red Hat, Inc.
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of either
##
##   * the GNU Lesser General Public License as published by the Free
##     Software Foundation; either version 3 of the License, or (at
##     your option) any later version
##
## or
##
##   * the GNU General Public License as published by the Free
##     Software Foundation; either version 2 of the License, or (at
##     your option) any later version
##
## or both in parallel, as here.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received copies of the GNU General Public License and
## the GNU Lesser General Public License along with this program.  If
## not, see <http://www.gnu.org/licenses/>.
##
EXTRA_DIST = elfutils.spec.in known-dwarf.awk 10-default-yama-scope.conf \
	     libelf.pc.in libdw.pc.in libdebuginfod.pc.in \
	     debuginfod.service debuginfod.sysconfig profile.sh.in profile.csh.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libelf.pc libdw.pc
if LIBDEBUGINFOD
pkgconfig_DATA += libdebuginfod.pc

install-data-local:
	$(INSTALL_DATA) profile.sh -D $(DESTDIR)$(sysconfdir)/profile.d/debuginfod.sh
	$(INSTALL_DATA) profile.csh -D $(DESTDIR)$(sysconfdir)/profile.d/debuginfod.csh
	mkdir -p $(DESTDIR)$(sysconfdir)/debuginfod
	if [ -n "@DEBUGINFOD_URLS@" ]; then \
		echo "@DEBUGINFOD_URLS@" > $(DESTDIR)$(sysconfdir)/debuginfod/elfutils.urls; \
	fi

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/profile.d/debuginfod.sh
	rm -f $(DESTDIR)$(sysconfdir)/profile.d/debuginfod.csh
	rm -f $(DESTDIR)$(sysconfdir)/debuginfod/elfutils.urls
	-rmdir $(DESTDIR)$(sysconfdir)/debuginfod
endif

if MAINTAINER_MODE
$(srcdir)/elfutils.spec.in: $(top_srcdir)/NEWS
	@tmpname=$$(mktemp $${TMPDIR:-/tmp}/elfutils.XXXXXX); \
	date +'* %a %b %e %Y' | tr '[\n]' '[ ]' > $$tmpname; \
	username=$$(git config --get user.name); \
	useremail=$$(git config --get user.email); \
	echo -n "$$username <$$useremail> " >> $$tmpname; \
	awk '\
	  $$1 == "Version" && started { exit } \
	  $$1 == "Version" { started=1; line=""; sub(/:/,"",$$2); \
			     print $$2 "-1"; next } \
	  NF > 0 { line = (line != "") ? (line " " $$0) : ("- " $$0) } \
	  NF == 0 && line != "" { print line; line="" } \
	  END { if (line != "") print line; print "" }' $< \
	| fold -s -w 70 | sed '1!s/^[^-]/  &/' >> $$tmpname; \
	sed "/^%changelog/r $$tmpname" $@ > $@.new; \
	rm -f $$tmpname; \
	mv -f $@.new $@
endif
