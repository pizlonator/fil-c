#! /bin/sh
# Copyright (C) 2012, 2013, 2014 Red Hat, Inc.
# Copyright (C) H.J. Lu <hjl.tools@gmail.com>, 2015.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/test-subr.sh

testfiles testfile63

testrun_compare ${abs_top_builddir}/src/readelf -n testfile63 <<\EOF

Note segment of 892 bytes at offset 0x274:
  Owner          Data size  Type
  CORE                 148  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 11087, ppid: 11063, pgrp: 11087, sid: 11063
    utime: 0.000000, stime: 0.010000, cutime: 0.000000, cstime: 0.000000
    orig_r0: -1, fpvalid: 1
    r0:             1  r1:   -1091672508  r2:   -1091672500
    r3:             0  r4:             0  r5:             0
    r6:         33728  r7:             0  r8:             0
    r9:             0  r10:  -1225703496  r11:  -1091672844
    r12:            0  sp:    0xbeee64f4  lr:    0xb6dc3f48
    pc:    0x00008500  spsr:  0x60000010
  CORE                 124  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x00400500
    uid: 0, gid: 0, pid: 11087, ppid: 11063, pgrp: 11087, sid: 11063
    fname: a.out, psargs: ./a.out 
  CORE                 144  AUXV
    HWCAP: 0xe8d7  <swp half thumb fast-mult vfp edsp>
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x8034
    PHENT: 32
    PHNUM: 8
    BASE: 0xb6eee000
    FLAGS: 0
    ENTRY: 0x83c0
    UID: 0
    EUID: 0
    GID: 0
    EGID: 0
    SECURE: 0
    RANDOM: 0xbeee674e
    EXECFN: 0xbeee6ff4
    PLATFORM: 0xbeee675e
    NULL
  CORE                 116  FPREGSET
    f0: 0x000000000000000000000000  f1: 0x000000000000000000000000
    f2: 0x000000000000000000000000  f3: 0x000000000000000000000000
    f4: 0x000000000000000000000000  f5: 0x000000000000000000000000
    f6: 0x000000000000000000000000  f7: 0x000000000000000000000000
  LINUX                260  ARM_VFP
    fpscr: 0x00000000
    d0:  0x0000000000000000  d1:  0x0000000000000000
    d2:  0x0000000000000000  d3:  0x0000000000000000
    d4:  0x0000000000000000  d5:  0x0000000000000000
    d6:  0x0000000000000000  d7:  0x0000000000000000
    d8:  0x0000000000000000  d9:  0x0000000000000000
    d10: 0x0000000000000000  d11: 0x0000000000000000
    d12: 0x0000000000000000  d13: 0x0000000000000000
    d14: 0x0000000000000000  d15: 0x0000000000000000
    d16: 0x0000000000000000  d17: 0x0000000000000000
    d18: 0x0000000000000000  d19: 0x0000000000000000
    d20: 0x0000000000000000  d21: 0x0000000000000000
    d22: 0x0000000000000000  d23: 0x0000000000000000
    d24: 0x0000000000000000  d25: 0x0000000000000000
    d26: 0x0000000000000000  d27: 0x0000000000000000
    d28: 0x0000000000000000  d29: 0x0000000000000000
    d30: 0x0000000000000000  d31: 0x0000000000000000
EOF

testfiles testfile67
testrun_compare ${abs_top_builddir}/src/readelf -n testfile67 <<\EOF

Note segment of 1044 bytes at offset 0xe8:
  Owner          Data size  Type
  CORE                 336  PRSTATUS
    info.si_signo: 4, info.si_code: 0, info.si_errno: 0, cursig: 4
    sigpend: <>
    sighold: <>
    pid: 805, ppid: 804, pgrp: 804, sid: 699
    utime: 0.000042, stime: 0.000103, cutime: 0.000000, cstime: 0.000000
    orig_r2: 2571552016, fpvalid: 1
    pswm:   0x0705c00180000000  pswa:   0x00000000800000d6
    r0:         4393751543808  r1:         4398002544388
    r2:                    11  r3:            2571578208
    r4:            2571702016  r5:         4398003235624
    r6:            2571580768  r7:            2571702016
    r8:            2571578208  r9:            2571552016
    r10:           2571552016  r11:                    0
    r12:        4398003499008  r13:           2148274656
    r14:                    0  r15:        4398040761216
    a0:   0x000003ff  a1:   0xfd54a6f0  a2:   0x00000000  a3:   0x00000000
    a4:   0x00000000  a5:   0x00000000  a6:   0x00000000  a7:   0x00000000
    a8:   0x00000000  a9:   0x00000000  a10:  0x00000000  a11:  0x00000000
    a12:  0x00000000  a13:  0x00000000  a14:  0x00000000  a15:  0x00000000
  CORE                 136  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x0000000000400400
    uid: 0, gid: 0, pid: 805, ppid: 804, pgrp: 804, sid: 699
    fname: 1, psargs: ./1 
  CORE                 304  AUXV
    SYSINFO_EHDR: 0
    HWCAP: 0x37f
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x80000040
    PHENT: 56
    PHNUM: 2
    BASE: 0
    FLAGS: 0
    ENTRY: 0x800000d4
    UID: 0
    EUID: 0
    GID: 0
    EGID: 0
    SECURE: 0
    RANDOM: 0x3ffffa8463c
    EXECFN: 0x3ffffa85ff4
    PLATFORM: 0x3ffffa8464c
    NULL
  CORE                 136  FPREGSET
    fpc: 0x00000000
    f0:  0x0000000000000040  f1:  0x4b00000000000000
    f2:  0x0000000000000041  f3:  0x3ad50b5555555600
    f4:  0x0000000000000000  f5:  0x0000000000000000
    f6:  0x0000000000000000  f7:  0x0000000000000000
    f8:  0x0000000000000000  f9:  0x0000000000000000
    f10: 0x0000000000000000  f11: 0x0000000000000000
    f12: 0x0000000000000000  f13: 0x0000000000000000
    f14: 0x0000000000000000  f15: 0x0000000000000000
  LINUX                  8  S390_LAST_BREAK
    last_break: 0x000003fffd75ccbe
  LINUX                  4  S390_SYSTEM_CALL
    system_call: 0
EOF

testfiles testfile68
testrun_compare ${abs_top_builddir}/src/readelf -n testfile68 <<\EOF

Note segment of 852 bytes at offset 0x94:
  Owner          Data size  Type
  CORE                 224  PRSTATUS
    info.si_signo: 4, info.si_code: 0, info.si_errno: 0, cursig: 4
    sigpend: <>
    sighold: <>
    pid: 839, ppid: 838, pgrp: 838, sid: 699
    utime: 0.000043, stime: 0.000102, cutime: 0.000000, cstime: 0.000000
    orig_r2: -1723388288, fpvalid: 1
    pswm:  0x070dc000  pswa:  0x8040009a
    r0:            0  r1:    -43966716  r2:           11  r3:  -1723238816
    r4:  -1723265280  r5:    -43275480  r6:  -1723245280  r7:  -1723265280
    r8:  -1723238816  r9:  -1723388288  r10: -1723388288  r11:           0
    r12:   -43012096  r13: -2146692640  r14:           0  r15:  2139883440
    a0:   0x000003ff  a1:   0xfd54a6f0  a2:   0x00000000  a3:   0x00000000
    a4:   0x00000000  a5:   0x00000000  a6:   0x00000000  a7:   0x00000000
    a8:   0x00000000  a9:   0x00000000  a10:  0x00000000  a11:  0x00000000
    a12:  0x00000000  a13:  0x00000000  a14:  0x00000000  a15:  0x00000000
  CORE                 124  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x00400400
    uid: 0, gid: 0, pid: 839, ppid: 838, pgrp: 838, sid: 699
    fname: 2, psargs: ./2 
  CORE                 152  AUXV
    SYSINFO_EHDR: 0
    HWCAP: 0x37f
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x400034
    PHENT: 32
    PHNUM: 2
    BASE: 0
    FLAGS: 0
    ENTRY: 0x400098
    UID: 0
    EUID: 0
    GID: 0
    EGID: 0
    SECURE: 0
    RANDOM: 0x7f8c090c
    EXECFN: 0x7f8c1ff4
    PLATFORM: 0x7f8c091c
    NULL
  CORE                 136  FPREGSET
    fpc: 0x00000000
    f0:  0x0000000000000040  f1:  0x4b00000000000000
    f2:  0x0000000000000041  f3:  0x3ad50b5555555600
    f4:  0x0000000000000000  f5:  0x0000000000000000
    f6:  0x0000000000000000  f7:  0x0000000000000000
    f8:  0x0000000000000000  f9:  0x0000000000000000
    f10: 0x0000000000000000  f11: 0x0000000000000000
    f12: 0x0000000000000000  f13: 0x0000000000000000
    f14: 0x0000000000000000  f15: 0x0000000000000000
  LINUX                  8  S390_LAST_BREAK
    last_break: 0xfd75ccbe
  LINUX                  4  S390_SYSTEM_CALL
    system_call: 0
  LINUX                 64  S390_HIGH_GPRS
    high_r0: 0x000003ff, high_r1: 0x000003ff, high_r2: 0x00000000
    high_r3: 0x00000000, high_r4: 0x00000000, high_r5: 0x000003ff
    high_r6: 0x00000000, high_r7: 0x00000000, high_r8: 0x00000000
    high_r9: 0x00000000, high_r10: 0x00000000, high_r11: 0x00000000
    high_r12: 0x000003ff, high_r13: 0x00000000, high_r14: 0x00000000
    high_r15: 0x00000000
EOF

# To reproduce this core dump, do this on x86_64 machine with Linux
# 3.7 or later:
# $ gcc -x c <(echo 'int main () { return *(int *)0x12345678; }')
# $ ./a.out
testfiles testfile71
testrun_compare ${abs_top_builddir}/src/readelf -n testfile71 <<\EOF

Note segment of 1476 bytes at offset 0x430:
  Owner          Data size  Type
  CORE                 336  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 9664, ppid: 2868, pgrp: 9664, sid: 2868
    utime: 0.000000, stime: 0.004000, cutime: 0.000000, cstime: 0.000000
    orig_rax: -1, fpvalid: 0
    r15:                       0  r14:                       0
    r13:         140734971656848  r12:                 4195328
    rbp:      0x00007fff69fe39b0  rbx:                       0
    r11:            266286012928  r10:         140734971656256
    r9:                        0  r8:             266289790592
    rax:               305419896  rcx:                 4195584
    rdx:         140734971656872  rsi:         140734971656856
    rdi:                       1  rip:      0x00000000004004f9
    rflags:   0x0000000000010246  rsp:      0x00007fff69fe39b0
    fs.base:   0x00007fa1c8933740  gs.base:   0x0000000000000000
    cs: 0x0033  ss: 0x002b  ds: 0x0000  es: 0x0000  fs: 0x0000  gs: 0x0000
  CORE                 136  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x0000000000000200
    uid: 1000, gid: 1000, pid: 9664, ppid: 2868, pgrp: 9664, sid: 2868
    fname: a.out, psargs: ./a.out 
  CORE                 128  SIGINFO
    si_signo: 11, si_errno: 0, si_code: 1
    fault address: 0x12345678
  CORE                 304  AUXV
    SYSINFO_EHDR: 0x7fff69ffe000
    HWCAP: 0xafebfbff  <fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss tm pbe>
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x400040
    PHENT: 56
    PHNUM: 9
    BASE: 0
    FLAGS: 0
    ENTRY: 0x400400
    UID: 1000
    EUID: 1000
    GID: 1000
    EGID: 1000
    SECURE: 0
    RANDOM: 0x7fff69fe3d19
    EXECFN: 0x7fff69fe4ff0
    PLATFORM: 0x7fff69fe3d29
    NULL
  CORE                 469  FILE
    10 files:
      00400000-00401000 00000000 4096                /home/petr/a.out
      00600000-00601000 00000000 4096                /home/petr/a.out
      00601000-00602000 00001000 4096                /home/petr/a.out
      3dffa00000-3dffa21000 00000000 135168          /usr/lib64/ld-2.17.so
      3dffc20000-3dffc21000 00020000 4096            /usr/lib64/ld-2.17.so
      3dffc21000-3dffc22000 00021000 4096            /usr/lib64/ld-2.17.so
      3dffe00000-3dfffb6000 00000000 1794048         /usr/lib64/libc-2.17.so
      3dfffb6000-3e001b6000 001b6000 2097152         /usr/lib64/libc-2.17.so
      3e001b6000-3e001ba000 001b6000 16384           /usr/lib64/libc-2.17.so
      3e001ba000-3e001bc000 001ba000 8192            /usr/lib64/libc-2.17.so
EOF

# To reproduce this core dump, do this on an aarch64 machine:
# $ gcc -x c <(echo 'int main () { return *(int *)0x12345678; }')
# $ ./a.out
testfiles testfile_aarch64_core
testrun_compare ${abs_top_builddir}/src/readelf -n testfile_aarch64_core <<\EOF

Note segment of 2512 bytes at offset 0x270:
  Owner          Data size  Type
  CORE                 392  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 16547, ppid: 3822, pgrp: 16547, sid: 3822
    utime: 0.010000, stime: 0.000000, cutime: 0.000000, cstime: 0.000000
    pc: 0x0000000000400548, pstate: 0x0000000060000000, fpvalid: 1
    x0:             305419896  x1:          548685596648
    x2:          548685596664  x3:               4195648
    x4:                     0  x5:          548536191688
    x6:                     0  x7:  -6341196323062964528
    x8:                   135  x9:            4294967295
    x10:              4195026  x11:               184256
    x12:                  144  x13:                   15
    x14:         548536635328  x15:                    0
    x16:         548534815304  x17:              4262024
    x18:         548685596000  x19:                    0
    x20:                    0  x21:              4195296
    x22:                    0  x23:                    0
    x24:                    0  x25:                    0
    x26:                    0  x27:                    0
    x28:                    0  x29:         548685596320
    x30:         548534815544  sp:    0x0000007fc035c6a0
  CORE                 136  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x0000000000400400
    uid: 0, gid: 0, pid: 16547, ppid: 3822, pgrp: 16547, sid: 3822
    fname: a.out, psargs: ./a.out 
  CORE                 128  SIGINFO
    si_signo: 11, si_errno: 0, si_code: 1
    fault address: 0x12345678
  CORE                 304  AUXV
    SYSINFO_EHDR: 0x7fb7500000
    HWCAP: 0x3
    PAGESZ: 65536
    CLKTCK: 100
    PHDR: 0x400040
    PHENT: 56
    PHNUM: 7
    BASE: 0x7fb7520000
    FLAGS: 0
    ENTRY: 0x4003e0
    UID: 0
    EUID: 0
    GID: 0
    EGID: 0
    SECURE: 0
    RANDOM: 0x7fc035c9e8
    EXECFN: 0x7fc035fff0
    PLATFORM: 0x7fc035c9f8
    NULL
  CORE                 306  FILE
    6 files:
      00400000-00410000 00000000 65536               /root/elfutils/build/a.out
      00410000-00420000 00000000 65536               /root/elfutils/build/a.out
      7fb7370000-7fb74d0000 00000000 1441792         /usr/lib64/libc-2.17.so
      7fb74d0000-7fb74f0000 00150000 131072          /usr/lib64/libc-2.17.so
      7fb7520000-7fb7540000 00000000 131072          /usr/lib64/ld-2.17.so
      7fb7540000-7fb7550000 00010000 65536           /usr/lib64/ld-2.17.so
  CORE                 528  FPREGSET
    fpsr: 0x00000000, fpcr: 0x00000000
    v0:  0x00000000000af54b000000000000fe02
    v1:  0x00000000000000000000000000000000
    v2:  0x00000000000000000000000000000000
    v3:  0x00000000000000000000000000000000
    v4:  0x00000000000000000000000000000000
    v5:  0x00000000000000000000000000000000
    v6:  0x00000000000000000000000000000000
    v7:  0x00000000000000000000000000000000
    v8:  0x00000000000000000000000000000000
    v9:  0x00000000000000000000000000000000
    v10: 0x00000000000000000000000000000000
    v11: 0x00000000000000000000000000000000
    v12: 0x00000000000000000000000000000000
    v13: 0x00000000000000000000000000000000
    v14: 0x00000000000000000000000000000000
    v15: 0x00000000000000000000000000000000
    v16: 0x00000000000000000000000000000000
    v17: 0x00000000000000000000000000000000
    v18: 0x00000000000000000000000000000000
    v19: 0x00000000000000000000000000000000
    v20: 0x00000000000000000000000000000000
    v21: 0x00000000000000000000000000000000
    v22: 0x00000000000000000000000000000000
    v23: 0x00000000000000000000000000000000
    v24: 0x00000000000000000000000000000000
    v25: 0x00000000000000000000000000000000
    v26: 0x00000000000000000000000000000000
    v27: 0x00000000000000000000000000000000
    v28: 0x00000000000000000000000000000000
    v29: 0x00000000000000000000000000000000
    v30: 0x00000000000000000000000000000000
    v31: 0x00000000000000000000000000000000
  LINUX                  8  ARM_TLS
    tls: 0x0000007fb73606f0
  LINUX                264  ARM_HW_BREAK
    dbg_info: 0x00000610
    DBGBVR0_EL1: 0x0000000000000000, DBGBCR0_EL1: 0x00000000
    DBGBVR1_EL1: 0x0000000000000000, DBGBCR1_EL1: 0x00000000
    DBGBVR2_EL1: 0x0000000000000000, DBGBCR2_EL1: 0x00000000
    DBGBVR3_EL1: 0x0000000000000000, DBGBCR3_EL1: 0x00000000
    DBGBVR4_EL1: 0x0000000000000000, DBGBCR4_EL1: 0x00000000
    DBGBVR5_EL1: 0x0000000000000000, DBGBCR5_EL1: 0x00000000
    DBGBVR6_EL1: 0x0000000000000000, DBGBCR6_EL1: 0x00000000
    DBGBVR7_EL1: 0x0000000000000000, DBGBCR7_EL1: 0x00000000
    DBGBVR8_EL1: 0x0000000000000000, DBGBCR8_EL1: 0x00000000
    DBGBVR9_EL1: 0x0000000000000000, DBGBCR9_EL1: 0x00000000
    DBGBVR10_EL1: 0x0000000000000000, DBGBCR10_EL1: 0x00000000
    DBGBVR11_EL1: 0x0000000000000000, DBGBCR11_EL1: 0x00000000
    DBGBVR12_EL1: 0x0000000000000000, DBGBCR12_EL1: 0x00000000
    DBGBVR13_EL1: 0x0000000000000000, DBGBCR13_EL1: 0x00000000
    DBGBVR14_EL1: 0x0000000000000000, DBGBCR14_EL1: 0x00000000
    DBGBVR15_EL1: 0x0000000000000000, DBGBCR15_EL1: 0x00000000
  LINUX                264  ARM_HW_WATCH
    dbg_info: 0x00000610
    DBGWVR0_EL1: 0x0000000000000000, DBGWCR0_EL1: 0x00000000
    DBGWVR1_EL1: 0x0000000000000000, DBGWCR1_EL1: 0x00000000
    DBGWVR2_EL1: 0x0000000000000000, DBGWCR2_EL1: 0x00000000
    DBGWVR3_EL1: 0x0000000000000000, DBGWCR3_EL1: 0x00000000
    DBGWVR4_EL1: 0x0000000000000000, DBGWCR4_EL1: 0x00000000
    DBGWVR5_EL1: 0x0000000000000000, DBGWCR5_EL1: 0x00000000
    DBGWVR6_EL1: 0x0000000000000000, DBGWCR6_EL1: 0x00000000
    DBGWVR7_EL1: 0x0000000000000000, DBGWCR7_EL1: 0x00000000
    DBGWVR8_EL1: 0x0000000000000000, DBGWCR8_EL1: 0x00000000
    DBGWVR9_EL1: 0x0000000000000000, DBGWCR9_EL1: 0x00000000
    DBGWVR10_EL1: 0x0000000000000000, DBGWCR10_EL1: 0x00000000
    DBGWVR11_EL1: 0x0000000000000000, DBGWCR11_EL1: 0x00000000
    DBGWVR12_EL1: 0x0000000000000000, DBGWCR12_EL1: 0x00000000
    DBGWVR13_EL1: 0x0000000000000000, DBGWCR13_EL1: 0x00000000
    DBGWVR14_EL1: 0x0000000000000000, DBGWCR14_EL1: 0x00000000
    DBGWVR15_EL1: 0x0000000000000000, DBGWCR15_EL1: 0x00000000
EOF

# To reproduce this core dump, do this on an i686 machine:
# $ gcc -x c <(echo 'int main () { return *(int *)0x12345678; }')
# $ ./a.out
testfiles testfile_i686_core
testrun_compare ${abs_top_builddir}/src/readelf -n testfile_i686_core <<\EOF

Note segment of 1000 bytes at offset 0x214:
  Owner          Data size  Type
  CORE                 144  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 27395, ppid: 1130, pgrp: 27395, sid: 1130
    utime: 0.000000, stime: 0.001000, cutime: 0.000000, cstime: 0.000000
    orig_eax: -1, fpvalid: 0
    ebx:     1334976512  ecx:    -1239415396  edx:    -1079283900
    esi:              0  edi:              0  ebp:     0xbfab6f18
    eax:      305419896  eip:     0x08048408  eflags:  0x00010246
    esp:     0xbfab6f18
    ds: 0x007b  es: 0x007b  fs: 0x0000  gs: 0x0033  cs: 0x0073  ss: 0x007b
  CORE                 124  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x00000200
    uid: 1000, gid: 1000, pid: 27395, ppid: 1130, pgrp: 27395, sid: 1130
    fname: a.out, psargs: ./a.out 
  CORE                 128  SIGINFO
    si_signo: 11, si_errno: 0, si_code: 1
    fault address: 0x12345678
  CORE                 160  AUXV
    SYSINFO: 0xb77fc414
    SYSINFO_EHDR: 0xb77fc000
    HWCAP: 0x780abfd  <fpu de pse tsc msr pae mce cx8 apic sep pge cmov mmx fxsr sse sse2>
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x8048034
    PHENT: 32
    PHNUM: 9
    BASE: 0
    FLAGS: 0
    ENTRY: 0x80482f0
    UID: 1000
    EUID: 1000
    GID: 1000
    EGID: 1000
    SECURE: 0
    RANDOM: 0xbfab70eb
    EXECFN: 0xbfab7ff4
    PLATFORM: 0xbfab70fb
    NULL
  CORE                 275  FILE
    9 files:
      08048000-08049000 00000000 4096                /tmp/a.out
      08049000-0804a000 00000000 4096                /tmp/a.out
      0804a000-0804b000 00001000 4096                /tmp/a.out
      4f744000-4f763000 00000000 126976              /usr/lib/ld-2.18.so
      4f764000-4f765000 0001f000 4096                /usr/lib/ld-2.18.so
      4f765000-4f766000 00020000 4096                /usr/lib/ld-2.18.so
      4f768000-4f920000 00000000 1802240             /usr/lib/libc-2.18.so
      4f920000-4f922000 001b8000 8192                /usr/lib/libc-2.18.so
      4f922000-4f923000 001ba000 4096                /usr/lib/libc-2.18.so
  LINUX                 48  386_TLS
    index: 6, base: 0xb77da700, limit: 0x000fffff, flags: 0x00000051
    index: 7, base: 0x00000000, limit: 0x00000000, flags: 0x00000028
    index: 8, base: 0x00000000, limit: 0x00000000, flags: 0x00000028
EOF

# To reproduce this core dump, do this on x86_64 machine with Linux
# 3.7 or later:
# $ gcc -mx32 -x c <(echo 'int main () { return *(int *)0x12345678; }')
# $ ./a.out
testfiles testfile-x32-core
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-x32-core <<\EOF

Note segment of 2548 bytes at offset 0x234:
  Owner          Data size  Type
  CORE                 296  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 6885, ppid: 2792, pgrp: 6885, sid: 2792
    utime: 0.000000, stime: 0.001000, cutime: 0.000000, cstime: 0.000000
    orig_rax: -1, fpvalid: 1
    r15:                       0  r14:                       0
    r13:              4290830656  r12:                 4194960
    rbp:      0x00000000ffc0e070  rbx:                       0
    r11:              4145779200  r10:                       0
    r9:               4149627024  r8:               4149551744
    rax:               305419896  rcx:                 4195216
    rdx:              4290830668  rsi:              4290830660
    rdi:                       1  rip:      0x0000000000400380
    rflags:   0x0000000000010246  rsp:      0x00000000ffc0e070
    fs.base:   0x00000000f7754700  gs.base:   0x0000000000000000
    cs: 0x0033  ss: 0x002b  ds: 0x002b  es: 0x002b  fs: 0x0063  gs: 0x0000
  CORE                 124  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x00406600
    uid: 1000, gid: 1000, pid: 6885, ppid: 2792, pgrp: 6885, sid: 2792
    fname: a.out, psargs: ./a.out 
  CORE                 128  SIGINFO
    si_signo: 11, si_errno: 0, si_code: 1
    fault address: 0x12345678
  CORE                 152  AUXV
    SYSINFO_EHDR: 0xffd49000
    HWCAP: 0xbfebfbff  <fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe>
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x400034
    PHENT: 32
    PHNUM: 8
    BASE: 0xf7555000
    FLAGS: 0
    ENTRY: 0x400290
    UID: 1000
    EUID: 1000
    GID: 1000
    EGID: 1000
    SECURE: 0
    RANDOM: 0xffc0e2cb
    EXECFN: 0xffc0fff0
    PLATFORM: 0xffc0e2db
    NULL
  CORE                 361  FILE
    9 files:
      00400000-00401000 00000000 4096                /export/home/hjl/bugs/gdb/x32-1/a.out
      00600000-00601000 00000000 4096                /export/home/hjl/bugs/gdb/x32-1/a.out
      f71a2000-f734f000 00000000 1757184             /usr/libx32/libc-2.20.so
      f734f000-f754e000 001ad000 2093056             /usr/libx32/libc-2.20.so
      f754e000-f7551000 001ac000 12288               /usr/libx32/libc-2.20.so
      f7551000-f7552000 001af000 4096                /usr/libx32/libc-2.20.so
      f7555000-f7575000 00000000 131072              /usr/libx32/ld-2.20.so
      f7774000-f7775000 0001f000 4096                /usr/libx32/ld-2.20.so
      f7775000-f7776000 00020000 4096                /usr/libx32/ld-2.20.so
  CORE                 512  FPREGSET
    xmm0:  0x0000000000000000000000000000ff00
    xmm1:  0x2f2f2f2f2f2f2f2f2f2f2f2f2f2f2f2f
    xmm2:  0x00000000000000000000000000000000
    xmm3:  0x0000000000000000ff00000000000000
    xmm4:  0x000000000000000000ff000000000000
    xmm5:  0x00000000000000000000000000000000
    xmm6:  0x00000000000000000000000000000000
    xmm7:  0x00000000000000000000000000000000
    xmm8:  0x00000000000000000000000000000000
    xmm9:  0x00000000000000000000000000000000
    xmm10: 0x00000000000000000000000000000000
    xmm11: 0x00000000000000000000000000000000
    xmm12: 0x00000000000000000000000000000000
    xmm13: 0x00000000000000000000000000000000
    xmm14: 0x00000000000000000000000000000000
    xmm15: 0x00000000000000000000000000000000
    st0: 0x00000000000000000000  st1: 0x00000000000000000000
    st2: 0x00000000000000000000  st3: 0x00000000000000000000
    st4: 0x00000000000000000000  st5: 0x00000000000000000000
    st6: 0x00000000000000000000  st7: 0x00000000000000000000
    mxcsr:   0x0000ffff00001f80
    fcw: 0x037f  fsw: 0x0000
  LINUX                832  X86_XSTATE
EOF

# To reproduce this core dump, do this on an m68k machine:
# $ gcc -x c <(echo 'int main () { return *(int *)0x12345678; }')
# $ ./a.out
testfiles testfile-m68k-core
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-m68k-core <<\EOF

Note segment of 1056 bytes at offset 0x1f4:
  Owner          Data size  Type
  CORE                 154  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 1963, ppid: 1084, pgrp: 1963, sid: 1084
    utime: 0.000000, stime: 0.010000, cutime: 0.000000, cstime: 0.000000
    fpvalid: 1
    d1:           1  d2:           0  d3: -2146476616  d4: -2146476616
    d5:           0  d6: -2147393212  d7: -2144827216  a0:  0x12345678
    a1:  0xefe71460  a2:  0x00000000  a3:  0x80288df8  a4:  0x80000340
    a5:  0xc017a000  a6:  0xefe71434  d0: -1073595312  a7:  0xefe71434
    pc:  0x800003fe
  CORE                 124  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x00400600
    uid: 1000, gid: 501, pid: 1963, ppid: 1084, pgrp: 1963, sid: 1084
    fname: a.out, psargs: ./a.out 
  CORE                 128  SIGINFO
    si_signo: 11, si_errno: 0, si_code: 1
    fault address: 0x12345678
  CORE                 136  AUXV
    HWCAP: 0
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x80000034
    PHENT: 32
    PHNUM: 9
    BASE: 0xc0000000
    FLAGS: 0
    ENTRY: 0x80000340
    UID: 1000
    EUID: 1000
    GID: 501
    EGID: 501
    SECURE: 0
    RANDOM: 0xefe716d9
    EXECFN: 0xefe71ff4
    NULL
  CORE                 281  FILE
    10 files:
      80000000-80001000 00000000 4096                /tmp/a.out
      80003000-80004000 00001000 4096                /tmp/a.out
      80004000-80005000 00002000 4096                /tmp/a.out
      c0000000-c001c000 00000000 114688              /lib/ld-2.23.so
      c001f000-c0020000 0001d000 4096                /lib/ld-2.23.so
      c0020000-c0021000 0001e000 4096                /lib/ld-2.23.so
      c0032000-c0177000 00000000 1331200             /lib/libc-2.23.so
      c0177000-c0178000 00145000 4096                /lib/libc-2.23.so
      c0178000-c017a000 00144000 8192                /lib/libc-2.23.so
      c017a000-c017e000 00146000 16384               /lib/libc-2.23.so
  CORE                 108  FPREGSET
    fp0: 0x7fff0000ffffffffffffffff  fp1: 0x7fff0000ffffffffffffffff
    fp2: 0x7fff0000ffffffffffffffff  fp3: 0x7fff0000ffffffffffffffff
    fp4: 0x7fff0000ffffffffffffffff  fp5: 0x7fff0000ffffffffffffffff
    fp6: 0x7fff0000ffffffffffffffff  fp7: 0x7fff0000ffffffffffffffff
EOF

# To reproduce this core dump, do this on a riscv64 machine:
# $ gcc -x c <(echo 'int main () { return *(int *)0x12345678; }')
# $ ./a.out
testfiles testfile-riscv64-core
testrun_compare ${abs_top_builddir}/src/readelf -n testfile-riscv64-core <<\EOF

Note segment of 1408 bytes at offset 0x388:
  Owner          Data size  Type
  CORE                 376  PRSTATUS
    info.si_signo: 11, info.si_code: 0, info.si_errno: 0, cursig: 11
    sigpend: <>
    sighold: <>
    pid: 6801, ppid: 1155, pgrp: 6801, sid: 1155
    utime: 0.000000, stime: 0.110000, cutime: 0.000000, cstime: 0.000000
    pc: 0x000000000001049a, fpvalid: 0
    ra:    0x0000002000051c9a  sp:    0x0000003fff981240
    gp:    0x0000000000012828  tp:    0x00000020000311d0
    t0:          137439068496  t1:          137439288314
    t2:                 74672  s0:          274871095888
    s1:                 66724  a0:                     1
    a1:          274871096232  a2:          274871096248
    a3:                     0  a4:          274871095928
    a5:             305419896  a6:          137440357656
    a7:                     0  s2:          183254994416
    s3:          137439062288  s4:                     0
    s5:          183257703888  s6:          183256061824
    s7:                     0  s8:          183252656348
    s9:          183257666368  s10:         183257700608
    s11:                    0  t3:                130042
    t4:                     2  t5:                     3
    t6:                 15632
  CORE                 136  PRPSINFO
    state: 0, sname: R, zomb: 0, nice: 0, flag: 0x0000000000400600
    uid: 0, gid: 0, pid: 6801, ppid: 1155, pgrp: 6801, sid: 1155
    fname: a.out, psargs: /tmp/a.out 
  CORE                 128  SIGINFO
    si_signo: 11, si_errno: 0, si_code: 1
    fault address: 0x12345678
  CORE                 288  AUXV
    SYSINFO_EHDR: 0x200001d000
    HWCAP: 0x1105
    PAGESZ: 4096
    CLKTCK: 100
    PHDR: 0x10040
    PHENT: 56
    PHNUM: 9
    BASE: 0x2000000000
    FLAGS: 0
    ENTRY: 0x103e0
    UID: 0
    EUID: 0
    GID: 0
    EGID: 0
    SECURE: 0
    RANDOM: 0x3fff9816d6
    EXECFN: 0x3fff981fed
    NULL
  CORE                 379  FILE
    9 files:
      00010000-00011000 00000000 4096                /tmp/a.out
      00011000-00012000 00000000 4096                /tmp/a.out
      00012000-00013000 00001000 4096                /tmp/a.out
      2000000000-200001a000 00000000 106496          /lib64/ld-2.27.so
      200001a000-200001b000 00019000 4096            /lib64/ld-2.27.so
      200001b000-200001c000 0001a000 4096            /lib64/ld-2.27.so
      2000032000-2000151000 00000000 1175552         /lib64/libc-2.27.so
      2000151000-2000155000 0011e000 16384           /lib64/libc-2.27.so
      2000155000-2000157000 00122000 8192            /lib64/libc-2.27.so
EOF

exit 0
