# Contributing to kbd

I'm really glad you're reading this, because I need volunteer developers to
help this project.

Here are some important resources:


## Mailing list

* E-mail:  kbd@lists.altlinux.org
* URL:     https://lists.altlinux.org/mailman/listinfo/kbd
* Archive: https://lore.altlinux.org/kbd/

To protect subscribers from spam, the mailing list requires a subscription.


## Bug reporting

* E-mail: kbd@lists.altlinux.org
* Web:    https://github.com/legionus/kbd/issues

Report problems with this package to the mailing list or
directly to the `Alexey Gladkov <gladkov.alexey@gmail.com>`.


## NLS (PO translations)

PO files are maintained by:
https://translationproject.org/domain/kbd.html


# Source code

The latest stable version of kbd can always be found on:
* https://www.kernel.org/pub/linux/utils/kbd/

Web interface:
 * https://git.kernel.org/pub/scm/linux/kernel/git/legion/kbd.git
 * https://github.com/legionus/kbd

Versioning:

* Standard releases: `<major>.<minor>[.<maint>]`
  - `major` -- fatal and deep changes;
  - `minor` -- typical release with new features;
  - `maint` -- bug fixes.

* Development releases: `<major>.<minor>-rc<N>`

To summarize, the stable release is `2.1.0` while `2.0.9x` is a pre-releases.

Git repository:
* Primary: git://git.kernel.org/pub/scm/linux/kernel/git/legion/kbd.git
* Mirror:  git://github.com/legionus/kbd.git

Git Branches: `git branch -a`

* `master` branch
  - current development.
  - the source for stable releases when deemed ready.

* `for-master` branch
  - unstable changes for master. These changes can be reversed or rolled back.

Tags: `git tag`

- a new tag object is created for every release.
- tag name: `v<version>`.
- all tags are signed by the maintainer's PGP key.


# Patches

Send your patches to the mailing list or to the upstream maintainer (see the
[README](../../README.md) file in project root directory), but sending to the
mailing list is more preferable.

Email attachments are difficult to review and not recommended.
Hint: use `git send-email`.

Email is accepted as an inline patch with, or without, a git pull request. Pull
request emails need to include the patch set for review purposes. See
[howto-pull-request](howto-pull-request.md) and [README](../../README.md) for git
repository instructions.

Many small patches are preferred over a single large patch. Split patch sets
based upon logical functionality.

Don't include generated (autotools) files in your patches.
Hint: use `git clean -Xd`.

Neutrality: the files in kbd should be distribution-neutral. Packages like RPMs,
DEBs, and the rest, are not provided. They should be available from the
distribution.

Make sure that after applying your patch the file(s) will compile without errors
on top of the 'master' branch.

Test that the previously existing program behavior is not altered. If the patch
intentionally alters the behavior explain what changed, and the reason for it,
in the changelog/commit message.


# Sending Patches

Each submitted patch must have a "Signed-off-by" line.  Patches without
this line will not be accepted.

The sign-off is a simple line at the end of the explanation for the
patch, which certifies that you wrote it or otherwise have the right to
pass it on as an open-source patch.  The rules are pretty simple: if you
can certify the below:
```

    Developer's Certificate of Origin 1.1

    By making a contribution to this project, I certify that:

    (a) The contribution was created in whole or in part by me and I
         have the right to submit it under the open source license
         indicated in the file; or

    (b) The contribution is based upon previous work that, to the best
        of my knowledge, is covered under an appropriate open source
        license and I have the right under that license to submit that
        work with modifications, whether created in whole or in part
        by me, under the same open source license (unless I am
        permitted to submit under a different license), as indicated
        in the file; or

    (c) The contribution was provided directly to me by some other
        person who certified (a), (b) or (c) and I have not modified
        it.

    (d) I understand and agree that this project and the contribution
        are public and that a record of the contribution (including all
        personal information I submit with it, including my sign-off) is
        maintained indefinitely and may be redistributed consistent with
        this project or the open source license(s) involved.

```
then you just add a line saying ( git commit -s )
```
    Signed-off-by: Random J Developer <random@developer.example.org>
```
using your real name (sorry, no pseudonyms or anonymous contributions.)


# Coding Style

The clang-format used to format the code. Please use it before submitting
patches.  The preferred coding style described in the .clang-format in project
root directory.

Use 'FIXME:' with a good description, if you want to inform others that
something is not quite right, and you are unwilling to fix the issue in the
submitted change.


# Various Notes

Patches relying on kernel features that are not in Linus Torvalds's tree are not
accepted.
