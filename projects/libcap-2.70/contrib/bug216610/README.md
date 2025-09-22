# Linking psx and C code without cgo

## Overview

In some embedded situations, there is a desire to compile Go binaries
to include some C code, but not `libc` etc. For a long time, I had
assumed this was not possible, since using `cgo` *requires* `libc` and
`libpthread` linkage.

This _embedded compilation_ need was referenced in a [bug
filed](https://bugzilla.kernel.org/show_bug.cgi?id=216610) against the
[`"psx"`](https://pkg.go.dev/kernel.org/pub/linux/libs/security/libcap/psx)
package. The bug-filer was seeking an alternative to `CGO_ENABLED=1`
compilation _requiring_ the `cgo` variant of `psx` build. However, the
go `"runtime"` package will always
[`panic()`](https://cs.opensource.google/go/go/+/refs/tags/go1.19.2:src/runtime/os_linux.go;l=717-720)
if you try this because it needs `libpthread` and `[g]libc` to work.

In researching that bug report, however, I have learned there is a
trick to combining a non-CGO built binary with compiled C code. I
learned about it from a brief reference in the [Go Programming
Language
Wiki](https://zchee.github.io/golang-wiki/GcToolchainTricks/).

This present directory evolved from my attempt to understand and
hopefully resolve what was going on as reported in that bug into an
example of this _trick_. I was unable to resolve the problem as
reported because of the aformentioned `panic()` in the Go
runtime. However, I was able to demonstrate embedding C code in a Go
binary _without_ use of cgo. In such a binary, the Go-native version
of `"psx"` is thus achievable. This is what the example in this
present directory demonstrates.

*Caveat Emptor*: this example is very fragile. The Go team only
supports `cgo` linking against C. That being said, I'd certainly like
to receive bug fixes, etc for this directory if you find you need to
evolve it to make it work for your use case.

## Content

In this example we have:

- Some C code for the functions `fib_init()` and `fib_next()` that
combine to implement a _compute engine_ to determine [Fibonacci
Numbers](https://en.wikipedia.org/wiki/Fibonacci_number). The source
for this is in the sub directory `c/fib.c`.

- Some Go code, in the directory `go/fibber` that uses this C compiled
compute kernel.

- `c/gcc.sh` which is a wrapper for `gcc` that adjusts the compilation
to be digestible by Go's (internal) linker (the one that gets invoked
when compiling `CGO_ENABLED=0`. Using `gcc` directly instead of this
wrapper generates an incomplete binary - which miscomputes the
expected answers. See the discussion below for what seems to be going
on.

- A top level `Makefile` to build it all.

## Building and running the built binary

Set things up with:
```
$ git clone git://git.kernel.org/pub/scm/libs/libcap/libcap.git
$ cd libcap
$ make all
$ cd contrib/bug216610
$ make clean all
```
When you run `./go/fib` it should generate the following output:
```
$ ./go/fib
psx syscall result: PID=<nnnnn>
fib: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...
$
```
Where `<nnnnn>` is the PID of the program at runtime and will be
different each time the program is invoked.

## Discussion

The Fibonacci detail of what is going on is mostly uninteresting. The
reason for developing this example was to explore the build issues in
the reported [Bug
216610](https://bugzilla.kernel.org/show_bug.cgi?id=216610). Ultimately,
this example offers an alternative path to building a `nocgo` program
that links to compute kernel of C code.

The reason we have added the `c/gcc.sh` wrapper for `gcc` is that
we've found the Go linker has a hard time digesting the
cross-sectional `%rip` based data addressing that various optimization
modes of gcc like to use. Specifically, in the x86_64/amd64
architecture, if a `R_X86_64_PC32` relocation entry made in a `.text`
section refers to an `.rodata.cst8` section in a generated `.syso`
file, the Go linker seems to [replace this reference with a `0` offset
to
`(%rip)`](https://github.com/golang/go/issues/24321#issuecomment-1296084103). What
our wrapper script does is rewrite the generated assembly to store
these data references to the `.text` section. The Go linker has no
problem with this _same section_ relative addressing and is able to
link the resulting objects without problems.

If you want to cross compile, we have support for 32-bit arm
compilation: what is needed for the Raspberry PI. To get this support,
try:
```
$ make clean all arms
$ cd go
$ GOARCH=arm CGO_ENABLED=0 go build
```
The generated `fib` binary runs on a 32-bit Raspberry Pi.

## Future thoughts

At present, this example only works on Linux with `x86_64` and `arm`
build architectures. (In go-speak that is `linux_amd64` and
`linux_arm`). This is because I have only provided some bridging
assembly for Go to C calling conventions for those architecture
targets: `./go/fibber/fibs_linux_amd64.s` and
`./go/fibber/fibs_linux_arm.s`. The non-native, `make arms`, cross
compilation requires the `docker` command to be available.

I intend to implement an `arm64` build, when I have a system on which
to test it.

**Note** The Fedora system on which I've been developing this has some
  SELINUX impediment to naively using the `docker -v ...` bind mount
  option. I need the `:z` suffix for bind mounting. I don't know how
  common an issue this is. On Fedora, building the arm variants of the
  .syso file can be performed as follows:
```
$ docker run --rm -v $PWD/c:/shared:z -h debian -u $(id -u) -it expt shared/build.sh
```

## Reporting bugs

Please report issues or offer improvements to this example via the
[Fully Capable `libcap`](https://sites.google.com/site/fullycapable/)
website.
