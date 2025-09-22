The common case

```
./autogen.sh && ./configure && make
```

If something fails read the last lines. Typical reason to fail is a missing
dependency, such as libtool or gettext.


Autotools
=========

`./autogen.sh` generates all files needed to compile and install the code (run
it after checkout from git)

`make distclean` removes all unnecessary files, but the code can still be
recompiled with `./configure; make`

`make dist-gzip` (or -bzip2) creates a tarball that can be configured and
compiled without running `./autogen.sh`

