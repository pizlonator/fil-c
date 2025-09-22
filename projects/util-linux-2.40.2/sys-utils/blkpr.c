/*
 * blkpr.c -- persistent reservations on a block device.
 *
 * Copyright (C) 2021-2022 zhenwei pi <pizhenwei@bytedance.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This program uses IOC_PR_XXX ioctl to run persistent reservations
 * command on a block device if the device supports it.
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <locale.h>
#include <sys/ioctl.h>
#include <linux/pr.h>

#include "nls.h"
#include "c.h"
#include "closestream.h"
#include "strutils.h"
#include "xalloc.h"

struct type_string {
	int type;
	char *str;
	char *desc;
};

/* This array should keep align with enum pr_type of linux/types.h */
static const struct type_string pr_type[] = {
	{PR_WRITE_EXCLUSIVE,           "write-exclusive",
	"  * write-exclusive: Only the initiator that owns the reservation can\n"
	"    write to the device. Any initiator can read from the device.\n"},

	{PR_EXCLUSIVE_ACCESS,          "exclusive-access",
	"  * exclusive-access: Only the initiator that owns the reservation can\n"
	"    access the device.\n"},

	{PR_WRITE_EXCLUSIVE_REG_ONLY,  "write-exclusive-reg-only",
	"  * write-exclusive-reg-only: Only initiators with a registered key can\n"
	"    write to the device, any initiator can read from the device.\n"},

	{PR_EXCLUSIVE_ACCESS_REG_ONLY, "exclusive-access-reg-only",
	"  * exclusive-access-reg-only: Only initiators with a registered key can\n"
	"    access the device.\n"},

	{PR_WRITE_EXCLUSIVE_ALL_REGS,  "write-exclusive-all-regs",
	"  * write-exclusive-all-regs: Only initiators with a registered key can\n"
	"    write to the device.  Any initiator can read from the device.  All\n"
	"    initiators with a registered key are considered reservation holders.\n"
	"    Please reference the SPC spec on the meaning of a reservation holder\n"
	"    if you want to use this type.\n"},

	{PR_EXCLUSIVE_ACCESS_ALL_REGS, "exclusive-access-all-regs",
	"  * exclusive-access-all-regs: Only initiators with a registered key can\n"
	"    access the device.  All initiators with a registered key are considered\n"
	"    reservation holders. Please reference the SPC spec on the meaning of\n"
	"    a reservation holder if you want to use this type.\n"}
};

static const struct type_string pr_command[] = {
	{IOC_PR_REGISTER,      "register",
	"  * register: This command registers a new reservation if the key argument\n"
	"    is non-null. If no existing reservation exists oldkey must be zero, if\n"
	"    an existing reservation should be replaced oldkey must contain the old\n"
	"    reservation key. If the key argument is 0 it unregisters the existing\n"
	"    reservation passed in oldkey.\n"
	},

	{IOC_PR_RESERVE,       "reserve",
	"  * reserve: This command reserves the device and thus restricts access for\n"
	"    other devices based on the type argument.  The key argument must be\n"
	"    the existing reservation key for the device as acquired by the register,\n"
	"    preempt, preempt-abort commands.\n"},

	{IOC_PR_RELEASE,       "release",
	"  * release: This command releases the reservation specified by key and flags\n"
	"    and thus removes any access restriction implied by it.\n"},

	{IOC_PR_PREEMPT,       "preempt",
	"  * preempt: This command releases the existing reservation referred to by\n"
	"    old_key and replaces it with a new reservation of type for the\n"
	"    reservation key key.\n"},

	{IOC_PR_PREEMPT_ABORT, "preempt-abort",
	"  * preempt-abort: This command works like preempt except that it also aborts\n"
	"    any outstanding command sent over a connection identified by oldkey.\n"},

	{IOC_PR_CLEAR,         "clear",
	"  * clear: This command unregisters both key and any other reservation\n"
	"    key registered with the device and drops any existing reservation.\n"},
};

static const struct type_string pr_flag[] = {
	{PR_FL_IGNORE_KEY, "ignore-key",
	"  * ignore-key: Ignore the existing reservation key.  This is commonly\n"
	"    supported for register command, and some implementation may support\n"
	"    the flag for reserve command.\n"}
};

static void print_type(FILE *out, const struct type_string *ts, size_t nmem)
{
	size_t i;

	for (i = 0; i < nmem; i++) {
		fprintf(out, "%s\n", ts[i].desc);
	}
}


static int parse_type_by_str(const struct type_string *ts, int nmem, char *pattern)
{
	int i;

	for (i = 0; i < nmem; i++) {
		if (!strcmp(ts[i].str, pattern))
			return ts[i].type;
	}

	return -1;
}


#define PRINT_SUPPORTED(XX) \
	static void print_##XX(FILE *out) \
	{ print_type(out, XX, ARRAY_SIZE(XX)); }

#define PARSE(XX) \
	static int parse_##XX(char *pattern) \
	{ return parse_type_by_str(XX, ARRAY_SIZE(XX), pattern); }

PRINT_SUPPORTED(pr_type)
PRINT_SUPPORTED(pr_command)
PRINT_SUPPORTED(pr_flag)

PARSE(pr_type)
PARSE(pr_command)
PARSE(pr_flag)

static int do_pr(char *path, uint64_t key, uint64_t oldkey, int op, int type, int flag)
{
	struct pr_registration pr_reg;
	struct pr_reservation pr_res;
	struct pr_preempt pr_prt;
	struct pr_clear pr_clr;
	int fd, ret;

	fd = open(path, O_RDWR);
	if (fd < 0)
		err(EXIT_FAILURE, _("cannot open %s"), path);

	switch (op) {
	case IOC_PR_REGISTER:
		pr_reg.old_key = oldkey;
		pr_reg.new_key = key;
		pr_reg.flags = flag;
		ret = ioctl(fd, op, &pr_reg);
		break;
	case IOC_PR_RESERVE:
	case IOC_PR_RELEASE:
		pr_res.key = key;
		pr_res.type = type;
		pr_res.flags = flag;
		ret = ioctl(fd, op, &pr_res);
		break;
	case IOC_PR_PREEMPT:
	case IOC_PR_PREEMPT_ABORT:
		pr_prt.old_key = oldkey;
		pr_prt.new_key = key;
		pr_prt.type = type;
		pr_prt.flags = flag;
		ret = ioctl(fd, op, &pr_prt);
		break;
	case IOC_PR_CLEAR:
		pr_clr.key = key;
		pr_clr.flags = flag;
		ret = ioctl(fd, op, &pr_clr);
		break;
	default:
		errno = EINVAL;
		err(EXIT_FAILURE, _("unknown command"));
	}

	close(fd);
	if (ret < 0)
		err(EXIT_FAILURE, _("pr ioctl failed"));
	if (ret > 0)
		errx(EXIT_FAILURE, _("error code 0x%x, for more detailed information see specification of device model."), ret);

	return ret;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] <device>\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Persistent reservations on a device.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -c, --command <cmd>      command of persistent reservations\n"), out);
	fputs(_(" -k, --key <num>          key to operate\n"), out);
	fputs(_(" -K, --oldkey <num>       old key to operate\n"), out);
	fputs(_(" -f, --flag <flag>        command flag\n"), out);
	fputs(_(" -t, --type <type>        command type\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fputs(USAGE_ARGUMENTS, out);

	fputs(_(" <cmd> is an command, available command:\n"), out);
	print_pr_command(out);

	fputs(_(" <flag> is a command flag, available flags:\n"), out);
	print_pr_flag(out);

	fputs(_(" <type> is a command type, available types:\n"), out);
	print_pr_type(out);

	fprintf(out, USAGE_MAN_TAIL("blkpr(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int c;
	char *path;
	uint64_t key = 0, oldkey = 0;
	int command = -1, type = -1, flag = 0;

	static const struct option longopts[] = {
	    { "help",            no_argument,       NULL, 'h' },
	    { "version",         no_argument,       NULL, 'V' },
	    { "command",         required_argument, NULL, 'c' },
	    { "key",             required_argument, NULL, 'k' },
	    { "oldkey",          required_argument, NULL, 'K' },
	    { "flag",            required_argument, NULL, 'f' },
	    { "type",            required_argument, NULL, 't' },
	    { NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	errno = EINVAL;
	while ((c = getopt_long(argc, argv, "hVc:k:K:f:t:", longopts, NULL)) != -1) {
		switch(c) {
		case 'k':
			key = strtosize_or_err(optarg,
					_("failed to parse key"));
			break;
		case 'K':
			oldkey = strtosize_or_err(optarg,
					_("failed to parse old key"));
			break;
		case 'c':
			command = parse_pr_command(optarg);
			if (command < 0)
				err(EXIT_FAILURE, _("unknown command"));
			break;
		case 't':
			type = parse_pr_type(optarg);
			if (type < 0)
				err(EXIT_FAILURE, _("unknown type"));
			break;
		case 'f':
			flag = parse_pr_flag(optarg);
			if (flag < 0)
				err(EXIT_FAILURE, _("unknown flag"));
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind == argc)
		errx(EXIT_FAILURE, _("no device specified"));

	path = argv[optind++];
	if (optind != argc) {
		warnx(_("unexpected number of arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	do_pr(path, key, oldkey, command, type, flag);

	return EXIT_SUCCESS;
}
