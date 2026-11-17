Virtual TErminal
================

VTE provides a virtual terminal widget for GTK applications.

CI status
---------

[![pipeline status](https://gitlab.gnome.org/GNOME/vte/badges/master/pipeline.svg)](https://gitlab.gnome.org/GNOME/vte/-/commits/master)

[![coverage report](https://gitlab.gnome.org/GNOME/vte/badges/master/coverage.svg)](https://gitlab.gnome.org/GNOME/vte/-/commits/master)

Releases
--------

[![Latest Release](https://gitlab.gnome.org/GNOME/vte/-/badges/release.svg)](https://gitlab.gnome.org/GNOME/vte/-/releases)

Tarballs for newer releases are available from the
[package registry](https://gitlab.gnome.org/GNOME/vte/-/packages)
and new and old release are also available on
[download.gnome.org](https://download.gnome.org/sources/vte/).

Source code
-----------

To get the source code, use
```
$ git clone https://gitlab.gnome.org/GNOME/vte
```

Installation
------------

```
$ git clone https://gitlab.gnome.org/GNOME/vte  # Get the source code of VTE
$ cd vte                                        # Change to the toplevel directory
$ meson _build                                  # Run the configure script
$ ninja -C _build                               # Build VTE
[ Optional ]
$ ninja -C _build install                       # Install VTE to default `/usr/local`
```

* By default, VTE will install under `/usr/local`. You can customize the
prefix directory by `--prefix` option, e.g. If you want to install VTE under
`~/foobar`, you should run `meson _build --prefix=~/foobar`. If you already
run the configure script before, you should also pass `--reconfigure` option to it.

* You may need to execute `ninja -C _build install` as root
(i.e. `sudo ninja -C _build install`) if installing to system directories.

* If you wish to test VTE before installing it, you may execute it directly from
its build directory. As `_build` directory, it should be `_build/src/app/vte-[version]`.

* You can pass `-Ddebugg=true` option to meson if you wish to enable debug function.


Debugging
---------

After installing VTE with `-Ddebugg=true` flag, you can use `VTE_DEBUG` variable to control
VTE to print out the debug information

```
# You should change vte-[2.91] to the version you build
$ VTE_DEBUG=selection ./_build/src/app/vte-2.91

# Or, you can mixup with multiple logging level
$ VTE_DEBUG=selection,draw,cell ./_build/src/app/vte-2.91

$ Or, you can use `all` to print out all logging message
$ VTE_DEBUG=all ./_build/src/app/vte-2.91
```

For logging level information, please refer to enum [VteDebugFlags](src/debug.h).


Contributing
------------

Bugs should be filed here: https://gitlab.gnome.org/GNOME/vte/issues/
Please note that this is *not a support forum*; if you are a end user,
always file bugs in your distribution's bug tracker, or use their
support forums.

If you want to provide a patch, please attach them to an issue in GNOME
GitLab, in the format output by the git format-patch command.
