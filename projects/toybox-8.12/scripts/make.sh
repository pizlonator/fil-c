#!/bin/bash

# Grab default values for $CFLAGS and such.
set -o pipefail
source scripts/portability.sh

# Shell functions called by the build

DASHN=-n
true & wait -n 2>/dev/null || { wait; unset DASHN; }
ratelimit()
{
  if [ "$#" -eq 0 ]
  then
    [ -z "$DASHN" ] && PIDS="$PIDS$! "
    [ $((++COUNT)) -lt $CPUS ] && return 0
  fi
  ((--COUNT))
  if [ -n "$DASHN" ]
  then
    wait -n
    DONE=$(($DONE+$?))
  else
    # MacOS uses an ancient version of bash which hasn't got "wait -n", and
    # wait without arguments always returns 0 instead of process exit code.
    # This keeps $CPUS less busy when jobs finish out of order.
    wait ${PIDS%% *}
    DONE=$(($DONE+$?))
    PIDS=${PIDS#* }
  fi

  return $DONE
}

# Respond to V= by echoing command lines as well as running them
do_loudly()
{
  { [ -n "$V" ] && echo "$@" || echo -n "$DOTPROG" ; } >&2
  "$@"
}

# Is anything under directory $2 newer than generated/$1 (or does it not exist)?
isnewer()
{
  [ -e "$GENDIR/$1" ] && [ -z "$(find "${@:2}" -newer "$GENDIR/$1")" ] &&
    return 1
  echo -n "${DIDNEWER:-$GENDIR/{}$1"
  DIDNEWER=,
}

# Build a tool that runs on the host
hostcomp()
{
  if [ ! -f "$UNSTRIPPED"/$1 ] || [ "$UNSTRIPPED"/$1 -ot scripts/$1.c ]
  then
    do_loudly $HOSTCC scripts/$1.c -o "$UNSTRIPPED"/$1 || exit 1
  fi
}

# --as-needed removes libraries we don't use any symbols out of, but the
# compiler has no way to ignore a library that doesn't exist, so detect
# and skip nonexistent libraries for it (probing in parallel).
LIBRARIES=$(
  [ -z "$V" ] && X=/dev/null || X=/dev/stderr
  for i in util crypt m resolv selinux smack attr crypto z log iconv tls ssl
  do
    do_loudly ${CROSS_COMPILE}${CC} $CFLAGS $LDFLAGS -xc - -l$i >>$X 2>&1 \
      -o /dev/null <<<"int main(int argc,char*argv[]){return 0;}" &&
      echo -l$i &
  done | sort | xargs
)
# Actually resolve dangling dependencies in extra libraries when static linking
[ -n "$LIBRARIES" ] && [ "$LDFLAGS" != "${LDFLAGS/-static/}" ] &&
  LIBRARIES="-Wl,--start-group $LIBRARIES -Wl,--end-group"

[ -z "$VERSION" ] && [ -d ".git" ] && [ -n "$(which git 2>/dev/null)" ] &&
  VERSION="$(git describe --tags --abbrev=12 2>/dev/null)"

# Set/record build environment information
compflags()
{
  # The #d lines tag dependencies that force full rebuild if changed
  echo '#!/bin/sh'
  echo
  echo "VERSION='$VERSION'"
  echo "LIBRARIES='$LIBRARIES'"
  echo "BUILD='${CROSS_COMPILE}${CC} $CFLAGS -I . $OPTIMIZE" \
       "'\"\${VERSION:+-DTOYBOX_VERSION=\\\"$VERSION\\\"}\" #d"
  echo "LINK='$LDOPTIMIZE $LDFLAGS '\"\$LIBRARIES\" #d"
  echo "#d Config was $KCONFIG_CONFIG"
  echo "#d PATH was '$PATH'"
}

# Make sure rm -rf isn't gonna go funny
B="$(readlink -f "$PWD")/" A="$(readlink -f "$GENDIR")" A="${A%/}"/
[ "$A" == "${B::${#A}}" ] &&
  { echo "\$GENDIR=$GENDIR cannot include \$PWD=$PWD"; exit 1; }
unset A B DOTPROG DIDNEWER

# Force full rebuild if our compiler/linker options changed
cmp -s <(compflags | grep '#d') <(grep '%d' "$GENDIR"/build.sh 2>/dev/null) ||
  rm -rf "$GENDIR"/* # Keep symlink, delete contents
mkdir -p "$UNSTRIPPED"  "$(dirname $OUTNAME)" || exit 1

# Extract a list of toys/*/*.c files to compile from the data in $KCONFIG_CONFIG
# (First command names, then filenames with relevant {NEW,OLD}TOY() macro.)

[ -n "$V" ] && echo -e "\nWhich C files to build..."
TOYFILES="$($SED -n 's/^CONFIG_\([^=]*\)=.*/\1/p' "$KCONFIG_CONFIG" | xargs | tr ' ' '|')"
TOYFILES="main.c $(egrep -l "^USE_($TOYFILES)[(]...TOY[(]" toys/*/*.c | xargs)"

if [ "${TOYFILES/pending//}" != "$TOYFILES" ]
then
  echo -e "\n\033[1;31mwarning: using unfinished code from toys/pending\033[0m"
fi

# Write build variables (and set them locally), then append build invocation.
compflags > "$GENDIR"/build.sh && source "$GENDIR/build.sh" &&
  {
    echo FILES=$'"\n'"$(fold -s <<<"$TOYFILES")"$'\n"' &&
    echo &&
    echo -e "\$BUILD lib/*.c \$FILES \$LINK -o $OUTNAME"
  } >> "$GENDIR"/build.sh &&
  chmod +x "$GENDIR"/build.sh || exit 1

if isnewer Config.in toys || isnewer Config.in Config.in
then
  scripts/genconfig.sh
fi

# Does .config need dependency recalculation because toolchain changed?
A="$($SED -n '/^config .*$/h;s/default \(.\)/\1/;T;H;g;s/config \([^\n]*\)[^yn]*\(.\)/\1=\2/p' "$GENDIR"/Config.probed | sort)"
B="$(egrep "^CONFIG_($(echo "$A" | sed 's/=[yn]//' | xargs | tr ' ' '|'))=" "$KCONFIG_CONFIG" | $SED 's/^CONFIG_//' | sort)"
A="$(echo "$A" | grep -v =n)"
[ "$A" != "$B" ] &&
  { echo -e "\nWarning: Config.probed changed, run 'make oldconfig'" >&2; }
unset A B

# Create a list of all the commands toybox can provide.
if isnewer newtoys.h toys
then
  # The multiplexer is the first element in the array
  echo "USE_TOYBOX(NEWTOY(toybox, 0, TOYFLAG_STAYROOT|TOYFLAG_NOHELP))" \
    > "$GENDIR"/newtoys.h
  # Sort rest by name for binary search (copy name to front, sort, remove copy)
  $SED -n 's/^\(USE_[^(]*(.*TOY(\)\([^,]*\)\(,.*\)/\2 \1\2\3/p' toys/*/*.c \
    | sort -s -k 1,1 | $SED 's/[^ ]* //'  >> "$GENDIR"/newtoys.h
  [ $? -ne 0 ] && exit 1
fi

# Rebuild config.h from .config
$SED -En $KCONFIG_CONFIG > "$GENDIR"/config.h \
  -e 's/^# CONFIG_(.*) is not set.*/#define CFG_\1 0\n#define USE_\1(...)/p;t' \
  -e 's/^CONFIG_(.*)=y.*/#define CFG_\1 1\n#define USE_\1(...) __VA_ARGS__/p;t'\
  -e 's/^CONFIG_(.*)=/#define CFG_\1 /p' || exit 1

# Process config.h and newtoys.h to generate FLAG_x macros. Note we must
# always #define the relevant macro, even when it's disabled, because we
# allow multiple NEWTOY() in the same C file. (When disabled the FLAG is 0,
# so flags&0 becomes a constant 0 allowing dead code elimination.)

hostcomp mkflags
if isnewer flags.h toys "$KCONFIG_CONFIG"
then
  # Parse files through C preprocessor twice, once to get flags for current
  # .config and once to get flags for allyesconfig
  for I in A B
  do
    (
    # define macros and select header files with option string data

    echo "#define NEWTOY(aa,bb,cc) aa $I bb"
    echo '#define OLDTOY(...)'
    if [ "$I" == A ]
    then
      cat "$GENDIR"/config.h
    else
      $SED '/USE_.*([^)]*)$/s/$/ __VA_ARGS__/' "$GENDIR"/config.h
    fi
    echo '#include "lib/toyflags.h"'
    cat "$GENDIR"/newtoys.h

    # Run result through preprocessor, glue together " " gaps leftover from USE
    # macros, delete comment lines, print any line with a quoted optstring,
    # turn any non-quoted opstring (NULL or 0) into " " (because fscanf can't
    # handle "" with nothing in it, and mkflags uses that).

    ) | ${CROSS_COMPILE}${CC} -E - | \
    $SED -n -e 's/" *"//g;/^#/d;t clear;:clear;s/"/"/p;t;s/\( [AB] \).*/\1 " "/p'

  # Sort resulting line pairs and glue them together into triplets of
  #   command "flags" "allflags"
  # to feed into mkflags C program that outputs actual flag macros
  # If no pair (because command's disabled in config), use " " for flags
  # so allflags can define the appropriate zero macros.

  done | sort -s | $SED -n -e 's/ A / /;t pair;h;s/\([^ ]*\).*/\1 " "/;x' \
    -e 'b single;:pair;h;n;:single;s/[^ ]* B //;H;g;s/\n/ /;p' | \
    tee "$GENDIR"/flags.raw | "$UNSTRIPPED"/mkflags > "$GENDIR"/flags.h || exit 1
fi

# Extract global structure definitions and flag definitions from toys/*/*.c

{
  STRUX="$($SED -ne 's/^#define[ \t]*FOR_\([^ \t]*\).*/\1/;T s1;h;:s1' \
  -e '/^GLOBALS(/,/^)/{s/^GLOBALS(//;T s2;g;s/.*/struct &_data {/;:s2;s/^)/};\n/;p}' \
  $TOYFILES)"
  echo "$STRUX" &&
  echo "extern union global_union {" &&
  $SED -n 's/^struct \(.*\)_data .*/\1/;T;s/.*/\tstruct &_data &;/p' \
    <<<"$STRUX" &&
  echo "} this;"
} > "$GENDIR"/globals.h || exit 1

# Recreate tags.h
$SED -ne '/TAGGED_ARRAY(/,/^)/{s/.*TAGGED_ARRAY[(]\([^,]*\),/\1/p' \
  -e 's/[^{]*{"\([^"]*\)"[^{]*/ _\1/gp}' toys/*/*.c | tr '[:punct:]' _ | \
while read i; do
  [ "$i" = "${i#_}" ] && { HEAD="$i"; X=0; LL=; continue;}
  for j in $i; do
    [ $X -eq 31 ] && LL=LL
    NAME="$HEAD$j"
    printf "#define $NAME %*s%s\n#define _$NAME %*s%s\n" \
      $((32-${#NAME})) "" "$X" $((31-${#NAME})) "" "(1$LL<<$((X++)))" || exit 1
  done
done > "$GENDIR"/tags.h || exit 1

# Create help.h, and zhelp.h if zcat enabled
hostcomp config2help
if isnewer help.h "$GENDIR"/Config.in
then
  "$UNSTRIPPED"/config2help Config.in $KCONFIG_CONFIG > "$GENDIR"/help.h||exit 1
fi

if grep -qx 'CONFIG_TOYBOX_ZHELP=y' "$KCONFIG_CONFIG"
then
  do_loudly $HOSTCC -I . scripts/install.c -o "$UNSTRIPPED"/instlist || exit 1
  { echo "#define ZHELP_LEN $("$UNSTRIPPED"/instlist --help | wc -c)" &&
    "$UNSTRIPPED"/instlist --help | gzip -9 | od -Anone -vtx1 | \
    sed 's/ /,0x/g;1s/^,/static char zhelp_data[] = {\n /;$s/.*/&};/'
  } > "$GENDIR"/zhelp.h || exit 1
else
  rm -f "$GENDIR"/zhelp.h
fi

[ -z "$DIDNEWER" ] || echo }
[ -n "$NOBUILD" ] && exit 0

echo "Compile $OUTNAME"
DOTPROG=.

# This is a parallel version of: do_loudly $BUILD lib/*.c $TOYFILES $LINK

# Build all if oldest generated/obj file isn't newer than all header files.
X="$(ls -1t "$GENDIR"/obj/* 2>/dev/null | tail -n 1)"
if [ ! -e "$X" ] || [ -n "$(find toys -name "*.h" -newer "$X")" ]
then
  rm -rf "$GENDIR"/obj && mkdir -p "$GENDIR"/obj || exit 1
else
  # always redo toy_list[] and help_data[]
  rm -f "$GENDIR"/obj/main.o || exit 1
fi

# build each generated/obj/*.o file in parallel

PENDING= LNKFILES= CLICK= DONE=0 COUNT=0
for i in lib/*.c click $TOYFILES
do
  [ "$i" == click ] && CLICK=1 && continue

  X=${i/lib\//lib_}
  X=${X##*/}
  OUT="$GENDIR/obj/${X%%.c}.o"
  LNKFILES="$LNKFILES $OUT"

  # Library files don't get rebuilt if older than .config, but commands do.
  [ "$OUT" -nt "$i" ] && [ -z "$CLICK" -o "$OUT" -nt "$KCONFIG_CONFIG" ] &&
    continue

  do_loudly $BUILD -c $i -o $OUT &

  ratelimit || break
done

# wait for all background jobs, detecting errors
while [ "$COUNT" -gt 0 ]
do
  ratelimit done
done
[ $DONE -ne 0 ] && exit 1

UNSTRIPPED="$UNSTRIPPED/${OUTNAME/*\//}"
do_loudly $BUILD $LNKFILES $LINK -o "$UNSTRIPPED" || exit 1
if [ -n "$NOSTRIP" ] ||
  ! do_loudly ${CROSS_COMPILE}${STRIP} "$UNSTRIPPED" -o "$OUTNAME"
then
  [ -z "$NOSTRIP" ] && echo "strip failed, using unstripped"
  rm -f "$OUTNAME" &&
  cp "$UNSTRIPPED" "$OUTNAME" || exit 1
fi

# Remove write bit set so buggy installs (like bzip's) don't overwrite the
# multiplexer binary via truncate-and-write through a symlink.
do_loudly chmod 555 "$OUTNAME" || exit 1

# Ensure make wrapper sees success return code
[ -z "$V" ] && echo >&2 || true
