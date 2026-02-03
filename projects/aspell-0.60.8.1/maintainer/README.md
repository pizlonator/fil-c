This is the Git repository for GNU Aspell

http://aspell.net

The following packages need to be installed to build this package
from Git:
  * perl (I use v5.6.0 but other versions may work)
  * libtool
  * gettext
  * autoconf
  * automake
  * makeinfo

Before the build:
```
./autogen

./configure --disable-static <options>
# or ./config-opt <options> or ./config-debug <options>
```

The `./config-*` will set things up for easier development.  If you want
to install Aspell to use it rather than develop with it, then use the
normal `configure`.  When `config-*` is used the default things will be
installed in `<build dir>/inst` for easier testing and debugging. You
can change that by using the `--prefix` option.

Autogen should be run when ever anything but the source files or
Makefile.am files are modified.

Then to build and install:
```
make
make install
```

You will then need to install a dictionary package for the new Aspell.
You can find them at http://aspell.net.  If Aspell is installed
somewhere other than `/usr/local`, you will probably need to add
`<prefix>/bin` to your PATH or make symbolic links to the executable in
order for the dictionary to build correctly.

To run the debugger on these programs if there are not installed use
```
libtool gdb <debugger parms>
```
or
```
libtool --mode execute <your debugger> <debugger parms>
```
For example to debug aspell with ddd use
```
libtool --mode execute ddd prog/.libs/aspell
```

Using libtool is necessary to make sure the shared libraries get loaded
right. If you debug them after they are installed this will not be
necessary.
