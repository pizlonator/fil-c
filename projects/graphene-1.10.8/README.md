# Graphene
###  A thin layer of types for graphic libraries

![Linux Build](https://github.com/ebassi/graphene/workflows/Ubuntu%20Build/badge.svg)
![MSVC Build](https://github.com/ebassi/graphene/workflows/MSVC%20Build/badge.svg)
![MSYS2 Build](https://github.com/ebassi/graphene/workflows/MSYS2%20Build/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/ebassi/graphene/badge.svg?branch=master)](https://coveralls.io/github/ebassi/graphene?branch=master)
[![License: MIT](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

When creating graphic libraries you most likely end up dealing with points
and rectangles. If you're particularly unlucky, you may end up dealing
with affine matrices and 2D transformations. If you're writing a graphic
library with 3D transformations, though, you are going to hit the jackpot:
4x4 matrices, projections, transformations, vectors, and quaternions.

Most of this stuff exists, in various forms, in other libraries, but it
has the major drawback of coming along with the rest of those libraries,
which may or may not be what you want. Those libraries are also available
in various languages, as long as those languages are C++; again, it may or
may not be something you want.

For this reason, I decided to write the thinnest, smallest possible layer
needed to write a canvas library; given its relative size, and the
propensity for graphics libraries to have a pun in their name, I decided
to call it Graphene.

This library provides types and their relative API; it does not deal with
windowing system surfaces, drawing, scene graphs, or input. You're
supposed to do that yourself, in your own canvas implementation, which is
the whole point of writing the library in the first place.

### Dependencies

Graphene has minimal dependencies.

Graphene contains optimizations for speeding up vector operations; those
optimizations are optional, and used only if both Graphene was compiled
with support for them *and* if the system you're running on has them.
Currently, Graphene supports the following platform-specific fast paths:

 * Streaming SIMD Extensions (SSE) 2
  * Optionally using SSE 4.1
 * ARM NEON
 * GCC vector extensions

In the remote case in which none of these optimizations are available,
Graphene will fall back to a naive scalar implementation.

Graphene can, optionally, provide types for integrating with
[GObject][gobject-api] properties and signals, as well as introspection
information for its use with other languages through introspection-based
bindings.

### Installation

In order to build and install Graphene you will need development tools and
the headers of the dependencies. You will also need:

 * [python3](https://www.python.org)
 * [meson](http://mesonbuild.com)
 * [ninja](https://ninja-build.org/)

First of all, clone the Git repository:

    $ git clone https://github.com/ebassi/graphene
    $ cd graphene

Then run:

    $ meson _build    # on Windows, it may be "meson.py"
    $ cd _build
    $ ninja test
    # ninja install

It is possible, when building Graphene, to disable specific optimizations by
using configuration options:

 * `-Dsse2=false` - will disable the SSE2 fast paths
 * `-Darm_neon=false` - will disable the ARM NEON fast paths
 * `-Dgcc_vector=false` - will disable the GCC vector intrinsics

If you don't plan on generating introspection data, use `-Dintrospection=disabled`
when configuring Graphene; similarly, if you don't plan on using GObject with
Graphene, use `-Dgobject_types=false`. Disabling GObject types will also
automatically disable generating introspection data.

You can explicitly disable building the test suite and the benchmark suite,
using the `-Dtests=false` and `-Dbenchmarks=false` configuration switches
respectively. The tests suite depends on [µTest][mutest]; if it is not available
at configuration time, tests will be disabled automatically.

#### Building on Windows

Graphene supports the Microsoft Visual C compiler 2017 and later versions.

Graphene also supports the [MSYS2 toolchain](http://sourceforge.net/projects/msys2/).

When using MSYS2, it's recommended to have an up to date installation;
in order to build Graphene you will need to use the `pacman` command
to install the necessary build dependencies first:

    $ pacman -S base-devel
    $ pacman -S python3
    $ pacman -S mingw-w64-x86_64-meson	# only MINGW64 target
    $ pacman -S mingw-w64-i686-meson	# only MINGW32 target

For the optional support for GObject types, introspection, and
documentation, you will need to install additional dependencies:

    $ pacman -S gtk-doc                 # optional
    $ pacman -S mingw-w64-x86_64-glib2  # optional, MINGW64 target only
    $ pacman -S mingw-w64-i686-glib2    # optional, MINGW32 target only
    $ pacman -S glib2 glib2-devel       # optional, MSYS target only

After installing all dependencies, you can now clone the Graphene
repository locally, and follow the build instructions above.

Please note that on some MSYS2 installations the Meson binary may be called
`meson.py`.

## Documentation

### Release notes

The release notes are available on the Graphene
[wiki](https://github.com/ebassi/graphene/wiki/Release-Notes).

### Available types

Graphene provides common types needed to handle 3D transformations:

 * [2D points](http://ebassi.github.io/graphene/docs/graphene-Point.html)
 * [3D points](http://ebassi.github.io/graphene/docs/graphene-Point3D.html)
 * [triangles](http://ebassi.github.io/graphene/docs/graphene-Triangle.html)
 * [rectangles](http://ebassi.github.io/graphene/docs/graphene-Rectangle.html)
 * [quads](http://ebassi.github.io/graphene/docs/graphene-Quad.html)
 * [quaternions](http://ebassi.github.io/graphene/docs/graphene-Quaternion.html)
 * [euler angles](http://ebassi.github.io/graphene/docs/graphene-Euler.html)
 * [vectors](http://ebassi.github.io/graphene/docs/graphene-Vectors.html) (2, 3, or 4-sized)
 * [matrices](http://ebassi.github.io/graphene/docs/graphene-Matrix.html)
 * [planes](http://ebassi.github.io/graphene/docs/graphene-Plane.html)
 * [axis aligned bounding boxes](http://ebassi.github.io/graphene/docs/graphene-Box.html)
 * [spheres](http://ebassi.github.io/graphene/docs/graphene-Sphere.html)
 * [frustums](http://ebassi.github.io/graphene/docs/graphene-Frustum.html)

Graphene also provides its low-level SIMD [vector](http://ebassi.github.io/graphene/docs/graphene-SIMD-vector.html)
and [matrix](http://ebassi.github.io/graphene/docs/graphene-SIMD-matrix.html)
types, which are used to implement the API above.

All types can be placed on the stack, but provide allocation/free functions
for working on the heap as well. The contents of all structure types, unless
noted otherwise, should be considered private, and should never be accessed
directly.

The full API reference for Graphene is [available online](http://ebassi.github.io/graphene/docs/).

### License

Graphene is released under the terms of the MIT/X11 license.

See the [license file](./LICENSE.txt) for more details.

[mutest]: https://github.com/ebassi/mutest
[gobject-api]: https://developer.gnome.org/gobject/stable/
