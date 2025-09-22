#!/bin/bash
#
# Run through a series of tests to try out the various capability
# manipulations possible through exec.
#
# [Run this as root in a root-enabled process tree.]

try_capsh () {
    echo "TEST: ./capsh $*"
    ./capsh "$@"
    if [ $? -ne 0 ]; then
	echo FAILED
	return 1
    else
	echo PASSED
	return 0
    fi
}

fail_capsh () {
    echo -n "EXPECT FAILURE: "
    try_capsh "$@"
    if [ $? -eq 1 ]; then
	echo "[WHICH MEANS A PASS!]"
	return 0
    else
	echo "Undesired result - aborting"
	echo "PROBLEM TEST: $*"
	exit 1
    fi
}

pass_capsh () {
    echo -n "EXPECT SUCCESS: "
    try_capsh "$@"
    if [ $? -eq 0 ]; then
	return 0
    else
	echo "Undesired result - aborting"
	echo "PROBLEM TEST: $*"
	exit 1
    fi
}

pass_capsh --print
pass_capsh --current

# Validate that PATH expansion works
PATH=$(/bin/pwd)/junk:$(/bin/pwd) capsh == == == --modes
if [ $? -ne 0 ]; then
    echo "Failed to execute capsh consecutively for capability manipulation"
    exit 1
fi

# Make a local non-setuid-0 version of capsh and call it privileged
cp ./tcapsh-static ./privileged && /bin/chmod -s ./privileged
if [ $? -ne 0 ]; then
    echo "Failed to copy capsh for capability manipulation"
    exit 1
fi

# Give it the forced capability it could need
./setcap all=ep ./privileged
if [ $? -ne 0 ]; then
    echo "Failed to set all capabilities on file"
    exit 1
fi
./setcap cap_setuid,cap_setgid=ep ./privileged
if [ $? -ne 0 ]; then
    echo "Failed to set limited capabilities on privileged file"
    exit 1
fi

# validate libcap modes:
pass_capsh --inh=cap_chown --mode=PURE1E --print --inmode=PURE1E
pass_capsh --mode=NOPRIV --print --inmode=NOPRIV
pass_capsh --mode=PURE1E --print --mode=NOPRIV --inmode=NOPRIV
fail_capsh --mode=NOPRIV --print --mode=PURE1E
fail_capsh --user=nobody --mode=NOPRIV --print -- ./privileged

# simple IAB setting (no ambient) in pure1e mode.
pass_capsh --mode=PURE1E --iab='!%cap_chown,cap_setuid'

# Explore keep_caps support
pass_capsh --keep=0 --keep=1 --keep=0 --keep=1 --print

/bin/rm -f tcapsh
/bin/cp tcapsh-static tcapsh
/bin/chown root.root tcapsh
/bin/chmod u+s tcapsh
/bin/ls -l tcapsh

# leverage keep caps to maintain capabilities across a change of euid
# from setuid root to capable luser (as per wireshark/dumpcap 0.99.7)
# This test is subtle. It is testing that a change to self, dropping
# euid=0 back to that of the luser keeps capabilities.
pass_capsh --uid=1 -- -c "./tcapsh --keep=1 --caps=\"cap_net_raw,cap_net_bind_service=ip\" --print --uid=1 --print --caps=\"cap_net_raw,cap_net_bind_service=pie\" --print"

# this test is a change of user to a new user, note we need to raise
# the cap_setuid capability (libcap has a function for that) in this case.
pass_capsh --uid=1 -- -c "./tcapsh --caps=\"cap_net_raw,cap_net_bind_service=ip cap_setuid=p\" --print --cap-uid=2 --print --caps=\"cap_net_raw,cap_net_bind_service=pie\" --print"

# This fails, on 2.6.24, but shouldn't
pass_capsh --uid=1 -- -c "./tcapsh --keep=1 --caps=\"cap_net_raw,cap_net_bind_service=ip\" --uid=1 --forkfor=10 --caps= --print --killit=9 --print"

# only continue with these if --secbits is supported
./capsh --secbits=0x2f > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "unable to test securebits manipulation - assume not supported (PASS)"
    rm -f tcapsh
    rm -f privileged
    exit 0
fi

# nobody's uid. Static compilation of the capsh binary can disable pwd
# info discovery.
nouid=$(/usr/bin/id nobody -u)

pass_capsh --secbits=42 --print
fail_capsh --secbits=32 --keep=1 --keep=0 --print
pass_capsh --secbits=10 --keep=0 --keep=1 --print
fail_capsh --secbits=47 -- -c "./tcapsh --uid=$nouid"

/bin/rm -f tcapsh

# Suppress uid=0 privilege
fail_capsh --secbits=47 --print -- -c "./capsh --uid=$nouid"

# suppress uid=0 privilege and test this privileged
pass_capsh --secbits=0x2f --print -- -c "./privileged --uid=$nouid"

# observe that the bounding set can be used to suppress this forced capability
fail_capsh --drop=cap_setuid --secbits=0x2f --print -- \
	   -c "./privileged --uid=$nouid"

# observe that effective cap_setpcap is required to drop bset
fail_capsh --caps="=ep cap_setpcap-ep" --drop=cap_setuid --current
pass_capsh --strict --caps="cap_setpcap=ep" --drop=cap_setuid --current
fail_capsh --strict --caps="cap_setpcap=p" --drop=cap_setuid --current
fail_capsh --strict --caps="=ep cap_setpcap-e" --drop=cap_setuid --current

# observe that effective cap_setpcap is required to raise non-p bits
fail_capsh --strict --caps="cap_setpcap=p" --inh=cap_chown --current
# non-strict mode and capsh figures it out
pass_capsh --caps="cap_setpcap=p" --inh=cap_chown --current

# permitted bits can be raised in inheritable flag without being effective.
pass_capsh --strict --caps="cap_chown=p" --inh=cap_chown --current

# change the way the capability is obtained (make it inheritable)
./setcap cap_setuid,cap_setgid=ei ./privileged

# Note, the bounding set (edited with --drop) only limits p
# capabilities, not i's.
pass_capsh --secbits=47 --inh=cap_setuid,cap_setgid --drop=cap_setuid \
    --uid=1 --print -- -c "./privileged --uid=$nouid"

# test that we do not support capabilities on setuid shell-scripts
/bin/cat > hack.sh <<EOF
#!/bin/bash
/usr/bin/id
mypid=\$\$
caps=\$(./getpcaps \$mypid 2>&1 | /usr/bin/cut -d: -f2)
if [ "\$caps" != " =" ]; then
  echo "Shell script got [\$caps] - you should upgrade your kernel"
  exit 1
else
  ls -l \$0
  echo "Good, no capabilities [\$caps] for this setuid-0 shell script"
fi
exit 0
EOF
/bin/chmod +xs hack.sh
./capsh --uid=1 --inh=none --print -- ./hack.sh
status=$?
/bin/rm -f ./hack.sh
if [ $status -ne 0 ]; then
    echo "shell scripts can have capabilities (bug)"
    exit 1
fi

# Max lockdown (ie., pure capability model as POSIX.1e intended).
secbits=0x2f
if ./capsh --has-ambient ; then
    secbits="0xef --noamb"
fi
pass_capsh --keep=1 --uid=$nouid --caps=cap_setpcap=ep \
	   --drop=all --secbits=$secbits --caps= --print

# Verify we can chroot
pass_capsh --chroot=$(/bin/pwd)
pass_capsh -- -c "./tcapsh-static --chroot=$(/bin/pwd) =="
fail_capsh --chroot=$(/bin/pwd) -- -c "echo oops"

./capsh --has-ambient
if [ $? -eq 0 ]; then
    echo "test ambient capabilities"

    # Ambient capabilities (any file can inherit capabilities)
    pass_capsh --noamb

    # test that shell scripts can inherit through ambient capabilities
    /bin/cat > hack.sh <<EOF
#!/bin/bash
/usr/bin/id
mypid=\$\$
caps=\$(./getpcaps \$mypid 2>&1 | /usr/bin/cut -d: -f2)
if [ "\$caps" != " = cap_setuid+i" ]; then
  echo "Shell script got [\$caps]"
  exit 0
fi
ls -l \$0
echo "no capabilities [\$caps] for this shell script"
exit 1
EOF
    /bin/chmod +x hack.sh
    pass_capsh --keep=1 --uid=$nouid --inh=cap_setuid --addamb=cap_setuid -- \
	       ./hack.sh

    /bin/rm -f hack.sh

    # Next force the privileged binary to have an empty capability set.
    # This is sort of the opposite of privileged - it should ensure that
    # the file can never acquire privilege by the ambient method.
    ./setcap = ./privileged
    fail_capsh --keep=1 --uid=$nouid --inh=cap_setuid --addamb=cap_setuid -- \
	       -c "./privileged --print --uid=1"

    pass_capsh --keep=1 --uid=$nouid --strict \
	       --caps="cap_setuid=p cap_setpcap=ep" \
	       --inh=cap_setuid --addamb=cap_setuid --current

    # No effective capabilities are needed to raise or lower ambient values.
    pass_capsh --keep=1 --uid=$nouid --strict --caps="cap_setuid=p" \
	       --inh=cap_setuid --addamb=cap_setuid --current
    pass_capsh --keep=1 --uid=$nouid --strict --iab="!^cap_setuid" \
	       --caps="cap_setuid=pi" --current --delamb=cap_setuid --current


    # finally remove the capability from the privileged binary and try again.
    ./setcap -r ./privileged
    pass_capsh --keep=1 --uid=$nouid --inh=cap_setuid --addamb=cap_setuid -- \
	       -c "./privileged --print --uid=1"

    # validate IAB setting with an ambient capability
    pass_capsh --iab='!%cap_chown,^cap_setpcap,cap_setuid'
    fail_capsh --mode=PURE1E --iab='!%cap_chown,^cap_setuid'
fi
/bin/rm -f ./privileged

echo "testing namespaced file caps"

# nsprivileged capsh will have an ns rootid value (this is
# the same setup as an earlier test but with a ns file cap).
rm -f nsprivileged
cp ./tcapsh-static ./nsprivileged && /bin/chmod -s ./nsprivileged
./setcap -n 1 all=ep ./nsprivileged
if [ $? -eq 0 ]; then
    ./getcap -n ./nsprivileged | grep -F "[rootid=1]"
    if [ $? -ne 0 ]; then
	echo "FAILED setting ns rootid on file"
	exit 1
    fi
    # since this is a ns file cap and not a regular one, it should not
    # lead to a privilege escalation outside of the namespace it
    # refers to. We suppress uid=0 privilege and confirm this
    # nsprivileged binary does not have the power to change uid.
    fail_capsh --secbits=$secbits --print -- -c "./nsprivileged --uid=$nouid"
else
    echo "ns file caps not supported - skipping test"
fi
rm -f nsprivileged

# If the build tree compiled the Go cap package.
if [ -f ../go/compare-cap ]; then
    cp ../go/compare-cap .
    LD_LIBRARY_PATH=../libcap ./compare-cap
    if [ $? -ne 0 ]; then
	echo "FAILED to execute go binary"
	exit 1
    fi
    LD_LIBRARY_PATH=../libcap ./compare-cap 2>&1 | \
	grep "skipping file cap tests"
    if [ $? -eq 0 ]; then
	echo "FAILED not engaging file cap tests"
	exit 1
    fi
    echo "PASSED"
else
    echo "no Go support compiled, so skipping Go tests"
fi
rm -f compare-cap

echo "attempt to exploit kernel bug"
./uns_test
if [ $? -ne 0 ]; then
    echo "upgrade your kernel"
    exit 1
fi

echo "ALL TESTS PASSED!"
