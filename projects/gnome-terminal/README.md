GNOME Terminal
==============

CI status
---------

[![pipeline status](https://gitlab.gnome.org/GNOME/gnome-terminal/badges/master/pipeline.svg)](https://gitlab.gnome.org/GNOME/gnome-terminal/-/commits/master)

[![coverage report](https://gitlab.gnome.org/GNOME/gnome-terminal/badges/master/coverage.svg)](https://gitlab.gnome.org/GNOME/gnome-terminal/-/commits/master)

Releases
--------

[![Latest Release](https://gitlab.gnome.org/GNOME/gnome-terminal/-/badges/release.svg)](https://gitlab.gnome.org/GNOME/gnome-terminal/-/releases)

Tarballs for newer releases are available from the
[package registry](https://gitlab.gnome.org/GNOME/gnome-terminal/-/packages)
and new and old release are also available on
[download.gnome.org](https://download.gnome.org/sources/gnome-terminal/).

Source code
-----------

To get the source code, use
```
$ git clone https://gitlab.gnome.org/GNOME/gnome-terminal
```

Building from source
--------------------

You will most likely need to also build `vte` from source; see https://gitlab.gnome.org/GNOME/vte/-/blob/master/README.md .

Start by installing the build dependencies, and a C++ compiler.

For fedora and related distributions, use
```
sudo dnf build-dep vte291 gnome-terminal
sudo dnf install g++
```
while for debian and related distributions, use
```
sudo apt-get build-dep libvte-2.91-0 gnome-terminal
sudo apt-get install g++
```

First build `vte` according to its own instructions. Then:
```
$ # Get the source code
$ git clone https://gitlab.gnome.org/GNOME/gnome-terminal
$
$ # Change to the toplevel directory
$ cd gnome-terminal
$
$ # Run the configure script (choose an appropriate path instead of "/some/where"!)
$ # Don't forget to make sure that pkg-config can find your self-build vte!
$ # e.g. by doing:
$ # export PKG_CONFIG_PATH=/some/where/lib64/pkg-config:$PKG_CONFIG_PATH
$ #
$ # If you compiled gnome-shell into the same prefix, you can omit disabling
$ # the search provider.
$ #
$ meson _build --prefix=/some/where -Dsearch_provider=false
$
$ # Build
$ ninja -C _build
$
$ # Install
$ ninja -C _build install
```

* By default, GNOME Terminal will install under `/usr/local`, which is not usually
the right choice. You can customize the prefix directory by `--prefix` option, e.g.
if you want to install GNOME-TERMINAL under `~/foobar`, you should run
`meson _build --prefix=$HOME/foobar`. If you already run the configure script before,
you should also pass `--reconfigure` option to it.

* You may need to execute `ninja -C _build install` as root
(i.e. `sudo ninja -C _build install`) if installing to system directories. Use a
user-writable directory as `--prefix` instead to avoid that.

* Since GNOME Terminal uses a D-Bus activated server, you cannot simply run
the self-built gnome-terminal directly. Instead, you need to start the new `gnome-terminal-server` directly using
```
$ ./_build/src/gnome-terminal-server --app-id test.Terminal &
```
and then you have 10s time to open a window in that server using
```
$ ./_build/src/gnome-terminal --app-id test.Terminal
```

Also see https://wiki.gnome.org/Apps/Terminal/Debugging for more information.

Debugging
---------

After installing GNOME-TERMINAL with `-Ddebugg=true` flag, you can use `GNOME_TERMINAL_DEBUG` variable to control
GNOME-TERMINAL to print out the debug information

```
$ GNOME_TERMINAL_DEBUG=selection ./_build/src/gnome-terminal-server [...]

# Or, you can mixup with multiple logging level
$ GNOME_TERMINAL_DEBUG=selection,draw,cell ./_build/src/gnome-terminal-server [...]

$ Or, you can use `all` to print out all logging message
$ GNOME_TERMINAL_DEBUG=all ./_build/src/gnome-terminal-server [...]
```

For logging level information, please refer to enum [TerminalDebugFlags](src/terminal-debug.hh).

Contributing
------------

Bugs should be filed here: https://gitlab.gnome.org/GNOME/gnome-terminal/issues/
Please note that this is a bug tracker to be used for developers of GNOME Terminal,
and contributors of code, documentation, and translations to GNOME Terminal,
and *not a support forum*.

If you are an end user, always file bugs in your distribution's bug tracker, or use their
support forums.

If you want to provide a patch, please attach them to an issue in GNOME
GitLab, in the format output by the `git format-patch` command.
