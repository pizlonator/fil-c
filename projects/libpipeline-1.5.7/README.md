# libpipeline, a pipeline manipulation library

Git repository: https://gitlab.com/libpipeline/libpipeline

libpipeline is a C library for setting up and running pipelines of
processes, without needing to involve shell command-line parsing which is
often error-prone and insecure.  This alleviates programmers of the need to
laboriously construct pipelines using lower-level primitives such as fork(2)
and execve(2).

Full programmers' documentation may be found using `man libpipeline`, and
the [project homepage](https://libpipeline.gitlab.io/libpipeline/) has more
background.

## Installation

If you need to install libpipeline starting from source code, then you will
need these separate packages installed before configuring libpipeline in
order to run its test suite:

 * [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config)
 * [check >= 0.9.10](https://libcheck.github.io/check/)

See the INSTALL file for general installation instructions.

## Using the library

When the author took over [man-db](https://nongnu.org/man-db) in 2001, one
of the major problems that became evident after maintaining it for a while
was the way it handled subprocesses.  The nature of man and friends means
that it spends a lot of time calling sequences of programs such as `zsoelim
< input-file | tbl | nroff -mandoc -Tutf8`.  Back then, it was using C
library facilities such as `system` and `popen` for all this, and there were
several bugs where those functions were being called with untrusted input as
arguments without properly escaping metacharacters.  Of course it was
possible to chase around every such call inserting appropriate escaping
functions, but this was always bound to be error-prone and one of the tasks
that rapidly became important was arranging to start subprocesses in a way
that was fundamentally immune to this kind of bug.

In higher-level languages, there are usually standard constructs which are
safer than just passing a command line to the shell.  For example, in Perl
you can use something like this to invoke a program with arguments without
the interference of the shell:

```perl
system([$command, $arg1, $arg2, ...])
```

[perlipc(1)](https://perldoc.perl.org/perlipc) describes various facilities
for connecting them together.

In Python, the
[subprocess](https://docs.python.org/3/library/subprocess.html) module
allows you to create pipelines easily and safely (as long as you remember
the [SIGPIPE
gotcha](https://www.chiark.greenend.org.uk/~cjwatson/blog/python-sigpipe.html)).

By contrast, C has the `fork` and `execve` primitives, but assembling these
to construct full-blown pipelines correctly is difficult and error-prone, so
many programmers don't bother and use the simple but unsafe library
facilities instead.

libpipeline solves this problem.  In the following examples, function names
starting with `pipecmd_` or `pipeline_` are real functions in the library,
while any other function names are pseudocode.

Constructing the simplified example pipeline from the first paragraph above
using this library looks like this:

```c
pipeline *p;
int status;

p = pipeline_new ();
pipeline_want_infile (p, "input-file");
pipeline_command_args (p, "zsoelim", NULL);
pipeline_command_args (p, "tbl", NULL);
pipeline_command_args (p, "nroff", "-mandoc", "-Tutf8", NULL);
status = pipeline_run (p);
```

You might want to construct a command more dynamically:

```c
pipecmd *manconv = pipecmd_new_args ("manconv", "-f", from_code,
                                     "-t", "UTF-8", NULL);
if (quiet)
	pipecmd_arg (manconv, "-q");
pipeline_command (p, manconv);
```

Perhaps you want an environment variable set only while running a certain
command:

```c
pipecmd *less = pipecmd_new ("less");
pipecmd_setenv (less, "LESSCHARSET", lesscharset);
```

You might find yourself needing to pass the output of one pipeline to
several other pipelines, in a "tee" arrangement:

```c
pipeline *source, *sink1, *sink2;

source = make_source ();
sink1 = make_sink1 ();
sink2 = make_sink2 ();
pipeline_connect (source, sink1, sink2, NULL);
/* Pump data among these pipelines until there's nothing left. */
pipeline_pump (source, sink1, sink2, NULL);
pipeline_free (sink2);
pipeline_free (sink1);
pipeline_free (source);
```

Maybe one of your commands is actually an in-process function, rather than
an external program:

```c
pipecmd *inproc = pipecmd_new_function ("in-process", &func, NULL, NULL);
pipeline_command (p, inproc);
```

Sometimes your program needs to consume the output of a pipeline, rather
than sending it all to some other subprocess:

```c
pipeline *p = make_pipeline ();
const char *line;

pipeline_want_out (p, -1);
pipeline_start (p);
line = pipeline_peekline (p);
if (!strstr (line, "coding: UTF-8"))
	printf ("Unicode text follows:\n");
while (line = pipeline_readline (p))
	printf ("  %s", line);
pipeline_free (p);
```

## Building programs with libpipeline

libpipeline supplies a pkg-config file which lists appropriate compiler and
linker flags for building programs using it.  The output of `pkg-config
--cflags libpipeline` should be passed to the compiler (typically `CFLAGS`)
and the output of `pkg-config --libs libpipeline` should be passed to the
linker (typically `LDFLAGS`).

If your program uses the GNU Autotools, then you can put this in
configure.ac:

```
PKG_CHECK_MODULES([libpipeline], [libpipeline])
```

... and this in the appropriate Makefile.am (replacing 'program' with the
Automake-canonicalised name for your program):

```make
AM_CFLAGS = $(libpipeline_CFLAGS)
program_LDADD = $(libpipeline_LIBS)
```

The details may vary for particular build systems, but this should be a
reasonable start.

When building with GCC, you should use at least the `-Wformat` option
(included in `-Wall`) to ensure that the 'sentinel' function attribute is
checked.  This means that your program will produce a warning if it calls
any of the several libpipeline functions that require a trailing NULL
without passing that trailing NULL.

## Copyright and licensing

Copyright (C) 1994 Markus Armbruster.
Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002, 2003, 2004, 2005,
              2006, 2007, 2008, 2009, 2010
              Free Software Foundation, Inc.
Copyright (C) 2003-2020 Colin Watson.

libpipeline is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

libpipeline is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with libpipeline; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
USA.

## Note on GPL versions

(This note is informative, and if it conflicts with the terms of the licence
then the licence is correct.  See the full text of the licence in the
COPYING file for precise details.)

The core source code of libpipeline is licensed under GPL v2 or later.
However, libpipeline incorporates parts of the Gnulib portability library,
copyrighted by the Free Software Foundation and others, and much of Gnulib
is distributed under GPL v3 or later.  This means that libpipeline as a
whole falls under the terms of the GPL v3 or later.  Unless you take special
pains to remove the GPL v3 portions, you must therefore follow the terms and
conditions of the GPL v3 or later when distributing libpipeline itself, or
distributing code linked against it.

Note that this does not require that your own source code be licensed under
the GPL v3, contrary to popular misunderstanding.  However, you must be
prepared to distribute your work as a whole under the terms of the GPL v3 or
later, which requires that your licence be compatible with the GPL v3.  See
https://www.gnu.org/licenses/license-list.html#GPLCompatibleLicenses if you
need advice on compatibility.

The GPL mainly restricts distribution ("conveying", in the specific language
of GPL v3), and is careful not to restrict private use.  Therefore, you may
write programs for your own use that use libpipeline without needing to
license them under GPL v3-compatible terms.  If you distribute these
programs to others, then you must take care to use compatible licensing.

## Credits

Thanks to Scott James Remnant for code review, Ian Jackson for an extensive
design review, and Kees Cook and Matthias Klose for helpful conversations.

## Bug reporting

You can [report bugs on
GitLab](https://gitlab.com/libpipeline/libpipeline/-/issues), or see [bugs
from before the migration to
GitLab](https://savannah.nongnu.org/bugs/?group=libpipeline).
