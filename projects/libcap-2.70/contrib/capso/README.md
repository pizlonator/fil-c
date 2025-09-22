# Leveraging file capabilities on shared libraries

This directory contains an example of a shared library (`capso.so`)
that can be installed with file capabilities. When the library is
linked against an unprivileged program, it includes internal support
for re-invoking itself as a child subprocess to execute a privileged
operation on bahalf of the parent.

The idea for doing this was evolved from the way `pam_unix.so` is able
to leverage a separate program, and `libcap`'s recently added support
for supporting binary execution of all the `.so` files built by the
package.

The actual program example `./bind` leverages the
`"cap_net_bind_service=p"` enabled `./capso.so` file to bind to the
privileged port 80.

A writeup of how to build and explore the behavior of this example is
provided on the `libcap` distribution website:

https://sites.google.com/site/fullycapable/capable-shared-objects
