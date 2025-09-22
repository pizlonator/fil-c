# Libtasn1 README -- Introduction information

This is GNU Libtasn1, a small ASN.1 library.

The C library (libtasn1.*) is licensed under the GNU Lesser General
Public License version 2.1 or later.  See the file COPYING.LIB.

The command line tool, self tests, examples, and other auxilliary
files, are licensed under the GNU General Public License version 3.0
or later.  See the file COPYING.

## Building the library

We require several tools to build the software, including:

* [Make](https://www.gnu.org/software/make/)
* [Automake](https://www.gnu.org/software/automake/) (use 1.11.3 or later)
* [Autoconf](https://www.gnu.org/software/autoconf/)
* [Libtool](https://www.gnu.org/software/libtool/)
* [Texinfo](https://www.gnu.org/software/texinfo/)
* [help2man](http://www.gnu.org/software/help2man/)
* [Tar](https://www.gnu.org/software/tar/)
* [Gzip](https://www.gnu.org/software/gzip/)
* [bison](https://www.gnu.org/software/bison/)
* [Texlive & epsf](https://www.tug.org/texlive/) (for PDF manual)
* [GTK-DOC](https://www.gtk.org/gtk-doc/) (for API manual)
* [Git](https://git-scm.com/)
* [libabigail](https://pagure.io/libabigail/) (for abi comparison in make dist)
* [Valgrind](https://valgrind.org/) (optional)

The required software is typically distributed with your operating
system, and the instructions for installing them differ.  Here are
some hints:

Debian/Ubuntu:
```
sudo apt-get install make git autoconf automake libtool bison
sudo apt-get install texinfo help2man gtk-doc-tools valgrind abigail-tools
```

PDF manual - Debian <= stretch:
```
sudo apt-get install texlive-generic-recommended texlive texlive-extra-utils
```

PDF manual - Debian >= buster:
```
sudo apt-get install texlive-plain-generic texlive texlive-extra-utils
```

The next step is to run autoreconf, ./configure, etc:

```
$ ./bootstrap
```

Then build the project normally:

```
$ ./configure
$ make check
```

Happy hacking!


## Manual

The manual is in the `doc/` directory of the release.

You can also browse the manual online at:

 - https://www.gnu.org/software/libtasn1/manual/
 - https://gnutls.gitlab.io/libtasn1/manual/
 - https://gnutls.gitlab.io/libtasn1/manual/libtasn1.html
 - https://gnutls.gitlab.io/libtasn1/manual/libtasn1.pdf
 - https://gnutls.gitlab.io/libtasn1/reference/
 - https://gnutls.gitlab.io/libtasn1/reference/libtasn1.pdf


## Code coverage report

The coverage report is at:

 - https://gnutls.gitlab.io/libtasn1/coverage


## Issue trackers

 - [Main issue tracker](https://gitlab.com/gnutls/libtasn1/issues)
 - [oss-fuzz found issues](https://bugs.chromium.org/p/oss-fuzz/issues/list?q=libtasn1&can=2)


## Homepage

The project homepage at the gnu site is at:

https://www.gnu.org/software/libtasn1/
