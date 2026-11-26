#!/bin/bash

set -e
set -x

# shadow-4.16.0 rebuilt WITH Linux-PAM support (it was first built without
# PAM back in the build_postlc stage).  We reuse the Fil-C-patched
# pizlonated-shadow tarball that the earlier stage left in /sources.
#
# BLFS seds are re-applied here because they are chroot-side build tweaks,
# not part of the tarball.  /etc/login.defs is reinstalled by "make install"
# (sysconf_DATA), so the seds are applied to the source copy BEFORE the
# install so that the installed file ends up correct.

# Save the account databases so we can verify them after the reinstall.
cp -a /etc/passwd /tmp/passwd.pre-shadow-pam
cp -a /etc/group /tmp/group.pre-shadow-pam
cp -a /etc/shadow /tmp/shadow.pre-shadow-pam
cp -a /etc/gshadow /tmp/gshadow.pre-shadow-pam
cp -a /etc/login.defs /tmp/login.defs.pre-shadow-pam

rm -rf pizlonated-shadow
tar -xf pizlonated-shadow.tar.gz
cd pizlonated-shadow
sed -i 's/groups$(EXEEXT) //' src/Makefile.in
find man -name Makefile.in -exec sed -i 's/groups\.1 / /' {} \;
find man -name Makefile.in -exec sed -i 's/getspnam\.3 / /' {} \;
find man -name Makefile.in -exec sed -i 's/passwd\.5 / /' {} \;
sed -e 's:#ENCRYPT_METHOD DES:ENCRYPT_METHOD YESCRYPT:' \
    -e 's:/var/spool/mail:/var/mail:' \
    -e '/PATH=/{s@/sbin:@@;s@/bin:@@}' \
    -i etc/login.defs
touch /usr/bin/passwd
./configure --sysconfdir=/etc \
    --disable-static \
    --with-{b,yes}crypt \
    --without-libbsd \
    --without-sssd \
    --with-libpam \
    --with-group-name-max-length=32
make
make exec_prefix=/usr install
make -C man install-man
cd ..
rm -rf pizlonated-shadow
hash -r

# The account databases must be untouched by the reinstall.
for f in passwd group shadow gshadow; do
    diff -q /tmp/$f.pre-shadow-pam /etc/$f
done

# Now write working PAM configurations (BLFS Linux-PAM "Using PAM" section).
# shadow's "make install" just laid down files that all "include system-auth"
# (which does not exist here), and whose login references SELinux-only
# modules, so replace them with a set that works.

cat > /etc/pam.d/system-auth << "EOF"
#%PAM-1.0

auth       required    pam_env.so
auth       required    pam_unix.so     nullok
account    required    pam_unix.so
password   required    pam_unix.so     shadow nullok
session    required    pam_env.so
session    required    pam_unix.so
EOF

cat > /etc/pam.d/login << "EOF"
# Begin /etc/pam.d/login

auth     required      pam_shells.so
auth     requisite     pam_nologin.so
auth     required      pam_env.so
auth     required      pam_unix.so
account  required      pam_access.so
account  required      pam_unix.so
session  required      pam_env.so
session  required      pam_limits.so
session  required      pam_elogind.so
session  required      pam_unix.so
password required      pam_unix.so

# End /etc/pam.d/login
EOF

cat > /etc/pam.d/passwd << "EOF"
# Begin /etc/pam.d/passwd

password    required    pam_unix.so    shadow nullok

# End /etc/pam.d/passwd
EOF

cat > /etc/pam.d/su << "EOF"
# Begin /etc/pam.d/su

auth       sufficient  pam_rootok.so
auth       required    pam_unix.so
account    required    pam_unix.so
session    required    pam_env.so
session    required    pam_unix.so

# End /etc/pam.d/su
EOF

# End-to-end check of the PAM stack: chpasswd goes through
# /etc/pam.d/chpasswd -> system-auth -> pam_unix.so.
echo root:root | chpasswd
