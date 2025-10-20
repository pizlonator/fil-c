#
# Copyright (c) 2006 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = Linux-PAM_SAG.fo *~

EXTRA_DIST = $(XMLS)

XMLS = Linux-PAM_SAG.xml $(shell ls $(srcdir)/pam_*.xml)
DEP_XMLS = $(shell ls $(top_srcdir)/modules/pam_*/pam_*.xml)

if ENABLE_REGENERATE_MAN
MAINTAINERCLEANFILES = Linux-PAM_SAG.txt Linux-PAM_SAG.pdf html/*.html

all: Linux-PAM_SAG.txt html/Linux-PAM_SAG.html Linux-PAM_SAG.pdf

Linux-PAM_SAG.pdf: $(XMLS) $(DEP_XMLS)
if ENABLE_GENERATE_PDF
	$(XMLLINT) --nonet --xinclude --relaxng $(DOCBOOK_RNG) --noent --noout $<
	$(XSLTPROC) --stringparam generate.toc "book toc" \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 2 --xinclude --nonet \
	$(PDF_STYLESHEET) $< > Linux-PAM_SAG.fo
	$(FO2PDF) Linux-PAM_SAG.fo $@
else
	echo "No fo2pdf processor installed, skip PDF generation"
endif

Linux-PAM_SAG.txt: $(XMLS) $(DEP_XMLS)
	$(XMLLINT) --nonet --xinclude --relaxng $(DOCBOOK_RNG) --noent --noout $<
	$(XSLTPROC) --stringparam generate.toc "book toc" \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 2 --xinclude --nonet \
	  $(TXT_STYLESHEET) $< | $(BROWSER) > $@

html/Linux-PAM_SAG.html: $(XMLS) $(DEP_XMLS)
	@test -d html || mkdir -p html
	$(XMLLINT) --nonet --xinclude --relaxng $(DOCBOOK_RNG) --noent --noout $<
	$(XSLTPROC) --stringparam base.dir html/ \
	  --stringparam root.filename Linux-PAM_SAG \
	  --stringparam use.id.as.filename 1 \
	  --stringparam chunk.first.sections 1 \
	  --stringparam section.autolabel 1 \
	  --stringparam section.label.includes.component.label 1 \
	  --stringparam toc.max.depth 2 --xinclude --nonet \
	  --stringparam chunker.output.encoding UTF-8 \
	  $(HTML_STYLESHEET) $<

distclean-local:
	-rm -rf html Linux-PAM_SAG.txt Linux-PAM_SAG.pdf
endif

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	$(mkinstalldirs) $(DESTDIR)$(pdfdir)
	$(mkinstalldirs) $(DESTDIR)$(htmldir)
	if test -f html/Linux-PAM_SAG.html; then \
	    $(install_sh_DATA) html/Linux-PAM_SAG.html html/sag-*.html \
		$(DESTDIR)$(htmldir)/; \
	elif test -f $(srcdir)/html/Linux-PAM_SAG.html; then \
	    $(install_sh_DATA) $(srcdir)/html/Linux-PAM_SAG.html \
		$(srcdir)/html/sag-*.html \
		$(DESTDIR)$(htmldir)/; \
	fi
	if test -f Linux-PAM_SAG.txt; then \
	    $(install_sh_DATA) Linux-PAM_SAG.txt $(DESTDIR)$(docdir)/; \
	elif test -f $(srcdir)/Linux-PAM_SAG.txt; then \
	    $(install_sh_DATA) $(srcdir)/Linux-PAM_SAG.txt \
		$(DESTDIR)$(docdir)/; \
	fi
	if test -f Linux-PAM_SAG.pdf; then \
	    $(install_sh_DATA) Linux-PAM_SAG.pdf $(DESTDIR)$(pdfdir)/; \
	elif test -f $(srcdir)/Linux-PAM_SAG.pdf; then \
	    $(install_sh_DATA) $(srcdir)/Linux-PAM_SAG.pdf \
		$(DESTDIR)$(pdfdir)/; \
	fi

uninstall-local:
	-rm $(DESTDIR)$(htmldir)/Linux-PAM_SAG.html
	-rm $(DESTDIR)$(htmldir)/sag-*.html
	-rm $(DESTDIR)$(docdir)/Linux-PAM_SAG.txt
	-rm $(DESTDIR)$(pdfdir)/Linux-PAM_SAG.pdf

releasedocs: all
	$(mkinstalldirs) $(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/html
	if test -f html/Linux-PAM_SAG.html; then \
	    cp -ap html/Linux-PAM_SAG.html html/sag-*.html \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/html/; \
	elif test -f $(srcdir)/html/Linux-PAM_SAG.html; then \
	    cp -ap $(srcdir)/html/Linux-PAM_SAG.html \
		$(srcdir)/html/sag-*.html \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/html/; \
	else exit 1; \
	fi
	if test -f Linux-PAM_SAG.txt; then \
	    cp -p Linux-PAM_SAG.txt \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/; \
	elif test -f $(srcdir)/Linux-PAM_SAG.txt; then \
	    cp -p $(srcdir)/Linux-PAM_SAG.txt \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/; \
	else exit 1; \
	fi
	if test -f Linux-PAM_SAG.pdf; then \
	    cp -p Linux-PAM_SAG.pdf \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/; \
	elif test -f $(srcdir)/Linux-PAM_SAG.pdf; then \
	    cp -p $(srcdir)/Linux-PAM_SAG.pdf \
		$(top_builddir)/Linux-PAM-$(VERSION)/doc/sag/; \
	else exit 1; \
	fi
