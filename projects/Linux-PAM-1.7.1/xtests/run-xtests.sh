#!/bin/sh

SRCDIR=$1
shift 1
[ -z "${SRCDIR}" ] && SRCDIR='.'

if test `id -u` -ne 0 ; then
  echo "You need to be root to run the tests"
  exit 77
fi

XTESTS="$@"

failed=0
pass=0
skipped=0
all=0

mkdir -p /etc/security
for config in access.conf group.conf time.conf limits.conf ; do
	[ -f "/etc/security/$config" ] &&
		mv /etc/security/$config /etc/security/$config-pam-xtests
	install -m 644 "${SRCDIR}"/$config /etc/security/$config
done
[ -f /etc/shells ] &&
        mv /etc/shells /etc/shells-pam-xtests
install -m 644 "${SRCDIR}"/shells.conf /etc/shells

[ -f /etc/security/opasswd ] &&
	mv /etc/security/opasswd /etc/security/opasswd-pam-xtests

for testname in $XTESTS ; do
	  for cfg in "${SRCDIR}"/$testname*.pamd ; do
	    install -m 644 $cfg /etc/pam.d/$(basename $cfg .pamd)
	  done
	  if test -f "${SRCDIR}"/$testname.sh ; then
            test -x "${SRCDIR}"/$testname.sh || chmod 755 "${SRCDIR}"/$testname.sh
            "${SRCDIR}"/$testname.sh > /dev/null
          else
	    ./$testname > /dev/null
	  fi
          RETVAL=$?
          if test $RETVAL -eq 77 ; then
            echo "SKIP: $testname"
            skipped=`expr $skipped + 1`
	  elif test $RETVAL -ne 0 ; then
	    echo "FAIL: $testname"
	    failed=`expr $failed + 1`
          else
	    echo "PASS: $testname"
	    pass=`expr $pass + 1`
          fi
	  all=`expr $all + 1`
	  rm -f /etc/pam.d/$testname*
done

for config in access.conf group.conf time.conf limits.conf opasswd ; do
	if [ -f "/etc/security/$config-pam-xtests" ]; then
		mv /etc/security/$config-pam-xtests /etc/security/$config
	else
		rm -f /etc/security/$config
	fi
done

[ -f "/etc/shells-pam-xtests" ] &&
        mv /etc/shells-pam-xtests /etc/shells

if test "$failed" -ne 0; then
	  echo "==================="
	  echo "$failed of $all tests failed"
          echo "$skipped tests not run"
	  echo "==================="
	  exit 1
else
	  echo "=================="
	  echo "$all tests passed"
	  echo "$skipped tests not run"
	  echo "=================="
fi
exit 0
