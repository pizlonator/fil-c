# Customize maint.mk                           -*- makefile -*-
# Copyright (C) 2003-2013, 2015-2023 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Use the direct link.  This is guaranteed to work immediately, while
# it can take a while for the faster mirror links to become usable.
url_dir_list = https://ftp.gnu.org/gnu/$(PACKAGE)

# Used in maint.mk's web-manual rule
manual_title = Comparing and Merging Files

# Tests not to run as part of "make distcheck".
local-checks-to-skip =		\
  sc_error_message_period	\
  sc_error_message_uppercase	\
  sc_indent

# Tools used to bootstrap this package, used for "announcement".
bootstrap-tools = autoconf,automake,gnulib

# Override the default Cc: used in generating an announcement.
announcement_Cc_ = $(translation_project_), $(PACKAGE)-devel@gnu.org

# Now that we have better tests, make this the default.
export VERBOSE = yes

old_NEWS_hash = cf070086af56e7394cc5a0c862d0cd11

# Tell maint.mk's syntax-check rules that diff gets config.h directly or
# via diff.h or system.h.
config_h_header = (<config\.h>|"(diff|system)\.h")

update-copyright-env = \
  UPDATE_COPYRIGHT_USE_INTERVALS=1 \
  UPDATE_COPYRIGHT_MAX_LINE_LENGTH=79

-include $(srcdir)/dist-check.mk

_cf_state_dir ?= .config-state
_date_time := $(shell date +%F.%T)
config-compare:
	diff -u					\
	  -I'define VERSION '			\
	  -I'define PACKAGE_VERSION '		\
	  -I'define PACKAGE_STRING '		\
	  $(_cf_state_dir)/latest lib/config.h
	diff -u					\
	  -I'$(PACKAGE_NAME)'			\
	  -I'[SD]\["VERSION"\]'			\
	  -I'[SD]\["PACKAGE_VERSION"\]'		\
	  -I'D\["PACKAGE_STRING"\]'		\
	  $(_cf_state_dir)/latest config.status

config-save:
	$(MAKE) --quiet config-compare > /dev/null 2>&1 \
	  && { echo no change; exit 1; } || :
	mkdir -p $(_cf_state_dir)/$(_date_time)
	ln -nsf $(_date_time) $(_cf_state_dir)/latest
	cp lib/config.h config.status $(_cf_state_dir)/latest

exclude_file_name_regexp--sc_space_tab = ^gl/lib/.*\.c\.diff$$
exclude_file_name_regexp--sc_prohibit_doubled_word = ^tests/y2038-vs-32bit$$

# Tell gnulib's tight_scope rule that we mark externs with XTERN
export _gl_TS_extern = extern|XTERN|DIFF_INLINE
