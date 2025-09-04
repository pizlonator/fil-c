/* vi: set sw=4 ts=4: */
/*
 * Small implementation of brctl for busybox.
 *
 * Copyright (C) 2008 by Bernhard Reutner-Fischer
 *
 * Some helper functions from bridge-utils are
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config BRCTL
//config:	bool "brctl (9.9 kb)"
//config:	default y
//config:	help
//config:	Manage ethernet bridges.
//config:	Supports addbr/delbr and addif/delif.
//config:
//config:config FEATURE_BRCTL_FANCY
//config:	bool "Fancy options"
//config:	default y
//config:	depends on BRCTL
//config:	help
//config:	Add support for extended option like:
//config:		setageing, setfd, sethello, setmaxage,
//config:		setpathcost, setportprio, setbridgeprio,
//config:		stp
//config:	This adds about 600 bytes.
//config:
//config:config FEATURE_BRCTL_SHOW
//config:	bool "Support show"
//config:	default y
//config:	depends on BRCTL && FEATURE_BRCTL_FANCY
//config:	help
//config:	Add support for option which prints the current config:
//config:		show

//applet:IF_BRCTL(APPLET_NOEXEC(brctl, brctl, BB_DIR_USR_SBIN, BB_SUID_DROP, brctl))

//kbuild:lib-$(CONFIG_BRCTL) += brctl.o

//usage:#define brctl_trivial_usage
//usage:       "COMMAND [BRIDGE [ARGS]]"
//usage:#define brctl_full_usage "\n\n"
//usage:       "Manage ethernet bridges"
//usage:     "\nCommands:"
//usage:	IF_FEATURE_BRCTL_SHOW(
//usage:     "\n	show [BRIDGE]...	Show bridges"
//usage:	)
//usage:     "\n	addbr BRIDGE		Create BRIDGE"
//usage:     "\n	delbr BRIDGE		Delete BRIDGE"
//usage:     "\n	addif BRIDGE IFACE	Add IFACE to BRIDGE"
//usage:     "\n	delif BRIDGE IFACE	Delete IFACE from BRIDGE"
//usage:	IF_FEATURE_BRCTL_FANCY(
//usage:     "\n	showmacs BRIDGE			List MAC addresses"
//usage:     "\n	showstp	BRIDGE			Show STP info"
//usage:     "\n	stp BRIDGE 1/yes/on|0/no/off	Set STP on/off"
//usage:     "\n	setageing BRIDGE SECONDS	Set ageing time"
//usage:     "\n	setfd BRIDGE SECONDS		Set bridge forward delay"
//usage:     "\n	sethello BRIDGE SECONDS		Set hello time"
//usage:     "\n	setmaxage BRIDGE SECONDS	Set max message age"
//usage:     "\n	setbridgeprio BRIDGE PRIO	Set bridge priority"
//usage:     "\n	setportprio BRIDGE IFACE PRIO	Set port priority"
//usage:     "\n	setpathcost BRIDGE IFACE COST	Set path cost"
//usage:	)
// Not yet implemented:
//			hairpin BRIDGE IFACE on|off	Set hairpin on/off

#include "libbb.h"
#include "common_bufsiz.h"
#include <linux/sockios.h>
#include <net/if.h>

#ifndef SIOCBRADDBR
# define SIOCBRADDBR BRCTL_ADD_BRIDGE
#endif
#ifndef SIOCBRDELBR
# define SIOCBRDELBR BRCTL_DEL_BRIDGE
#endif
#ifndef SIOCBRADDIF
# define SIOCBRADDIF BRCTL_ADD_IF
#endif
#ifndef SIOCBRDELIF
# define SIOCBRDELIF BRCTL_DEL_IF
#endif

#if ENABLE_FEATURE_BRCTL_FANCY
static unsigned str_to_jiffies(const char *time_str)
{
	double dd;
	char *endptr;
//TODO: needs setlocale(LC_NUMERIC, "C")?
	dd = /*bb_*/strtod(time_str, &endptr);
	if (endptr == time_str || dd < 0)
		bb_error_msg_and_die(bb_msg_invalid_arg_to, time_str, "timespec");

	dd *= 100;
	/* For purposes of brctl,
	 * capping SECONDS by ~20 million seconds is quite enough:
	 */
	if (dd > INT_MAX)
		dd = INT_MAX;

	return dd;
}
#endif

#define filedata bb_common_bufsiz1

#if ENABLE_FEATURE_BRCTL_SHOW || ENABLE_FEATURE_BRCTL_FANCY
static int read_file(const char *name)
{
	int n = open_read_close(name, filedata, COMMON_BUFSIZE - 1);
	if (n < 0) {
		filedata[0] = '\0';
	} else {
		filedata[n] = '\0';
		if (n != 0 && filedata[n - 1] == '\n')
			filedata[--n] = '\0';
	}
	return n;
}
#endif

#if ENABLE_FEATURE_BRCTL_SHOW
/* NB: we are in /sys/class/net
 */
static int show_bridge(const char *name, int need_hdr)
{
/* Output:
 *bridge name	bridge id		STP enabled	interfaces
 *br0		8000.000000000000	no		eth0
 */
	char pathbuf[IFNAMSIZ + sizeof("/bridge/bridge_id") + 8];
	int tabs;
	DIR *ifaces;
	struct dirent *ent;
	char *sfx;

#if IFNAMSIZ == 16
	sfx = pathbuf + sprintf(pathbuf, "%.16s/bridge/", name);
#else
	sfx = pathbuf + sprintf(pathbuf, "%.*s/bridge/", (int)IFNAMSIZ, name);
#endif
	strcpy(sfx, "bridge_id");
	if (read_file(pathbuf) < 0)
		return -1; /* this iface is not a bridge */

	if (need_hdr)
		puts("bridge name\tbridge id\t\tSTP enabled\tinterfaces");
	printf("%s\t\t%s\t", name, filedata);

	strcpy(sfx, "stp_state");
	read_file(pathbuf);
	if (LONE_CHAR(filedata, '0'))
		strcpy(filedata, "no");
	else
	if (LONE_CHAR(filedata, '1'))
		strcpy(filedata, "yes");
	fputs_stdout(filedata);

	/* sfx points past "BR/bridge/", turn it into "BR/brif": */
	sfx[-4] = 'f'; sfx[-3] = '\0';
	tabs = 0;
	ifaces = opendir(pathbuf);
	if (ifaces) {
		while ((ent = readdir(ifaces)) != NULL) {
			if (DOT_OR_DOTDOT(ent->d_name))
				continue; /* . or .. */
			if (tabs)
				printf("\t\t\t\t\t");
			else
				tabs = 1;
			printf("\t\t%s\n", ent->d_name);
		}
		closedir(ifaces);
	}
	if (!tabs)  /* bridge has no interfaces */
		bb_putchar('\n');
	return 0;
}
#endif

#if ENABLE_FEATURE_BRCTL_FANCY
static void write_uint(const char *name, const char *leaf, unsigned val)
{
	char pathbuf[IFNAMSIZ + sizeof("/bridge/bridge_id") + 32];
	int fd, n;

#if IFNAMSIZ == 16
	sprintf(pathbuf, "%.16s/%s", name, leaf);
#else
	sprintf(pathbuf, "%.*s/%s", (int)IFNAMSIZ, name, leaf);
#endif
	fd = xopen(pathbuf, O_WRONLY);
	n = sprintf(filedata, "%u\n", val);
	if (write(fd, filedata, n) < 0)
		bb_simple_perror_msg_and_die(name);
	/* So far all callers exit very soon after calling us.
	 * Do not bother closing fd (unless debugging):
	 */
	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
}

struct fdb_entry {
	uint8_t mac_addr[6];
	uint8_t port_no;
	uint8_t is_local;
	uint32_t ageing_timer_value;
	uint8_t port_hi;
	uint8_t pad0;
	uint16_t unused;
};

static int compare_fdbs(const void *_f0, const void *_f1)
{
	const struct fdb_entry *f0 = _f0;
	const struct fdb_entry *f1 = _f1;

	return memcmp(f0->mac_addr, f1->mac_addr, 6);
}

static size_t read_bridge_forward_db(const char *name, struct fdb_entry **_fdb)
{
	char pathbuf[IFNAMSIZ + sizeof("/brforward") + 8];
	struct fdb_entry *fdb;
	size_t nentries;
	int fd;
	ssize_t cc;

#if IFNAMSIZ == 16
	sprintf(pathbuf, "%.16s/brforward", name);
#else
	sprintf(pathbuf, "%.*s/brforward", (int)IFNAMSIZ, name);
#endif
	fd = open(pathbuf, O_RDONLY);
	if (fd < 0)
		bb_error_msg_and_die("bridge %s does not exist", name);

	fdb = NULL;
	nentries = 0;
	for (;;) {
		fdb = xrealloc_vector(fdb, 4, nentries);
		cc = full_read(fd, &fdb[nentries], sizeof(*fdb));
		if (cc == 0) {
			break;
		}
		if (cc != sizeof(*fdb)) {
			bb_perror_msg_and_die("can't read bridge %s forward db", name);
		}
		++nentries;
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	qsort(fdb, nentries, sizeof(*fdb), compare_fdbs);

	*_fdb = fdb;
	return nentries;
}

static void show_bridge_macs(const char *name)
{
	struct fdb_entry *fdb;
	size_t nentries;
	size_t i;

	nentries = read_bridge_forward_db(name, &fdb);

	printf("port no\tmac addr\t\tis local?\tageing timer\n");
	for (i = 0; i < nentries; ++i) {
		const struct fdb_entry *f = &fdb[i];
		unsigned tv_sec = f->ageing_timer_value / 100;
		unsigned tv_csec = f->ageing_timer_value % 100;
		printf("%3u\t"
			"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\t"
			"%s\t\t"
			"%4u.%.2u\n",
			f->port_no,
			f->mac_addr[0], f->mac_addr[1], f->mac_addr[2],
			f->mac_addr[3], f->mac_addr[4], f->mac_addr[5],
			(f->is_local ? "yes" : "no"),
			tv_sec, tv_csec
		);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		free(fdb);
}

static void show_bridge_timer(const char *msg)
{
	unsigned long long centisec = xstrtoull(filedata, 0);
	unsigned tv_sec = centisec / 100;
	unsigned tv_csec = centisec % 100;
	printf("%s%4u.%.2u", msg, tv_sec, tv_csec);
}

static const char *show_bridge_state(unsigned state)
{
	/* See linux/if_bridge.h, BR_STATE_ constants */
	static const char state_names[] ALIGN1 =
		"disabled\0"	//BR_STATE_DISABLED   0
		"listening\0"   //BR_STATE_LISTENING  1
		"learning\0"    //BR_STATE_LEARNING   2
		"forwarding\0"  //BR_STATE_FORWARDING 3
		"blocking"      //BR_STATE_BLOCKING   4
	;
	if (state < 5)
		return nth_string(state_names, state);
	return utoa(state);
}

static void printf_xstrtou(const char *fmt)
{
	printf(fmt, xstrtou(filedata, 0));
}

static NOINLINE void show_bridge_port(const char *name)
{
	char pathbuf[IFNAMSIZ + sizeof("/brport/forward_delay_timer") + 8];
	char *sfx;

#if IFNAMSIZ == 16
	sfx = pathbuf + sprintf(pathbuf, "%.16s/brport/", name);
#else
	sfx = pathbuf + sprintf(pathbuf, "%.*s/brport/", (int)IFNAMSIZ, name);
#endif

	strcpy(sfx, "port_no");
	read_file(pathbuf);
	printf("%s (%u)\n", name, xstrtou(filedata, 0));

	strcpy(sfx + 5, "id"); // "port_id"
	read_file(pathbuf);
	printf_xstrtou(" port id\t\t%.4x");

	strcpy(sfx, "state");
	read_file(pathbuf);
	printf("\t\t\tstate\t\t%15s\n", show_bridge_state(xstrtou(filedata, 0)));

	strcpy(sfx, "designated_root");
	read_file(pathbuf);
	printf(" designated root\t%s", filedata);

	strcpy(sfx, "path_cost");
	read_file(pathbuf);
	printf_xstrtou("\tpath cost\t\t%4u\n");

	strcpy(sfx, "designated_bridge");
	read_file(pathbuf);
	printf(" designated bridge\t%s", filedata);

	strcpy(sfx, "message_age_timer");
	read_file(pathbuf);
	show_bridge_timer("\tmessage age timer\t");

	strcpy(sfx, "designated_port");
	read_file(pathbuf);
	printf_xstrtou("\n designated port\t%.4x");

	strcpy(sfx, "forward_delay_timer");
	read_file(pathbuf);
	show_bridge_timer("\t\t\tforward delay timer\t");

	strcpy(sfx, "designated_cost");
	read_file(pathbuf);
	printf_xstrtou("\n designated cost\t%4u");

	strcpy(sfx, "hold_timer");
	read_file(pathbuf);
	show_bridge_timer("\t\t\thold timer\t\t");

	printf("\n flags\t\t\t");

	strcpy(sfx, "config_pending");
	read_file(pathbuf);
	if (!LONE_CHAR(filedata, '0'))
		printf("CONFIG_PENDING ");

	strcpy(sfx, "change_ack");
	read_file(pathbuf);
	if (!LONE_CHAR(filedata, '0'))
		printf("TOPOLOGY_CHANGE_ACK ");

	strcpy(sfx, "hairpin_mode");
	read_file(pathbuf);
	if (!LONE_CHAR(filedata, '0'))
		printf_xstrtou("\n hairpin mode\t\t%4u");

	printf("\n\n");
}

static void show_bridge_stp(const char *name)
{
	char pathbuf[IFNAMSIZ + sizeof("/bridge/topology_change_timer") + 8];
	char *sfx;

#if IFNAMSIZ == 16
	sfx = pathbuf + sprintf(pathbuf, "%.16s/bridge/", name);
#else
	sfx = pathbuf + sprintf(pathbuf, "%.*s/bridge/", (int)IFNAMSIZ, name);
#endif

	strcpy(sfx, "bridge_id");
	if (read_file(pathbuf) < 0)
		bb_error_msg_and_die("bridge %s does not exist", name);

	printf("%s\n"
		" bridge id\t\t%s", name, filedata);

	strcpy(sfx, "root_id");
	read_file(pathbuf);
	printf("\n designated root\t%s", filedata);

	strcpy(sfx + 5, "port"); // "root_port"
	read_file(pathbuf);
	printf_xstrtou("\n root port\t\t%4u\t\t\t");

	strcpy(sfx + 6, "ath_cost"); // "root_path_cost"
	read_file(pathbuf);
	printf_xstrtou("path cost\t\t%4u\n");

	strcpy(sfx, "max_age");
	read_file(pathbuf);
	show_bridge_timer(" max age\t\t");
	show_bridge_timer("\t\t\tbridge max age\t\t");

	strcpy(sfx, "hello_time");
	read_file(pathbuf);
	show_bridge_timer("\n hello time\t\t");
	show_bridge_timer("\t\t\tbridge hello time\t");

	strcpy(sfx, "forward_delay");
	read_file(pathbuf);
	show_bridge_timer("\n forward delay\t\t");
	show_bridge_timer("\t\t\tbridge forward delay\t");

	strcpy(sfx, "ageing_time");
	read_file(pathbuf);
	show_bridge_timer("\n ageing time\t\t");

	strcpy(sfx, "hello_timer");
	read_file(pathbuf);
	show_bridge_timer("\n hello timer\t\t");

	strcpy(sfx, "tcn_timer");
	read_file(pathbuf);
	show_bridge_timer("\t\t\ttcn timer\t\t");

	strcpy(sfx, "topology_change_timer");
	read_file(pathbuf);
	show_bridge_timer("\n topology change timer\t");

	strcpy(sfx, "gc_timer");
	read_file(pathbuf);
	show_bridge_timer("\t\t\tgc timer\t\t");

	printf("\n flags\t\t\t");

	strcpy(sfx, "topology_change");
	read_file(pathbuf);
	if (!LONE_CHAR(filedata, '0'))
		printf("TOPOLOGY_CHANGE ");

	strcpy(sfx, "topology_change_detected");
	read_file(pathbuf);
	if (!LONE_CHAR(filedata, '0'))
		printf("TOPOLOGY_CHANGE_DETECTED ");
	printf("\n\n\n");

	/* Show bridge ports */
	{
		DIR *ifaces;

		/* sfx points past "BR/bridge/", turn it into "BR/brif": */
		sfx[-4] = 'f'; sfx[-3] = '\0';
		ifaces = opendir(pathbuf);
		if (ifaces) {
			struct dirent *ent;
			while ((ent = readdir(ifaces)) != NULL) {
				if (DOT_OR_DOTDOT(ent->d_name))
					continue; /* . or .. */
				show_bridge_port(ent->d_name);
			}
			if (ENABLE_FEATURE_CLEAN_UP)
				closedir(ifaces);
		}
	}
}
#endif

int brctl_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int brctl_main(int argc UNUSED_PARAM, char **argv)
{
	static const char keywords[] ALIGN1 =
		"addbr\0" "delbr\0" "addif\0" "delif\0"
	IF_FEATURE_BRCTL_FANCY(
		"stp\0"
		"showstp\0"
		"setageing\0" "setfd\0" "sethello\0" "setmaxage\0"
		"setpathcost\0" "setportprio\0"
		"setbridgeprio\0"
		"showmacs\0"
	)
	IF_FEATURE_BRCTL_SHOW("show\0");
	enum { ARG_addbr = 0, ARG_delbr, ARG_addif, ARG_delif
		IF_FEATURE_BRCTL_FANCY(,
			ARG_stp,
			ARG_showstp,
			ARG_setageing, ARG_setfd, ARG_sethello, ARG_setmaxage,
			ARG_setpathcost, ARG_setportprio,
			ARG_setbridgeprio,
			ARG_showmacs
		)
		IF_FEATURE_BRCTL_SHOW(, ARG_show)
	};
	int key;
	char *br;

	argv++;
	if (!*argv) {
		/* bare "brctl" shows --help */
		bb_show_usage();
	}

	xchdir("/sys/class/net");

	key = index_in_strings(keywords, *argv);
	if (key == -1) /* no match found in keywords array, bail out. */
		bb_error_msg_and_die(bb_msg_invalid_arg_to, *argv, applet_name);
	argv++;

#if ENABLE_FEATURE_BRCTL_SHOW
	if (key == ARG_show) { /* show [BR]... */
		DIR *net;
		struct dirent *ent;
		int need_hdr = 1;
		exitcode_t exitcode = EXIT_SUCCESS;

		if (*argv) {
			/* "show BR1 BR2 BR3" */
			do {
				if (show_bridge(*argv, need_hdr) >= 0) {
					need_hdr = 0;
				} else {
					bb_error_msg("bridge %s does not exist", *argv);
//TODO: if device exists, but is not a BR, brctl from bridge-utils 1.6
//says this instead: "device eth0 is not a bridge"
					exitcode = EXIT_FAILURE;
				}
			} while (*++argv != NULL);
			return exitcode;
		}

		/* "show" (if no ifaces, shows nothing, not even header) */
		net = xopendir(".");
		while ((ent = readdir(net)) != NULL) {
			if (DOT_OR_DOTDOT(ent->d_name))
				continue; /* . or .. */
			if (show_bridge(ent->d_name, need_hdr) >= 0)
				need_hdr = 0;
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			closedir(net);
		return exitcode;
	}
#endif

	if (!*argv) /* All of the below need at least one argument */
		bb_show_usage();

	br = *argv++;

	if (key == ARG_addbr || key == ARG_delbr) {
		/* brctl from bridge-utils 1.6 still uses ioctl
		 * for SIOCBRADDBR / SIOCBRDELBR, not /sys accesses
		 */
		int fd = xsocket(AF_INET, SOCK_STREAM, 0);
		ioctl_or_perror_and_die(fd,
			key == ARG_addbr ? SIOCBRADDBR : SIOCBRDELBR,
			br, "bridge %s", br
		);
		//close(fd);
		//goto done;
		/* bridge-utils 1.6 simply ignores trailing args:
		 * "brctl addbr BR1 ARGS" ignores ARGS
		 */
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
		return EXIT_SUCCESS;
	}

#if ENABLE_FEATURE_BRCTL_FANCY
	if (key == ARG_showmacs) {
		show_bridge_macs(br);
		return EXIT_SUCCESS;
	}
	if (key == ARG_showstp) {
		show_bridge_stp(br);
		return EXIT_SUCCESS;
	}
#endif

	if (!*argv) /* All of the below need at least two arguments */
		bb_show_usage();

#if ENABLE_FEATURE_BRCTL_FANCY
	if (key == ARG_stp) {
		static const char no_yes[] ALIGN1 =
			"0\0" "off\0" "n\0" "no\0"   /* 0 .. 3 */
			"1\0" "on\0"  "y\0" "yes\0"; /* 4 .. 7 */
		int onoff = index_in_strings(no_yes, *argv);
		if (onoff < 0)
			bb_error_msg_and_die(bb_msg_invalid_arg_to, *argv, applet_name);
		onoff = (unsigned)onoff / 4;
		write_uint(br, "bridge/stp_state", onoff);
		return EXIT_SUCCESS;
	}

	if ((unsigned)(key - ARG_setageing) < 4) { /* time related ops */
		/* setageing BR N: "N*100\n" to /sys/class/net/BR/bridge/ageing_time
		 * setfd BR N:     "N*100\n" to /sys/class/net/BR/bridge/forward_delay
		 * sethello BR N:  "N*100\n" to /sys/class/net/BR/bridge/hello_time
		 * setmaxage BR N: "N*100\n" to /sys/class/net/BR/bridge/max_age
		 */
		write_uint(br,
			nth_string(
				"bridge/ageing_time"  "\0" /* ARG_setageing */
				"bridge/forward_delay""\0" /* ARG_setfd     */
				"bridge/hello_time"   "\0" /* ARG_sethello  */
				"bridge/max_age",          /* ARG_setmaxage */
				key - ARG_setageing
			),
			str_to_jiffies(*argv)
		);
		return EXIT_SUCCESS;
	}

	if (key == ARG_setbridgeprio) {
		write_uint(br, "bridge/priority", xatoi_positive(*argv));
		return EXIT_SUCCESS;
	}

	if (key == ARG_setpathcost
	 || key == ARG_setportprio
	) {
		if (!argv[1])
			bb_show_usage();
		/* BR is not used (and ignored!) for these commands:
		 * "setpathcost BR PORT N" writes "N\n" to
		 * /sys/class/net/PORT/brport/path_cost
		 * "setportprio BR PORT N" writes "N\n" to
		 * /sys/class/net/PORT/brport/priority
		 */
		write_uint(argv[0],
			nth_string(
				"brport/path_cost" "\0" /* ARG_setpathcost */
				"brport/priority",      /* ARG_setportprio */
				key - ARG_setpathcost
			),
			xatoi_positive(argv[1])
		);
		return EXIT_SUCCESS;
	}
#endif
	/* always true: if (key == ARG_addif || key == ARG_delif) */ {
		struct ifreq ifr;
		int fd = xsocket(AF_INET, SOCK_STREAM, 0);

		strncpy_IFNAMSIZ(ifr.ifr_name, br);
		ifr.ifr_ifindex = if_nametoindex(*argv);
		if (ifr.ifr_ifindex == 0) {
			bb_perror_msg_and_die("iface %s", *argv);
		}
		ioctl_or_perror_and_die(fd,
			key == ARG_addif ? SIOCBRADDIF : SIOCBRDELIF,
			&ifr, "bridge %s", br
		);
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
	}

	return EXIT_SUCCESS;
}
