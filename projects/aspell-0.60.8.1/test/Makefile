CXXFLAGS = -O -g
CFLAGS   = -O -g
ASPELL_WRAP = # valgrind
export ASPELL = ${CURDIR}/inst/bin/aspell
export PREZIP = ${CURDIR}/inst/bin/prezip-bin
EXTRA_CONFIG_FLAGS =
ifdef SLOPPY
  EXTRA_CONFIG_FLAGS += --enable-sloppy-null-term-strings
endif

.PHONY: all prep sanity filter-test suggest wide cxx_warnings
all: prep sanity filter-test suggest wide cxx_warnings
	cat test-res

# warning-settings.mk defines EXTRA_CXXFLAGS
warning-settings.mk: warning-settings.cpp
	$(CXX) warning-settings.cpp -o warning-settings
	./warning-settings > warning-settings.mk
include warning-settings.mk

prep: inst/bin/aspell inst/lib/aspell-0.60/en.multi tmp
	rm -f test-res

tmp:
	mkdir tmp

sanity: prep
	./sanity
	echo "all ok (sanity)" >> test-res

filter-test: prep
	./filter-test "${ASPELL_WRAP} ${ASPELL}" < markdown.dat
	echo "all ok (markdown filter-test)" >> test-res

inst/bin/aspell: build/Makefile phony
	$(MAKE) -C build aspell && ( cmp -s build/aspell inst/bin/aspell || $(MAKE) -s -C build install )

inst/lib/aspell-0.60/en.multi: inst/bin/aspell aspell6-en-2018.04.16-0.tar.bz2
	tar xf aspell6-en-2018.04.16-0.tar.bz2
	cp en_phonet.dat aspell6-en-2018.04.16-0
	cd aspell6-en-2018.04.16-0 && ./configure
	$(MAKE) -C aspell6-en-2018.04.16-0 install
	cp en.dat en.prepl en_repl.dat en_phonet.dat inst/lib/aspell-0.60
	echo 'add en_US.multi' > inst/lib/aspell-0.60/en_US-w_repl.multi
	echo 'add en.prepl' >> inst/lib/aspell-0.60/en_US-w_repl.multi

build/Makefile:
	mkdir -p build
	cd build && ../../configure "CXXFLAGS=${CXXFLAGS} ${EXTRA_CXXFLAGS}" "CFLAGS=${CFLAGS}" -q --enable-silent-rules --disable-shared --disable-pspell-compatibility --prefix="${CURDIR}/inst" $(EXTRA_CONFIG_FLAGS)

aspell6-en-2018.04.16-0.tar.bz2:
	curl -O ftp://ftp.gnu.org/gnu/aspell/dict/en/aspell6-en-2018.04.16-0.tar.bz2

.PHONY: clean
clean:
	rm -rf warning-settings warning-settings.mk inst build tmp aspell6-en-2018.04.16-0

suggest.mk: suggest/mkmk
	suggest/mkmk > suggest.mk

include suggest.mk

.PHONY: wide_test_invalid wide_test_invalid-but_ok wide_test_valid
wide: wide_test_invalid wide_test_invalid-but_ok wide_test_valid

wide_test_invalid: wide_test_invalid.c prep
	$(CC) $(CFLAGS) -Iinst/include -c $< -o tmp/$@.o
	$(CXX) $(CXXFLAGS) tmp/$@.o inst/lib/libaspell.a -ldl -o $@
ifdef SLOPPY
	./$@
	echo "ok ($@ SLOPPY)" >> test-res
else
	./$@ 2> tmp/$@.log || true
	fgrep -q 'Null-terminated wide-character strings unsupported when used this way.' tmp/$@.log
	echo "ok ($@)" >> test-res
endif

wide_test_valid: wide_test_valid.c prep
	$(CC) $(CFLAGS) -Iinst/include -c $< -o tmp/$@.o
	$(CXX) $(CXXFLAGS) tmp/$@.o inst/lib/libaspell.a -ldl -o $@
	./$@
	echo "ok ($@)" >> test-res

wide_test_invalid-but_ok: wide_test_invalid.c prep
	$(CC) $(CFLAGS) -DASPELL_ENCODE_SETTING_SECURE -Iinst/include -c $< -o tmp/$@.o
	$(CXX) $(CXXFLAGS) tmp/$@.o inst/lib/libaspell.a -ldl -o $@
	./$@
	echo "ok ($@)" >> test-res

cxx_warnings:  cxx_warnings_test.cpp prep
	$(CXX) $(CXXFLAGS) -Wall -Wconversion -Werror -Iinst/include -c $<
	echo "ok ($@)" >> test-res

.PHONY: phony
phony:
