/*
 * pmap.c - print process memory mapping
 *
 * Copyright © 2010-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2002-2009 Albert Cahalan
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "xalloc.h"

#include "pids.h"

static struct pids_info *Pids_info;

enum pids_item Pid_items[] = {
    PIDS_ID_PID,  PIDS_ID_TGID,
    PIDS_CMDLINE, PIDS_ADDR_STACK_START };
enum rel_items { pid, tgid, cmdline, start_stack };

const char *nls_Address,
	   *nls_Offset,
	   *nls_Device,
	   *nls_Mapping,
	   *nls_Perm,
	   *nls_Inode,
	   *nls_Kbytes,
	   *nls_Mode,
	   *nls_RSS,
	   *nls_Dirty;

static void nls_initialize(void)
{
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* these are the headings shared across all options,
	   though their widths when output might differ */
	nls_Address = _("Address");
	nls_Offset  = _("Offset");
	nls_Device  = _("Device");
	nls_Mapping = _("Mapping");

	/* these headings are used only by the -X/-XX options,
	   and are not derived literally from /proc/#/smaps */
	nls_Perm    = _("Perm");
	nls_Inode   = _("Inode");

	/* these are potentially used for options other than -X/-XX, */
	nls_Kbytes  = _("Kbytes");
	nls_Mode    = _("Mode");
	nls_RSS     = _("RSS");
	nls_Dirty   = _("Dirty");
}

static int justify_print(const char *str, int width, int right)
{
	if (width < 1)
		puts(str);
	else {
		int len = strlen(str);
		if (width < len) width = len;
		printf(right ? "%*.*s " : "%-*.*s ", width, width, str);
	}
	return width;
}

static int integer_width(unsigned long number)
{
	int result;

	result = !(number > 0);
	while (number) {
		result++;
		number /= 10;
	}

	return result;
}


static void __attribute__ ((__noreturn__))
usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %s [options] PID [PID ...]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -x, --extended              show details\n"), out);
	fputs(_(" -X                          show even more details\n"), out);
	fputs(_("            WARNING: format changes according to /proc/PID/smaps\n"), out);
	fputs(_(" -XX                         show everything the kernel provides\n"), out);
	fputs(_(" -c, --read-rc               read the default rc\n"), out);
	fputs(_(" -C, --read-rc-from=<file>   read the rc from file\n"), out);
	fputs(_(" -n, --create-rc             create new default rc\n"), out);
	fputs(_(" -N, --create-rc-to=<file>   create new rc to file\n"), out);
	fputs(_("            NOTE: pid arguments are not allowed with -n, -N\n"), out);
	fputs(_(" -d, --device                show the device format\n"), out);
	fputs(_(" -q, --quiet                 do not display header and footer\n"), out);
	fputs(_(" -p, --show-path             show path in the mapping\n"), out);
	fputs(_(" -A, --range=<low>[,<high>]  limit results to the given range\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("pmap(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static char mapbuf[1024];

static unsigned long range_low;
static unsigned long range_high = ~0ul;

static int c_option = 0;
static int C_option = 0;
static int d_option = 0;
static int n_option = 0;
static int N_option = 0;
static int q_option = 0;
static int x_option = 0;
static int X_option = 0;

static int map_desc_showpath;

static unsigned shm_minor = ~0u;

static void discover_shm_minor(void)
{
	void *addr;
	int shmid;
	char mapbuf_b[256];
    FILE *fp;

    if ( (fp = fopen("/proc/self/maps", "r")) == NULL)
		return;

	/* create */
	shmid = shmget(IPC_PRIVATE, 42, IPC_CREAT | 0666);
	if (shmid == -1)
    {
		/* failed; oh well */
        fclose(fp);
		return;
    }
	/* attach */
	addr = shmat(shmid, NULL, SHM_RDONLY);
	if (addr == (void *)-1)
		goto out_destroy;

	while (fgets(mapbuf_b, sizeof mapbuf_b, fp)) {
		char perms[32];
		/* to clean up unprintables */
		char *tmp;
		unsigned long start, end;
		unsigned long long file_offset, inode;
		unsigned dev_major, dev_minor;
		if (sscanf(mapbuf_b, "%lx-%lx %31s %llx %x:%x %llu", &start,
		       &end, perms, &file_offset, &dev_major, &dev_minor, &inode) < 6)
            continue;
		tmp = strchr(mapbuf_b, '\n');
		if (tmp)
			*tmp = '\0';
		tmp = mapbuf_b;
		while (*tmp) {
			if (!isprint(*tmp))
				*tmp = '?';
			tmp++;
		}
		if (start > (unsigned long)addr)
			continue;
		if (dev_major)
			continue;
		if (perms[3] != 's')
			continue;
		if (strstr(mapbuf_b, "/SYSV")) {
			shm_minor = dev_minor;
			break;
		}
	}

	if (shmdt(addr))
		perror(_("shared memory detach"));

out_destroy:
    fclose(fp);
	if (shmctl(shmid, IPC_RMID, NULL) && errno != EINVAL)
		perror(_("shared memory remove"));

	return;
}

static const char *mapping_name(struct pids_stack *p, unsigned long addr,
				unsigned long len, const char *mapbuf_b,
				unsigned showpath, unsigned dev_major,
				unsigned dev_minor, unsigned long long inode)
{
	const char *cp;

	if (!dev_major && dev_minor == shm_minor && strstr(mapbuf_b, "/SYSV")) {
		static char shmbuf[64];
		snprintf(shmbuf, sizeof shmbuf, "  [ shmid=0x%llx ]", inode);
		return shmbuf;
	}

	cp = strrchr(mapbuf_b, '/');
	if (cp) {
		if (showpath)
			return strchr(mapbuf_b, '/');
		return cp[1] ? cp + 1 : cp;
	}

	cp = _("  [ anon ]");
	if (PIDS_VAL(start_stack, ul_int, p, Pids_info) >= addr
	&& (PIDS_VAL(start_stack, ul_int, p, Pids_info) <= addr + len))
		cp = _("  [ stack ]");
	return cp;
}


#define DETAIL_LENGTH 32
#define DETL "31"		/* for format strings */
#define NUM_LENGTH 21		/* python says: len(str(2**64)) == 20 */
#define NUML "20"		/* for format strings */
#define VMFLAGS_LENGTH 128	/* 30 2-char space-separated flags == 90+1, but be safe */
#define VMFL "127"		/* for format strings */

struct listnode {
	char description[DETAIL_LENGTH];
	char value_str[NUM_LENGTH];
	unsigned long value;
	unsigned long total;
	int max_width;
	struct listnode *next;
};

static struct listnode *listhead=NULL, *listtail=NULL, *listnode;


struct cnf_listnode {
	char description[DETAIL_LENGTH];
	struct cnf_listnode *next;
};

static struct cnf_listnode *cnf_listhead=NULL, *cnf_listnode;

static int is_unimportant (const char *s)
{
	if (strcmp(s, "AnonHugePages") == 0) return 1;
	if (strcmp(s, "KernelPageSize") == 0) return 1;
	if (strcmp(s, "MMUPageSize") == 0) return 1;
	if (strcmp(s, "Shared_Dirty") == 0) return 1;
	if (strcmp(s, "Private_Dirty") == 0) return 1;
	if (strcmp(s, "Shared_Clean") == 0) return 1;
	if (strcmp(s, "Private_Clean") == 0) return 1;
	if (strcmp(s, "VmFlags") == 0) return 1;
	return 0;
}

/* check, whether we want to display the field or not */
static int is_enabled (const char *s)
{
	if (X_option == 1) return !is_unimportant(s);

	if (c_option) {  /* taking the list of disabled fields from the rc file */

		for (cnf_listnode = cnf_listhead; cnf_listnode; cnf_listnode = cnf_listnode -> next) {
			if (!strcmp(s, cnf_listnode -> description)) return 1;
		}
		return 0;

	}

	return 1;
}


static void print_extended_maps (FILE *f)
{
	char perms[DETAIL_LENGTH], map_desc[128],
	     detail_desc[DETAIL_LENGTH], value_str[NUM_LENGTH],
	     start[NUM_LENGTH], end[NUM_LENGTH],
	     offset[NUM_LENGTH], inode[NUM_LENGTH],
	     dev[64], vmflags[VMFLAGS_LENGTH];
	int maxw1=0, maxw2=0, maxw3=0, maxw4=0, maxw5=0, maxwv=0;
	int nfields, firstmapping, footer_gap, i, maxw_;
	char *ret, *map_basename, c, has_vmflags = 0;

	ret = fgets(mapbuf, sizeof mapbuf, f);
	firstmapping = 2;
	while (ret != NULL) {
		/* === READ MAPPING === */
		map_desc[0] = '\0';
		nfields = sscanf(mapbuf,
				 "%"NUML"[0-9a-f]-%"NUML"[0-9a-f] "
				 "%"DETL"s %"NUML"[0-9a-f] "
				 "%63[0-9a-f:] %"NUML"s %127[^\n]",
				 start, end, perms, offset,
				 dev, inode, map_desc);
		/* Must read at least up to inode, else something has changed! */
		if (nfields < 6)
			xerrx(EXIT_FAILURE, _("Unknown format in smaps file!"));
		/* If line too long we dump everything else. */
		c = mapbuf[strlen(mapbuf) - 1];
		while (c != '\n') {
			fgets(mapbuf, sizeof mapbuf, f);
			if (!ret || !mapbuf[0])
				xerrx(EXIT_FAILURE, _("Unknown format in smaps file!"));
			c = mapbuf[strlen(mapbuf) - 1];
		}

		/* Store maximum widths for printing nice later */
		if (strlen(start ) > maxw1)	maxw1 = strlen(start);
		if (strlen(perms ) > maxw2)	maxw2 = strlen(perms);
		if (strlen(offset) > maxw3)	maxw3 = strlen(offset);
		if (strlen(dev   ) > maxw4)	maxw4 = strlen(dev);
		if (strlen(inode ) > maxw5)	maxw5 = strlen(inode);

		ret = fgets(mapbuf, sizeof mapbuf, f);
		nfields = ret ? sscanf(mapbuf, "%"DETL"[^:]: %"NUML"[0-9] kB %c",
					detail_desc, value_str, &c) : 0;
		listnode = listhead;
		/* === READ MAPPING DETAILS === */
		while (ret != NULL && nfields == 2) {

			if (!is_enabled(detail_desc)) goto loop_end;

			/* === CREATE LIST AND FILL description FIELD === */
			if (listnode == NULL) {
				assert(firstmapping == 2);
				listnode = calloc(1, sizeof *listnode);
				if (listnode == NULL)
					xerrx(EXIT_FAILURE, _("ERROR: memory allocation failed"));

				if (listhead == NULL) {
					assert(listtail == NULL);
					listhead = listnode;
				} else {
					listtail->next = listnode;
				}
				listtail = listnode;
				/* listnode was calloc()ed so all fields are already NULL! */
				strcpy(listnode->description, detail_desc);
				if (!q_option) listnode->max_width = strlen(detail_desc);
				else listnode->max_width = 0;
			} else {
			/* === LIST EXISTS  === */
				if (strcmp(listnode->description, detail_desc) != 0)
					xerrx(EXIT_FAILURE, "ERROR: %s %s",
					      _("inconsistent detail field in smaps file, line:\n"),
					      mapbuf);
			}
			strcpy(listnode->value_str, value_str);
			sscanf(value_str, "%lu", &listnode->value);
			if (firstmapping == 2) {
				listnode->total += listnode->value;
				if (q_option) {
					maxw_ = strlen(listnode->value_str);
					if (maxw_ > listnode->max_width)
						listnode->max_width = maxw_;
				}
			}
			listnode = listnode->next;
loop_end:
			ret = fgets(mapbuf, sizeof mapbuf, f);
			nfields = ret ? sscanf(mapbuf, "%"DETL"[^:]: %"NUML"[0-9] kB %c",
						detail_desc, value_str, &c) : 0;
		}

		/* === GET VMFLAGS === */
		nfields = ret ? sscanf(mapbuf, "VmFlags: %"VMFL"[a-z ]", vmflags) : 0;
		if (nfields == 1) {
			int len = strlen(vmflags);
			if (len > 0 && vmflags[len-1] == ' ') vmflags[--len] = '\0';
			if (len > maxwv) maxwv = len;
			if (! has_vmflags) has_vmflags = 1;
			ret = fgets(mapbuf, sizeof mapbuf, f);
		}

		if (firstmapping == 2) { /* width measurement stage, do not print anything yet */
			if (ret == NULL) {	/* once the end of file is reached ...*/
				firstmapping = 1;  /* ... we reset the file position to the beginning of the file */
				fseek(f, 0, SEEK_SET);  /* ... and repeat the process with printing enabled */
				ret = fgets(mapbuf, sizeof mapbuf, f); /* this is not ideal and needs to be redesigned one day */

				if (!q_option) {
					/* calculate width of totals */
					for (listnode=listhead; listnode!=NULL; listnode=listnode->next) {
						maxw_ = integer_width(listnode->total);
						if (maxw_ > listnode->max_width)
							listnode->max_width = maxw_;
					}
				}

			}
		} else {		 /* the maximum widths have been measured, we've already reached the printing stage */
			/* === PRINT THIS MAPPING === */

			/* Print header */
			if (firstmapping && !q_option) {

				maxw1 = justify_print(nls_Address, maxw1, 1);

				if (is_enabled(nls_Perm))
					maxw2 = justify_print(nls_Perm, maxw2, 1);

				if (is_enabled(nls_Offset))
					maxw3 = justify_print(nls_Offset, maxw3, 1);

				if (is_enabled(nls_Device))
					maxw4 = justify_print(nls_Device, maxw4, 1);

				if (is_enabled(nls_Inode))
					maxw5 = justify_print(nls_Inode, maxw5, 1);

				for (listnode=listhead; listnode!=NULL; listnode=listnode->next)
					justify_print(listnode->description, listnode->max_width, 1);

				if (has_vmflags && is_enabled("VmFlags"))
					maxwv = justify_print("VmFlags", maxwv, 1);

				if (is_enabled(nls_Mapping))
					justify_print(nls_Mapping, 0, 0);
				else
					printf("\n");
			}

			/* Print data */
			printf("%*s", maxw1, start);    /* Address field is always enabled */

			if (is_enabled(nls_Perm))
				printf(" %*s", maxw2, perms);

			if (is_enabled(nls_Offset))
				printf(" %*s", maxw3, offset);

			if (is_enabled(nls_Device))
				printf(" %*s", maxw4, dev);

			if (is_enabled(nls_Inode))
				printf(" %*s", maxw5, inode);

			for (listnode=listhead; listnode!=NULL; listnode=listnode->next)
				printf(" %*s", listnode->max_width, listnode->value_str);

			if (has_vmflags && is_enabled("VmFlags"))
				printf(" %*s", maxwv, vmflags);

			if (is_enabled(nls_Mapping)) {
				if (map_desc_showpath) {
					printf(" %s", map_desc);
				} else {
					map_basename = strrchr(map_desc, '/');
					if (!map_basename) {
						printf(" %s", map_desc);
					} else {
						printf(" %s", map_basename + 1);
					}

				}
			}

			printf("\n");

			firstmapping = 0;

		}
	}
	/* === PRINT TOTALS === */
	if (!q_option && listhead!=NULL) { /* footer enabled and non-empty */

		                            footer_gap  = maxw1 + 1; /* Address field is always enabled */
		if (is_enabled(nls_Perm  )) footer_gap += maxw2 + 1;
		if (is_enabled(nls_Offset)) footer_gap += maxw3 + 1;
		if (is_enabled(nls_Device)) footer_gap += maxw4 + 1;
		if (is_enabled(nls_Inode )) footer_gap += maxw5 + 1;

		for (i=0; i<footer_gap; i++) putc(' ', stdout);

		for (listnode=listhead; listnode!=NULL; listnode=listnode->next) {
			for (i=0; i<listnode->max_width; i++)
				putc('=', stdout);
			putc(' ', stdout);
		}

		putc('\n', stdout);

		for (i=0; i<footer_gap; i++) putc(' ', stdout);

		for (listnode=listhead; listnode!=NULL; listnode=listnode->next) {
			printf("%*lu ", listnode->max_width, listnode->total);
                        listnode->total = 0;
                }

		fputs("KB \n", stdout);
	}
	/* We don't free() the list, it's used for all PIDs passed as arguments */
}

	// variable placed here to silence compiler 'uninitialized' warning
static unsigned long start_To_Avoid_Warning;

static int one_proc (struct pids_stack *p)
{
	char buf[32];
	FILE *fp;
	unsigned long total_shared = 0ul;
	unsigned long total_private_readonly = 0ul;
	unsigned long total_private_writeable = 0ul;
	unsigned long diff = 0;
	unsigned long end;
	char perms[32] = "";
	const char *cp2 = NULL;
	unsigned long long rss = 0ull;
	unsigned long long private_dirty = 0ull;
	unsigned long long shared_dirty = 0ull;
	unsigned long long total_rss = 0ull;
	unsigned long long total_private_dirty = 0ull;
	unsigned long long total_shared_dirty = 0ull;
	int maxw1=0, maxw2=0, maxw3=0, maxw4=0, maxw5=0;

	printf("%u:   %s\n", PIDS_VAL(tgid, s_int, p, Pids_info), PIDS_VAL(cmdline, str, p, Pids_info));

	if (x_option || X_option || c_option) {
		snprintf(buf, sizeof buf, "/proc/%u/smaps", PIDS_VAL(tgid, s_int, p, Pids_info));
		if ((fp = fopen(buf, "r")) == NULL)
			return 1;
	} else {
		snprintf(buf, sizeof buf, "/proc/%u/maps", PIDS_VAL(tgid, s_int, p, Pids_info));
		if ((fp = fopen(buf, "r")) == NULL)
			return 1;
	}

	if (X_option || c_option) {
		print_extended_maps(fp);
        fclose(fp);
		return 0;
	}

	if (x_option) {
		maxw1 = 16;
		if (sizeof(long) == 4) maxw1 = 8;
		maxw2 = maxw3 = maxw4 = 7;
		maxw5 = 5;
		if (!q_option) {
			maxw1 = justify_print(nls_Address, maxw1, 0);
			maxw2 = justify_print(nls_Kbytes, maxw2, 1);
			maxw3 = justify_print(nls_RSS, maxw3, 1);
			maxw4 = justify_print(nls_Dirty, maxw4, 1);
			maxw5 = justify_print(nls_Mode, maxw5, 0);
			justify_print(nls_Mapping, 0, 0);
		}
	}

	if (d_option) {
		maxw1 = 16;
		if (sizeof(long) == 4) maxw1 = 8;
		maxw2 = 7;
		maxw3 = 5;
		maxw4 = 16;
		maxw5 = 9;
		if (!q_option) {
			maxw1 = justify_print(nls_Address, maxw1, 0);
			maxw2 = justify_print(nls_Kbytes, maxw2, 1);
			maxw3 = justify_print(nls_Mode, maxw3, 0);
			maxw4 = justify_print(nls_Offset, maxw4, 0);
			maxw5 = justify_print(nls_Device, maxw5, 0);
			justify_print(nls_Mapping, 0, 0);
		}
	}

	while (fgets(mapbuf, sizeof mapbuf, fp)) {
		/* to clean up unprintables */
		char *tmp;
		unsigned long long file_offset, inode;
		unsigned dev_major, dev_minor;
		unsigned long long smap_value;
		char smap_key[21];

		/* hex values are lower case or numeric, keys are upper */
		if (mapbuf[0] >= 'A' && mapbuf[0] <= 'Z') {
			/* Its a key */
			if (sscanf(mapbuf, "%20[^:]: %llu", smap_key, &smap_value) == 2) {
				if (strcmp("Rss", smap_key) == 0) {
					rss = smap_value;
					total_rss += smap_value;
					continue;
				}
				if (strcmp("Shared_Dirty", smap_key) == 0) {
					shared_dirty = smap_value;
					total_shared_dirty += smap_value;
					continue;
				}
				if (strcmp("Private_Dirty", smap_key) == 0) {
					private_dirty = smap_value;
					total_private_dirty += smap_value;
					continue;
				}
				if (strcmp("Swap", smap_key) == 0) {
					/* doesn't matter as long as last */
					if (cp2)
                        printf("%0*lx %*lu %*llu %*llu %*s %s\n",
                               maxw1, start_To_Avoid_Warning,
                               maxw2, (unsigned long)(diff >> 10),
                               maxw3, rss,
                               maxw4, (private_dirty + shared_dirty),
                               maxw5, perms,
                               cp2);
					/* reset some counters */
					rss = shared_dirty = private_dirty = 0ull;
                    diff = end = 0;
                    perms[0] = '\0';
                    cp2 = NULL;
					continue;
				}
			}
			/* Other keys or not a key-value pair */
			continue;
		}
		sscanf(mapbuf, "%lx-%lx %31s %llx %x:%x %llu",
		       &start_To_Avoid_Warning,
		       &end, perms, &file_offset, &dev_major, &dev_minor,
		       &inode);

		if (end - 1 < range_low)
			continue;
		if (range_high < start_To_Avoid_Warning)
			break;

		tmp = strchr(mapbuf, '\n');
		if (tmp)
			*tmp = '\0';
		tmp = mapbuf;
		while (*tmp) {
			if (!isprint(*tmp))
				*tmp = '?';
			tmp++;
		}

		diff = end - start_To_Avoid_Warning;
		if (perms[3] == 's')
			total_shared += diff;
		if (perms[3] == 'p') {
			perms[3] = '-';
			if (perms[1] == 'w')
				total_private_writeable += diff;
			else
				total_private_readonly += diff;
		}
		/* format used by Solaris 9 and procps-3.2.0+ an 'R'
		 * if swap not reserved (MAP_NORESERVE, SysV ISM
		 * shared mem, etc.)
		 */
		perms[4] = '-';
		perms[5] = '\0';

		if (x_option) {
			cp2 =
			    mapping_name(p, start_To_Avoid_Warning, diff, mapbuf, map_desc_showpath, dev_major,
					 dev_minor, inode);
			/* printed with the keys */
			continue;
		}
		if (d_option) {
			const char *cp =
			    mapping_name(p, start_To_Avoid_Warning, diff, mapbuf, map_desc_showpath, dev_major,
					 dev_minor, inode);
			printf("%0*lx %*lu %*s %0*llx %*.*s%03x:%05x %s\n",
			       maxw1, start_To_Avoid_Warning,
			       maxw2, (unsigned long)(diff >> 10),
			       maxw3, perms,
			       maxw4, file_offset,
			       (maxw5-9), (maxw5-9), " ", dev_major, dev_minor,
			       cp);
		}
		if (!x_option && !d_option) {
			const char *cp =
			    mapping_name(p, start_To_Avoid_Warning, diff, mapbuf, map_desc_showpath, dev_major,
					 dev_minor, inode);
			printf((sizeof(long) == 8)
			       ? "%016lx %6luK %s %s\n"
			       : "%08lx %6luK %s %s\n",
			       start_To_Avoid_Warning, (unsigned long)(diff >> 10), perms, cp);
		}

	}
	fclose(fp);
	if (!q_option) {
		if (x_option) {
			if (sizeof(long) == 4)
				justify_print("--------", maxw1, 0);
			else
				justify_print("----------------", maxw1, 0);
			justify_print("-------", maxw2, 1);
			justify_print("-------", maxw3, 1);
			justify_print("-------", maxw4, 1);
			printf("\n");

			printf("%-*s ", maxw1, _("total kB"));
			printf("%*ld %*llu %*llu\n",
				maxw2, (total_shared +
					total_private_writeable +
					total_private_readonly) >> 10,
				maxw3, total_rss,
				maxw4, (total_shared_dirty +
					total_private_dirty));
		}
		if (d_option) {
			printf
			    (_("mapped: %ldK    writeable/private: %ldK    shared: %ldK\n"),
			     (total_shared + total_private_writeable +
			      total_private_readonly) >> 10,
			     total_private_writeable >> 10, total_shared >> 10);
		}
		if (!x_option && !d_option) {
			if (sizeof(long) == 8)
				/* Translation Hint: keep total string length
				 * as 24 characters. Adjust %16 if needed*/
				printf(_(" total %16ldK\n"),
				       (total_shared + total_private_writeable +
					total_private_readonly) >> 10);
			else
				/* Translation Hint: keep total string length
				 * as 16 characters. Adjust %8 if needed*/
				printf(_(" total %8ldK\n"),
				       (total_shared + total_private_writeable +
					total_private_readonly) >> 10);
		}
	}

	return 0;
}

static void range_arguments(const char *optarg)
{
	char *buf, *arg1, *arg2;

	if ((buf = xstrdup(optarg)) == NULL) {
        xerrx(EXIT_FAILURE, "%s: '%s'", _("failed to parse argument"),
              (optarg?optarg:"(null)"));
    }
	arg1 = buf;
	arg2 = strchr(arg1, ',');
	if (arg2)
		*arg2++ = '\0';
	else
		arg2 = arg1;
	if (arg1[0] != '\0')
		range_low = strtoul(arg1, &arg1, 16);
	if (arg2[0] != '\0')
		range_high = strtoul(arg2, &arg2, 16);
	if (*arg1 || *arg2) {
        free(buf);
		xerrx(EXIT_FAILURE, "%s: '%s'", _("failed to parse argument"),
		      optarg);
    }
    free(buf);
}


#define MAX_CNF_LINE_LEN		1024

#define SECTION_ID_NONE			0
#define SECTION_ID_UNSUPPORTED		1

#define SECTION_STR_FIELDS_DISPLAY	"[Fields Display]"
#define SECTION_STR_FIELDS_DISPLAY_LEN	(sizeof(SECTION_STR_FIELDS_DISPLAY) - 1)
#define SECTION_ID_FIELDS_DISPLAY	2

#define SECTION_STR_MAPPING		"[Mapping]"
#define SECTION_STR_MAPPING_LEN		(sizeof(SECTION_STR_MAPPING) - 1)
#define SECTION_ID_MAPPING		3

static int config_read (char *rc_filename)
{
	FILE *f;
	char line_buf[MAX_CNF_LINE_LEN + 1];
	char tmp_buf [MAX_CNF_LINE_LEN + 1];
	char *trimmed;
	int length;
	char *tail, *token;
	int line_cnt, section_id;

	f = fopen(rc_filename, "r");

	if (!f) return 0; /* can't open the file for reading */

	line_cnt = 0;
	section_id = SECTION_ID_NONE;

	while (fgets (line_buf, MAX_CNF_LINE_LEN + 1, f)) {

		line_cnt++;

		/* get rid of the LF char */
		length = strlen(line_buf);
		if (length > 0 && line_buf[length - 1] == '\n') {
			line_buf[length - 1] = '\0';
		} else if (length == MAX_CNF_LINE_LEN) { /* no LF char -> line too long */
			xwarnx(_("config line too long - line %d"), line_cnt);
			/* ignoring the tail */
			while (fgets (tmp_buf, MAX_CNF_LINE_LEN + 1, f) &&
				(length = strlen(tmp_buf))>0 &&
				 tmp_buf[length - 1] != '\n') ;
		}

		/* trim leading whitespaces */
		trimmed = line_buf;
		while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

		/* skip comments and empty lines */
		if (*trimmed == '#' || *trimmed == '\0') continue;

		if (*trimmed == '[') { /* section */
			if        (!strncmp(trimmed, SECTION_STR_FIELDS_DISPLAY, SECTION_STR_FIELDS_DISPLAY_LEN)) {
				trimmed += SECTION_STR_FIELDS_DISPLAY_LEN;
				section_id = SECTION_ID_FIELDS_DISPLAY;
			} else if (!strncmp(trimmed, SECTION_STR_MAPPING, SECTION_STR_MAPPING_LEN)) {
				trimmed += SECTION_STR_MAPPING_LEN;
				section_id = SECTION_ID_MAPPING;
			} else {
				while (*trimmed != ']' && *trimmed != '\0') trimmed++;
				if (*trimmed == ']') {
					section_id = SECTION_ID_UNSUPPORTED;
					xwarnx(_("unsupported section found in the config - line %d"), line_cnt);
					trimmed++;
				} else {
					xwarnx(_("syntax error found in the config - line %d"), line_cnt);
				}
			}

			/* trim trailing whitespaces */
			while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

			/* skip comments and empty tails */
			if (*trimmed == '#' || *trimmed == '\0') continue;

			/* anything else found on the section line ??? */
			xwarnx(_("syntax error found in the config - line %d"), line_cnt);
		}

		switch (section_id) {
			case SECTION_ID_FIELDS_DISPLAY:
				token = strtok (trimmed, " \t");

				if (token) {
					tail = strtok (NULL, " \t");

					if (tail && *tail != '#') {
						xwarnx(_("syntax error found in the config - line %d"), line_cnt);
					}

					/* add the field in the list */
					if (!(cnf_listnode = calloc(1, sizeof *cnf_listnode))) {
						xwarnx(_("memory allocation failed"));
						fclose(f);
						return 0;
					}
					snprintf(cnf_listnode -> description, sizeof(cnf_listnode -> description), "%s", token);
					cnf_listnode -> next = cnf_listhead;
					cnf_listhead = cnf_listnode;
				}

				break;

			case SECTION_ID_MAPPING:
				token = strtok (trimmed, " \t");

				if (token) {
					tail = strtok (NULL, " \t");

					if (tail && *tail != '#') {
						xwarnx(_("syntax error found in the config - line %d"), line_cnt);
					}

					if (!strcmp(token,"ShowPath")) map_desc_showpath = !map_desc_showpath;
				}

				break;

			case SECTION_ID_UNSUPPORTED:
				break; /* ignore the content */

			default:
				xwarnx(_("syntax error found in the config - line %d"), line_cnt);
		}
        }

	fclose(f);

	return 1;
}


static int config_create (const char *rc_filename)
{
	FILE *f;

	if (rc_filename == NULL)
		return 0;

	/* check if rc exists */
	f = fopen(rc_filename, "r");

	if (f) {  /* file exists ... let user to delete/remove it first */
		fclose(f);
		xwarnx(_("the file already exists - delete or rename it first"));
		return 0;
	}

	/* file doesn't exist */

	f = fopen(rc_filename, "w");

	if (!f) return 0; /* can't open the file for writing */

	/* current rc template, might change in the future */
	fprintf(f,"# pmap's Config File\n");
	fprintf(f,"\n");
	fprintf(f,"# All the entries are case sensitive.\n");
	fprintf(f,"# Unsupported entries are ignored!\n");
	fprintf(f,"\n");
	fprintf(f,"[Fields Display]\n");
	fprintf(f,"\n");
	fprintf(f,"# To enable a field uncomment its entry\n");
	fprintf(f,"\n");
	fprintf(f,"#%s\n", nls_Perm);
	fprintf(f,"#%s\n", nls_Offset);
	fprintf(f,"#%s\n", nls_Device);
	fprintf(f,"#%s\n", nls_Inode);
	fprintf(f,"#Size\n");
	fprintf(f,"#Rss\n");
	fprintf(f,"#Pss\n");
	fprintf(f,"#Shared_Clean\n");
	fprintf(f,"#Shared_Dirty\n");
	fprintf(f,"#Private_Clean\n");
	fprintf(f,"#Private_Dirty\n");
	fprintf(f,"#Referenced\n");
	fprintf(f,"#Anonymous\n");
	fprintf(f,"#AnonHugePages\n");
	fprintf(f,"#Swap\n");
	fprintf(f,"#KernelPageSize\n");
	fprintf(f,"#MMUPageSize\n");
	fprintf(f,"#Locked\n");
	fprintf(f,"#VmFlags\n");
	fprintf(f,"#%s\n", nls_Mapping);
	fprintf(f,"\n");
	fprintf(f,"\n");
	fprintf(f,"[Mapping]\n");
	fprintf(f,"\n");
	fprintf(f,"# to show paths in the mapping column uncomment the following line\n");
	fprintf(f,"#ShowPath\n");
	fprintf(f,"\n");

	fclose(f);

	return 1;
}


/* returns rc filename based on the program_invocation_short_name */
static char *get_default_rc_filename(void)
{
	char *rc_filename;
	int rc_filename_len;
	const char *homedir;

	homedir = getenv("HOME");
	if (!homedir) {
		xwarnx(_("HOME variable undefined"));
		return NULL;
	}

	rc_filename_len = snprintf(NULL, 0, "%s/.%src", homedir, program_invocation_short_name);

	rc_filename = (char *) calloc (1, rc_filename_len + 1);
	if (!rc_filename) {
		xwarnx(_("memory allocation failed"));
		return NULL;
	}

	snprintf(rc_filename, rc_filename_len + 1, "%s/.%src", homedir, program_invocation_short_name);

	return rc_filename;
}


int main(int argc, char **argv)
{
	struct pids_fetch *pids_fetch;
	unsigned *pidlist;
	int reap_count, user_count;
	int ret = 0, c, conf_ret;
	char *rc_filename = NULL;

	static const struct option longopts[] = {
		{"extended", no_argument, NULL, 'x'},
		{"device", no_argument, NULL, 'd'},
		{"quiet", no_argument, NULL, 'q'},
		{"range", required_argument, NULL, 'A'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"read-rc", no_argument, NULL, 'c'},
		{"read-rc-from", required_argument, NULL, 'C'},
		{"create-rc", no_argument, NULL, 'n'},
		{"create-rc-to", required_argument, NULL, 'N'},
		{"show-path", no_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	nls_initialize();
	atexit(close_stdout);

	if (argc < 2)
		usage(stderr);

	while ((c = getopt_long(argc, argv, "xXrdqA:hVcC:nN:p", longopts, NULL)) != -1)
		switch (c) {
		case 'x':
			x_option = 1;
			break;
		case 'X':
			X_option++;
			break;
		case 'r':
			xwarnx(_("option -r is ignored as SunOS compatibility"));
			break;
		case 'd':
			d_option = 1;
			break;
		case 'q':
			q_option = 1;
			break;
		case 'A':
			range_arguments(optarg);
			break;
		case 'h':
			usage(stdout);
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		case 'c':
			c_option = 1;
			break;
		case 'C':
			C_option = 1;
			rc_filename = optarg;
			break;
		case 'n':
			n_option = 1;
			break;
		case 'N':
			N_option = 1;
			rc_filename = optarg;
			break;
		case 'p':
			map_desc_showpath = 1;
			break;
		case 'a':	/* Sun prints anon/swap reservations */
		case 'F':	/* Sun forces hostile ptrace-like grab */
		case 'l':	/* Sun shows unresolved dynamic names */
		case 'L':	/* Sun shows lgroup info */
		case 's':	/* Sun shows page sizes */
		case 'S':	/* Sun shows swap reservations */
		default:
			usage(stderr);
		}

	argc -= optind;
	argv += optind;

	if (c_option + C_option + d_option + n_option + N_option + x_option + !!X_option > 1)
		xerrx(EXIT_FAILURE, _("options -c, -C, -d, -n, -N, -x, -X are mutually exclusive"));

	if ((n_option || N_option) && (q_option || map_desc_showpath))
		xerrx(EXIT_FAILURE, _("options -p, -q are mutually exclusive with -n, -N"));

	if ((n_option || N_option) && argc > 0)
		xerrx(EXIT_FAILURE, _("too many arguments"));

	if (N_option) {
		if (config_create(rc_filename)) {
			xwarnx(_("rc file successfully created, feel free to edit the content"));
			return (EXIT_SUCCESS);
		} else {
			xerrx(EXIT_FAILURE, _("couldn't create the rc file"));
		}
	}

	if (n_option) {
		rc_filename = get_default_rc_filename();

		if (!rc_filename) return(EXIT_FAILURE);

		conf_ret = config_create(rc_filename); free(rc_filename);

		if (conf_ret) {
			xwarnx(_("~/.%src file successfully created, feel free to edit the content"), program_invocation_short_name);
			return (EXIT_SUCCESS);
		} else {
			xerrx(EXIT_FAILURE, _("couldn't create ~/.%src"), program_invocation_short_name);
		}
	}

	if (argc < 1)
		xerrx(EXIT_FAILURE, _("argument missing"));

	if (C_option) c_option = 1;

	if (c_option) {
		if (!C_option) rc_filename = get_default_rc_filename();

		if (!rc_filename) return(EXIT_FAILURE);

		conf_ret = config_read(rc_filename);

		if (!conf_ret) {
			if (C_option) {
				xerrx(EXIT_FAILURE, _("couldn't read the rc file"));
			} else {
				xwarnx(_("couldn't read ~/.%src"), program_invocation_short_name);
				free(rc_filename);
				return(EXIT_FAILURE);
			}
		}
	}
	if ((size_t)argc >= INT_MAX / sizeof(pid_t))
		xerrx(EXIT_FAILURE, _("too many arguments"));
	if (procps_pids_new(&Pids_info, Pid_items, 4))
		xerrx(EXIT_FAILURE, _("library failed pids statistics"));
	pidlist = xmalloc(sizeof(pid_t) * argc);

	user_count = 0;
	while (*argv) {
		char *walk = *argv++;
		char *endp;
		unsigned long pid;
		if (!strncmp("/proc/", walk, 6)) {
			walk += 6;
			/*  user allowed to do: pmap /proc/PID */
			if (*walk < '0' || *walk > '9')
				continue;
		}
		if (*walk < '0' || *walk > '9')
			usage(stderr);
		pid = strtoul(walk, &endp, 0);
		if (pid < 1ul || pid > 0x7ffffffful || *endp)
			usage(stderr);
		pidlist[user_count++] = pid;
	}

	discover_shm_minor();

	if (!(pids_fetch = procps_pids_select(Pids_info, pidlist, user_count, PIDS_SELECT_PID)))
		xerrx(EXIT_FAILURE, _("library failed pids statistics"));

	for (reap_count = 0; reap_count < pids_fetch->counts->total; reap_count++) {
		ret |= one_proc(pids_fetch->stacks[reap_count]);
	}

	free(pidlist);
	procps_pids_unref(&Pids_info);

	/* cleaning the list used for the -c/-X/-XX modes */
	for (listnode = listhead; listnode != NULL ; ) {
		listnode = listnode -> next;
		free(listhead);
		listhead = listnode;
	}

	/* cleaning the list used for the -c mode */
	for (cnf_listnode = cnf_listhead; cnf_listnode != NULL ; ) {
		cnf_listnode = cnf_listnode -> next;
		free(cnf_listhead);
		cnf_listhead = cnf_listnode;
	}

	if (reap_count < user_count)
		/* didn't find all processes asked for */
		ret |= 42;
	return ret;
}
