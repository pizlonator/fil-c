/*
 * blkid.c - User command-line interface for libblkid
 *
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#define OUTPUT_FULL		(1 << 0)
#define OUTPUT_VALUE_ONLY	(1 << 1)
#define OUTPUT_DEVICE_ONLY	(1 << 2)
#define OUTPUT_PRETTY_LIST	(1 << 3)		/* deprecated */
#define OUTPUT_UDEV_LIST	(1 << 4)		/* deprecated */
#define OUTPUT_EXPORT_LIST	(1 << 5)

#define BLKID_EXIT_NOTFOUND	2	/* token or device not found */
#define BLKID_EXIT_OTHER	4	/* bad usage or other error */
#define BLKID_EXIT_AMBIVAL	8	/* ambivalent low-level probing detected */

#include <blkid.h>

#include "ismounted.h"

#include "strutils.h"
#define OPTUTILS_EXIT_CODE	BLKID_EXIT_OTHER	/* exclusive_option() */
#include "optutils.h"
#define CLOSE_EXIT_CODE		BLKID_EXIT_OTHER	/* close_stdout() */
#include "closestream.h"

#include "nls.h"
#include "ttyutils.h"

#define XALLOC_EXIT_CODE    BLKID_EXIT_OTHER    /* x.*alloc(), xstrndup() */
#include "xalloc.h"

#include "sysfs.h"

struct blkid_control {
	int output;
	uintmax_t offset;
	uintmax_t size;
	char *show[128];
	unsigned int
		eval:1,
		gc:1,
		lookup:1,
		lowprobe:1,
		lowprobe_superblocks:1,
		lowprobe_topology:1,
		no_part_details:1,
		raw_chars:1;
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(	" %s --label <label> | --uuid <uuid>\n\n"), program_invocation_short_name);
	fprintf(out, _(	" %s [--cache-file <file>] [-ghlLv] [--output <format>] [--match-tag <tag>] \n"
			"       [--match-token <token>] [<dev> ...]\n\n"), program_invocation_short_name);
	fprintf(out, _(	" %s -p [--match-tag <tag>] [--offset <offset>] [--size <size>] \n"
			"       [--output <format>] <dev> ...\n\n"), program_invocation_short_name);
	fprintf(out, _(	" %s -i [--match-tag <tag>] [--output <format>] <dev> ...\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(	" -c, --cache-file <file>    read from <file> instead of reading from the default\n"
			"                              cache file (-c /dev/null means no cache)\n"), out);
	fputs(_(	" -d, --no-encoding          don't encode non-printing characters\n"), out);
	fputs(_(	" -g, --garbage-collect      garbage collect the blkid cache\n"), out);
	fputs(_(	" -o, --output <format>      output format; can be one of:\n"
			"                              value, device, export or full; (default: full)\n"), out);
	fputs(_(	" -k, --list-filesystems     list all known filesystems/RAIDs and exit\n"), out);
	fputs(_(	" -s, --match-tag <tag>      show specified tag(s) (default show all tags)\n"), out);
	fputs(_(	" -t, --match-token <token>  find device with a specific token (NAME=value pair)\n"), out);
	fputs(_(	" -l, --list-one             look up only first device with token specified by -t\n"), out);
	fputs(_(	" -L, --label <label>        convert LABEL to device name\n"), out);
	fputs(_(	" -U, --uuid <uuid>          convert UUID to device name\n"), out);
	fputs(          "\n", out);
	fputs(_(	"Low-level probing options:\n"), out);
	fputs(_(	" -p, --probe                low-level superblocks probing (bypass cache)\n"), out);
	fputs(_(	" -i, --info                 gather information about I/O limits\n"), out);
	fputs(_(        " -H, --hint <value>         set hint for probing function\n"), out);
	fputs(_(	" -S, --size <size>          overwrite device size\n"), out);
	fputs(_(	" -O, --offset <offset>      probe at the given offset\n"), out);
	fputs(_(	" -u, --usages <list>        filter by \"usage\" (e.g. -u filesystem,raid)\n"), out);
	fputs(_(	" -n, --match-types <list>   filter by filesystem type (e.g. -n vfat,ext3)\n"), out);
	fputs(_(	" -D, --no-part-details      don't print info from partition table\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(28));

	fputs(USAGE_ARGUMENTS, out);
	fprintf(out, USAGE_ARG_SIZE(_("<size> and <offset>")));
	fputs(USAGE_ARG_SEPARATOR, out);
	fputs(_(" <dev> specify device(s) to probe (default: all devices)\n"), out);

	fprintf(out, USAGE_MAN_TAIL("blkid(8)"));
	exit(EXIT_SUCCESS);
}

/*
 * This function does "safe" printing.  It will convert non-printable
 * ASCII characters using '^' and M- notation.
 *
 * If 'esc' is defined then escape all chars from esc by \.
 */
static void safe_print(const struct blkid_control *ctl, const char *cp, int len,
		       const char *esc)
{
	unsigned char	ch;

	if (len < 0)
		len = strlen(cp);

	while (len--) {
		ch = *cp++;
		if (!ctl->raw_chars) {
			if (ch >= 128) {
				fputs("M-", stdout);
				ch -= 128;
			}
			if ((ch < 32) || (ch == 0x7f)) {
				fputc('^', stdout);
				ch ^= 0x40; /* ^@, ^A, ^B; ^? for DEL */

			} else if (esc && strchr(esc, ch))
				fputc('\\', stdout);
		}
		fputc(ch, stdout);
	}
}

static int pretty_print_word(const char *str, int max_len,
			     int left_len, int overflow_nl)
{
	int len = strlen(str) + left_len;
	int ret = 0;

	fputs(str, stdout);
	if (overflow_nl && len > max_len) {
		fputc('\n', stdout);
		len = 0;
	} else if (len > max_len)
		ret = len - max_len;
	do {
		fputc(' ', stdout);
	} while (len++ < max_len);
	return ret;
}

static void pretty_print_line(const char *device, const char *fs_type,
			      const char *label, const char *mtpt,
			      const char *uuid)
{
	static int device_len = 10, fs_type_len = 7;
	static int label_len = 8, mtpt_len = 14;
	static int term_width = -1;
	int len, w;

	if (term_width < 0) {
		term_width = get_terminal_width(80);
	}
	if (term_width > 80) {
		term_width -= 80;
		w = term_width / 10;
		if (w > 8)
			w = 8;
		term_width -= 2*w;
		label_len += w;
		fs_type_len += w;
		w = term_width/2;
		device_len += w;
		mtpt_len +=w;
	}

	len = pretty_print_word(device, device_len, 0, 1);
	len = pretty_print_word(fs_type, fs_type_len, len, 0);
	len = pretty_print_word(label, label_len, len, 0);
	pretty_print_word(mtpt, mtpt_len, len, 0);

	fputs(uuid, stdout);
	fputc('\n', stdout);
}

static void pretty_print_dev(blkid_dev dev)
{
	blkid_tag_iterate	iter;
	const char		*type, *value, *devname;
	const char		*uuid = "", *fs_type = "", *label = "";
	int			len, mount_flags;
	char			mtpt[80];
	int			retval;

	if (dev == NULL) {
		pretty_print_line("device", "fs_type", "label",
				  "mount point", "UUID");
		for (len=get_terminal_width(0)-1; len > 0; len--)
			fputc('-', stdout);
		fputc('\n', stdout);
		return;
	}

	devname = blkid_dev_devname(dev);
	if (access(devname, F_OK))
		return;

	/* Get the uuid, label, type */
	iter = blkid_tag_iterate_begin(dev);
	while (blkid_tag_next(iter, &type, &value) == 0) {
		if (!strcmp(type, "UUID"))
			uuid = value;
		if (!strcmp(type, "TYPE"))
			fs_type = value;
		if (!strcmp(type, "LABEL"))
			label = value;
	}
	blkid_tag_iterate_end(iter);

	/* Get the mount point */
	mtpt[0] = 0;
	retval = check_mount_point(devname, &mount_flags, mtpt, sizeof(mtpt));
	if (retval == 0) {
		const char *msg = NULL;

		if (mount_flags & MF_MOUNTED) {
			if (!mtpt[0])
				msg = _("(mounted, mtpt unknown)");
		} else if (mount_flags & MF_BUSY)
			msg = _("(in use)");
		else
			msg = _("(not mounted)");

		if (msg)
			xstrncpy(mtpt, msg, sizeof(mtpt));
	}

	pretty_print_line(devname, fs_type, label, mtpt, uuid);
}

static void print_udev_format(const char *name, const char *value)
{
	char enc[265], safe[256];
	size_t namelen = strlen(name);

	*safe = *enc = '\0';

	if (!strcmp(name, "TYPE")
	    || !strcmp(name, "VERSION")
	    || !strcmp(name, "SYSTEM_ID")
	    || !strcmp(name, "PUBLISHER_ID")
	    || !strcmp(name, "APPLICATION_ID")
	    || !strcmp(name, "BOOT_SYSTEM_ID")
	    || !strcmp(name, "VOLUME_ID")
	    || !strcmp(name, "LOGICAL_VOLUME_ID")
	    || !strcmp(name, "VOLUME_SET_ID")
	    || !strcmp(name, "DATA_PREPARER_ID")) {
		blkid_encode_string(value, enc, sizeof(enc));
		printf("ID_FS_%s=%s\n", name, enc);

	} else if (!strcmp(name, "UUID") ||
		 !strncmp(name, "LABEL", 5) ||
		 !strcmp(name, "UUID_SUB")) {

		blkid_safe_string(value, safe, sizeof(safe));
		printf("ID_FS_%s=%s\n", name, safe);

		blkid_encode_string(value, enc, sizeof(enc));
		printf("ID_FS_%s_ENC=%s\n", name, enc);

	} else if (!strcmp(name, "PTUUID")) {
		printf("ID_PART_TABLE_UUID=%s\n", value);

	} else if (!strcmp(name, "PTTYPE")) {
		printf("ID_PART_TABLE_TYPE=%s\n", value);

	} else if (!strcmp(name, "PART_ENTRY_NAME") ||
		  !strcmp(name, "PART_ENTRY_TYPE")) {

		blkid_encode_string(value, enc, sizeof(enc));
		printf("ID_%s=%s\n", name, enc);

	} else if (!strncmp(name, "PART_ENTRY_", 11))
		printf("ID_%s=%s\n", name, value);

	else if (namelen >= 15 && (
		   !strcmp(name + (namelen - 12), "_SECTOR_SIZE") ||
		   !strcmp(name + (namelen - 8), "_IO_SIZE") ||
		   !strcmp(name, "ALIGNMENT_OFFSET")))
			printf("ID_IOLIMIT_%s=%s\n", name, value);
	else
		printf("ID_FS_%s=%s\n", name, value);
}

static int has_item(const struct blkid_control *ctl, const char *item)
{
	char * const *p;

	for (p = ctl->show; *p != NULL; p++)
		if (!strcmp(item, *p))
			return 1;
	return 0;
}

static void print_value(const struct blkid_control *ctl, int num,
			const char *devname, const char *value,
			const char *name, size_t valsz)
{
	if (ctl->output & OUTPUT_VALUE_ONLY) {
		fputs(value, stdout);
		fputc('\n', stdout);

	} else if (ctl->output & OUTPUT_UDEV_LIST) {
		print_udev_format(name, value);

	} else if (ctl->output & OUTPUT_EXPORT_LIST) {
		if (num == 1 && devname)
			printf("DEVNAME=%s\n", devname);
		fputs(name, stdout);
		fputs("=", stdout);
		safe_print(ctl, value, valsz, " \\\"'$`<>");
		fputs("\n", stdout);

	} else {
		if (num == 1 && devname)
			printf("%s:", devname);
		fputs(" ", stdout);
		fputs(name, stdout);
		fputs("=\"", stdout);
		safe_print(ctl, value, valsz, "\"\\");
		fputs("\"", stdout);
	}
}

static void print_tags(const struct blkid_control *ctl, blkid_dev dev)
{
	blkid_tag_iterate	iter;
	const char		*type, *value, *devname;
	int			num = 1;
	static int		first = 1;

	if (!dev)
		return;

	if (ctl->output & OUTPUT_PRETTY_LIST) {
		pretty_print_dev(dev);
		return;
	}

	devname = blkid_dev_devname(dev);

	if (ctl->output & OUTPUT_DEVICE_ONLY) {
		printf("%s\n", devname);
		return;
	}

	iter = blkid_tag_iterate_begin(dev);
	while (blkid_tag_next(iter, &type, &value) == 0) {
		if (ctl->show[0] && !has_item(ctl, type))
			continue;

		if (num == 1 && !first &&
		    (ctl->output & (OUTPUT_UDEV_LIST | OUTPUT_EXPORT_LIST)))
			/* add extra line between output from more devices */
			fputc('\n', stdout);

		print_value(ctl, num++, devname, value, type, strlen(value));
	}
	blkid_tag_iterate_end(iter);

	if (num > 1) {
		if (!(ctl->output & (OUTPUT_VALUE_ONLY | OUTPUT_UDEV_LIST |
						OUTPUT_EXPORT_LIST)))
			printf("\n");
		first = 0;
	}
}


static int append_str(char **res, size_t *sz, const char *a, const char *b)
{
	char *str = *res;
	size_t asz = a ? strlen(a) : 0;
	size_t bsz = b ? strlen(b) : 0;
	size_t len = *sz + asz + bsz;

	if (!len)
		return -1;

	*res = str = xrealloc(str, len + 1);
	str += *sz;

	if (a)
		str = mempcpy(str, a, asz);
	if (b)
		str = mempcpy(str, b, bsz);

	*str = '\0';
	*sz = len;
	return 0;
}

/*
 * Compose and print ID_FS_AMBIVALENT for udev
 */
static int print_udev_ambivalent(blkid_probe pr)
{
	char *val = NULL;
	size_t valsz = 0;
	int count = 0, rc = -1;

	while (!blkid_do_probe(pr)) {
		const char *usage_txt = NULL, *type = NULL, *version = NULL;
		char enc[256];

		blkid_probe_lookup_value(pr, "USAGE", &usage_txt, NULL);
		blkid_probe_lookup_value(pr, "TYPE", &type, NULL);
		blkid_probe_lookup_value(pr, "VERSION", &version, NULL);

		if (!usage_txt || !type)
			continue;

		blkid_encode_string(usage_txt, enc, sizeof(enc));
		if (append_str(&val, &valsz, enc, ":"))
			goto done;

		blkid_encode_string(type, enc, sizeof(enc));
		if (append_str(&val, &valsz, enc, version ? ":" : " "))
			goto done;

		if (version) {
			blkid_encode_string(version, enc, sizeof(enc));
			if (append_str(&val, &valsz, enc, " "))
				goto done;
		}
		count++;
	}

	if (count > 1) {
		*(val + valsz - 1) = '\0';		/* rem tailing whitespace */
		printf("ID_FS_AMBIVALENT=%s\n", val);
		rc = 0;
	}
done:
	free(val);
	return rc;
}

static int lowprobe_superblocks(blkid_probe pr, struct blkid_control *ctl)
{
	struct stat st;
	int rc, fd = blkid_probe_get_fd(pr);

	if (fd < 0 || fstat(fd, &st))
		return -1;

	blkid_probe_enable_partitions(pr, 1);

	if (!S_ISCHR(st.st_mode) && blkid_probe_get_size(pr) <= 1024 * 1440 &&
	    blkid_probe_is_wholedisk(pr)) {
		/*
		 * check if the small disk is partitioned, if yes then
		 * don't probe for filesystems.
		 */
		blkid_probe_enable_superblocks(pr, 0);

		rc = blkid_do_fullprobe(pr);
		if (rc < 0)
			return rc;	/* -1 = error, 1 = nothing, 0 = success */

		if (blkid_probe_lookup_value(pr, "PTTYPE", NULL, NULL) == 0)
			return 0;	/* partition table detected */
	}

	if (!ctl->no_part_details)
		blkid_probe_set_partitions_flags(pr, BLKID_PARTS_ENTRY_DETAILS);
	blkid_probe_enable_superblocks(pr, 1);

	return blkid_do_safeprobe(pr);
}

static int lowprobe_topology(blkid_probe pr)
{
	/* enable topology probing only */
	blkid_probe_enable_topology(pr, 1);

	blkid_probe_enable_superblocks(pr, 0);
	blkid_probe_enable_partitions(pr, 0);

	return blkid_do_fullprobe(pr);
}

static int lowprobe_device(blkid_probe pr, const char *devname,
			   struct blkid_control *ctl)
{
	const char *data;
	const char *name;
	int nvals = 0, n, num = 1;
	size_t len;
	int fd;
	int rc = 0;
	static int first = 1;

	fd = open(devname, O_RDONLY|O_CLOEXEC|O_NONBLOCK);
	if (fd < 0) {
		warn(_("error: %s"), devname);
		return BLKID_EXIT_NOTFOUND;
	}
	errno = 0;
	if (blkid_probe_set_device(pr, fd, ctl->offset, ctl->size)) {
		if (errno)
			warn(_("error: %s"), devname);
		goto done;
	}

	if (ctl->lowprobe_topology)
		rc = lowprobe_topology(pr);
	if (rc >= 0 && ctl->lowprobe_superblocks)
		rc = lowprobe_superblocks(pr, ctl);
	if (rc < 0)
		goto done;

	if (!rc)
		nvals = blkid_probe_numof_values(pr);

	if (nvals && !first && ctl->output & (OUTPUT_UDEV_LIST | OUTPUT_EXPORT_LIST))
		/* add extra line between output from devices */
		fputc('\n', stdout);

	if (nvals && (ctl->output & OUTPUT_DEVICE_ONLY)) {
		printf("%s\n", devname);
		goto done;
	}

	for (n = 0; n < nvals; n++) {
		if (blkid_probe_get_value(pr, n, &name, &data, &len))
			continue;
		if (ctl->show[0] && !has_item(ctl, name))
			continue;
		len = strnlen(data, len);
		print_value(ctl, num++, devname, data, name, len);
	}

	if (first)
		first = 0;

	if (nvals >= 1 && !(ctl->output & (OUTPUT_VALUE_ONLY |
					OUTPUT_UDEV_LIST | OUTPUT_EXPORT_LIST)))
		printf("\n");
done:
	if (rc == -2) {
		if (ctl->output & OUTPUT_UDEV_LIST)
			print_udev_ambivalent(pr);
		else
			warnx(_("%s: ambivalent result (probably more "
				"filesystems on the device, use wipefs(8) "
				"to see more details)"),
				devname);
	}
	close(fd);

	if (rc == -2)
		return BLKID_EXIT_AMBIVAL;	/* ambivalent probing result */
	if (!nvals)
		return BLKID_EXIT_NOTFOUND;	/* nothing detected */

	return 0;		/* success */
}

/* converts comma separated list to BLKID_USAGE_* mask */
static int list_to_usage(const char *list, int *flag)
{
	int mask = 0;
	const char *word = NULL, *p = list;

	if (p && strncmp(p, "no", 2) == 0) {
		*flag = BLKID_FLTR_NOTIN;
		p += 2;
	}
	if (!p || !*p)
		goto err;
	while(p) {
		word = p;
		p = strchr(p, ',');
		if (p)
			p++;
		if (!strncmp(word, "filesystem", 10))
			mask |= BLKID_USAGE_FILESYSTEM;
		else if (!strncmp(word, "raid", 4))
			mask |= BLKID_USAGE_RAID;
		else if (!strncmp(word, "crypto", 6))
			mask |= BLKID_USAGE_CRYPTO;
		else if (!strncmp(word, "other", 5))
			mask |= BLKID_USAGE_OTHER;
		else
			goto err;
	}
	return mask;
err:
	*flag = 0;
	warnx(_("unknown keyword in -u <list> argument: '%s'"),
			word ? word : list);
	exit(BLKID_EXIT_OTHER);
}

/* converts comma separated list to types[] */
static char **list_to_types(const char *list, int *flag)
{
	int i;
	const char *p = list;
	char **res = NULL;

	if (p && strncmp(p, "no", 2) == 0) {
		*flag = BLKID_FLTR_NOTIN;
		p += 2;
	}
	if (!p || !*p) {
		warnx(_("error: -u <list> argument is empty"));
		goto err;
	}
	for (i = 1; p && (p = strchr(p, ',')); i++, p++);

	res = xcalloc(i + 1, sizeof(char *));
	p = *flag & BLKID_FLTR_NOTIN ? list + 2 : list;
	i = 0;

	while(p) {
		const char *word = p;
		p = strchr(p, ',');
		res[i++] = p ? xstrndup(word, p - word) : xstrdup(word);
		if (p)
			p++;
	}
	res[i] = NULL;
	return res;
err:
	*flag = 0;
	free(res);
	exit(BLKID_EXIT_OTHER);
}

static void free_types_list(char *list[])
{
	char **n;

	if (!list)
		return;
	for (n = list; *n; n++)
		free(*n);
	free(list);
}

int main(int argc, char **argv)
{
	struct blkid_control ctl = { .output = OUTPUT_FULL, 0 };
	blkid_cache cache = NULL;
	char **devices = NULL;
	char *search_type = NULL, *search_value = NULL;
	char *read = NULL, *hint = NULL;
	int fltr_usage = 0;
	char **fltr_type = NULL;
	int fltr_flag = BLKID_FLTR_ONLYIN;
	unsigned int numdev = 0, numtag = 0;
	int err = BLKID_EXIT_OTHER;
	unsigned int i;
	int c;

	static const struct option longopts[] = {
		{ "cache-file",	      required_argument, NULL, 'c' },
		{ "no-encoding",      no_argument,	 NULL, 'd' },
		{ "no-part-details",  no_argument,       NULL, 'D' },
		{ "garbage-collect",  no_argument,	 NULL, 'g' },
		{ "output",	      required_argument, NULL, 'o' },
		{ "list-filesystems", no_argument,	 NULL, 'k' },
		{ "match-tag",	      required_argument, NULL, 's' },
		{ "match-token",      required_argument, NULL, 't' },
		{ "list-one",	      no_argument,	 NULL, 'l' },
		{ "label",	      required_argument, NULL, 'L' },
		{ "uuid",	      required_argument, NULL, 'U' },
		{ "probe",	      no_argument,	 NULL, 'p' },
		{ "hint",	      required_argument, NULL, 'H' },
		{ "info",	      no_argument,	 NULL, 'i' },
		{ "size",	      required_argument, NULL, 'S' },
		{ "offset",	      required_argument, NULL, 'O' },
		{ "usages",	      required_argument, NULL, 'u' },
		{ "match-types",      required_argument, NULL, 'n' },
		{ "version",	      no_argument,	 NULL, 'V' },
		{ "help",	      no_argument,       NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'n','u' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(BLKID_EXIT_OTHER);

	while ((c = getopt_long (argc, argv,
			    "c:DdgH:hilL:n:ko:O:ps:S:t:u:U:w:Vv", longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'c':
			read = optarg;
			break;
		case 'd':
			ctl.raw_chars = 1;
			break;
		case 'D':
			ctl.no_part_details = 1;
			break;
		case 'H':
			hint = optarg;
			break;
		case 'L':
			ctl.eval = 1;
			search_value = xstrdup(optarg);
			search_type = xstrdup("LABEL");
			break;
		case 'n':
			fltr_type = list_to_types(optarg, &fltr_flag);
			break;
		case 'u':
			fltr_usage = list_to_usage(optarg, &fltr_flag);
			break;
		case 'U':
			ctl.eval = 1;
			search_value = xstrdup(optarg);
			search_type = xstrdup("UUID");
			break;
		case 'i':
			ctl.lowprobe_topology = 1;
			break;
		case 'l':
			ctl.lookup = 1;
			break;
		case 'g':
			ctl.gc = 1;
			break;
		case 'k':
		{
			size_t idx = 0;
			const char *name = NULL;

			while (blkid_superblocks_get_name(idx++, &name, NULL) == 0)
				printf("%s\n", name);
			exit(EXIT_SUCCESS);
		}
		case 'o':
			if (!strcmp(optarg, "value"))
				ctl.output = OUTPUT_VALUE_ONLY;
			else if (!strcmp(optarg, "device"))
				ctl.output = OUTPUT_DEVICE_ONLY;
			else if (!strcmp(optarg, "list"))
				ctl.output = OUTPUT_PRETTY_LIST;	/* deprecated */
			else if (!strcmp(optarg, "udev"))
				ctl.output = OUTPUT_UDEV_LIST;
			else if (!strcmp(optarg, "export"))
				ctl.output = OUTPUT_EXPORT_LIST;
			else if (!strcmp(optarg, "full"))
				ctl.output = 0;
			else
				errx(BLKID_EXIT_OTHER, _("unsupported output format %s"), optarg);
			break;
		case 'O':
			ctl.offset = strtosize_or_err(optarg, _("invalid offset argument"));
			break;
		case 'p':
			ctl.lowprobe_superblocks = 1;
			break;
		case 's':
			if (numtag + 1 >= sizeof(ctl.show) / sizeof(*ctl.show)) {
				warnx(_("Too many tags specified"));
				errtryhelp(err);
			}
			ctl.show[numtag++] = optarg;
			break;
		case 'S':
			ctl.size = strtosize_or_err(optarg, _("invalid size argument"));
			break;
		case 't':
			if (search_type) {
				warnx(_("Can only search for "
					"one NAME=value pair"));
				errtryhelp(err);
			}
			if (blkid_parse_tag_string(optarg,
						   &search_type,
						   &search_value)) {
				warnx(_("-t needs NAME=value pair"));
				errtryhelp(err);
			}
			break;
		case 'V':
		case 'v':
			fprintf(stdout, _("%s from %s  (libblkid %s, %s)\n"),
				program_invocation_short_name, PACKAGE_STRING,
				LIBBLKID_VERSION, LIBBLKID_DATE);
			err = 0;
			goto exit;
		case 'w':
			/* ignore - backward compatibility */
			break;
		case 'h':
			usage();
			break;
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (ctl.lowprobe_topology || ctl.lowprobe_superblocks)
		ctl.lowprobe = 1;

	/* The rest of the args are device names */
	if (optind < argc) {
		devices = xcalloc(argc - optind, sizeof(char *));
		while (optind < argc) {
			char *dev = argv[optind++];
			struct stat sb;

			if (stat(dev, &sb) != 0)
				continue;
			else if (S_ISBLK(sb.st_mode))
				;
			else if (S_ISREG(sb.st_mode))
				;
			else if (S_ISCHR(sb.st_mode)) {
				char buf[PATH_MAX];

				if (!sysfs_chrdev_devno_to_devname(
						sb.st_rdev, buf, sizeof(buf)))
					continue;
				if (strncmp(buf, "ubi", 3) != 0)
					continue;
			} else
				continue;

			devices[numdev++] = dev;
		}

		if (!numdev) {
			/* only unsupported devices specified */
			err = BLKID_EXIT_NOTFOUND;
			goto exit;
		}
	}

	/* convert LABEL/UUID lookup to evaluate request */
	if (ctl.lookup && ctl.output == OUTPUT_DEVICE_ONLY && search_type &&
	    (!strcmp(search_type, "LABEL") || !strcmp(search_type, "UUID"))) {
		ctl.eval = 1;
		ctl.lookup = 0;
	}

	if (!ctl.lowprobe && !ctl.eval && blkid_get_cache(&cache, read) < 0)
		goto exit;

	if (ctl.gc) {
		blkid_gc_cache(cache);
		err = 0;
		goto exit;
	}
	err = BLKID_EXIT_NOTFOUND;

	if (ctl.eval == 0 && (ctl.output & OUTPUT_PRETTY_LIST)) {
		if (ctl.lowprobe)
			errx(BLKID_EXIT_OTHER,
			     _("The low-level probing mode does not "
			       "support 'list' output format"));
		pretty_print_dev(NULL);
	}

	if (ctl.lowprobe) {
		/*
		 * Low-level API
		 */
		blkid_probe pr;

		if (!numdev)
			errx(BLKID_EXIT_OTHER,
			     _("The low-level probing mode "
			       "requires a device"));

		/* automatically enable 'export' format for I/O Limits */
		if (!ctl.output  && ctl.lowprobe_topology)
			ctl.output = OUTPUT_EXPORT_LIST;

		pr = blkid_new_probe();
		if (!pr)
			goto exit;
		if (hint && blkid_probe_set_hint(pr, hint, 0) != 0) {
			warn(_("Failed to use probing hint: %s"), hint);
			goto exit;
		}

		if (ctl.lowprobe_superblocks) {
			blkid_probe_set_superblocks_flags(pr,
				BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID |
				BLKID_SUBLKS_TYPE | BLKID_SUBLKS_SECTYPE |
				BLKID_SUBLKS_USAGE | BLKID_SUBLKS_VERSION |
				BLKID_SUBLKS_FSINFO);

			if (fltr_usage &&
			    blkid_probe_filter_superblocks_usage(pr, fltr_flag, fltr_usage))
				goto exit;

			else if (fltr_type &&
				 blkid_probe_filter_superblocks_type(pr, fltr_flag, fltr_type))
				goto exit;
		}

		for (i = 0; i < numdev; i++) {
			err = lowprobe_device(pr, devices[i], &ctl);
			if (err)
				break;
		}
		blkid_free_probe(pr);
	} else if (ctl.eval) {
		/*
		 * Evaluate API
		 */
		char *res = blkid_evaluate_tag(search_type, search_value, NULL);
		if (res) {
			err = 0;
			printf("%s\n", res);
		}
	} else if (ctl.lookup) {
		/*
		 * Classic (cache based) API
		 */
		blkid_dev dev;

		if (!search_type)
			errx(BLKID_EXIT_OTHER,
			     _("The lookup option requires a "
			       "search type specified using -t"));
		/* Load any additional devices not in the cache */
		for (i = 0; i < numdev; i++)
			blkid_get_dev(cache, devices[i], BLKID_DEV_NORMAL);

		if ((dev = blkid_find_dev_with_tag(cache, search_type,
						   search_value))) {
			print_tags(&ctl, dev);
			err = 0;
		}
	/* If we didn't specify a single device, show all available devices */
	} else if (!numdev) {
		blkid_dev_iterate	iter;
		blkid_dev		dev;

		blkid_probe_all(cache);

		iter = blkid_dev_iterate_begin(cache);
		blkid_dev_set_search(iter, search_type, search_value);
		while (blkid_dev_next(iter, &dev) == 0) {
			dev = blkid_verify(cache, dev);
			if (!dev)
				continue;
			print_tags(&ctl, dev);
			err = 0;
		}
		blkid_dev_iterate_end(iter);
	/* Add all specified devices to cache (optionally display tags) */
	} else for (i = 0; i < numdev; i++) {
		blkid_dev dev = blkid_get_dev(cache, devices[i],
						  BLKID_DEV_NORMAL);

		if (dev) {
			if (search_type &&
			    !blkid_dev_has_tag(dev, search_type,
					       search_value))
				continue;
			print_tags(&ctl, dev);
			err = 0;
		}
	}

exit:
	free(search_type);
	free(search_value);
	free_types_list(fltr_type);
	if (!ctl.lowprobe && !ctl.eval)
		blkid_put_cache(cache);
	free(devices);
	return err;
}
