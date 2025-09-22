/*
 * vmstat - report memory statistics
 *
 * Copyright © 2011-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2012-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2010      Jan Görig <jgorig@redhat.com>
 * Copyright © 2003      Fabian Frederick
 * Copyright © 1998-2002 Albert Cahalan
 * Copyright © 1994      Henry Ware <al172@yfn.ysu.edu>
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

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"

#include "diskstats.h"
#include "meminfo.h"
#include "misc.h"
#include "slabinfo.h"
#include "stat.h"
#include "vmstat.h"

#define UNIT_B        1
#define UNIT_k        1000
#define UNIT_K        1024
#define UNIT_m        1000000
#define UNIT_M        1048576

static unsigned long dataUnit = UNIT_K;
static char szDataUnit[3] = "K";

#define VMSTAT        0
#define DISKSTAT      0x00000001
#define VMSUMSTAT     0x00000002
#define SLABSTAT      0x00000004
#define PARTITIONSTAT 0x00000008
#define DISKSUMSTAT   0x00000010

static int statMode = VMSTAT;

/* "-a" means "show active/inactive" */
static int a_option;

/* "-w" means "wide output" */
static int w_option;

/* "-y" means "skip first output" */
static int y_option;

/* "-t" means "show timestamp" */
static int t_option;

static unsigned sleep_time = 1;
static int infinite_updates = 0;
static unsigned long num_updates =1;
/* window height */
static unsigned int height;
static unsigned int moreheaders = TRUE;

static enum stat_item First_stat_items[] = {
    STAT_SYS_PROC_RUNNING,
    STAT_SYS_PROC_BLOCKED,
    STAT_SYS_INTERRUPTS,
    STAT_SYS_CTX_SWITCHES,
    STAT_TIC_USER,
    STAT_TIC_NICE,
    STAT_TIC_SYSTEM,
    STAT_TIC_IRQ,
    STAT_TIC_SOFTIRQ,
    STAT_TIC_IDLE,
    STAT_TIC_IOWAIT,
    STAT_TIC_STOLEN,
    STAT_TIC_GUEST,
    STAT_TIC_GUEST_NICE
};
static enum stat_item Loop_stat_items[] = {
    STAT_SYS_PROC_RUNNING,
    STAT_SYS_PROC_BLOCKED,
    STAT_SYS_DELTA_INTERRUPTS,
    STAT_SYS_DELTA_CTX_SWITCHES,
    STAT_TIC_DELTA_USER,
    STAT_TIC_DELTA_NICE,
    STAT_TIC_DELTA_SYSTEM,
    STAT_TIC_DELTA_IRQ,
    STAT_TIC_DELTA_SOFTIRQ,
    STAT_TIC_DELTA_IDLE,
    STAT_TIC_DELTA_IOWAIT,
    STAT_TIC_DELTA_STOLEN,
    STAT_TIC_DELTA_GUEST,
    STAT_TIC_DELTA_GUEST_NICE
};
enum Rel_statitems {
    stat_PRU, stat_PBL, stat_INT, stat_CTX,
    stat_USR, stat_NIC, stat_SYS, stat_IRQ, stat_SRQ,
    stat_IDL, stat_IOW, stat_STO, stat_GST, stat_GNI,
    MAX_stat
};

static enum meminfo_item Mem_items[] = {
    MEMINFO_SWAP_USED,
    MEMINFO_MEM_FREE,
    MEMINFO_MEM_ACTIVE,
    MEMINFO_MEM_INACTIVE,
    MEMINFO_MEM_BUFFERS,
    MEMINFO_MEM_CACHED_ALL
};
enum Rel_memitems {
    mem_SUS, mem_FREE, mem_ACT, mem_INA, mem_BUF, mem_CAC,  MAX_mem
};

static enum diskstats_item Disk_items[] = {
    DISKSTATS_TYPE,
    DISKSTATS_NAME,
    DISKSTATS_READS,
    DISKSTATS_READS_MERGED,
    DISKSTATS_READ_SECTORS,
    DISKSTATS_READ_TIME,
    DISKSTATS_WRITES,
    DISKSTATS_WRITES_MERGED,
    DISKSTATS_WRITE_SECTORS,
    DISKSTATS_WRITE_TIME,
    DISKSTATS_IO_INPROGRESS,
    DISKSTATS_IO_TIME,
    DISKSTATS_WEIGHTED_TIME
};
enum Rel_diskitems {
    disk_TYPE,  disk_NAME,
    disk_READ,  disk_READ_MERGE,  disk_READ_SECT,  disk_READ_TIME,
    disk_WRITE, disk_WRITE_MERGE, disk_WRITE_SECT, disk_WRITE_TIME,
    disk_IO,    disk_IO_TIME,     disk_IO_WTIME,   MAX_disk
};

static enum diskstats_item Part_items[] = {
    DISKSTATS_READS,
    DISKSTATS_READ_SECTORS,
    DISKSTATS_WRITES,
    DISKSTATS_WRITE_SECTORS
};
enum Rel_partitems {
    part_READ, part_READ_SECT, part_WRITE, part_WRITE_SECT, MAX_part
};

static enum stat_item Sum_stat_items[] = {
    STAT_TIC_USER,
    STAT_TIC_NICE,
    STAT_TIC_SYSTEM,
    STAT_TIC_IDLE,
    STAT_TIC_IOWAIT,
    STAT_TIC_IRQ,
    STAT_TIC_SOFTIRQ,
    STAT_TIC_STOLEN,
    STAT_TIC_GUEST,
    STAT_TIC_GUEST_NICE,
    STAT_SYS_CTX_SWITCHES,
    STAT_SYS_INTERRUPTS,
    STAT_SYS_TIME_OF_BOOT,
    STAT_SYS_PROC_CREATED
};
enum Rel_sumstatitems {
    sstat_USR, sstat_NIC, sstat_SYS, sstat_IDL, sstat_IOW, sstat_IRQ,
    sstat_SRQ, sstat_STO, sstat_GST, sstat_GNI, sstat_CTX, sstat_INT,
    sstat_TOB, sstat_PCR
};

static enum meminfo_item Sum_mem_items[] = {
    MEMINFO_MEM_TOTAL,
    MEMINFO_MEM_USED,
    MEMINFO_MEM_ACTIVE,
    MEMINFO_MEM_INACTIVE,
    MEMINFO_MEM_FREE,
    MEMINFO_MEM_BUFFERS,
    MEMINFO_MEM_CACHED_ALL,
    MEMINFO_SWAP_TOTAL,
    MEMINFO_SWAP_USED,
    MEMINFO_SWAP_FREE,
};
enum Rel_summemitems {
    smem_MTOT, smem_MUSE, smem_MACT, smem_MIAC, smem_MFRE,
    smem_MBUF, smem_MCAC, smem_STOT, smem_SUSE, smem_SFRE
};


static void __attribute__ ((__noreturn__))
    usage(FILE * out)
{
    fputs(USAGE_HEADER, out);
    fprintf(out,
        _(" %s [options] [delay [count]]\n"),
            program_invocation_short_name);
    fputs(USAGE_OPTIONS, out);
    fputs(_(" -a, --active           active/inactive memory\n"), out);
    fputs(_(" -f, --forks            number of forks since boot\n"), out);
    fputs(_(" -m, --slabs            slabinfo\n"), out);
    fputs(_(" -n, --one-header       do not redisplay header\n"), out);
    fputs(_(" -s, --stats            event counter statistics\n"), out);
    fputs(_(" -d, --disk             disk statistics\n"), out);
    fputs(_(" -D, --disk-sum         summarize disk statistics\n"), out);
    fputs(_(" -p, --partition <dev>  partition specific statistics\n"), out);
    fputs(_(" -S, --unit <char>      define display unit\n"), out);
    fputs(_(" -w, --wide             wide output\n"), out);
    fputs(_(" -t, --timestamp        show timestamp\n"), out);
    fputs(_(" -y, --no-first         skips first line of output\n"), out);
    fputs(USAGE_SEPARATOR, out);
    fputs(USAGE_HELP, out);
    fputs(USAGE_VERSION, out);
    fprintf(out, USAGE_MAN_TAIL("vmstat(8)"));

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void new_header(void)
{
    struct tm *tm_ptr;
    time_t the_time;
    char timebuf[32];

    /* Translation Hint: Translating folloging header & fields
     * that follow (marked with max x chars) might not work,
     * unless manual page is translated as well.  */
    const char *header =
        _("procs -----------memory---------- ---swap-- -----io---- -system-- -------cpu-------");
    const char *wide_header =
        _("--procs-- -----------------------memory---------------------- ---swap-- -----io---- -system-- ----------cpu----------");
    const char *timestamp_header = _(" -----timestamp-----");

    const char format[] =
        "%2s %2s %6s %6s %6s %6s %4s %4s %5s %5s %4s %4s %2s %2s %2s %2s %2s %2s";
    const char wide_format[] =
        "%4s %4s %12s %12s %12s %12s %4s %4s %5s %5s %4s %4s %3s %3s %3s %3s %3s %3s";


    printf("%s", w_option ? wide_header : header);

    if (t_option) {
        printf("%s", timestamp_header);
    }

    printf("\n");

    printf(
        w_option ? wide_format : format,
        /* Translation Hint: max 2 chars */
         _("r"),
        /* Translation Hint: max 2 chars */
         _("b"),
        /* Translation Hint: max 6 chars */
         _("swpd"),
        /* Translation Hint: max 6 chars */
         _("free"),
        /* Translation Hint: max 6 chars */
         a_option ? _("inact") :
        /* Translation Hint: max 6 chars */
            _("buff"),
        /* Translation Hint: max 6 chars */
         a_option ? _("active") :
        /* Translation Hint: max 6 chars */
            _("cache"),
        /* Translation Hint: max 4 chars */
         _("si"),
        /* Translation Hint: max 4 chars */
         _("so"),
        /* Translation Hint: max 5 chars */
         _("bi"),
        /* Translation Hint: max 5 chars */
         _("bo"),
        /* Translation Hint: max 4 chars */
         _("in"),
        /* Translation Hint: max 4 chars */
         _("cs"),
        /* Translation Hint: max 2 chars */
         _("us"),
        /* Translation Hint: max 2 chars */
         _("sy"),
        /* Translation Hint: max 2 chars */
         _("id"),
        /* Translation Hint: max 2 chars */
         _("wa"),
        /* Translation Hint: max 2 chars */
         _("st"),
        /* Translation Hint: max 2 chars */
         _("gu"));

    if (t_option) {
        (void) time( &the_time );
        tm_ptr = localtime( &the_time );
        if (tm_ptr && strftime(timebuf, sizeof(timebuf), "%Z", tm_ptr)) {
            const size_t len = strlen(timestamp_header);
            if (len >= 1 && len - 1 < sizeof(timebuf)) {
                timebuf[len - 1] = '\0';
            }
        } else {
            timebuf[0] = '\0';
        }
        printf(" %*s", (int)(strlen(timestamp_header) - 1), timebuf);
    }

    printf("\n");
}


static unsigned long unitConvert(unsigned long size)
{
    double cvSize;
    cvSize = (double)size / dataUnit * ((statMode == SLABSTAT) ? 1 : 1024);
    return ((unsigned long)cvSize);
}

static void new_format(void)
{
#define TICv(E) STAT_VAL(E, ull_int, stat_stack, stat_info)
#define DTICv(E) STAT_VAL(E, sl_int, stat_stack, stat_info)
#define SYSv(E) STAT_VAL(E, ul_int, stat_stack, stat_info)
#define MEMv(E) MEMINFO_VAL(E, ul_int, mem_stack, mem_info)
#define DSYSv(E) STAT_VAL(E, s_int, stat_stack, stat_info)
    const char format[] =
        "%2lu %2lu %6lu %6lu %6lu %6lu %4u %4u %5u %5u %4u %4u %2u %2u %2u %2u %2u %2u";
    const char wide_format[] =
        "%4lu %4lu %12lu %12lu %12lu %12lu %4u %4u %5u %5u %4u %4u %3u %3u %3u %3u %3u %3u";

    unsigned int tog = 0;    /* toggle switch for cleaner code */
    unsigned int i;
    long long cpu_use, cpu_sys, cpu_idl, cpu_iow, cpu_sto, cpu_gue;
    long long Div, divo2;
    unsigned long pgpgin[2], pgpgout[2], pswpin[2] = {0,0}, pswpout[2];
    unsigned int sleep_half;
    unsigned long kb_per_page = sysconf(_SC_PAGESIZE) / 1024ul;
    int debt = 0;        /* handle idle ticks running backwards */
    struct tm *tm_ptr;
    time_t the_time;
    char timebuf[32];
    double uptime;
    struct vmstat_info *vm_info = NULL;
    struct stat_info *stat_info = NULL;
    struct stat_stack *stat_stack;
    struct meminfo_info *mem_info = NULL;
    struct meminfo_stack *mem_stack;

    sleep_half = (sleep_time / 2);
    // long hz = procps_hertz_get();

    if (procps_vmstat_new(&vm_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create vmstat structure"));
    if (procps_stat_new(&stat_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create system stat structure"));
    if (procps_meminfo_new(&mem_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create meminfo structure"));
    if (procps_uptime(&uptime, NULL) < 0)
        xerr(EXIT_FAILURE, _("Unable to get uptime"));
    if (0.0 == uptime)
        uptime = 1.0;
    new_header();

    pgpgin[tog] = VMSTAT_GET(vm_info, VMSTAT_PGPGIN, ul_int);
    pgpgout[tog] = VMSTAT_GET(vm_info, VMSTAT_PGPGOUT, ul_int);
    pswpin[tog] = VMSTAT_GET(vm_info, VMSTAT_PSWPIN, ul_int);
    pswpout[tog] = VMSTAT_GET(vm_info, VMSTAT_PSWPOUT, ul_int);

    if (!(mem_stack = procps_meminfo_select(mem_info, Mem_items, MAX_mem)))
        xerrx(EXIT_FAILURE, _("Unable to select memory information"));

    if (y_option == 0) {
        if (t_option) {
            (void) time( &the_time );
            tm_ptr = localtime( &the_time );
            if (tm_ptr && strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr)) {
                ;
            } else {
                timebuf[0] = '\0';
            }
        }
        /* Do the initial fill */
        if (!(stat_stack = procps_stat_select(stat_info, First_stat_items, MAX_stat)))
            xerrx(EXIT_FAILURE, _("Unable to select stat information"));
        cpu_use = TICv(stat_USR) + TICv(stat_NIC);
        cpu_sys = TICv(stat_SYS) + TICv(stat_IRQ) + TICv(stat_SRQ);
        cpu_idl = TICv(stat_IDL);
        cpu_iow = TICv(stat_IOW);
        cpu_sto = TICv(stat_STO);
        cpu_gue = TICv(stat_GST) + TICv(stat_GNI);

        Div = cpu_use + cpu_sys + cpu_idl + cpu_iow + cpu_sto;
        if (!Div) {
            Div = 1;
            cpu_idl = 1;
        }
        divo2 = Div / 2UL;
        cpu_use = (cpu_use >= cpu_gue)? cpu_use - cpu_gue : 0;

        printf(w_option ? wide_format : format,
               SYSv(stat_PRU),
               SYSv(stat_PBL),
               unitConvert(MEMv(mem_SUS)),
               unitConvert(MEMv(mem_FREE)),
               unitConvert((a_option?MEMv(mem_INA):MEMv(mem_BUF))),
               unitConvert((a_option?MEMv(mem_ACT):MEMv(mem_CAC))),
               (unsigned)( unitConvert(VMSTAT_GET(vm_info, VMSTAT_PSWPIN, ul_int)  * kb_per_page) / uptime ),
               (unsigned)( unitConvert(VMSTAT_GET(vm_info, VMSTAT_PSWPOUT, ul_int)  * kb_per_page) / uptime ),
               (unsigned)( VMSTAT_GET(vm_info, VMSTAT_PGPGIN, ul_int) / uptime ),
               (unsigned)( VMSTAT_GET(vm_info, VMSTAT_PGPGOUT, ul_int) / uptime ),
               (unsigned)( SYSv(stat_INT) / uptime ),
               (unsigned)( SYSv(stat_CTX) / Div ),
               (unsigned)( (100*cpu_use        + divo2) / Div ),
               (unsigned)( (100*cpu_sys        + divo2) / Div ),
               (unsigned)( (100*cpu_idl        + divo2) / Div ),
               (unsigned)( (100*cpu_iow        + divo2) / Div ),
               (unsigned)( (100*cpu_sto        + divo2) / Div ),
               (unsigned)( (100*cpu_gue        + divo2) / Div )
        );

        if (t_option) {
            printf(" %s", timebuf);
        }

        printf("\n");
    } else
        num_updates++;

    /* main loop */
    for (i = 1; infinite_updates || i < num_updates; i++) {
        sleep(sleep_time);
        if (moreheaders && ((i % height) == 0))
            new_header();
        tog = !tog;

        if (!(stat_stack = procps_stat_select(stat_info, Loop_stat_items, MAX_stat)))
            xerrx(EXIT_FAILURE, _("Unable to select stat information"));

        cpu_use = DTICv(stat_USR) + DTICv(stat_NIC);
        cpu_sys = DTICv(stat_SYS) + DTICv(stat_IRQ) + DTICv(stat_SRQ);
        cpu_idl = DTICv(stat_IDL);
        cpu_iow = DTICv(stat_IOW);
        cpu_sto = DTICv(stat_STO);
        cpu_gue = TICv(stat_GST) + TICv(stat_GNI);
        pgpgin[tog] = VMSTAT_GET(vm_info, VMSTAT_PGPGIN, ul_int);
        pgpgout[tog] = VMSTAT_GET(vm_info, VMSTAT_PGPGOUT, ul_int);
        pswpin[tog] = VMSTAT_GET(vm_info, VMSTAT_PSWPIN, ul_int);
        pswpout[tog] = VMSTAT_GET(vm_info, VMSTAT_PSWPOUT, ul_int);

        if (!(mem_stack = procps_meminfo_select(mem_info, Mem_items, MAX_mem)))
                xerrx(EXIT_FAILURE, _("Unable to select memory information"));

        if (t_option) {
            (void) time( &the_time );
            tm_ptr = localtime( &the_time );
            if (tm_ptr && strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr)) {
                ;
            } else {
                timebuf[0] = '\0';
            }
        }

        /* idle can run backwards for a moment -- kernel "feature" */
        if (debt) {
            cpu_idl = (int)cpu_idl + debt;
            debt = 0;
        }
        if ((int)cpu_idl < 0) {
            debt = (int)cpu_idl;
            cpu_idl = 0;
        }

        Div = cpu_use + cpu_sys + cpu_idl + cpu_iow + cpu_sto;
        if (!Div) Div = 1, cpu_idl = 1;
        divo2 = Div / 2UL;

        /* guest time is also in user time, we need to subtract. Due to timing
         * effects guest could be larger than user. We use 0 that case */
        if (cpu_use >= cpu_gue) {
            cpu_use -= cpu_gue;
        } else {
            cpu_use = 0;
        }

        printf(w_option ? wide_format : format,
               SYSv(stat_PRU),
               SYSv(stat_PBL),
               unitConvert(MEMv(mem_SUS)),
               unitConvert(MEMv(mem_FREE)),
               unitConvert((a_option?MEMv(mem_INA):MEMv(mem_BUF))),
               unitConvert((a_option?MEMv(mem_ACT):MEMv(mem_CAC))),
               /*si */
               (unsigned)( ( unitConvert((pswpin [tog] - pswpin [!tog])*kb_per_page)+sleep_half )/sleep_time ),
               /* so */
               (unsigned)( ( unitConvert((pswpout[tog] - pswpout[!tog])*kb_per_page)+sleep_half )/sleep_time ),
               /* bi */
               (unsigned)( (  pgpgin [tog] - pgpgin [!tog]           +sleep_half )/sleep_time ),
               /* bo */
               (unsigned)( (  pgpgout[tog] - pgpgout[!tog]           +sleep_half )/sleep_time ),
               /* in */
               (unsigned)( (  DSYSv(stat_INT)           +sleep_half )/sleep_time ),
               /* cs */
               (unsigned)( (  DSYSv(stat_CTX)           +sleep_half )/sleep_time ),
               /* us */
               (unsigned)( (100*cpu_use+divo2)/Div ),
               /* sy */
               (unsigned)( (100*cpu_sys+divo2)/Div ),
               /* id */
               (unsigned)( (100*cpu_idl+divo2)/Div ),
               /* wa */
               (unsigned)( (100*cpu_iow+divo2)/Div ),
               /* st */
               (unsigned)( (100*cpu_sto+divo2)/Div ),
           /* gu */
               (unsigned)( (100*cpu_gue+divo2)/Div )
        );

        if (t_option) {
            printf(" %s", timebuf);
        }

        printf("\n");
    }
    /* Cleanup */
    procps_stat_unref(&stat_info);
    procps_vmstat_unref(&vm_info);
    procps_meminfo_unref(&mem_info);
#undef TICv
#undef DTICv
#undef SYSv
#undef DSYSv
#undef MEMv
}

static void diskpartition_header(const char *partition_name)
{
    printf("%-10s %10s  %16s  %10s  %16s\n",
        partition_name,

       /* Translation Hint: Translating folloging disk partition
    * header fields that follow (marked with max x chars) might
    * not work, unless manual page is translated as well. */
           /* Translation Hint: max 10 chars */
           _("reads"),
           /* Translation Hint: max 16 chars */
           _("read sectors"),
           /* Translation Hint: max 10 chars */
           _("writes"),
           /* Translation Hint: max 16 chars */
           _("requested writes"));
}

static void diskpartition_format(const char *partition_name)
{
 #define partVAL(x) DISKSTATS_VAL(x, ul_int, stack, disk_stat)
    struct diskstats_info *disk_stat = NULL;
    struct diskstats_stack *stack;
    struct diskstats_result *got;
    const char format[] = "%21lu  %16lu  %10lu  %16lu\n";
    int i;

    if (procps_diskstats_new(&disk_stat) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create diskstat structure"));

    if (!(got = procps_diskstats_get(disk_stat, partition_name, DISKSTATS_TYPE)))
        xerrx(EXIT_FAILURE, _("Disk/Partition %s not found"), partition_name);

    diskpartition_header(partition_name);

    for (i = 0; infinite_updates || i < num_updates ; i++) {
        if (!(stack = procps_diskstats_select(disk_stat, partition_name, Part_items, MAX_part)))
            xerrx(EXIT_FAILURE, _("Disk/Partition %s not found"), partition_name);
        printf(format,
            partVAL(part_READ),
            partVAL(part_READ_SECT),
            partVAL(part_WRITE),
            partVAL(part_WRITE_SECT));
        if (infinite_updates || i+1 < num_updates)
            sleep(sleep_time);
    }
    procps_diskstats_unref(&disk_stat);
 #undef partVAL
}

static void diskheader(void)
{
    struct tm *tm_ptr;
    time_t the_time;
    char timebuf[32];

    /* Translation Hint: Translating folloging header & fields
     * that follow (marked with max x chars) might not work,
     * unless manual page is translated as well.  */
    const char *header =
        _("disk- ------------reads------------ ------------writes----------- -----IO------");
    const char *wide_header =
        _("disk- -------------------reads------------------- -------------------writes------------------ ------IO-------");
    const char *timestamp_header = _(" -----timestamp-----");

    const char format[] =
        "%5s %6s %6s %7s %7s %6s %6s %7s %7s %6s %6s";
    const char wide_format[] =
        "%5s %9s %9s %11s %11s %9s %9s %11s %11s %7s %7s";

    printf("%s", w_option ? wide_header : header);

    if (t_option) {
        printf("%s", timestamp_header);
    }

    printf("\n");

    printf(w_option ? wide_format : format,
           " ",
           /* Translation Hint: max 6 chars */
           _("total"),
           /* Translation Hint: max 6 chars */
           _("merged"),
           /* Translation Hint: max 7 chars */
           _("sectors"),
           /* Translation Hint: max 7 chars */
           _("ms"),
           /* Translation Hint: max 6 chars */
           _("total"),
           /* Translation Hint: max 6 chars */
           _("merged"),
           /* Translation Hint: max 7 chars */
           _("sectors"),
           /* Translation Hint: max 7 chars */
           _("ms"),
           /* Translation Hint: max 6 chars */
           _("cur"),
           /* Translation Hint: max 6 chars */
           _("sec"));

    if (t_option) {
        (void) time( &the_time );
        tm_ptr = localtime( &the_time );
        if (tm_ptr && strftime(timebuf, sizeof(timebuf), "%Z", tm_ptr)) {
            const size_t len = strlen(timestamp_header);
            if (len >= 1 && len - 1 < sizeof(timebuf)) {
                timebuf[len - 1] = '\0';
            }
        } else {
            timebuf[0] = '\0';
        }
        printf(" %*s", (int)(strlen(timestamp_header) - 1), timebuf);
    }

    printf("\n");
}

static void diskformat(void)
{
#define diskVAL(e,t) DISKSTATS_VAL(e, t, reap->stacks[j], disk_stat)
    struct diskstats_info *disk_stat = NULL;
    struct diskstats_reaped *reap;
    int i, j;
    time_t the_time;
    struct tm *tm_ptr;
    char timebuf[32];
    const char format[] = "%-5s %6lu %6lu %7lu %7lu %6lu %6lu %7lu %7lu %6d %6lu";
    const char wide_format[] = "%-5s %9lu %9lu %11lu %11lu %9lu %9lu %11lu %11lu %7d %7lu";

    if (procps_diskstats_new(&disk_stat) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create diskstat structure"));

    if (!moreheaders)
        diskheader();

    for (i=0; infinite_updates || i < num_updates ; i++) {
        if (!(reap = procps_diskstats_reap(disk_stat, Disk_items, MAX_disk)))
            xerrx(EXIT_FAILURE, _("Unable to retrieve disk statistics"));
        if (t_option) {
            (void) time( &the_time );
            tm_ptr = localtime( &the_time );
            if (tm_ptr && strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr)) {
                ;
            } else {
                timebuf[0] = '\0';
            }
        }
        for (j = 0; j < reap->total; j++) {
            if (diskVAL(disk_TYPE, s_int) != DISKSTATS_TYPE_DISK)
                continue; /* not a disk */
            if (moreheaders && ((j % height) == 0))
                diskheader();
            printf(w_option ? wide_format : format,
                diskVAL(disk_NAME, str),
                diskVAL(disk_READ, ul_int),
                diskVAL(disk_READ_MERGE, ul_int),
                diskVAL(disk_READ_SECT, ul_int),
                diskVAL(disk_READ_TIME, ul_int),
                diskVAL(disk_WRITE, ul_int),
                diskVAL(disk_WRITE_MERGE, ul_int),
                diskVAL(disk_WRITE_SECT, ul_int),
                diskVAL(disk_WRITE_TIME, ul_int),
                diskVAL(disk_IO, s_int) / 1000,
                diskVAL(disk_IO_TIME, ul_int) / 1000);
            if (t_option)
                printf(" %s\n", timebuf);
            else
                printf("\n");
            fflush(stdout);
        }
        if (infinite_updates || i+1 < num_updates)
            sleep(sleep_time);
    }
#undef diskVAL
    procps_diskstats_unref(&disk_stat);
}

static void slabheader(void)
{
    printf("%-24s %6s %6s %6s %6s\n",
    /* Translation Hint: Translating folloging slab fields that
     * follow (marked with max x chars) might not work, unless
     * manual page is translated as well.  */
           /* Translation Hint: max 24 chars */
           _("Cache"),
           /* Translation Hint: max 6 chars */
           _("Num"),
           /* Translation Hint: max 6 chars */
           _("Total"),
           /* Translation Hint: max 6 chars */
           _("Size"),
           /* Translation Hint: max 6 chars */
           _("Pages"));
}

static void slabformat (void)
{
 #define MAX_ITEMS (int)(sizeof(node_items) / sizeof(node_items[0]))
 #define slabVAL(e,t) SLABINFO_VAL(e, t, p, slab_info)
    struct slabinfo_info *slab_info = NULL;
    struct slabinfo_reaped *reaped;
    int i, j;
    enum slabinfo_item node_items[] = {
        SLAB_ACTIVE_OBJS, SLAB_NUM_OBJS,
        SLAB_OBJ_SIZE,    SLAB_OBJ_PER_SLAB,
        SLAB_NAME };
    enum rel_enums {
        slab_AOBJS, slab_OBJS, slab_OSIZE, slab_OPS, slab_NAME };

    if (procps_slabinfo_new(&slab_info) < 0)
        xerr(EXIT_FAILURE, _("Unable to create slabinfo structure"));

    if (!moreheaders)
        slabheader();

    for (i = 0; infinite_updates || i < num_updates; i++) {
        if (!(reaped = procps_slabinfo_reap(slab_info, node_items, MAX_ITEMS)))
            xerrx(EXIT_FAILURE, _("Unable to get slabinfo node data"));
        if (!(procps_slabinfo_sort(slab_info, reaped->stacks, reaped->total, SLAB_NAME, SLABINFO_SORT_ASCEND)))
            xerrx(EXIT_FAILURE, _("Unable to sort slab nodes"));

        for (j = 0; j < reaped->total; j++) {
            struct slabinfo_stack *p = reaped->stacks[j];
            if (moreheaders && ((j % height) == 0))
                slabheader();
            printf("%-24.24s %6u %6u %6u %6u\n",
                slabVAL(slab_NAME,  str),
                slabVAL(slab_AOBJS, u_int),
                slabVAL(slab_OBJS,  u_int),
                slabVAL(slab_OSIZE, u_int),
                slabVAL(slab_OPS,   u_int));
        }
        if (infinite_updates || i+1 < num_updates)
            sleep(sleep_time);
    }
    procps_slabinfo_unref(&slab_info);
 #undef MAX_ITEMS
 #undef slabVAL
}

static void disksum_format(void)
{
#define diskVAL(e,t) DISKSTATS_VAL(e, t, reap->stacks[j], disk_stat)
    struct diskstats_info *disk_stat = NULL;
    struct diskstats_reaped *reap;
    int j, disk_count, part_count;
    unsigned long reads, merged_reads, read_sectors, milli_reading, writes,
                  merged_writes, written_sectors, milli_writing, inprogress_IO,
                  milli_spent_IO, weighted_milli_spent_IO;

    reads = merged_reads = read_sectors = milli_reading = writes =
        merged_writes = written_sectors = milli_writing = inprogress_IO =
        milli_spent_IO = weighted_milli_spent_IO = 0;
    disk_count = part_count = 0;

    if (procps_diskstats_new(&disk_stat) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create diskstat structure"));
    if (!(reap = procps_diskstats_reap(disk_stat, Disk_items, MAX_disk)))
        xerrx(EXIT_FAILURE, _("Unable to retrieve disk statistics"));

    for (j = 0; j < reap->total; j++) {
        if (diskVAL(disk_TYPE, s_int) != DISKSTATS_TYPE_DISK) {
            part_count++;
            continue; /* not a disk */
        }
        disk_count++;

        reads += diskVAL(disk_READ, ul_int);
        merged_reads += diskVAL(disk_READ_MERGE, ul_int);
        read_sectors += diskVAL(disk_READ_SECT, ul_int);
        milli_reading += diskVAL(disk_READ_TIME, ul_int);
        writes += diskVAL(disk_WRITE, ul_int);
        merged_writes += diskVAL(disk_WRITE_MERGE, ul_int);
        written_sectors += diskVAL(disk_WRITE_SECT, ul_int);
        milli_writing += diskVAL(disk_WRITE_TIME, ul_int);
        inprogress_IO += diskVAL(disk_IO, s_int) / 1000;
        milli_spent_IO += diskVAL(disk_IO_TIME, ul_int) / 1000;
        weighted_milli_spent_IO += diskVAL(disk_IO_WTIME, ul_int) / 1000;
    }
    printf(_("%13d disks\n"), disk_count);       // <== old vmstat had a trailing space here
    printf(_("%13d partitions\n"), part_count);  // <== old vmstat had a trailing space here too
    printf(_("%13lu total reads\n"), reads);
    printf(_("%13lu merged reads\n"), merged_reads);
    printf(_("%13lu read sectors\n"), read_sectors);
    printf(_("%13lu milli reading\n"), milli_reading);
    printf(_("%13lu writes\n"), writes);
    printf(_("%13lu merged writes\n"), merged_writes);
    printf(_("%13lu written sectors\n"), written_sectors);
    printf(_("%13lu milli writing\n"), milli_writing);
    printf(_("%13lu inprogress IO\n"), inprogress_IO);
    printf(_("%13lu milli spent IO\n"), milli_spent_IO);
    printf(_("%13lu milli weighted IO\n"), weighted_milli_spent_IO);

    procps_diskstats_unref(&disk_stat);
#undef diskVAL
}

static void sum_format(void)
{
#define TICv(E) STAT_VAL(E, ull_int, stat_stack, stat_info)
#define SYSv(E) STAT_VAL(E, ul_int, stat_stack, stat_info)
#define MEMv(E) unitConvert(MEMINFO_VAL(E, ul_int, mem_stack, mem_info))
    struct stat_info *stat_info = NULL;
    struct vmstat_info *vm_info = NULL;
    struct meminfo_info *mem_info = NULL;
    struct stat_stack *stat_stack;
    struct meminfo_stack *mem_stack;

    if (procps_stat_new(&stat_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create system stat structure"));
    if (!(stat_stack = procps_stat_select(stat_info, Sum_stat_items, 14)))
        xerrx(EXIT_FAILURE, _("Unable to select stat information"));
    if (procps_vmstat_new(&vm_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create vmstat structure"));
    if (procps_meminfo_new(&mem_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create meminfo structure"));
    if (!(mem_stack = procps_meminfo_select(mem_info, Sum_mem_items, 10)))
        xerrx(EXIT_FAILURE, _("Unable to select memory information"));

    printf(_("%13lu %s total memory\n"), MEMv(smem_MTOT), szDataUnit);
    printf(_("%13lu %s used memory\n"), MEMv(smem_MUSE), szDataUnit);
    printf(_("%13lu %s active memory\n"), MEMv(smem_MACT), szDataUnit);
    printf(_("%13lu %s inactive memory\n"), MEMv(smem_MIAC), szDataUnit);
    printf(_("%13lu %s free memory\n"), MEMv(smem_MFRE), szDataUnit);
    printf(_("%13lu %s buffer memory\n"), MEMv(smem_MBUF), szDataUnit);
    printf(_("%13lu %s swap cache\n"), MEMv(smem_MCAC), szDataUnit);
    printf(_("%13lu %s total swap\n"), MEMv(smem_STOT), szDataUnit);
    printf(_("%13lu %s used swap\n"), MEMv(smem_SUSE), szDataUnit);
    printf(_("%13lu %s free swap\n"), MEMv(smem_SFRE), szDataUnit);
    printf(_("%13lld non-nice user cpu ticks\n"), TICv(sstat_USR));
    printf(_("%13lld nice user cpu ticks\n"), TICv(sstat_NIC));
    printf(_("%13lld system cpu ticks\n"), TICv(sstat_SYS));
    printf(_("%13lld idle cpu ticks\n"), TICv(sstat_IDL));
    printf(_("%13lld IO-wait cpu ticks\n"), TICv(sstat_IOW));
    printf(_("%13lld IRQ cpu ticks\n"), TICv(sstat_IRQ));
    printf(_("%13lld softirq cpu ticks\n"), TICv(sstat_SRQ));
    printf(_("%13lld stolen cpu ticks\n"), TICv(sstat_STO));
    printf(_("%13lld non-nice guest cpu ticks\n"), TICv(sstat_GST));
    printf(_("%13lld nice guest cpu ticks\n"), TICv(sstat_GNI));
    printf(_("%13lu K paged in\n"), VMSTAT_GET(vm_info, VMSTAT_PGPGIN, ul_int));
    printf(_("%13lu K paged out\n"), VMSTAT_GET(vm_info, VMSTAT_PGPGOUT, ul_int));
    printf(_("%13lu pages swapped in\n"), VMSTAT_GET(vm_info, VMSTAT_PSWPIN, ul_int));
    printf(_("%13lu pages swapped out\n"), VMSTAT_GET(vm_info, VMSTAT_PSWPOUT, ul_int));
    printf(_("%13lu interrupts\n"), SYSv(sstat_INT));
    printf(_("%13lu CPU context switches\n"), SYSv(sstat_CTX));
    printf(_("%13lu boot time\n"), SYSv(sstat_TOB));
    printf(_("%13lu forks\n"), SYSv(sstat_PCR));

    /* Cleanup */
    procps_stat_unref(&stat_info);
    procps_vmstat_unref(&vm_info);
    procps_meminfo_unref(&mem_info);
#undef TICv
#undef SYSv
#undef MEMv
}

static void fork_format(void)
{
    struct stat_info *stat_info = NULL;

    if (procps_stat_new(&stat_info) < 0)
    xerrx(EXIT_FAILURE, _("Unable to create system stat structure"));

    printf(_("%13lu forks\n"), STAT_GET(stat_info, STAT_SYS_PROC_CREATED, ul_int));
    /* Cleanup */
    procps_stat_unref(&stat_info);
}

static int winhi(void)
{
    struct winsize win;
    int rows = 24;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1 && 0 < win.ws_row)
        rows = win.ws_row;

    return rows;
}

int main(int argc, char *argv[])
{
    char *partition = NULL;
    int c;
    long tmp;

    static const struct option longopts[] = {
        {"active", no_argument, NULL, 'a'},
        {"forks", no_argument, NULL, 'f'},
        {"slabs", no_argument, NULL, 'm'},
        {"one-header", no_argument, NULL, 'n'},
        {"stats", no_argument, NULL, 's'},
        {"disk", no_argument, NULL, 'd'},
        {"disk-sum", no_argument, NULL, 'D'},
        {"partition", required_argument, NULL, 'p'},
        {"unit", required_argument, NULL, 'S'},
        {"wide", no_argument, NULL, 'w'},
        {"timestamp", no_argument, NULL, 't'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"no-first", no_argument, NULL, 'y'},
        {NULL, 0, NULL, 0}
    };

#ifdef HAVE_PROGRAM_INVOCATION_NAME
    program_invocation_name = program_invocation_short_name;
#endif
    setlocale (LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    atexit(close_stdout);

    while ((c =
        getopt_long(argc, argv, "afmnsdDp:S:wthVy", longopts, NULL)) != -1)
        switch (c) {
        case 'V':
            printf(PROCPS_NG_VERSION);
            return EXIT_SUCCESS;
        case 'h':
            usage(stdout);
        case 'd':
            statMode |= DISKSTAT;
            break;
        case 'a':
            /* active/inactive mode */
            a_option = 1;
            break;
        case 'f':
            /* FIXME: check for conflicting args */
            fork_format();
            exit(0);
        case 'm':
            statMode |= SLABSTAT;
            break;
        case 'D':
            statMode |= DISKSUMSTAT;
            break;
        case 'n':
            /* print only one header */
            moreheaders = FALSE;
            break;
        case 'p':
            statMode |= PARTITIONSTAT;
            partition = optarg;
            if (strncmp(partition, "/dev/", 5) == 0)
                partition += 5;
            break;
        case 'S':
            switch (optarg[0]) {
            case 'b':
            case 'B':
                dataUnit = UNIT_B;
                break;
            case 'k':
                dataUnit = UNIT_k;
                break;
            case 'K':
                dataUnit = UNIT_K;
                break;
            case 'm':
                dataUnit = UNIT_m;
                break;
            case 'M':
                dataUnit = UNIT_M;
                break;
            default:
                /* Translation Hint: do not change argument characters */
                xerrx(EXIT_FAILURE, _("-S requires k, K, m or M (default is KiB)"));
            }
            szDataUnit[0] = optarg[0];
            break;
        case 's':
            statMode |= VMSUMSTAT;
            break;
        case 'w':
            w_option = 1;
            break;
        case 't':
            t_option = 1;
            break;
        case 'y':
            /* Don't display stats since system restart */
            y_option = 1;
            break;
        default:
            /* no other aguments defined yet. */
            usage(stderr);
        }

    if (optind < argc) {
        tmp = strtol_or_err(argv[optind++], _("failed to parse argument"));
        if (tmp < 1)
            xerrx(EXIT_FAILURE, _("delay must be positive integer"));
        else if (UINT_MAX < tmp)
            xerrx(EXIT_FAILURE, _("too large delay value"));
        sleep_time = tmp;
        infinite_updates = 1;
    }
    num_updates = 1;
    if (optind < argc) {
        num_updates = strtol_or_err(argv[optind++], _("failed to parse argument"));
        infinite_updates = 0;
    }
    if (optind < argc)
        usage(stderr);

    if (moreheaders) {
        int wheight = winhi() - 3;
        height = ((wheight > 0) ? wheight : 22);
    }
    setlinebuf(stdout);
    switch (statMode) {
    case (VMSTAT):
        new_format();
        break;
    case (VMSUMSTAT):
        sum_format();
        break;
    case (DISKSTAT):
        diskformat();
        break;
    case (PARTITIONSTAT):
        diskpartition_format(partition);
        break;
    case (SLABSTAT):
        slabformat();
        break;
    case (DISKSUMSTAT):
        disksum_format();
        break;
    default:
        usage(stderr);
        break;
    }
    return 0;
}
