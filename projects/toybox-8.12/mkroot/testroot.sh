#!/bin/bash

# usage: mkroot/testroot.sh [TARGET...]
#
# Test system image(s) (by booting qemu with -hda test.img providing /mnt/init)
# and check that:
#
# A) it boots and runs our code (which means /dev/hda works)
# B) the clock is set sanely ("make" is unhappy when source newer than output)
# C) it can talk to the virtual network
#
# Each successful test prints a === line, and all 3 means it passed.
# Writes result into root/build/test/$TARGET-test.txt
#
# With arguments, tests those targets (verbosely). With no arguments, tests
# each target with a linux-kernel (in parallel) and prints pass/fail summary.

die() { echo "$@"; exit 1; }

[ -n "$(which toybox)" -a -n "$(which mksquashfs)" ] ||
  die "Need toybox and mksquashfs in $PATH"

mkdir -p "${TEST:=$PWD/root/build/test}" &&

# Setup test filesystem and package it into a squashfs.
cat > "$TEST"/init << 'EOF' &&
#!/bin/sh

echo
echo === init $HOST
[ "$(date +%s)" -gt 1500000000 ] && echo === date ok $HOST
wget http://10.0.2.2:65432 -O -
# TODO: cd /mnt && scripts/test.sh
EOF
chmod +x "$TEST"/init &&

mksquashfs "$TEST"/init configure scripts/ tests/ "$TEST"/init.sqf -noappend -all-root >/dev/null &&

# Setup server on host's loopback for network smoke test
echo === net ok > "$TEST"/index.html || die "smoketest setup"
toybox netcat -p 65432 -s 127.0.0.1 -L toybox httpd "$TEST" &
trap "kill $!" EXIT
sleep .25

[ -n "$(toybox wget http://127.0.0.1:65432/ -O - | grep ===)" ] || die "wget"

do_test()
{
  X=$(dirname "$1") Y=$(basename $X)
  [ ! -e "$1" ] && { echo skip "$Y"; return 0; }
  # Alas KARGS=quiet doesn't silence qemu's bios, so filter output ourselves.
  # QEMU broke -hda because too many people know how to use it, this is
  # the new edgier version they added to be -hda without gratuitous breakage.
  {
    cd $X || continue
    echo === $X
    # Can't point two QEMU instances at same sqf because gratuitous file locking
    cp "$TEST"/init.{sqf,$BASHPID} &&
    # When stdin is a tty QEMU will SIGTTOU itself here, so </dev/null.
    toybox timeout -i 10 bash -c "./run-qemu.sh -drive format=raw,file='$TEST'/init.$BASHPID < /dev/null 2>&1"
    rm -f "$TEST/init.$BASHPID"
    cd ../..
  } | tee root/build/log/$Y-test.txt | { [ -z "$V" ] && cat >/dev/null || { [ "$V" -gt 1 ] && cat || grep '^=== '; } }
}

# Just test targets on command line?
if [ $# -gt 0 ]; then
  ((V++))
  for I in "$@"; do do_test root/"$I"/linux-kernel; done
  exit
fi

COUNT=0 CPUS=$(($(nproc)+0))
for I in root/*/linux-kernel
do
  do_test "$I" | { [ -z "$V" ] && cat >/dev/null || { [ "$V" -gt 1 ] && cat || grep '^=== '; } } &
  [ $((++COUNT)) -ge $CPUS ] &&
    { wait -n; ((--COUNT)); [ -z "$V" ] && echo -n .; }
done

while [ $COUNT -gt 0 ]; do
  wait -n; ((--COUNT)); [ -z "$V" ] && echo -n .
done
echo

PASS= NOPASS=
for I in root/*/linux-kernel
do
  [ ! -e "$I" ] && continue
  X=$(dirname $I) Y=$(basename $X)

  [ "$(grep '^=== ' root/build/log/$Y-test.txt | wc -l)" -eq 4 ] &&
    PASS+="$Y " || NOPASS+="$Y "
done

[ -n "$PASS" ] && echo PASS=$PASS
[ -n "$NOPASS" ] && echo NOPASS=$NOPASS
bd() { sed 's@.*/\([^/]*\)/[^/]*@\1@'; }
NO="$(ls -d root/*/fs | bd | egrep -xv "$(ls root/*/linux-kernel | bd | tr '\n' '|')build" | xargs)"
[ -n "$NO" ] && echo No kernel: $NO
