How to do a GTK release?
========================

## Before we begin

Make sure you have suitable versions of Meson and Ninja.

## Release check list

  0. Save all your work, then move to the branch from which you want
     to release. Go back to a pristine working directory. With Git,
     this works:

```sh
$ git clean -dfx
```

  1. Build using the common sequence:

```sh
$ meson setup _build
$ meson compile -C _build
```

  2. Update NEWS based on the content of git log; follow the format of prior
     entries. This includes finding noteworthy new features, collecting
     summaries for all the fixed bugs that are referenced and collecting all
     updated translations. Also collect the names of all contributors that
     are mentioned. We don't discriminate between bug reporters, patch
     writers, committers, etc. Anybody who is mentioned in the commit log
     gets a credit, but only real names, not email addresses or nicknames.

  3. Update the pot file and commit the changes:

```sh
$ ninja -C _build gtk40-pot
```

  4. If this is a major, stable, release, verify that the release notes
     in the API reference contain the relevant items.

  5. Verify that the version in `meson.build` has been bumped after the last
     release. **Note**: this is critical, a slip-up here will cause the soname
     to change.

  6. Make sure that `meson test` is happy (`ninja dist` will also run the test
     suite, but it's better to catch issues before committing and tagging
     the release). Typical problems to expect here (depending on whether this
     is a devel  snapshot or a stable release):

    * forgotten source files
    * new symbols missing the `GDK_AVAILABLE_IN_` annotation
    * wrong introspection annotations
    * missing documentation

  7. If this is a devel release, make sure that the docs for new symbols are
     in good shape. Look at the -unused.txt files and add stuff found there
     to the corresponding `-sections.txt` file. Look at the `-undocumented.txt`
     files and see if there is anything in there that should be documented.
     If it is, this may be due to typos in the doc comments in the source.
     Make sure that all new symbols have proper Since: tags, and that there
     is an index in the main `-docs.xml` for the next stable version.

  8. Run `meson dist -C_build` to generate the tarball.

  9. Fix broken stuff found by 8), commit changes, repeat.

  10. Once `dist` succeeds, verify that the tree is clean and all the changes
     needed to make the release have been committed: `git diff` should come
     up empty. The last commit must be the commit that bumps up the version
     of the release in the `meson.build` file. Use `git rebase` if you had to
     add more commits after the version bump and `dist` successfully passing.
     If you change the history, remember to rebuild the tarball.

  11. Now you've got the tarball. Check that the tarball size looks reasonable
    compared to previous releases. If the size goes down a lot, likely the
    docs went missing for some reason. Or the translations. If the size goes
    up by a lot, something else may be wrong.

  12. Tag the release. The git command for doing that looks like:

```sh
$ git tag -m "GTK 4.2.0" 4.2.0
```

  13. Bump the version number in `meson.build`, and add a section for the next
      release in NEWS and commit the change.

  14. Push the changes upstream, and push the tag as well. The git command for
    doing that is:

```sh
$ git push origin
$ git push origin 4.2.0
```

  15. Upload the tarball to `master.gnome.org` and run `ftpadmin install` to
    transfer it to `download.gnome.org`. If you don't have an account on
    `master.gnome.org`, find someone who can do it for you. The command for
    this looks like:

```sh
$ scp gtk-4.2.0.tar.xz matthiasc@master.gnome.org:
$ ssh matthiasc@master.gnome.org
$ ftpadmin install gtk-4.2.0.tar.xz
```

  16. Go to the gnome-announce list archives, find the last announce message,
    create a new message in the same form, replacing version numbers,
    commentary at the top about "what this release is about" and the
    summary of changes.
