#
# Copyright (c) 2006 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = Linux-PAM_MWG.fo *~

EXTRA_DIST = $(XMLS)

XMLS = Linux-PAM_MWG.xml $(shell ls $(srcdir)/pam_*.xml)
DEP_XMLS = $(shell ls $(top_srcdir)/doc/man/pam_*.xml)

if ENABLE_REGENERATE_MAN
MAINTAINERCLEANFILES = Linux-PAM_MWG.txt Linux-PAM_MWG.pdf html/*.html

all: Linux-PAM_MWG.txt html/Linux-PAM_MWG.html Linux-PAM_MWG.pdf

Linux-PAM_MWG.pdf: $(XMLS) $(DEP_XMLS)
if ENABLE_GENERATE_PDF
	$(XMLLINT) --nonet --xinclude --relaxng $(DOCBOOK_RNG) --noent --noout $<
	$(XSLTPROC) --stringparam generate.toc "book toc" \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 3 --xinclude --nonet \
	$(PDF_STYLESHEET) $< > Linux-PAM_MWG.fo
	$(FO2PDF) Linux-PAM_MWG.fo $@
else
	echo "No fo2pdf processor installed, skip PDF generation"
endif

Linux-PAM_MWG.txt: $(XMLS) $(DEP_XMLS)
	$(XMLLINT) --nonet --xinclude --relaxng $(DOCBOOK_RNG) --noent --noout $<
	$(XSLTPROC) --stringparam generate.toc "book toc" \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 3 --xinclude --nonet \
	  $(TXT_STYLESHEET) $< | $(BROWSER) > $@

html/Linux-PAM_MWG.html: $(XMLS) $(DEP_XMLS)
	@test -d html || mkdir -p html
	$(XMLLINT) --nonet --xinclude --relaxng $(DOCBOOK_RNG) --noent --noout $<
	$(XSLTPROC) --stringparam base.dir html/ \
	  --stringparam root.filename Linux-PAM_MWG \
	  --stringparam use.id.as.filename 1 \
	  --stringparam chunk.first.sections 1 \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 3 --xinclude --nonet \
	  --stringparam chunker.output.encoding UTF-8 \
	  $(HTML_STYLESHEET) $<

distclean-local:
	-rm -rf html Linux-PAM_MWG.txt Linux-PAM_MWG.pdf

endif

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	$(mkinstalldirs) $(DESTDIR)$(pdfdir)
	$(mkinstalldirs) $(DESTDIR)$(htmldir)
	if test -f html/Linux-PAM_MWG.html; then \
	    $(install_sh_DATA) html/Linux-PAM_MWG.html html/mwg-*.html \
		$(DESTDIR)$(htmldir)/; \
	elif test -f $(srcdir)/html/Linux-PAM_MWG.html; then \
	    $(install_sh_DATA) $(srcdir)/html/Linux-PAM_MWG.html \
		$(srcdir)/html/mwg-*.html \
		$(DESTDIR)$(htmldir)/; \
	fi
	if test -f Linux-PAM_MWG.txt; then \
	    $(install_sh_DATA) Linux-PAM_MWG.txt $(DESTDIR)$(docdir)/; \
	elif test -f $(srcdir)/Linux-PAM_MWG.txt; then \
	    $(install_sh_DATA) $(srcdir)/Linux-PAM_MWG.txt \
		$(DESTDIR)$(docdir)/; \
	fi
	if test -f Linux-PAM_MWG.pdf; then \
	    $(install_sh_DATA) Linux-PAM_MWG.pdf $(DESTDIR)$(pdfdir)/; \
	elif test -f $(srcdir)/Linux-PAM_MWG.pdf; then \
	    $(install_sh_DATA) $(srcdir)/Linux-PAM_MWG.pdf \
		$(DESTDIR)$(pdfdir)/; \
	fi

uninstall-local:
	-rm $(DESTDIR)$(htmldir)/Linux-PAM_MWG.html
	-rm $(DESTDIR)$(htmldir)/mwg-*.html
	-rm $(DESTDIR)$(docdir)/Linux-PAM_MWG.txt
	-rm $(DESTDIR)$(pdfdir)/Linux-PAM_MWG.pdf

releasedocs: all
	$(mkinstalldirs) $(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/html
	if test -f html/Linux-PAM_MWG.html; then \
	    cp -ap html/Linux-PAM_MWG.html html/mwg-*.html \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/html/; \
	elif test -f $(srcdir)/html/Linux-PAM_MWG.html; then \
	    cp -ap $(srcdir)/html/Linux-PAM_MWG.html \
		$(srcdir)/html/mwg-*.html \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/html/; \
	else exit 1; \
	fi
	if test -f Linux-PAM_MWG.txt; then \
	    cp -p Linux-PAM_MWG.txt \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/; \
	elif test -f $(srcdir)/Linux-PAM_MWG.txt; then \
	    cp -p $(srcdir)/Linux-PAM_MWG.txt \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/; \
	else exit 1; \
	fi
	if test -f Linux-PAM_MWG.pdf; then \
	    cp -p Linux-PAM_MWG.pdf \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/; \
	elif test -f $(srcdir)/Linux-PAM_MWG.pdf; then \
	    cp -p $(srcdir)/Linux-PAM_MWG.pdf \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/mwg/; \
	else exit 1; \
	fi
