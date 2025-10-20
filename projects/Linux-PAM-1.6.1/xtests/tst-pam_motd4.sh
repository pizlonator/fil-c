#!/bin/bash

TST_DIR="tst-pam_motd4.d"

function tst_cleanup() {
    rm -rf "${TST_DIR}"
    rm -f tst-pam_motd4.out
}

mkdir -p ${TST_DIR}/etc

# Verify the case of single motd with no motd_dir given in tst-pam_motd4.pamd
echo "motd: /etc/motd" > ${TST_DIR}/etc/motd

./tst-pam_motd tst-pam_motd4 > tst-pam_motd4.out

RET=$?

motd_to_show_output=$(cat tst-pam_motd4.out | grep "motd: /etc/motd")
if [ -z "${motd_to_show_output}" ];
then
    tst_cleanup
    exit 1
fi

tst_cleanup
exit $RET
