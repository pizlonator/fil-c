#! /bin/sh
# Copyright (C) 2012 Red Hat, Inc.
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

testfiles testfile62

testrun_compare ${abs_top_builddir}/src/readelf -n testfile62 <<\EOF

Note segment of 2104 bytes at offset 0x158:
  Owner          Data size  Type
  CORE                 336  PRSTATUS
    info.si_signo: 0, info.si_code: 0, info.si_errno: 0, cursig: 0
    sigpend: <>
    sighold: <>
    pid: 3519, ppid: 0, pgrp: 0, sid: 0
    utime: 0.000000, stime: 0.000000, cutime: 0.000000, cstime: 0.000000
    orig_rax: -1, fpvalid: 0
    r15:                     662  r14:                       4
    r13:             -2119649152  r12:                       0
    rbp:      0xffff880067e39e48  rbx:                      99
    r11:        -131940469531936  r10:             -2124150080
    r9:         -131940469531936  r8:                        0
    rax:                      16  rcx:                    7813
    rdx:                       0  rsi:                       0
    rdi:                      99  rip:      0xffffffff812ba86f
    rflags:   0x0000000000010096  rsp:      0xffff880067e39e48
    fs.base:   0x00007f95a7b09720  gs.base:   0x0000000000000000
    cs: 0x0010  ss: 0x0018  ds: 0x0000  es: 0x0000  fs: 0x0000  gs: 0x0000
  CORE                 336  PRSTATUS
    info.si_signo: 0, info.si_code: 0, info.si_errno: 0, cursig: 0
    sigpend: <>
    sighold: <>
    pid: 0, ppid: 0, pgrp: 0, sid: 0
    utime: 0.000000, stime: 0.000000, cutime: 0.000000, cstime: 0.000000
    orig_rax: -1, fpvalid: 0
    r15:                       0  r14:                       0
    r13:     1348173392195389970  r12:                       1
    rbp:      0xffff88007a829e48  rbx:                      16
    r11:        -131940468065880  r10:            435505529489
    r9:                   158960  r8:                        0
    rax:                      16  rcx:                       1
    rdx:                       0  rsi:                       3
    rdi:        -131939339960320  rip:      0xffffffff810118bb
    rflags:   0x0000000000000046  rsp:      0xffff88007a829e38
    fs.base:   0x0000000000000000  gs.base:   0x0000000000000000
    cs: 0x0010  ss: 0x0018  ds: 0x0000  es: 0x0000  fs: 0x0000  gs: 0x0000
  VMCOREINFO          1366  <unknown>: 0
    OSRELEASE=2.6.35.11-83.fc14.x86_64
    PAGESIZE=4096
    SYMBOL(init_uts_ns)=ffffffff81a4c5b0
    SYMBOL(node_online_map)=ffffffff81b840b0
    SYMBOL(swapper_pg_dir)=ffffffff81a42000
    SYMBOL(_stext)=ffffffff81000190
    SYMBOL(vmlist)=ffffffff81db07e8
    SYMBOL(mem_section)=ffffffff81dbab00
    LENGTH(mem_section)=4096
    SIZE(mem_section)=32
    OFFSET(mem_section.section_mem_map)=0
    SIZE(page)=56
    SIZE(pglist_data)=81664
    SIZE(zone)=1792
    SIZE(free_area)=88
    SIZE(list_head)=16
    SIZE(nodemask_t)=64
    OFFSET(page.flags)=0
    OFFSET(page._count)=8
    OFFSET(page.mapping)=24
    OFFSET(page.lru)=40
    OFFSET(pglist_data.node_zones)=0
    OFFSET(pglist_data.nr_zones)=81472
    OFFSET(pglist_data.node_start_pfn)=81496
    OFFSET(pglist_data.node_spanned_pages)=81512
    OFFSET(pglist_data.node_id)=81520
    OFFSET(zone.free_area)=112
    OFFSET(zone.vm_stat)=1328
    OFFSET(zone.spanned_pages)=1704
    OFFSET(free_area.free_list)=0
    OFFSET(list_head.next)=0
    OFFSET(list_head.prev)=8
    OFFSET(vm_struct.addr)=8
    LENGTH(zone.free_area)=11
    SYMBOL(log_buf)=ffffffff81a532a8
    SYMBOL(log_end)=ffffffff81d0bc50
    SYMBOL(log_buf_len)=ffffffff81a532a4
    SYMBOL(logged_chars)=ffffffff81d0bd70
    LENGTH(free_area.free_list)=5
    NUMBER(NR_FREE_PAGES)=0
    NUMBER(PG_lru)=5
    NUMBER(PG_private)=11
    NUMBER(PG_swapcache)=16
    SYMBOL(phys_base)=ffffffff81a4a010
    SYMBOL(init_level4_pgt)=ffffffff81a42000
    SYMBOL(node_data)=ffffffff81b80df0
    LENGTH(node_data)=512
    CRASHTIME=1348173392

EOF

exit 0
