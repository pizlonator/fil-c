#!/bin/bash

TST_DIR="tst-pam_motd1.d"

function tst_cleanup() {
    rm -rf "${TST_DIR}"
    rm -f tst-pam_motd1.out
}

mkdir -p ${TST_DIR}
mkdir -p ${TST_DIR}/etc/motd.d

# Verify the case of single motd and motd.d directory works
echo "motd: /etc/motd" > ${TST_DIR}/etc/motd
echo "motd: /etc/motd.d/test" > ${TST_DIR}/etc/motd.d/test

./tst-pam_motd tst-pam_motd1 > tst-pam_motd1.out

RET=$?

motd_to_show_output=$(cat tst-pam_motd1.out | grep "motd: /etc/motd")
if [ -z "${motd_to_show_output}" ];
then
    tst_cleanup
    exit 1
fi

motd_dir_to_show_output=$(cat tst-pam_motd1.out | grep "motd: /etc/motd.d/test")
if [ -z "${motd_dir_to_show_output}" ];
then
    tst_cleanup
    exit 1
fi

tst_cleanup
exit $RET
