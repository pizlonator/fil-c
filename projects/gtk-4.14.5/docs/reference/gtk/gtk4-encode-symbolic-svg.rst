.. _gtk4-encode-symbolic-svg(1):

========================
gtk4-encode-symbolic-svg
========================

--------------------------------
Symbolic icon conversion utility
--------------------------------

SYNOPSIS
--------

|   **gtk4-encode-symbolic-svg** [OPTIONS...] <PATH> <WIDTH>x<HEIGHT>

DESCRIPTION
-----------

``gtk4-encode-symbolic-svg`` converts symbolic SVG icons into specially prepared
PNG files. GTK can load and recolor these PNGs, just like original SVGs, but
loading them is much faster.

``PATH`` is the name of a symbolic SVG file, ``WIDTH`` x ``HEIGHT`` are the
desired dimensions for the generated PNG file.

To distinguish them from ordinary PNGs, the generated files have the extension
``.symbolic.png``.

OPTIONS
-------

``-o, --output DIRECTORY``

  Write png files to ``DIRECTORY`` instead of the current working directory.

``--debug``

  Generate PNG files of the various channels during the conversion. If these
  files are not monochrome green, they are often helpful in pinpointing the
  problematic parts of the source SVG.
