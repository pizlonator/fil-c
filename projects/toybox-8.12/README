Toybox: all-in-one Linux command line.

--- Getting started

You can download static binaries for various targets from:

  http://landley.net/toybox/bin

The special name "." indicates the current directory (just like ".." means
the parent directory), and you can run a program that isn't in the $PATH by
specifying a path to it, so this should work:

  wget http://landley.net/toybox/bin/toybox-x86_64
  chmod +x toybox-x86_64
  ./toybox-x86_64 echo hello world

--- Building toybox

Type "make help" for build instructions.

Toybox uses the "make menuconfig; make; make install" idiom same as
the Linux kernel. Usually you want something like:

  make defconfig
  make
  make install

Or maybe:

  LDFLAGS="--static" CROSS_COMPILE=armv5l- make defconfig toybox
  PREFIX=/path/to/root/filesystem/bin make install_flat

The file "configure" defines default values for many environment variables
that control the toybox build; if you export any of these variables into your
environment, your value is used instead of the default in that file.

The CROSS_COMPILE argument above is optional, the default builds a version of
toybox to run on the current machine. Cross compiling requires an appropriately
prefixed cross compiler toolchain, several example toolchains (built using
the file "scripts/mcm-buildall.sh" in the toybox source) are available at:

  https://landley.net/toybox/downloads/binaries/toolchains/latest

For the "CROSS_COMPILE=armv5l-" example above, download
armv5l-linux-musleabihf-cross.tar.xz, extract it, and add its "bin"
subdirectory to your $PATH. (And yes, the trailing - is significant,
because the prefix includes a dash.)

For more about cross compiling, see:

  https://landley.net/toybox/faq.html#cross
  http://landley.net/writing/docs/cross-compiling.html
  http://landley.net/aboriginal/architectures.html

For a more thorough description of the toybox build process, see:

  http://landley.net/toybox/code.html#building

--- Using toybox

The toybox build produces a multicall binary, a "swiss-army-knife" program
that acts differently depending on the name it was called by (cp, mv, cat...).
Installing toybox adds symlinks for each command name to the $PATH.

The special "toybox" command treats its first argument as the command to run.
With no arguments, it lists available commands. This allows you to use toybox
without installing it, and is the only command that can have an arbitrary
suffix (hence "toybox-armv5l").

The "help" command provides information about each command (ala "help cat"),
and "help toybox" provides general information about toybox.

--- Configuring toybox

It works like the Linux kernel: allnoconfig, defconfig, and menuconfig edit
a ".config" file that selects which features to include in the resulting
binary. You can save and re-use your .config file, but may want to
run "make oldconfig" to re-run the dependency resolver when migrating to
new versions.

The maximum sane configuration is "make defconfig": allyesconfig isn't
recommended as a starting point for toybox because it enables unfinished
commands, debug code, and optional dependencies your build environment may
not provide.

--- Creating a Toybox-based Linux system

Toybox has a built-in simple system builder (scripts/mkroot.sh) with a
Makefile target:

  make root
  sudo chroot root/host/fs /init

Type "exit" to get back out. If you install appropriate cross compilers and
point it at Linux source code, it can build simple three-package systems
that boot to a shell prompt under qemu:

  make root CROSS_COMPILE=sh4-linux-musl- LINUX=~/linux
  cd root/sh4
  ./qemu-sh4.sh

By calling scripts/mkroot.sh directly you can add additional packages
to the build, see scripts/root/dropbear as an example.

The FAQ explains this in a lot more detail:

  https://landley.net/toybox/faq.html#system
  https://landley.net/toybox/faq.html#mkroot

--- Presentations

1) "Why Toybox?" talk at the Embedded Linux Conference in 2013

    outline: http://landley.net/talks/celf-2013.txt
    video: http://youtu.be/SGmtP5Lg_t0

    The https://landley.net/toybox/about.html page has nav links breaking that
    talk down into sections.

2) "Why Public Domain?" The rise and fall of copyleft, Ohio LinuxFest 2013

    outline: http://landley.net/talks/ohio-2013.txt
    audio: https://archive.org/download/OhioLinuxfest2013/24-Rob_Landley-The_Rise_and_Fall_of_Copyleft.mp3

3) Why did I do Aboriginal Linux (which led me here)

    260 slide presentation:
      https://speakerdeck.com/landley/developing-for-non-x86-targets-using-qemu

    How and why to make android self-hosting:
      http://landley.net/aboriginal/about.html#selfhost

    More backstory than strictly necessary:
      https://landley.net/aboriginal/history.html

4) What's new with toybox (ELC 2015 status update):

    video: http://elinux.org/ELC_2015_Presentations
    outline: http://landley.net/talks/celf-2015.txt

5) Toybox vs BusyBox (2019 ELC talk):

    outline: http://landley.net/talks/elc-2019.txt
    video: https://www.youtube.com/watch?v=MkJkyMuBm3g

--- Contributing

The three important URLs for communicating with the toybox project are:

  web page: http://landley.net/toybox

  mailing list: http://lists.landley.net/listinfo.cgi/toybox-landley.net

  git repo: http://github.com/landley/toybox

The maintainer prefers patches be sent to the mailing list. If you use git,
the easy thing to do is:

  git format-patch -1 $HASH

Then send a file attachment. The list holds messages from non-subscribers
for moderation, but I usually get to them in a day or two.

I download github pull requests as patches and apply them with "git am"
(which avoids gratuitous merge commits). Sometimes I even remember to close
the pull request.

If I haven't responded to your patch after one week, feel free to remind
me of it.

Android's policy for toybox patches is that non-build patches should go
upstream first (into vanilla toybox, with discussion on the toybox mailing
list) and then be pulled into android's toybox repo from there. (They
generally resync on fridays). The exception is patches to their build scripts
(Android.mk and the checked-in generated/* files) which go directly to AOSP.

(As for the other meaning of "contributing", https://patreon.com/landley is
always welcome but I warn you up front I'm terrible about updating it.)
