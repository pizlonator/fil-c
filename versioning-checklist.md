All of these places have the Fil-C version in them, and so need to be changed if the version gets bumped:

- Manifesto.md
- README.md
- clang/lib/Basic/Version.cpp
- filc/include/stdfil.h
- package-build.sh
- optfil/build_finish.sh
- optfil/setup.sh
- the website (in another repository)

If you're bumping the version in a Fil-C checkout, then you can ignore the website.

