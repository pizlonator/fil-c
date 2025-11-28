## µTest - A small C testing library

 - **Linux**: [![Build Status](https://travis-ci.org/ebassi/mutest.svg?branch=master)](https://travis-ci.org/ebassi/mutest)
 - **Windows**: [![Build status](https://ci.appveyor.com/api/projects/status/1ghtdpt42u3vy8s9/branch/master?svg=true)](https://ci.appveyor.com/project/ebassi/mutest/branch/master)
 - [![Coverage Status](https://coveralls.io/repos/github/ebassi/mutest/badge.svg?branch=master)](https://coveralls.io/github/ebassi/mutest?branch=master)

## What is µTest

µTest aims to be a small unit testing library for C projects, with an API
heavily modelled on high level [Behavior-Driver Development][bdd] frameworks
like [Jasmine][jasminejs] or [Mocha][mochajs].

µTest is written in C99, and can be used on multiple platforms.

### Features

 - Small API footprint
 - Easy to use
 - Portable
 - Zero dependencies
 - Supports [TAP](https://testanything.org)

### Screenshot

![µTest on Linux](./mutest-mocha-format.png)

![µTest on Windows, using MSYS2](./mutest-msys2.png)

[bdd]: https://en.wikipedia.org/wiki/Behavior-driven_development
[jasminejs]: https://jasmine.github.io/
[mochajs]: https://mochajs.org/

**WARNING**: µTest's API is still in flux!

---

## Getting Started

### Building µTest

Simply clone this repository, and use [Meson](http://mesonbuild.com) to
generate the appropriate build instructions:

```sh
$ git clone https://github.com/ebassi/mutest.git
$ cd mutest
$ meson _build .
$ cd _build
$ ninja
$ meson test
$ sudo ninja install
```

### Using µTest

The documentation of µTest is [available online](https://ebassi.github.io/mutest/mutest.md.html).

### Contributing

See the [Contribution guide](./CONTRIBUTING.md).

## License

µTest is released under the terms of the [MIT](./LICENSE.txt) license.
