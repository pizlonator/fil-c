gen_MANS = loadunimap.8 mapscrn.8 setfont.8
dist_man_MANS = getkeycodes.8 kbdrate.8 resizecons.8 setkeycodes.8 \
		showconsolefont.8 setvtrgb.8 $(gen_MANS)

install-data-hook:
	cd $(DESTDIR)$(mandir)/man8 && \
	for f in $(gen_MANS); do \
		$(SED) -e "s#@DATADIR\@#${datadir}#g" "$$f" > "$$f.new"; \
		mv -f -- "$$f.new" "$$f"; \
	done
