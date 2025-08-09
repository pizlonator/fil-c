#!/bin/bash

# This has to be a separate file from scripts/make.sh so it can be called
# before menuconfig. (It's called again from scripts/make.sh just to be sure.)

source scripts/portability.sh

mkdir -p "$GENDIR"

probecc()
{
  ${CROSS_COMPILE}${CC} $CFLAGS $LDFLAGS -xc -o /dev/null - "$@"
}

# Probe for a single config symbol with a "compiles or not" test.
# Symbol name is first argument, flags second, feed C file to stdin
probesymbol()
{
  probecc "${@:2}" 2>/dev/null && DEFAULT=y || DEFAULT=n
  rm a.out 2>/dev/null
  echo -e "config $1\n\tbool\n\tdefault $DEFAULT\n" || exit 1
}

probeconfig()
{
  # Some commands are android-specific
  probesymbol TOYBOX_ON_ANDROID -c << EOF
    #ifndef __ANDROID__
    #error nope
    #endif
EOF

  # nommu support
  probesymbol TOYBOX_FORK << EOF
    #include <unistd.h>
    int main(int argc, char *argv[]) { return fork(); }
EOF
  echo -e '\tdepends on !TOYBOX_FORCE_NOMMU'
}

genconfig()
{
  # Reverse sort puts posix first, examples last.
  for j in $(ls toys/*/README | sort -s -r)
  do
    DIR="$(dirname "$j")"

    [ $(ls "$DIR" | wc -l) -lt 2 ] && continue

    echo "menu \"$(head -n 1 $j)\""
    echo

    # extract config stanzas from each source file, in alphabetical order
    for i in $(ls -1 $DIR/*.c)
    do
      # Grab the config block for Config.in
      echo "# $i"
      $SED -n '/^\*\//q;/^config [A-Z]/,$p' $i || return 1
      echo
    done

    echo endmenu
  done
}

probeconfig > "$GENDIR"/Config.probed || rm "$GENDIR"/Config.probed
genconfig > "$GENDIR"/Config.in || rm "$GENDIR"/Config.in

# Find names of commands that can be built standalone in these C files
toys()
{
  grep 'TOY(.*)' "$@" | grep -v TOYFLAG_NOFORK | grep -v "0))" | \
    $SED -En 's/([^:]*):.*(OLD|NEW)TOY\( *([a-zA-Z][^,]*) *,.*/\1:\3/p'
}

WORKING= PENDING= EXAMPLE=
toys toys/*/*.c | (
while IFS=":" read FILE NAME
do
  echo -e "test_$NAME:\n\tscripts/test.sh $NAME\n"
  [ "$NAME" == help ] && continue
  [ "$NAME" == install ] && continue
  [ "$NAME" == sh ] && FILE="toys/*/*.c"
  echo -e "$NAME: $FILE *.[ch] lib/*.[ch]\n\tscripts/single.sh $NAME\n"
  [ "${FILE/example//}" != "$FILE" ] && EXAMPLE="$EXAMPLE $NAME" ||
  [ "${FILE/pending//}" != "$FILE" ] && PENDING="$PENDING $NAME" ||
    WORKING="$WORKING $NAME"
done &&
echo -e "clean::\n\t@rm -f $WORKING $PENDING" &&
echo -e "list:\n\t@echo $(echo $WORKING | tr ' ' '\n' | sort | xargs)" &&
echo -e "list_example:\n\t@echo $(echo $EXAMPLE | tr ' ' '\n' | sort | xargs)"&&
echo -e "list_pending:\n\t@echo $(echo $PENDING | tr ' ' '\n' | sort | xargs)"&&
echo -e ".PHONY: $WORKING $PENDING" | $SED 's/ \([^ ]\)/ test_\1/g'
) > .singlemake
