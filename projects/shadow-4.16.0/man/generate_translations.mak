LANG=$(notdir $(CURDIR))

if ENABLE_REGENERATE_MAN
config.xml: ../config.xml.in
	$(MAKE) -C .. config.xml
	cp ../config.xml $@

messages.mo: ../po/$(LANG).po
	msgfmt $< -o messages.mo

login.defs.d:
	ln -sf $(srcdir)/../login.defs.d login.defs.d

%.xml: ../%.xml messages.mo login.defs.d
	if grep -q SHADOW-CONFIG-HERE $< ; then \
	    sed -e 's/^<!-- SHADOW-CONFIG-HERE -->/<!ENTITY % config SYSTEM "config.xml">%config;/' $< > $@; \
	else \
	    sed -e 's/^\(<!DOCTYPE .*docbookx.dtd"\)>/\1 [<!ENTITY % config SYSTEM "config.xml">%config;]>/' $< > $@; \
	fi
	itstool -i $(srcdir)/../its.rules -d -l $(LANG) -m messages.mo -o . $@
	sed -i 's:\(^<refentry .*\)>:\1 lang="$(LANG)">:' $@

include ../generate_mans.mak

else
$(man_MANS):
	@echo you need to run configure with --enable-man to generate man pages
endif

CLEANFILES = messages.mo login.defs.d $(EXTRA_DIST) $(addsuffix .xml,$(EXTRA_DIST)) config.xml
