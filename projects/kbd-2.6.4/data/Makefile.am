EXTRA_DIST = consolefonts consoletrans keymaps partialfonts unimaps

# If you change the names of any of the following subdirs,
# also change paths.h.
OLDKEYMAPDIR = keytables
KEYMAPDIR    = keymaps
UNIMAPDIR    = unimaps
FONTDIR      = consolefonts
PARTIALDIR   = partialfonts
TRANSDIR     = consoletrans

KEYMAPSUBDIRS = include sun amiga atari i386/azerty i386/bepo i386/dvorak \
	i386/fgGIod i386/qwerty i386/qwertz i386/include i386/olpc \
	i386/colemak i386/carpalx i386/neo mac/include mac/all \
	pine

# Do not use GZIP - it is interpreted by gzip
MYGZIP = gzip -f -n -9
MYGZIP_SUFFIX = .gz

install-data-hook: $(KEYMAPDIR)_Z $(FONTDIR)_Z
	install -d -m 755 $(DESTDIR)$(datadir)
	install -d -m 755 $(DESTDIR)$(datadir)/$(FONTDIR)
	install -m 644 $(FONTDIR)_Z/* $(DESTDIR)$(datadir)/$(FONTDIR)
	install -d -m 755 $(DESTDIR)$(datadir)/$(FONTDIR)/$(PARTIALDIR)
	install -m 644 $(PARTIALDIR)_Z/* $(DESTDIR)$(datadir)/$(FONTDIR)/$(PARTIALDIR)
	install -d -m 755 $(DESTDIR)$(datadir)/$(TRANSDIR)
	install -m 644 $(srcdir)/$(TRANSDIR)/* $(DESTDIR)$(datadir)/$(TRANSDIR)
	install -d -m 755 $(DESTDIR)$(datadir)/$(UNIMAPDIR)
	install -m 644 $(srcdir)/$(UNIMAPDIR)/* $(DESTDIR)$(datadir)/$(UNIMAPDIR)
	install -d -m 755 $(DESTDIR)$(datadir)/$(KEYMAPDIR)
	install -d -m 755 $(DESTDIR)$(datadir)/$(KEYMAPDIR)/i386
	install -d -m 755 $(DESTDIR)$(datadir)/$(KEYMAPDIR)/mac
	for i in $(KEYMAPSUBDIRS); do \
		install -d -m 755 $(DESTDIR)$(datadir)/$(KEYMAPDIR)/$$i ;\
		install -m 644 $(KEYMAPDIR)_Z/$$i/* $(DESTDIR)$(datadir)/$(KEYMAPDIR)/$$i ;\
	done
	rm -f $(DESTDIR)$(datadir)/$(KEYMAPDIR)/ppc; $(LN_S) mac $(DESTDIR)$(datadir)/$(KEYMAPDIR)/ppc
	@if [ -f $(DESTDIR)$(datadir)/$(OLDKEYMAPDIR) ]; then \
	  echo "Done. You may want to remove old keymaps with" ; \
	  echo "  rm -rf $(DESTDIR)$(datadir)/$(OLDKEYMAPDIR)" ; \
	  echo "But be careful to preserve your default map if it is" ; \
	  echo "nonstandard, and to adapt any scripts in rc.local or so." ; \
	fi

# compress data files - do not touch the distribution but copy first
$(KEYMAPDIR)_Z:
	cp -r $(srcdir)/$(KEYMAPDIR) $(KEYMAPDIR)_Z
	chmod -R u+w $(KEYMAPDIR)_Z
	cd $(KEYMAPDIR)_Z; \
	for f in */*.map */*/*.map; do \
	  $(MYGZIP) < "$$f" > "$$f$(MYGZIP_SUFFIX)"; \
	done; \
	rm -f -- */*.map */*/*.map;

$(FONTDIR)_Z:
	cp -r $(srcdir)/$(FONTDIR) $(FONTDIR)_Z
	cp -r $(srcdir)/$(PARTIALDIR) $(PARTIALDIR)_Z
	chmod -R u+w $(FONTDIR)_Z $(PARTIALDIR)_Z
	cd $(FONTDIR)_Z && $(MYGZIP) * && gunzip ERRORS* README*
	cd $(PARTIALDIR)_Z && $(MYGZIP) *
# (not yet screenmaps - some other time)

clean-local:
	rm -rf $(KEYMAPDIR)_Z $(FONTDIR)_Z $(PARTIALDIR)_Z

uninstall-hook:
	cd $(DESTDIR)$(datadir) && rm -rf $(FONTDIR) $(PARTIALDIR) $(TRANSDIR) $(UNIMAPDIR) $(KEYMAPDIR)
