
info_TEXINFOS = aspell.texi aspell-dev.texi

AM_MAKEINFOFLAGS = --enable-encoding \
  --set-customization-variable 'EXTRA_HEAD <meta name=viewport content="width=device-width, initial-scale=1">' \
  --css-include=custom.css

aspell_TEXINFOS = readme.texi fdl.texi lgpl.texi \
  sug-scores.texi custom.css

aspell_dev_TEXINFOS = mksrc.texi fdl.texi

man_MANS = aspell.1         pspell-config.1    word-list-compress.1\
           aspell-import.1  run-with-aspell.1  prezip-bin.1

EXTRA_DIST = $(man_MANS) aspell.html aspell-dev.html

