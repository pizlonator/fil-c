if DOCS
dist_man1_MANS = mg.1
dist_doc_DATA  = tutorial .mg mg.png
endif
EXTRA_DIST     = AUTHORS

if TUTOR
pkgdata_DATA   = tutorial.gz

clean-local:
	-rm -f $(builddir)/tutorial.gz

%.gz: %
	gzip -c $< >$(builddir)/$@
endif
