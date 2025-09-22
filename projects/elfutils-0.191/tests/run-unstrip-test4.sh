# Test whether unstrip can combine a stripped kernel object that has
# limited .symtab/.strtab data, with a separate .debuginfo binary that
# has full .symtab/.strtab data.
#
# This was generated as part of a Chromium OS kernel build:
#
#   emerge-kevin chromeos-kernel-4_4
#
# Setup instructions:
#
#   https://www.chromium.org/chromium-os/developer-guide
#   https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/kernel-faq

original=testfile-strtab
stripped=testfile-strtab.stripped
debugfile=testfile-strtab.debuginfo

. $srcdir/run-unstrip-test.sh
