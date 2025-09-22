/*
 * libprocps - Library to read proc filesystem
 * Tests for Item_table/enumerator synchronization
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>

#include "diskstats.h"
#include "meminfo.h"
#include "pids.h"
#include "slabinfo.h"
#include "stat.h"
#include "vmstat.h"

#include "tests.h"

static int check_diskstats (void *data) {
    struct diskstats_info *ctx = NULL;
    testname = "Itemtable check, diskstats";
    if (0 == procps_diskstats_new(&ctx))
        procps_diskstats_unref(&ctx);
    return 1;
}

static int check_meminfo (void *data) {
    struct meminfo_info *ctx = NULL;
    testname = "Itemtable check, meminfo";
    if (0 == procps_meminfo_new(&ctx))
        procps_meminfo_unref(&ctx);
    return 1;
}

static int check_pids (void *data) {
    struct pids_info *ctx = NULL;
    testname = "Itemtable check, pids";
    if (0 == procps_pids_new(&ctx, NULL, 0))
        procps_pids_unref(&ctx);
    return 1;
}

static int check_slabinfo (void *data) {
    struct slabinfo_info *ctx = NULL;
    testname = "Itemtable check, slabinfo";
    if (0 == procps_slabinfo_new(&ctx))
        procps_slabinfo_unref(&ctx);
    return 1;
}

static int check_stat (void *data) {
    struct stat_info *ctx = NULL;
    testname = "Itemtable check, stat";
    if (0 == procps_stat_new(&ctx))
        procps_stat_unref(&ctx);
    return 1;
}

static int check_vmstat (void *data) {
    struct vmstat_info *ctx = NULL;
    testname = "Itemtable check, vmstat";
    if (0 == procps_vmstat_new(&ctx))
        procps_vmstat_unref(&ctx);
    return 1;
}

static TestFunction test_funcs[] = {
    check_diskstats,
    check_meminfo,
    check_pids,
    check_slabinfo,
    check_stat,
    check_vmstat,
    NULL
};

int main (void) {
    return run_tests(test_funcs, NULL);
}
