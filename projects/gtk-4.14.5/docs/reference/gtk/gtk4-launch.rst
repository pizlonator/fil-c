.. _gtk4-launch(1):

===========
gtk4-launch
===========

---------------------
Launch an application
---------------------

SYNOPSIS
--------

|   **gtk4-launch** [OPTIONS...] <APPLICATION> [URI...]

DESCRIPTION
-----------

``gtk4-launch`` launches an application using the given name. The application is
started with proper startup notification on a default display, unless specified
otherwise.

``gtk4-launch`` takes at least one argument, the name of the application to
launch. The name should match application desktop file name, as residing in the
applications subdirectories of the XDG data directories, with or without the
``.desktop`` suffix.

If called with more than one argument, the rest of them besides the application
name are considered URI locations and are passed as arguments to the launched
application.

OPTIONS
-------

``-?, -h, --help``

  Print the command's help and exit.

``--version``

  Print the command's version and exit.

ENVIRONMENT
-----------

Some environment variables affect the behavior of ``gtk4-launch``:

``XDG_DATA_HOME, XDG_DATA_DIRS``

  The environment variables specifying the XDG data directories.
