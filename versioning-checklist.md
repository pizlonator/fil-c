All of these places have the Fil-C version in them, and so need to be changed if the version gets bumped:

- Manifesto.md
- README.md
- clang/lib/Basic/Version.cpp
- filc/include/stdfil.h
- libpas/src/libpas/filc_runtime.c
- package-build.sh
- optfil/build_finish.sh
- optfil/setup.sh
- the Fil-C rustc's `--version` output (build_rustc.sh reads the version number from the clang/lib/Basic/Version.cpp FILC_VERSION #define, composes "Fil-C <version> git@github.com:pizlonator/fil-c.git <hash>", and exports it as the FILC_VERSION environment variable; the compiler bakes it into `rustc --version` with an option_env! in compiler/rustc_driver_impl/src/lib.rs, carried by projects/rust.projeny)
- the website (in another repository)

If you're bumping the version in a Fil-C checkout, then you can ignore the website.

