# Fil-C 0.674

Fil-C is a fanatically compatible memory-safe implementation of C and C++. Lots
of software compiles and runs with Fil-C with zero or minimal changes. All
memory safety errors are caught as Fil-C panics. Fil-C achieves this using a
combination of concurrent garbage collection and invisible capabilities (each
pointer in memory has a corresponding capability, not visible to the C address
space). Every fundamental C operation (as seen in LLVM IR) is checked against
the capability. Fil-C has no `unsafe` statement and only limited FFI to unsafe
code.

Fil-C is special because:

- Fil-C achieves full safety with no escape hatches. There is no `unsafe`
  keyword in Fil-C that could be used to turn off protections. Linking to unsafe
  code is severely restricted.

- Fil-C's capability-based approach achieves a similar level of safety to
  hardware capabilities like [CHERI](https://www.cl.cam.ac.uk/research/security/ctsrd/cheri/),
  except that it runs on stock hardware (X86_64, currently).

- Fil-C is engineered to prevent memory safety bugs from being used for
  exploitation rather than just simply flagging them often enough to find bugs.
  This makes Fil-C different from [AddressSanitizer](https://github.com/google/sanitizers/wiki/addresssanitizer),
  HWAsan, or [MTE](https://developer.arm.com/documentation/108035/0100/Introduction-to-the-Memory-Tagging-Extension),
  which can all be bypassed by attackers. The key difference that makes this
  possible is that Fil-C is capability based (so each pointer knows what range
  of memory it may access, and how it may access it) rather than tag based
  (where pointer accesses are allowed if they hit valid memory).

- From a language user standpoint, Fil-C is just C and C++ with GCC/clang
  extensions. It's more likely than not that your favorite C or C++ program or
  library compiles in Fil-C with zero changes. The Fil-C compiler is based on
  clang 20.1.8, so it supports C17 and C++20.

## License

The compiler (clang + LLVM) is covered by LLVM-LICENSE.txt. The runtime is
covered by PAS-LICENSE.txt (see libpas/LICENSE.txt in the source distribution).
In the case of the classic musl-based Fil-C distribution, the musl libc is
covered by MUSL-LICENSE.txt (see projects/yolomusl/COPYRIGHT and
projects/usermusl/COPYRIGHT in the source distribution). In the case of the
/opt/fil distribution, glibc is covered by glibc-LICENSE.txt, and all other
included programs are covered by the respective <program-name>-LICENSE.txt
files. The C++ libraries (libc++/libc++abi) are covered by LLVM-LICENSE.txt.

You can fetch the source for the compiler, runtime, libc++/libc++abi, libc
(musl and glibc), and all included programs from
https://github.com/pizlonator/fil-c. The source distribution also includes
many additional programs that have been ported to Fil-C in the projects/
and pizlix/ directories, and they have a variety of licenses.

## Requirements

Fil-C only works on Linux/X86_64.

Previous versions worked on Darwin/ARM64 and FreeBSD, but now I'm focusing just
on Linux/X86_64 because it allows me to do a more faithful job of implementing
libc. There's nothing fundamentally stopping Fil-C from working on ARM or OSes
other than Linux.

## Getting Started

If you downloaded Fil-C binaries, run:

    ./setup.sh

This has a different effect depending on which binary distribution you
selected:

- In case of the classic musl-based distribution
  (`filc-0.674-linux-x86_64.tar.xz`), this sets up Fil-C to run in the current
  directory.

- In case of the /opt/fil glibc-based distribution
  (`optfil-0.674-linux-x86_64.tar.xz`), this sets up Fil-C in `/opt/fil`.

If you downloaded Fil-C source, run:

    ./build_all_fast.sh

Then you'll be able to use Fil-C from within this directory.

The binary distribution of Fil-C comes with musl as the libc. Using
`./build_all_fast.sh` in the source distribution also builds Fil-C using musl.
If you are using source, then you can also:

- `./build_all_fast_glibc.sh` - builds a similar setup but with glibc 2.40 as
  the libc.

- `./build_all.sh` - full musl-based build (also builds lots of software that
  was ported to Fil-C).

- `./build_all_glibc.sh` - full glibc-based build (builds even more software
  that was ported to Fil-C).

- `cd pizlix && sudo ./build.sh` - builds the [Pizlix](https://fil-c.org/pizlix)
  Linux distribution.

- `cd optfil && sudo ./build.sh` - builds the `/opt/fil` distribution.

## Things That Work

Lots of software packages work in Fil-C with zero or minimal changes, including
big ones like openssl, CPython, SQLite, and [many others](https://fil-c.org/programs_that_work).
Fil-C is powerful enough to support a [fully memory safe Linux userland](https://fil-c.org/pizlix).

Fil-C has full support for C and C++ plus almost all of the extensions that
clang 20 supports. Fil-C has excellent support for atomics and SIMD intrinsics,
for example.

Fil-C catches all of the stuff that makes memory safety in C hard, like:

- Out-of-bounds on the heap or stack.

- Use-after free (also heap or stack).

- Type confusion between pointers and non-pointers.

- Type errors arising from linking.

- Type errors arising from misuse of va_lists.

- Pointer races.

- System calls. All buffers passed to system calls are checked for bounds and
  type.

- Lots of other stuff.

Fil-C comes with a reasonably complete POSIX libc and even supports tricky
features like threads, signal handling, `mmap`/`munmap`, `longjmp`/`setjmp`,
and C++ exceptions.

## Learn More

You can learn more about Fil-C by [visiting the website](https://fil-c.org/).

You can also e-mail me: pizlo@mac.com

Follow me on [Twitter](https://x.com/filpizlo).

File issues at [GH](https://github.com/pizlonator/fil-c/issues).

