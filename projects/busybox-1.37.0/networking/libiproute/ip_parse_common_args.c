/* vi: set sw=4 ts=4: */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929: resolve addresses
 */
#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "utils.h"

/* iproute2-5.17.0:
-V, -Version
       Print the version of the ip utility and exit.
-h, -human, -human-readable
       output statistics with human readable values followed by suffix.
-b, -batch FILENAME
       Read commands from provided file or standard input and invoke them.
       First failure will cause termination of ip.
-force Don't terminate ip on errors in batch mode.  If there were any errors
       during execution of the commands, the application return code will be
       non zero.
-s, -stats, -statistics
       Output more information. If the option appears twice or more,
       the amount of information increases.  As a rule, the information
       is statistics or some time values.
-d, -details
       Output more detailed information.
-l, -loops COUNT
       Specify maximum number of loops the 'ip address flush' logic will
       attempt before giving up. The default is 10.  Zero (0) means loop
       until all addresses are removed.
-f, -family FAMILY
       Specifies the protocol family to use. The protocol family identifier
       can be one of inet, inet6, bridge, mpls or link.  If this option is
       not present, the protocol family is guessed from other arguments.
       If the rest of the command line does not give enough information
       to guess the family, ip falls back to the default one, usually inet
       or any.  link is a special family identifier meaning that
       no networking protocol is involved.
-4     shortcut for -family inet.
-6     shortcut for -family inet6.
-B     shortcut for -family bridge.
-M     shortcut for -family mpls.
-0     shortcut for -family link.
-o, -oneline
       output each record on a single line, replacing line feeds with the '\'
       character. This is convenient when you want to count records with wc(1)
       or to grep(1) the output.
-r, -resolve
       use the system's name resolver to print DNS names instead of addresses.
-n, -netns NETNS
       switches ip to the specified network namespace NETNS.  Actually it just
       simplifies executing of:
       ip netns exec NETNS ip [ OPTIONS ] OBJECT { COMMAND | help }
       to
       ip -n[etns] NETNS [ OPTIONS ] OBJECT { COMMAND | help }
-N, -Numeric
       Print the number of protocol, scope, dsfield, etc directly instead of
       converting it to human readable name.
-a, -all
       executes specified command over all objects, it depends if command
       supports this option.
-c[color][={always|auto|never}
       Configure color output. If parameter is omitted or always, color output
       is enabled regardless of stdout state. If parameter is auto, stdout is
       checked to be a terminal before enabling color output. If parameter is
       never, color output is disabled. If specified multiple times, the last
       one takes precedence. This flag is ignored if -json is also given.
       Used color palette can be influenced by COLORFGBG environment variable.
-t, -timestamp
       display current time when using monitor option.
-ts, -tshort
       Like -timestamp, but use shorter format.
-rc, -rcvbuf SIZE
       Set the netlink socket receive buffer size, defaults to 1MB.
-iec   print human readable rates in IEC units (e.g. 1Ki = 1024).
-br, -brief
       Print only basic information in a tabular format for better readability.
       This option is currently only supported by ip addr show , ip link show
       & ip neigh show commands.
-j, -json
       Output results in JavaScript Object Notation (JSON).
-p, -pretty
       The default JSON format is compact and more efficient to parse but hard
       for most users to read.  This flag adds indentation for readability.
*/

family_t preferred_family = AF_UNSPEC;
smallint oneline;
char _SL_;

char** FAST_FUNC ip_parse_common_args(char **argv)
{
	static const char ip_common_commands[] ALIGN1 =
		"oneline" "\0"
		"family" "\0"
		"4" "\0"
		"6" "\0"
		"0" "\0"
		;
	enum {
		ARG_oneline,
		ARG_family,
		ARG_IPv4,
		ARG_IPv6,
		ARG_packet,
	};
	static const family_t af_numbers[] = { AF_INET, AF_INET6, AF_PACKET };
	int arg;

	while (*argv) {
		char *opt = *argv;

		if (opt[0] != '-')
			break;
		opt++;
		if (opt[0] == '-') {
			opt++;
			if (!opt[0]) { /* "--" */
				argv++;
				break;
			}
		}
		arg = index_in_substrings(ip_common_commands, opt);
		if (arg < 0)
			bb_show_usage();
		if (arg == ARG_oneline) {
			oneline = 1;
			argv++;
			continue;
		}
		if (arg == ARG_family) {
			static const char families[] ALIGN1 =
				"inet" "\0" "inet6" "\0" "link" "\0";
			argv++;
			if (!*argv)
				bb_show_usage();
			arg = index_in_strings(families, *argv);
			if (arg < 0)
				invarg_1_to_2(*argv, "family");
			/* now arg == 0, 1 or 2 */
		} else {
			arg -= ARG_IPv4;
			/* now arg == 0, 1 or 2 */
		}
		preferred_family = af_numbers[arg];
		argv++;
	}
	_SL_ = oneline ? '\\' : '\n';
	return argv;
}
