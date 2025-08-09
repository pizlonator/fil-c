#!/bin/bash

# Script to build all cross and native compilers supported by musl-libc.
# This isn't directly used by toybox, but is useful for testing.

trap "exit 1" INT

if [ ! -d litecross ]
then
  echo Run this script in musl-cross-make directory to make "ccc" directory.
  echo
  echo "  "git clone https://github.com/richfelker/musl-cross-make
  echo "  "cd musl-cross-make
  echo '  ~/toybox/scripts/mcm-buildall.sh'
  exit 1
fi

# List of known targets. The format is TARGET[@RENAME]:EXTRA:CONFIG resulting
# in a gcc tuple TARGET-linux-muslEXTRA with CONFIG appended to $GCC_CONFIG
# (and thus the gcc ./configure command line). So i686:: builds i686-linux-musl
# and sh2eb::fdpic:--with-cpu=mj2 builds sh2eb-linux-muslfdpic adding
# --with-cpu=mj2 to the gcc ./configure command line. @RENAME (if present)
# changes the TARGET part of the toolchain prefix names afterwards.

TARGETS=(i686:: aarch64:eabi:
  armv4l:eabihf:"--with-arch=armv5t --with-float=soft"
  "armv5l:eabihf:--with-arch=armv5t --with-fpu=vfpv2 --with-float=hard"
  "armv7l:eabihf:--with-arch=armv7-a --with-fpu=vfpv3-d16 --with-float=hard"
  "armv7m:eabi:--with-arch=armv7-m --with-mode=thumb --disable-libatomic --enable-default-pie"
  armv7r:eabihf:"--with-arch=armv7-r --enable-default-pie"
  i486:: m68k:: microblaze:: mips:: mips64:: mipsel:: or1k:: powerpc::
  powerpc64:: powerpc64le:: riscv32:: riscv64:: s390x::
  sh2eb:fdpic:--with-cpu=mj2
  sh4::--enable-incomplete-targets sh4eb::--enable-incomplete-targets
  x86_64::--with-mtune=nocona x86_64@x32:x32:
)

# All toolchains after the first are themselves cross compiled (so they
# can be statically linked against musl on the host, for binary portability.)
: ${BOOTSTRAP:=$(uname -m)} ${OUTPUT:="$PWD/ccc"}

# Move the corresponding target to the front so rest of targets get built
# with full instead of partial -host compiler (missing threads and NLS and such)
unset TEMP
for i in $(seq 0 $((${#TARGETS[@]}-1)));
do
  [ "${TARGETS[$i]}" == "${TARGETS[$i]#"$BOOTSTRAP:"}" ] && continue
  TEMP="${TARGETS[$i]}"
  TARGETS[$i]="$TARGETS[0]"
  TARGETS[0]="$TEMP"
  break;
done
[ -z "$TEMP" ] && { echo unknown target "$BOOTSTRAP"; exit 1; }

if [ "$1" == clean ]
then
  rm -rf ccc host-* *.log # Not gonna rm -rf "$OUTPUT" blindly, could be $HOME
  make clean
  exit
fi

make_toolchain()
{

  # Set cross compiler path
  LP="$PATH"
  if [ "$TYPE" == host ]
  then
    OUTPUT="$PWD/host-$TARGET"
    EXTRASUB=y
  else
    if [ "$TYPE" == static ]
    then
      HOST=$BOOTSTRAP
      [ "$TARGET" = "$HOST" ] && LP="$PWD/host-$HOST/bin:$LP" &&
        GCC_CONFIG="--build=${TARGET%%-*}-donotmatch-linux $GCC_CONFIG"
      TYPE=cross
      EXTRASUB=y
      LP="$OUTPUT/$HOST-cross/bin:$LP"
    else
      HOST="$TARGET"
      export NATIVE=y
      LP="$OUTPUT/${RENAME:-$TARGET}-cross/bin:$LP"
      [ -z "$(PATH="$LP" which $TARGET-cc)" ] &&
         echo "no $TARGET-cc in $LP" && return
    fi
    COMMON_CONFIG="CC=\"$HOST-gcc -static --static\" CXX=\"$HOST-g++ -static --static\""
    export -n HOST
    OUTPUT="$OUTPUT/${RENAME:-$TARGET}-$TYPE"
  fi

  # Skip outputs that already exist
  if [ -e "$OUTPUT.sqf" ] || [ -e "$OUTPUT/bin/$TARGET-cc" ] ||
     [ -e "$OUTPUT/bin/cc" ]
  then
    return
  fi

  # Change title bar to say what we're currently building

  echo === building $TARGET-$TYPE
  echo -en "\033]2;$TARGET-$TYPE\007"

  rm -rf build/"$TARGET" "$OUTPUT" &&
  set -x &&
  PATH="$LP" make OUTPUT="$OUTPUT" TARGET="$TARGET" \
    GCC_CONFIG="--disable-nls --disable-libquadmath --disable-decimal-float --disable-multilib --enable-languages=c,c++ $GCC_CONFIG" \
    COMMON_CONFIG="CFLAGS=\"$CFLAGS -g0 -O2\" CXXFLAGS=\"$CXXFLAGS -g0 -O2\" \
    LDFLAGS=\"$LDFLAGS -s\" $COMMON_CONFIG" install -j$(nproc) || exit 1
  set +x
  echo -e '#ifndef __MUSL__\n#define __MUSL__ 1\n#endif' \
    >> "$OUTPUT/${EXTRASUB:+$TARGET/}include/features.h"

  if [ ! -z "$RENAME" ] && [ "$TYPE" == cross ]
  then
    CONTEXT="output/$RENAME-cross/bin"
    for i in "$CONTEXT/$TARGET-"*
    do
      X="$(echo $i | sed "s@.*/$TARGET-\([^-]*\)@\1@")"
      ln -sf "$TARGET-$X" "$CONTEXT/$RENAME-$X"
    done
  fi

  # Prevent cross compiler reusing dynamically linked host build files for
  # $BOOTSTRAP arch
  [ -z "$TYPE" ] && {
    [ -e musl-git-master ] && mv musl-git-master keep-this-dir
    make clean
    [ -e keep-this-dir ] && mv keep-this-dir musl-git-master
  }

  if [ "$TYPE" == native ]
  then
    # gcc looks in "../usr/include" but not "/bin/../include" (relative to the
    # executable). That means /usr/bin/gcc looks in /usr/usr/include, so that's
    # not a fix either. So add a NOP symlink as a workaround for The Crazy.
    ln -s . "$OUTPUT/usr" || exit 1
    [ ! -z "$(which mksquashfs 2>/dev/null)" ] &&
      mksquashfs "$OUTPUT" "$OUTPUT.sqf" -all-root &&
      [ -z "$CLEANUP" ] && rm -rf "$OUTPUT"
  fi
}

split_tuple()
{
  PART1=${1/:*/}
  PART3=${1/*:/}
  PART2=${1:$((${#PART1}+1)):$((${#1}-${#PART3}-${#PART1}-2))}

  # Do we need to rename this toolchain after building it?
  RENAME=${PART1/*@/}
  [ "$RENAME" == "$PART1" ] && RENAME=
  PART1=${PART1/@*/}
  TARGET=${PART1}-linux-musl${PART2} 
}

# Expand compressed target into binutils/gcc "tuple" and call make_toolchain
make_tuple()
{
  split_tuple "$@"

  [ -z "$NOCLEAN" ] && rm -rf build

  for TYPE in "${@:2}"
  do
    TYPE=$TYPE TARGET=$TARGET GCC_CONFIG="$PART3" RENAME="$RENAME" \
      make_toolchain 2>&1 | tee "$OUTPUT"/log/${RENAME:-$PART1}-${TYPE}.log
  done
}

patch_mcm()
{
  # musl-cross-make commit fe915821b652 has been current for a year and a half,
  # and doesn't even use the latest musl release by default, so fix it up.

  # Select newer package versions and don't use dodgy mirrors
  sed -i 's/mirror//;s/\(LINUX_VER =\).*/\1 6.8/;s/\(GCC_VER =\).*/\1 11.2.0/' \
    Makefile &&
  echo 'c9ab5b95c0b8e9e41290d3fc53b4e5cb2e8abb75  linux-6.8.tar.xz' > \
    hashes/linux-6.8.tar.xz.sha1 &&
  # mcm redundantly downloads tarball if hash file has newer timestamp,
  # and it whack-a-moles how to download kernels by version for some reason.
  touch -d @1 hashes/linux-6.8.tar.xz.sha1 &&
  sed -i 's/\(.*linux-\)3\(.*\)v3.x/\16\2v6.x/' Makefile &&

  # nommu toolchains need to vfork()+pipe, and or1k has different kernel arch
  sed -i -e 's/--enable-fdpic$/& --enable-twoprocess/' \
      -e '/or1k/!s/^\(TARGET_ARCH_MANGLED = \)\(.*\)/\1$(patsubst or1k,openrisc,\2)/' \
      litecross/Makefile &&

  # Packages detect nommu via the absence of fork(). Musl provides a broken
  # fork() on nommu builds that always returns -ENOSYS at runtime. Rip it out.
  # (Currently only for superh/jcore since no generic #ifdef __FDPIC__ symbol.)

  PP=patches/musl-"$(sed -n 's/MUSL_VER[ \t]*=[ \t]*//p' Makefile)" &&
  mkdir -p "$PP" &&
  cat > "$PP"/0001-nommu.patch << 'EOF' &&
--- a/src/legacy/daemon.c
+++ b/src/legacy/daemon.c
@@ -17,3 +17,3 @@
 
-	switch(fork()) {
+	switch(vfork()) {
 	case 0: break;
@@ -25,3 +25,3 @@
 
-	switch(fork()) {
+	switch(vfork()) {
 	case 0: break;
--- a/src/misc/forkpty.c
+++ b/src/misc/forkpty.c
@@ -8,2 +8,3 @@
 
+#ifndef __SH_FDPIC__
 int forkpty(int *pm, char *name, const struct termios *tio, const struct winsize *ws)
@@ -57,1 +58,2 @@
 }
+#endif
--- a/src/misc/wordexp.c
+++ b/src/misc/wordexp.c
@@ -25,2 +25,3 @@
 
+#ifndef __SH_FDPIC__
 static int do_wordexp(const char *s, wordexp_t *we, int flags)
@@ -177,2 +178,3 @@
 }
+#endif
 
--- a/src/process/fork.c
+++ b/src/process/fork.c
@@ -7,2 +7,3 @@
 
+#ifndef __SH_FDPIC__
 static void dummy(int x)
@@ -37,1 +38,2 @@
 }
+#endif
--- a/Makefile
+++ b/Makefile
@@ -100,3 +100,3 @@
 	cp $< $@
-	sed -n -e s/__NR_/SYS_/p < $< >> $@
+	sed -e s/__NR_/SYS_/ < $< >> $@
 
--- a/arch/sh/bits/syscall.h.in
+++ b/arch/sh/bits/syscall.h.in
@@ -2,3 +2,5 @@
 #define __NR_exit                   1
+#ifndef __SH_FDPIC__
 #define __NR_fork                   2
+#endif
 #define __NR_read                   3
EOF

  # I won't sign the FSF's copyright assignment
  tee $(for i in patches/gcc-*; do echo $i/099-vfork.patch; done) > /dev/null << 'EOF'
--- gcc-8.3.0/fixincludes/procopen.c	2005-08-14 19:50:43.000000000 -0500
+++ gcc-bak/fixincludes/procopen.c	2020-02-06 23:27:15.408071708 -0600
@@ -116,3 +116,3 @@
    */
-  ch_id = fork ();
+  ch_id = vfork ();
   switch (ch_id)
EOF

  # Fix a gcc bug that breaks x86-64 build in gcc 11.2.0,
  # from https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100017#c20

  PP=patches/gcc-"$(sed -n 's/GCC_VER[ \t]*=[ \t]*//p' Makefile)" &&
  mkdir -p "$PP" &&
  cat > "$PP"/0006-fixinc.patch << 'EOF' &&
diff -ruN gcc-11.2.0.orig/configure gcc-11.2.0/configure
--- gcc-11.2.0.orig/configure	2021-07-28 01:55:06.628278148 -0500
+++ gcc-11.2.0/configure	2023-11-17 03:07:53.819283027 -0600
@@ -16478,7 +16478,7 @@
 fi
 
 
-RAW_CXX_FOR_TARGET="$CXX_FOR_TARGET"
+RAW_CXX_FOR_TARGET="$CXX_FOR_TARGET -nostdinc++"
 
 { $as_echo "$as_me:${as_lineno-$LINENO}: checking where to find the target ar" >&5
 $as_echo_n "checking where to find the target ar... " >&6; }
diff -ruN gcc-11.2.0.orig/configure.ac gcc-11.2.0/configure.ac
--- gcc-11.2.0.orig/configure.ac	2021-07-28 01:55:06.628278148 -0500
+++ gcc-11.2.0/configure.ac	2023-11-17 03:08:05.975282593 -0600
@@ -3520,7 +3520,7 @@
 ACX_CHECK_INSTALLED_TARGET_TOOL(WINDRES_FOR_TARGET, windres)
 ACX_CHECK_INSTALLED_TARGET_TOOL(WINDMC_FOR_TARGET, windmc)
 
-RAW_CXX_FOR_TARGET="$CXX_FOR_TARGET"
+RAW_CXX_FOR_TARGET="$CXX_FOR_TARGET -nostdinc++"
 
 GCC_TARGET_TOOL(ar, AR_FOR_TARGET, AR, [binutils/ar])
 GCC_TARGET_TOOL(as, AS_FOR_TARGET, AS, [gas/as-new])
EOF

  # So the && chain above is easier to extend
  true
}

patch_mcm || exit 1
mkdir -p "$OUTPUT"/log

# Make bootstrap compiler (no $TYPE, dynamically linked against host libc)
# We build the rest of the cross compilers with this so they're linked against
# musl-libc, because glibc doesn't fully support static linking and dynamic
# binaries aren't really portable between distributions
make_tuple "${TARGETS[0]}" host 2>&1 | tee -a "$OUTPUT/log/$BOOTSTRAP"-host.log
split_tuple "${TARGETS[0]}"
BOOTSTRAP="$TARGET"

if [ $# -gt 0 ]
then
  for i in "${TARGETS[0]}" "$@"
  do
    make_tuple "$i" static native
  done
else
  # Here's the list of cross compilers supported by this build script.

  # First target builds a proper version of the $BOOTSTRAP compiler above,
  # which is used to build the rest (in alphabetical order)
  for i in $(seq 0 $((${#TARGETS[@]}-1)))
  do
    make_tuple "${TARGETS[$i]}" static native
  done
fi
