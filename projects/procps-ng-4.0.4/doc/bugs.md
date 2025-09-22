BUG REPORTS
===========

The following is information for reporting bugs. Please read
the file as well as the documentation for the relevant program
before posting. This document is more useful for advanced users
and the people that package for the distributions.

Also if you are an end-user of the programs and not the packager
and are using a distribution, check their bug tracker first,
you may find its a known bug already.


Where to send
-------------
You can raise issues on the GitLab issues tracker which is
located at https://gitlab.com/procps-ng/procps/issues You
will need a GitLab login to do so.

Alternatively send comments, bug reports, patches, etc.
to the email list procps@freelists.org

What to send
------------
It is much more useful to us if a program really crashes to recompile it
with make `CFLAGS=-ggdb -O`, run it with `gdb prog` and `run` and send
me a stack trace (`bt` command).  That said, any bug report is still
better than none.

strace and ltrace output are very helpful:

> strace -o output-file ps --blah
> bzip2 output-file

The output of `ps --info` is often quite useful, even if the problem
is not with ps itself. A lot of the utilities use the same library.

Merge Requests
--------------
Merge requests are fine to use and give a central place for
everyone involved to have a look. Merge requests are found
on GitLab at https://gitlab.com/procps-ng/procps/merge_requests
It is best to follow up your merge request with an email to
the list saying what you have done.

Patches
-------
While merge requests are preferred, patches are also welcome.
Get latest version of the source from upstream git.

> git clone git@gitlab.com:procps-ng/procps.git

and use `git format-patch` format. It is fine to attach patches as
compressed tar balls.  When you are about to send very large number
of patches consider setting up your personal clone, and send a pull
request.

> git request-pull commit-id \
>	git://gitorious.org/~yourlogin/procps/your-clone.git


Kernel-Dependent Patches
------------------------
If you send patches which are specific to *running* with a particular
kernel version of /proc, please condition them with the runtime determined
variable `linux_version_code` from libproc/version.c.  It is the same
number as the macro `LINUX_VERSION_CODE` for which the kernel /proc fs
code was compiled.

A macro is provide in libproc/misc.h to construct the code from its
components, e.g.
>  if (linux_version_code < LINUX_VERSION(2,5,41))
>     /* blah blah blah */
A startup call to `set_linux_version` may also be necessary.

Of course, if a bug is due to a change in kernel file formats, it would
be best to first try to generalize the parsing, since the code is then
more resilient against future change.

Code Structure
--------------
A goal is to encapsulate *all* parsing dependent on /proc
file formats into the libproc library.  If the API is general enough
it can hopefully stabilize and then /proc changes might only require
updating libproc.so.  Beyond that having the set of utilities be simple
command lines parsers and output formatters and encapsulating all kernel
divergence in libproc is the way to go.

Hence if you are submitting a new program or are fixing an old one, keep
in mind that adding files to libproc which encapsulate such things is
more desirable than patching the actual driver program.  (well, except
to move it toward the API of the library).
