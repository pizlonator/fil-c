#!/bin/bash

# Grab default values for $CFLAGS and such.

source scripts/portability.sh

[ -z "$PREFIX" ] && PREFIX="$PWD/install"

# Parse command line arguments.

LONG_PATH=""
while [ ! -z "$1" ]
do
  # Create symlinks instead of hardlinks?
  [ "$1" == "--symlink" ] && LINK_TYPE="-s"

  # Uninstall?
  [ "$1" == "--uninstall" ] && UNINSTALL=Uninstall

  # Delete destination command if it exists?
  [ "$1" == "--force" ] && DO_FORCE="-f"

  # Use {,usr}/{bin,sbin} paths instead of all files in one directory?
  [ "$1" == "--long" ] && LONG_PATH="bin/"

  # Symlink host toolchain binaries to destination to create cross compile $PATH
  [ "$1" == "--airlock" ] && AIRLOCK=1

  shift
done

echo "Compile instlist..."

$DEBUG $HOSTCC -I . scripts/install.c -o "$UNSTRIPPED"/instlist || exit 1
COMMANDS="$("$UNSTRIPPED"/instlist $LONG_PATH)"

echo "${UNINSTALL:-Install} commands..."

# Copy toybox itself

if [ -z "$UNINSTALL" ]
then
  mkdir -p "${PREFIX}/${LONG_PATH}" &&
  rm -f "${PREFIX}/${LONG_PATH}/toybox" &&
  cp toybox"${TARGET:+-$TARGET}" ${PREFIX}/${LONG_PATH} || exit 1
else
  rm -f "${PREFIX}/${LONG_PATH}/toybox" 2>/dev/null
fi
cd "$PREFIX" || exit 1

# Make links to toybox

EXIT=0

for i in $COMMANDS
do
  # Figure out target of link

  if [ -z "$LONG_PATH" ]
  then
    DOTPATH=""
  else
    # Create subdirectory for command to go in (if necessary)

    DOTPATH="$(dirname "$i")"/
    if [ -z "$UNINSTALL" ]
    then
      mkdir -p "$DOTPATH" || exit 1
    fi

    if [ -z "$LINK_TYPE" ]
    then
      DOTPATH="bin/"
    else
      if [ "$DOTPATH" != "$LONG_PATH" ]
      then
        # For symlinks we need ../../bin style relative paths
        DOTPATH="$(echo $DOTPATH | sed -e 's@[^/]*/@../@g')"$LONG_PATH
      else
        DOTPATH=""
      fi
    fi
  fi

  # Create link
  if [ -z "$UNINSTALL" ]
  then
    ln $DO_FORCE $LINK_TYPE ${DOTPATH}"toybox${TARGET:+-$TARGET}" $i || EXIT=1
  else
    rm -f $i || EXIT=1
  fi
done

[ -z "$AIRLOCK" ] && exit $EXIT

# --airlock creates a single directory you can point the $PATH to for cross
# compiling, which contains just toybox and symlinks to toolchain binaries.

# This not only means you're building with a known set of tools (insulated from
# variations in the host distro), but that everything else is NOT in your PATH
# and thus various configure stages won't find things on thie host that won't
# be there on the target (such as the distcc build noticing the host has
# python and deciding to #include Python.h).

# The following are commands toybox should provide, but doesn't yet.
# For now symlink the host version. This list must go away by 1.0.

PENDING="expr git tr bash sh gzip   awk bison flex make ar"
TOOLCHAIN="${TOOLCHAIN//,/ } as cc ld objdump  bc gcc"

# Tools needed to build packages
for i in $TOOLCHAIN $PENDING $HOST_EXTRA
do
  if [ ! -f "$i" ]
  then
    # Loop through each instance, populating fallback directories (used by
    # things like distcc, which require multiple instances of the same binary
    # in a known order in the $PATH).

    X=0
    FALLBACK="$PREFIX"
    which -a "$i" | while read j
    do
      if [ ! -e "$FALLBACK/$i" ]
      then
        mkdir -p "$FALLBACK" &&
        ln -sf "$j" "$FALLBACK/$i" || exit 1
      fi

      X=$[$X+1]
      FALLBACK="$PREFIX/fallback-$X"
    done

    if [ ! -f "$PREFIX/$i" ]
    then
      echo "Toolchain component missing: $i" >&2
      [ -z "$PEDANTIC" ] || EXIT=1
    fi
  fi
done

exit $EXIT
