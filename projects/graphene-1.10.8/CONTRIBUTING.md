# Contributing to Graphene

Thank you for considering contributing to Graphene!

These guidelines are meant for new contributors, regardless of their level
of proficiency; following them allows the maintainer of Graphene to more
effectively evaluate your contribution, and provide prompt feedback to
you. Additionally, by following these guidelines you clearly communicate
that you respect the time and effort that the people developing Graphene
put into managing the project.

Graphene is a small library, developed on a volunteer basis, and it would
not exist without contributions from the free and open source software
community. There are many things that we value:

 - bug reporting and fixing
 - documentation and examples
 - tests
 - new features

Please, do not use the issue tracker for support questions. If you have
questions on how to use Graphene effectively, you can use:

 - the `#gtk` IRC channel on `irc.gnome.org`
 - [StackOverflow](https://stackoverflow.com)

The issue tracker is meant to be used for actionable issues only.

## How to report an issue

If you're reporting a bug make sure to list:

 - which version of Graphene are you using?
 - which operating system (Linux, Windows, macOS, etc) are you using?
 - which architecture (Intel, ARM, etc) are you using?
 - the necessary steps to reproduce the issue
 - the expected outcome
 - a description of the behavior
 - a small, self-contained example exhibiting the behavior; if this
   is not possible, you can modify the Graphene test suite

If the issue includes a crash, you should also include:

 - the eventual warnings printed on the terminal
 - a backtrace, obtained with tools such as GDB or LLDB

For small issues, such as:

 - spelling/grammar fixes in the documentation
 - typos correction
 - comment clean ups
 - changes to metadata files (CI, .gitignore)
 - build system changes
 - source tree clean ups and reorganizations

You should directly open a merge request instead of filing a new issue.

## Features and enhancements

Feature discussion can be open ended and require high bandwidth channels; if
you are proposing a new feature on the issue tracker, make sure to make
an actionable proposal, and list:

 - what you're trying to achieve
 - prior art, in other similar libraries

If you're proposing the integration of new features it helps to have
multiple applications using shared or similar code, especially if they have
iterated over it various times.

Each feature should also come fully documented, and with tests.

## Your first contribution

### Prerequisites

If you want to contribute to Graphene, you will need to have the development
tools appropriate for your operating system, including:

 - Python 3.x
 - [Meson](http://mesonbuild.com)
 - [Ninja](https://ninja-build.org/)
 - a C99 compatible compiler

In order to build the tests you will need [µTest](https://github.com/ebassi/mutest);
Graphene can automatically download and build µTest as a sub-project, so you
don't need a system installation for it.

If you want to build the documentation of Graphene, you will need to install
[GTK-Doc](https://gitlab.gnome.org/GNOME/gtk-doc).

### Getting started

You should start by forking the GTK repository from the GitLab web UI, and
cloning from your fork:

```
$ git clone https://github.com/youruser/graphene.git
$ cd graphene
```

Typically, you should work on your own branch:

```
$ git checkout -b your-issue
```

To compile the Graphene on your system, you will need to configure your build
using Meson:

```
$ meson _builddir .
$ cd _builddir
$ ninja
```

Once you've finished working on the bug fix or feature, push the branch
to your Git repository and open a new pull request.

Make sure that your contribution follows [the Graphene coding style guidelines](./doc/CODING_STYLE.md).
Contributions that do not follow the coding style will be rejected; as Graphene
is maintained on a strictly volunteer basis, doing coding style fixes eats a
lot of code reviewing time.

### Commit messages

The expected format for git commit messages is as follows:

```
Short explanation of the commit

Longer explanation explaining exactly what's changed, whether any
external or private interfaces changed, what bugs were fixed (with bug
tracker reference if applicable) and so forth. Be concise but not too
brief.

Closes #1234
```

 - Always add a brief description of the commit to the first line of
   the commit and terminate by two newlines (it will work without the
   second newline, but that is not nice for the interfaces).

 - The first line (the brief description) must only be one sentence and
   should start with a capital letter unless it starts with a lowercase
   symbol or identifier. Don't use a trailing period either. Don't exceed
   72 characters.

 - The main description (the body) is normal prose and should use normal
   punctuation and capital letters where appropriate. Consider the commit
   message as an email sent to the developers (or yourself, six months
   down the line) detailing why you changed something. There's no need
   to specify the how: the changes can be inlined.
   
 - Use the [issue closing syntax](https://help.github.com/en/articles/closing-issues-using-keywords)
   in your commit message if you're fixing a specific issue in it.

 - When committing code on behalf of others use the `--author` option, e.g.
   `git commit -a --author "Joe Coder <joe@coder.org>"`.

 - If you have a pull request with multiple commits and none of them
   completely fixes an issue, you should add a reference to the issue in
   the commit message, e.g. `Issue: #1234`, and use the automatic issue
   closing syntax `Fixes: #1234` in the description of the pull request.
