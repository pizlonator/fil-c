#!/bin/bash

# Build a standalone toybox command

[ -z "$1" ] && { echo "usage: single.sh command..." >&2; exit 1; }

source scripts/portability.sh

# Add trailing / to PREFIX when it's set but hasn't got one
[ "$PREFIX" == "${PREFIX%/}" ] && PREFIX="${PREFIX:+$PREFIX/}"

# Harvest TOYBOX_* symbols from .config, or fresh defconfig if none
export KCONFIG_CONFIG
if [ ! -e ${KCONFIG_CONFIG:=.config} ]
then
  KCONFIG_CONFIG=.singleconfig
  make defconfig
else
  # Force dependencies to rebuild headers if we build multiplexer after this.
  touch "$KCONFIG_CONFIG"
fi
GLOBDEP="$($SED -n 's/CONFIG_\(TOYBOX_[^=]*\)=y/\1/p' "$KCONFIG_CONFIG")"
KCONFIG_CONFIG=.singleconfig

for i in "$@"
do
  echo -n "$i:"
  TOYFILE="$(egrep -l "TOY[(]($i)[ ,]" toys/*/*.c)"

  if [ -z "$TOYFILE" ]
  then
    echo "Unknown command '$i'" >&2
    exit 1
  fi

  make allnoconfig > /dev/null || exit 1

  # For the shell pull in MAYFORK commands from other source files as builtins.
  unset DEPENDS MPDEL
  if [ "$i" == sh ]
  then
    DEPENDS="$($SED -n 's/USE_\([^(]*\)(...TOY([^,]*,.*TOYFLAG_MAYFORK.*/\1/p' toys/*/*.c)"
  else
    MPDEL='s/CONFIG_TOYBOX=y/# CONFIG_TOYBOX is not set/;t'
  fi

  # Enable stuff this command depends on
  DEPENDS="$({ echo $DEPENDS $GLOBDEP; $SED -n "/^config *$i"'$/,/^$/{s/^[ \t]*depends on //;T;s/[!][A-Z0-9_]*//g;s/ *&& */|/g;p}' $TOYFILE;}| xargs | tr ' ' '|')"
  NAME=$(echo $i | tr a-z- A-Z_)
  $SED -ri -e "$MPDEL" \
    -e "s/# (CONFIG_($NAME|${NAME}_.*${DEPENDS:+|$DEPENDS})) is not set/\1=y/" \
    "$KCONFIG_CONFIG" || exit 1

  export OUTNAME="$PREFIX$i"
  rm -f "$OUTNAME" &&
  scripts/make.sh || exit 1
done
