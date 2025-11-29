Compose data
============

GTK includes a copy of the X11 Compose file in a compact, precompiled
form (the same format is used for caching custom Compose files in
the users home directory).

The X11 Compose file can be found here:

  https://gitlab.freedesktop.org/xorg/lib/libx11/-/raw/master/nls/en_US.UTF-8/Compose.pre

The tool to convert the data is build during the GTK build, from
compose-parse.c in this directory. You run it like this:

  cpp -DXCOMM='#' Compose.pre | sed -e 's/^ *#/#/' > Compose
  compose-parse Compose sequences-little-endian sequences-big-endian chars gtkcomposedata.h

The GTK build expects the resulting files to be in the source tree,
in the gtk/cmompose directory.
