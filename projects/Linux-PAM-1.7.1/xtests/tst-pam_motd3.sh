#!/bin/bash

TST_DIR="tst-pam_motd3.d"

function tst_cleanup() {
    rm -rf "${TST_DIR}"
    rm -f tst-pam_motd3.out
}

mkdir -p ${TST_DIR}
mkdir -p ${TST_DIR}/etc/motd.d
mkdir -p ${TST_DIR}/run/motd.d
mkdir -p ${TST_DIR}/usr/lib/motd.d

# Verify motd is still displayed when not overridden
echo "motd: test-show in run - show" > ${TST_DIR}/run/motd.d/test-show.motd

# Test overridden by a symlink to a file that isn't /dev/null; symlink target should show
echo "motd: hidden-by-symlink in usr/lib - not show" > ${TST_DIR}/usr/lib/motd.d/hidden-by-symlink.motd
echo "motd: test-from-symlink - show" > ${TST_DIR}/test-from-symlink.motd
ln -sr ${TST_DIR}/test-from-symlink.motd ${TST_DIR}/run/motd.d/hidden-by-symlink.motd

# Test hidden by a null symlink
echo "motd: hidden-by-null-symlink in run - not show" > ${TST_DIR}/run/motd.d/hidden-by-null-symlink.motd
ln -s /dev/null ${TST_DIR}/etc/motd.d/hidden-by-null-symlink.motd

./tst-pam_motd tst-pam_motd3 > tst-pam_motd3.out

RET=$?

motd_dir_not_show_output=$(cat tst-pam_motd3.out | grep "not show")
if [ -n "${motd_dir_not_show_output}" ];
then
    tst_cleanup
    exit 1
fi

motd_test_show_output=$(cat tst-pam_motd3.out | grep "test-show.*- show")
if [ -z "${motd_test_show_output}" ];
then
    tst_cleanup
    exit 1
fi

motd_general_symlink_show_output=$(cat tst-pam_motd3.out | grep "test-from-symlink.*- show")
if [ -z "${motd_general_symlink_show_output}" ];
then
    tst_cleanup
    exit 1
fi

tst_cleanup
exit $RET
