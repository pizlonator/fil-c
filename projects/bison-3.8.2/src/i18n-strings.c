/* This file is not compiled in, it is used only to expose more
   strings to gettextize.

   Why is this needed?

   Bison emits strings to translate in the generated code, for builtin
   tokens.  So they appear only in generated parsers, which are not
   shipped, so they are not in the src tree, so we cannot use them in
   our POTFILE.

   Except src/parse-gram.c, which is in the source tree.  And even in
   the git repo.  But to avoid useless diffs in the repo, we do not
   keep the #line directives in the src tree.  Yet, for the user, we
   ship a src/parse-gram.c _with_ the #lines.  This is done in a
   dist-hook which regenerates src/parse-gram.c when we run "make
   dist".

   Unfortunately, then, update-po traverses the whole tree and sees
   that the location of the strings to translate in src/parse-gram.c
   have changed, so the bison.pot is to be updated.  And that is not
   possible in the "make dist" which is run within "make distcheck"
   (not the one preparing the dist for distcheck, the one run by
   distcheck to check that a distributed tarball can build a tarball)
   because then the src tree is read-only.

   So let's not put src/parse-gram.c in the POTFILE, and expose these
   strings to gettextize by hand.
  */

// Please syntax-check.
#include <config.h>

static const char *const msgid[] =
  {
    N_("end of file"),
    N_("invalid token")
  }
