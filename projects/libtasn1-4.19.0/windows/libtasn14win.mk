# Copyright (C) 2011-2013 Free Software Foundation, Inc.
#
# This file is part of LIBTASN1.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Written by Simon Josefsson October 2011.

PACKAGE = libtasn1
distdir = $(PACKAGE)-$(VERSION)
TGZ = $(distdir).tar.gz
URL = ftp://ftp.gnu.org/gnu/$(PACKAGE)/$(TGZ)

all:
	@echo 'Usage examples:'
	@echo '  make -f libtasn14win.mk libtasn14win VERSION=2.10'
	@echo '  make -f libtasn14win.mk libtasn14win32 VERSION=2.10 CHECK=check'

libtasn14win: libtasn14win32 libtasn14win64

libtasn14win32:
	$(MAKE) -f libtasn14win.mk doit ARCH=32 HOST=i686-w64-mingw32 CHECK=check

libtasn14win64:
	$(MAKE) -f libtasn14win.mk doit ARCH=64 HOST=x86_64-w64-mingw32

doit:
	rm -rf tmp && mkdir tmp && cd tmp && \
	cp ../../$(TGZ) . || wget $(URL) && \
	tar xfa $(TGZ) && \
	cd $(distdir) && \
	./configure --host=$(HOST) --build=x86_64-unknown-linux-gnu --prefix=$(PWD)/tmp/root && \
	make $(CHECK) install && \
	cd .. && \
	cd root && \
	zip -r ../../$(distdir)-win$(ARCH).zip *

upload:
	../build-aux/gnupload --to ftp.gnu.org:$(PACKAGE) $(distdir)-win32.zip $(distdir)-win64.zip
	cp $(distdir)-win32.zip $(distdir)-win32.zip.sig $(distdir)-win64.zip $(distdir)-win64.zip.sig ../../releases/$(PACKAGE)/
