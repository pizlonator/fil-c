.. _gtk4-node-editor(1):

=================
gtk4-node-editor
=================

-----------------
Editor render nodes
-----------------

SYNOPSIS
--------

|   **gtk4-node-editor** [OPTIONS...] [FILE]

DESCRIPTION
-----------

``gtk4-node-editor`` is a utility to show and edit render node files.
Such render node files can be obtained e.g. from the GTK inspector or
as part of the testsuite in the GTK sources.

``gtk4-node-editor`` is used by GTK developers for debugging and testing,
and it has built-in support for saving testcases as part of the GTK testsuite.

OPTIONS
-------

``-h, --help``

  Show the application help.

``--version``

  Show the program version.

``--reset``

  Don't restore autosaved content and remove autosave files.

ENVIRONMENT
-----------

``GTK_SOURCE_DIR``

  can be set to point to the location where the GTK sources reside, so that
  testcases can be saved to the right location. If unsed, `gtk4-node-editor``
  checks if the current working directory looks like a GTK checkout, and failing
  that, saves testcase in the the current working directory.
