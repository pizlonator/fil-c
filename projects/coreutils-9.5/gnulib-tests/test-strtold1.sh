#!/bin/sh

: "${LOCALE_FR=fr_FR}"
: "${LOCALE_FR_UTF8=fr_FR.UTF-8}"

if test $LOCALE_FR = none && test $LOCALE_FR_UTF8 = none; then
  if test -f /usr/bin/localedef; then
    echo "Skipping test: no locale for testing is installed"
  else
    echo "Skipping test: no locale for testing is supported"
  fi
  exit 77
fi

if $LC_NUMERIC_IMPLEMENTED; then
  :
else
  echo "Skipping test: LC_NUMERIC category of locales is not implemented"
  exit 77
fi

if test $LOCALE_FR != none; then
  LC_ALL=$LOCALE_FR      ${CHECKER} ./test-strtold1${EXEEXT} || exit 1
fi

if test $LOCALE_FR_UTF8 != none; then
  LC_ALL=$LOCALE_FR_UTF8 ${CHECKER} ./test-strtold1${EXEEXT} || exit 1
fi

exit 0
