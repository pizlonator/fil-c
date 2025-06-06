# Fil-C 0.668.7

Fil-C is a fanatically compatible memory-safe implementation of C and C++. Lots
of software compiles and runs with Fil-C with zero or minimal changes. All
memory safety errors are caught as Fil-C panics. Fil-C achieves this using a
combination of concurrent garbage collection and invisible capabilities (each
pointer in memory has a corresponding capability, not visible to the C address
space). Every fundamental C operation (as seen in LLVM IR) is checked against
the capability. Fil-C has no `unsafe` escape hatch of any kind.

## License

The compiler (clang + LLVM) is covered by LLVM-LICENSE.txt. The runtime is
covered by PAS-LICENSE.txt (see libpas/LICENSE.txt in the source distribution).
The libc is covered by MUSL-LICENSE.txt (see yolomusl/COPYRIGHT and
usermusl/COPYRIGHT in the source distribution). The libc++/libc++abi are
covered by LLVM-LICENSE.txt.

You can fetch the compiler, runtime, libc++/libc++abi source from
https://github.com/pizlonator/llvm-project-deluge.

You can fetch musl source from https://github.com/pizlonator/deluded-musl
(there are two of them - the yolomusl branch for the libc that sits below the
runtime and the usermusl branch for the libc that sits above the runtime).

## Requirements

Fil-C only works on Linux/X86_64.

Previous versions worked on Darwin/ARM64 and FreeBSD, but now I'm focusing just
on Linux/X86_64 because it allows me to do a more faithful job of implementing
libc. There's nothing fundamentally stopping Fil-C from working on ARM or OSes
other than Linux.

## Getting Started

If you downloaded Fil-C binaries, run:

    ./setup.sh

If you downloaded Fil-C source, run:

    ./setup_gits.sh
    ./build_all.sh

Then you'll be able to use Fil-C from within this directory. There is no way to
"install" it, because that would be a crazy thing to do for such an
experimental piece of software!

## Things That Work

Lots of software packages work in Fil-C with zero or minimal changes, including
big ones like openssl, CPython, SQLite, and many others.

Fil-C has full support for C and C++ plus almost all of the extensions that
clang 17 supports. Fil-C has excellent support for atomics and SIMD intrinsics,
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

You can learn more about Fil-C by reading these docs:

- [The Fil-C manifesto](https://github.com/pizlonator/llvm-project-deluge/blob/deluge/Manifesto.md).

- [Releases (Linux/X86_64 binaries)](https://github.com/pizlonator/llvm-project-deluge/releases).

- [Fil-C capabilities by example](https://github.com/pizlonator/llvm-project-deluge/blob/deluge/invisicaps_by_example.md).

- [Garbage-In, Memory Safety Out Semantics](https://github.com/pizlonator/llvm-project-deluge/blob/deluge/gimso_semantics.md).

- [Explanation of Disassembly of a Simple Fil-C Program](https://github.com/pizlonator/llvm-project-deluge/blob/deluge/test43.md).

You can also e-mail me: pizlo@mac.com

Follow me on [Twitter](https://x.com/filpizlo).

File issues at [GH](https://github.com/pizlonator/llvm-project-deluge/issues).

