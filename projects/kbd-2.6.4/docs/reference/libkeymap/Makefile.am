DOC_MODULE  = libkeymap

EXTRA_DIST  = Doxyfile.in Doxyheader
CLEANFILES  = Doxyfile html/*

htmldir = $(docdir)/html
html_DATA = html/*

$(abs_builddir)/Doxyfile: $(abs_srcdir)/Doxyfile.in
	$(SED) \
		-e 's,[@]VERSION[@],$(VERSION),g' \
		-e 's,[@]MODULE[@],$(DOC_MODULE),g' \
		-e 's,[@]TOPDIR[@],$(abs_top_srcdir),g' \
		-e 's,[@]SOURCEDIR[@],$(abs_top_srcdir)/src/$(DOC_MODULE),g' \
		-e 's,[@]BUILDDIR[@],$(abs_builddir),g' \
		<$< >$@

apidocs: $(abs_builddir)/Doxyfile
	doxygen $<

html/*: apidocs
