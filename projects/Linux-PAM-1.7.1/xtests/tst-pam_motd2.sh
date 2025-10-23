#!/bin/bash

TST_DIR="tst-pam_motd2.d"

function tst_cleanup() {
    rm -rf "${TST_DIR}"
    rm -f tst-pam_motd2.out
}

mkdir -p ${TST_DIR}
mkdir -p ${TST_DIR}/etc/motd.d
mkdir -p ${TST_DIR}/run/motd.d
mkdir -p ${TST_DIR}/usr/lib/motd.d

echo "motd: /etc/motd" > ${TST_DIR}/etc/motd
echo "motd: /run/motd" > ${TST_DIR}/run/motd
echo "motd: /usr/lib/motd" > ${TST_DIR}/usr/lib/motd

# Drop a motd file in test directories such that every overriding
# condition (for 3 directories in this case) will be seen.
echo "motd: e0r0u1 in usr/lib - will show" > ${TST_DIR}/usr/lib/motd.d/e0r0u1.motd
echo "motd: e0r1u0 in run - will show" > ${TST_DIR}/run/motd.d/e0r1u0.motd
echo "motd: e0r1u1 in usr/lib - not show" > ${TST_DIR}/usr/lib/motd.d/e0r1u1.motd
echo "motd: e0r1u1 in run - will show" > ${TST_DIR}/run/motd.d/e0r1u1.motd
echo "motd: e1r0u0 in etc - will show" > ${TST_DIR}/etc/motd.d/e1r0u0.motd
echo "motd: e1r0u1 in usr/lib - not show" > ${TST_DIR}/usr/lib/motd.d/e1r0u1.motd
echo "motd: e1r0u1 in etc - will show" > ${TST_DIR}/etc/motd.d/e1r0u1.motd
echo "motd: e1r1u0 in run - not show" > ${TST_DIR}/run/motd.d/e1r1u0.motd
echo "motd: e1r1u0 in etc - will show" > ${TST_DIR}/etc/motd.d/e1r1u0.motd
echo "motd: e1r1u1 in usr/lib - not show" > ${TST_DIR}/usr/lib/motd.d/e1r1u1.motd
echo "motd: e1r1u1 in run - not show" > ${TST_DIR}/run/motd.d/e1r1u1.motd
echo "motd: e1r1u1 in etc - will show" > ${TST_DIR}/etc/motd.d/e1r1u1.motd

./tst-pam_motd tst-pam_motd2 > tst-pam_motd2.out

RET=$?

motd_to_show_output=$(cat tst-pam_motd2.out | grep "motd: /etc/motd")
if [ -z "${motd_to_show_output}" ];
then
    tst_cleanup
    exit 1
fi

motd_dir_not_show_output=$(cat tst-pam_motd2.out | grep "not show")
if [ -n "${motd_dir_not_show_output}" ];
then
    tst_cleanup
    exit 1
fi

tst_cleanup
exit $RET
