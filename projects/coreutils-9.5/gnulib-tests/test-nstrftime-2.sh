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

final_rc=0

if test $LOCALE_FR != none; then
  LC_ALL=$LOCALE_FR      ${CHECKER} ./test-nstrftime${EXEEXT}
  rc=$?
  if test $rc = 77; then
    final_rc=77
  else
    if test $rc != 0; then
      exit 1
    fi
  fi
fi

if test $LOCALE_FR_UTF8 != none; then
  LC_ALL=$LOCALE_FR_UTF8 ${CHECKER} ./test-nstrftime${EXEEXT}
  rc=$?
  if test $rc = 77; then
    final_rc=77
  else
    if test $rc != 0; then
      exit 1
    fi
  fi
fi

exit $final_rc
