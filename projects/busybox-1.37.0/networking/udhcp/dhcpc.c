/* vi: set sw=4 ts=4: */
/*
 * udhcp client
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
//applet:IF_UDHCPC(APPLET(udhcpc, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_UDHCPC) += common.o packet.o signalpipe.o socket.o
//kbuild:lib-$(CONFIG_UDHCPC) += dhcpc.o
//kbuild:lib-$(CONFIG_FEATURE_UDHCPC_ARPING) += arpping.o
//kbuild:lib-$(CONFIG_FEATURE_UDHCP_RFC3397) += domain_codec.o

#include <syslog.h>
/* Override ENABLE_FEATURE_PIDFILE - ifupdown needs our pidfile to always exist */
#define WANT_PIDFILE 1
#include "common.h"
#include "dhcpd.h"
#include "dhcpc.h"

#include <netinet/if_ether.h>
#include <linux/filter.h>
#include <linux/if_packet.h>

#ifndef PACKET_AUXDATA
# define PACKET_AUXDATA 8
struct tpacket_auxdata {
	uint32_t tp_status;
	uint32_t tp_len;
	uint32_t tp_snaplen;
	uint16_t tp_mac;
	uint16_t tp_net;
	uint16_t tp_vlan_tci;
	uint16_t tp_padding;
};
#endif


/* "struct client_data_t client_data" is in bb_common_bufsiz1 */


#if ENABLE_LONG_OPTS
static const char udhcpc_longopts[] ALIGN1 =
	"clientid-none\0"  No_argument       "C"
	"vendorclass\0"    Required_argument "V" //deprecated
	"fqdn\0"           Required_argument "F"
	"interface\0"      Required_argument "i"
	"now\0"            No_argument       "n"
	"pidfile\0"        Required_argument "p"
	"quit\0"           No_argument       "q"
	"release\0"        No_argument       "R"
	"request\0"        Required_argument "r"
	"script\0"         Required_argument "s"
	"timeout\0"        Required_argument "T"
	"retries\0"        Required_argument "t"
	"tryagain\0"       Required_argument "A"
	"syslog\0"         No_argument       "S"
	"request-option\0" Required_argument "O"
	"no-default-options\0" No_argument   "o"
	"foreground\0"     No_argument       "f"
	USE_FOR_MMU(
	"background\0"     No_argument       "b"
	)
	"broadcast\0"      No_argument       "B"
	IF_FEATURE_UDHCPC_ARPING("arping\0"	Optional_argument "a")
	IF_FEATURE_UDHCP_PORT("client-port\0"	Required_argument "P")
	;
#endif
/* Must match getopt32 option string order */
enum {
	OPT_C = 1 << 0,
	OPT_V = 1 << 1,
	OPT_F = 1 << 2,
	OPT_i = 1 << 3,
	OPT_n = 1 << 4,
	OPT_p = 1 << 5,
	OPT_q = 1 << 6,
	OPT_R = 1 << 7,
	OPT_r = 1 << 8,
	OPT_s = 1 << 9,
	OPT_T = 1 << 10,
	OPT_t = 1 << 11,
	OPT_S = 1 << 12,
	OPT_A = 1 << 13,
	OPT_O = 1 << 14,
	OPT_o = 1 << 15,
	OPT_x = 1 << 16,
	OPT_f = 1 << 17,
	OPT_B = 1 << 18,
/* The rest has variable bit positions, need to be clever */
	OPTBIT_B = 18,
	USE_FOR_MMU(             OPTBIT_b,)
	IF_FEATURE_UDHCPC_ARPING(OPTBIT_a,)
	IF_FEATURE_UDHCP_PORT(   OPTBIT_P,)
	USE_FOR_MMU(             OPT_b = 1 << OPTBIT_b,)
	IF_FEATURE_UDHCPC_ARPING(OPT_a = 1 << OPTBIT_a,)
	IF_FEATURE_UDHCP_PORT(   OPT_P = 1 << OPTBIT_P,)
};


/*** Script execution code ***/
struct dhcp_optitem {
	unsigned len;
	uint8_t code;
	uint8_t malloced;
	uint8_t *data;
	char *env;
};

/* get a rough idea of how long an option will be (rounding up...) */
static const uint8_t len_of_option_as_string[] ALIGN1 = {
	[OPTION_IP              ] = sizeof("255.255.255.255 "),
	[OPTION_IP_PAIR         ] = sizeof("255.255.255.255 ") * 2,
	[OPTION_STATIC_ROUTES   ] = sizeof("255.255.255.255/32 255.255.255.255 "),
	[OPTION_6RD             ] = sizeof("132 128 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff 255.255.255.255 "),
	[OPTION_STRING          ] = 1,
	[OPTION_STRING_HOST     ] = 1,
#if ENABLE_FEATURE_UDHCP_RFC3397
	[OPTION_DNS_STRING      ] = 1, /* unused */
	/* Hmmm, this severely overestimates size if SIP_SERVERS option
	 * is in domain name form: N-byte option in binary form
	 * mallocs ~16*N bytes. But it is freed almost at once.
	 */
	[OPTION_SIP_SERVERS     ] = sizeof("255.255.255.255 "),
#endif
//	[OPTION_BOOLEAN         ] = sizeof("yes "),
	[OPTION_U8              ] = sizeof("255 "),
	[OPTION_U16             ] = sizeof("65535 "),
//	[OPTION_S16             ] = sizeof("-32768 "),
	[OPTION_U32             ] = sizeof("4294967295 "),
	[OPTION_S32             ] = sizeof("-2147483684 "),
};

/* note: ip is a pointer to an IP in network order, possibly misaliged */
static int sprint_nip(char *dest, const char *pre, const uint8_t *ip)
{
	return sprintf(dest, "%s%u.%u.%u.%u", pre, ip[0], ip[1], ip[2], ip[3]);
}

#if ENABLE_FEATURE_UDHCPC_SANITIZEOPT
/* Check if a given name represents a valid DNS name */
/* See RFC1035, 2.3.1 */
/* We don't need to be particularly anal. For example, allowing _, hyphen
 * at the end, or leading and trailing dots would be ok, since it
 * can't be used for attacks. (Leading hyphen can be, if someone uses cmd "$hostname"
 * in the script: then hostname may be treated as an option)
 */
static int good_hostname(const char *name)
{
	if (*name == '-') /* Can't start with '-' */
		return 0;

	while (*name) {
		unsigned char ch = *name++;
		if (!isalnum(ch))
			/* DNS allows only '-', but we are more permissive */
			if (ch != '-' && ch != '_' && ch != '.')
				return 0;
		// TODO: do we want to validate lengths against NS_MAXLABEL and NS_MAXDNAME?
	}
	return 1;
}
#else
# define good_hostname(name) 1
#endif

/* Create "opt_name=opt_value" string */
static NOINLINE char *xmalloc_optname_optval(const struct dhcp_optitem *opt_item, const struct dhcp_optflag *optflag, const char *opt_name)
{
	unsigned upper_length;
	int len, type, optlen;
	char *dest, *ret;
	uint8_t *option;

	option = opt_item->data;
	len = opt_item->len;
	type = optflag->flags & OPTION_TYPE_MASK;
	optlen = dhcp_option_lengths[type];
	upper_length = len_of_option_as_string[type]
		* ((unsigned)(len + optlen) / (unsigned)optlen);

	dest = ret = xmalloc(upper_length + strlen(opt_name) + 2);
	dest += sprintf(ret, "%s=", opt_name);

	while (len >= optlen) {
		switch (type) {
		case OPTION_IP:
		case OPTION_IP_PAIR:
			dest += sprint_nip(dest, "", option);
			if (type == OPTION_IP_PAIR)
				dest += sprint_nip(dest, "/", option + 4);
			break;
//		case OPTION_BOOLEAN:
//			dest += sprintf(dest, *option ? "yes" : "no");
//			break;
		case OPTION_U8:
			dest += sprintf(dest, "%u", *option);
			break;
//		case OPTION_S16:
		case OPTION_U16: {
			uint16_t val_u16;
			move_from_unaligned16(val_u16, option);
			dest += sprintf(dest, "%u", ntohs(val_u16));
			break;
		}
		case OPTION_S32:
		case OPTION_U32: {
			uint32_t val_u32;
			move_from_unaligned32(val_u32, option);
			dest += sprintf(dest, type == OPTION_U32 ? "%lu" : "%ld", (unsigned long) ntohl(val_u32));
			break;
		}
		/* Note: options which use 'return' instead of 'break'
		 * (for example, OPTION_STRING) skip the code which handles
		 * the case of list of options.
		 */
		case OPTION_STRING:
		case OPTION_STRING_HOST:
			memcpy(dest, option, len);
			dest[len] = '\0';
//TODO: it appears option 15 DHCP_DOMAIN_NAME is often abused
//by DHCP admins to contain a space-separated list of domains,
//not one domain name (presumably, to work as list of search domains,
//instead of using proper option 119 DHCP_DOMAIN_SEARCH).
//Currently, good_hostname() balks on strings containing spaces.
//Do we need to allow it? Only for DHCP_DOMAIN_NAME option?
			if (type == OPTION_STRING_HOST && !good_hostname(dest))
				safe_strncpy(dest, "bad", len);
			return ret;
		case OPTION_STATIC_ROUTES: {
			/* Option binary format:
			 * mask [one byte, 0..32]
			 * ip [big endian, 0..4 bytes depending on mask]
			 * router [big endian, 4 bytes]
			 * may be repeated
			 *
			 * We convert it to a string "IP/MASK ROUTER IP2/MASK2 ROUTER2"
			 */
			const char *pfx = "";

			while (len >= 1 + 4) { /* mask + 0-byte ip + router */
				uint32_t nip;
				uint8_t *p;
				unsigned mask;
				int bytes;

				mask = *option++;
				if (mask > 32)
					break;
				len--;

				nip = 0;
				p = (void*) &nip;
				bytes = (mask + 7) / 8; /* 0 -> 0, 1..8 -> 1, 9..16 -> 2 etc */
				while (--bytes >= 0) {
					*p++ = *option++;
					len--;
				}
				if (len < 4)
					break;

				/* print ip/mask */
				dest += sprint_nip(dest, pfx, (void*) &nip);
				pfx = " ";
				dest += sprintf(dest, "/%u ", mask);
				/* print router */
				dest += sprint_nip(dest, "", option);
				option += 4;
				len -= 4;
			}

			return ret;
		}
		case OPTION_6RD:
			/* Option binary format (see RFC 5969):
			 *  0                   1                   2                   3
			 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 * |  OPTION_6RD   | option-length |  IPv4MaskLen  |  6rdPrefixLen |
			 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 * |                           6rdPrefix                           |
			 * ...                        (16 octets)                        ...
			 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 * ...                   6rdBRIPv4Address(es)                    ...
			 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			 * We convert it to a string
			 * "IPv4MaskLen 6rdPrefixLen 6rdPrefix 6rdBRIPv4Address..."
			 *
			 * Sanity check: ensure that our length is at least 22 bytes, that
			 * IPv4MaskLen <= 32,
			 * 6rdPrefixLen <= 128,
			 * 6rdPrefixLen + (32 - IPv4MaskLen) <= 128
			 * (2nd condition needs no check - it follows from 1st and 3rd).
			 * Else, return envvar with empty value ("optname=")
			 */
			if (len >= (1 + 1 + 16 + 4)
			 && option[0] <= 32
			 && (option[1] + 32 - option[0]) <= 128
			) {
				/* IPv4MaskLen */
				dest += sprintf(dest, "%u ", *option++);
				/* 6rdPrefixLen */
				dest += sprintf(dest, "%u ", *option++);
				/* 6rdPrefix */
				dest += sprint_nip6(dest, /* "", */ option);
				option += 16;
				len -= 1 + 1 + 16;
				*dest++ = ' ';
				/* 6rdBRIPv4Address(es), use common IPv4 logic to process them */
				type = OPTION_IP;
				optlen = 4;
				continue;
			}

			return ret;
#if ENABLE_FEATURE_UDHCP_RFC3397
		case OPTION_DNS_STRING:
			/* unpack option into dest; use ret for prefix (i.e., "optname=") */
			dest = dname_dec(option, len, ret);
			if (dest) {
				free(ret);
				return dest;
			}
			/* error. return "optname=" string */
			return ret;
		case OPTION_SIP_SERVERS:
			/* Option binary format:
			 * type: byte
			 * type=0: domain names, dns-compressed
			 * type=1: IP addrs
			 */
			option++;
			len--;
			if (option[-1] == 1) {
				/* use common IPv4 logic to process IP addrs */
				type = OPTION_IP;
				optlen = 4;
				continue;
			}
			if (option[-1] == 0) {
				dest = dname_dec(option, len, ret);
				if (dest) {
					free(ret);
					return dest;
				}
			}
			return ret;
#endif
		} /* switch */

		/* If we are here, try to format any remaining data
		 * in the option as another, similarly-formatted option
		 */
		option += optlen;
		len -= optlen;
// TODO: it can be a list only if (optflag->flags & OPTION_LIST).
// Should we bail out/warn if we see multi-ip option which is
// not allowed to be such (for example, DHCP_BROADCAST)? -
		if (len < optlen /* || !(optflag->flags & OPTION_LIST) */)
			break;
		*dest++ = ' ';
		*dest = '\0';
	} /* while */

	return ret;
}

static void optitem_unset_env_and_free(void *item)
{
	struct dhcp_optitem *opt_item = item;
	bb_unsetenv_and_free(opt_item->env);
	if (opt_item->malloced)
		free(opt_item->data);
	free(opt_item);
}

/* Used by static options (interface, siaddr, etc) */
static void putenvp(char *new_opt)
{
	struct dhcp_optitem *opt_item;

	opt_item = xzalloc(sizeof(*opt_item));
	/* opt_item->code = 0, so it won't appear in concat_option's lookup */
	/* opt_item->malloced = 0 */
	/* opt_item->data = NULL */
	opt_item->env = new_opt;
	llist_add_to(&client_data.envp, opt_item);
	log2(" %s", new_opt);
	putenv(new_opt);
}

/* Support RFC3396 Long Encoded Options */
static struct dhcp_optitem *concat_option(uint8_t *data, uint8_t len, uint8_t code)
{
	llist_t *item;
	struct dhcp_optitem *opt_item;

	/* Check if an option with the code already exists.
	 * A possible optimization is to create a bitmap of all existing options in the packet,
	 * and iterate over the option list only if they exist.
	 * This will result in bigger code, and because dhcp packets don't have too many options it
	 * shouldn't have a big impact on performance.
	 */
	for (item = client_data.envp; item != NULL; item = item->link) {
		opt_item = (struct dhcp_optitem *)item->data;
		if (opt_item->code == code) {
			/* This option was seen already, concatenate */
			uint8_t *new_data;

			new_data = xmalloc(len + opt_item->len);
			memcpy(
				mempcpy(new_data, opt_item->data, opt_item->len),
				data, len
			);
			opt_item->len += len;
			if (opt_item->malloced)
				free(opt_item->data);
			opt_item->malloced = 1;
			opt_item->data = new_data;
			return opt_item;
		}
	}

	/* This is a new option, add a new dhcp_optitem to the list */
	opt_item = xzalloc(sizeof(*opt_item));
	opt_item->code = code;
	/* opt_item->malloced = 0 */
	opt_item->data = data;
	opt_item->len = len;
	llist_add_to(&client_data.envp, opt_item);
	return opt_item;
}

static const char* get_optname(uint8_t code, const struct dhcp_optflag **dh)
{
	/* Find the option:
	 * dhcp_optflags is sorted so we stop searching when dh->code >= code, which is faster
	 * than iterating over the entire array.
	 * Options which don't have a match in dhcp_option_strings[], e.g DHCP_REQUESTED_IP,
	 * are located after the sorted array, so these entries will never be reached
	 * and they'll count as unknown options.
	 */
	for (*dh = dhcp_optflags; (*dh)->code && (*dh)->code < code; (*dh)++)
		continue;

	if ((*dh)->code == code)
		return nth_string(dhcp_option_strings, (*dh - dhcp_optflags));

	return NULL;
}

/* put all the parameters into the environment */
static void fill_envp(struct dhcp_packet *packet)
{
	uint8_t *optptr;
	struct dhcp_scan_state scan_state;
	char *new_opt;

	putenvp(xasprintf("interface=%s", client_data.interface));

	if (!packet)
		return;

	init_scan_state(packet, &scan_state);

	/* Iterate over the packet options.
	 * Handle each option based on whether it's an unknown / known option.
	 * Long options are supported in compliance with RFC 3396.
	 */
	while ((optptr = udhcp_scan_options(packet, &scan_state)) != NULL) {
		const struct dhcp_optflag *dh;
		const char *opt_name;
		struct dhcp_optitem *opt_item;
		uint8_t code = optptr[OPT_CODE];
		uint8_t len = optptr[OPT_LEN];
		uint8_t *data = optptr + OPT_DATA;

		opt_item = concat_option(data, len, code);
		opt_name = get_optname(code, &dh);
		if (opt_name) {
			new_opt = xmalloc_optname_optval(opt_item, dh, opt_name);
			if (opt_item->code == DHCP_SUBNET && opt_item->len == 4) {
				/* Generate extra envvar for DHCP_SUBNET, $mask */
				uint32_t subnet;
				move_from_unaligned32(subnet, opt_item->data);
//FIXME: we do not check that subnet has bit pattern 11..10..0
				putenvp(xasprintf("mask=%u", bb_popcnt_32(subnet)));
			}
		} else {
			unsigned ofs;
			new_opt = xmalloc(sizeof("optNNN=") + 1 + opt_item->len*2);
			ofs = sprintf(new_opt, "opt%u=", opt_item->code);
			bin2hex(new_opt + ofs, (char *)opt_item->data, opt_item->len)[0] = '\0';
		}
		log2(" %s", new_opt);
		putenv(new_opt);
		/* putenv will replace the existing environment variable in case of a duplicate.
		 * Free the previous occurrence (NULL if it's the first one).
		 */
		free(opt_item->env);
		opt_item->env = new_opt;
	}

	/* Export BOOTP fields. Fields we don't (yet?) export:
	 * uint8_t op;      // always BOOTREPLY
	 * uint8_t htype;   // hardware address type. 1 = 10mb ethernet
	 * uint8_t hlen;    // hardware address length
	 * uint8_t hops;    // used by relay agents only
	 * uint32_t xid;
	 * uint16_t secs;   // elapsed since client began acquisition/renewal
	 * uint16_t flags;  // only one flag so far: bcast. Never set by server
	 * uint32_t ciaddr; // client IP (usually == yiaddr. can it be different
	 *                  // if during renew server wants to give us different IP?)
	 * uint8_t chaddr[16]; // link-layer client hardware address (MAC)
	 */
	/* Most important one: yiaddr as $ip */
	new_opt = xmalloc(sizeof("ip=255.255.255.255"));
	sprint_nip(new_opt, "ip=", (uint8_t *) &packet->yiaddr);
	putenvp(new_opt);

	if (packet->siaddr_nip) {
		/* IP address of next server to use in bootstrap */
		new_opt = xmalloc(sizeof("siaddr=255.255.255.255"));
		sprint_nip(new_opt, "siaddr=", (uint8_t *) &packet->siaddr_nip);
		putenvp(new_opt);
	}
	if (packet->gateway_nip) {
		/* IP address of DHCP relay agent */
		new_opt = xmalloc(sizeof("giaddr=255.255.255.255"));
		sprint_nip(new_opt, "giaddr=", (uint8_t *) &packet->gateway_nip);
		putenvp(new_opt);
	}
	if (!(scan_state.overload & FILE_FIELD) && packet->file[0]) {
		/* watch out for invalid packets */
		new_opt = xasprintf("boot_file=%."DHCP_PKT_FILE_LEN_STR"s", packet->file);
		putenvp(new_opt);
	}
	if (!(scan_state.overload & SNAME_FIELD) && packet->sname[0]) {
		/* watch out for invalid packets */
		new_opt = xasprintf("sname=%."DHCP_PKT_SNAME_LEN_STR"s", packet->sname);
		putenvp(new_opt);
	}
}

/* Call a script with env vars */
static void d4_run_script(struct dhcp_packet *packet, const char *name)
{
	char *argv[3];

	fill_envp(packet);

	/* call script */
	log1("executing %s %s", client_data.script, name);
	argv[0] = (char*) client_data.script;
	argv[1] = (char*) name;
	argv[2] = NULL;
	spawn_and_wait(argv);

	/* Free all allocated environment variables */
	llist_free(client_data.envp, optitem_unset_env_and_free);
	client_data.envp = NULL;
}

static void d4_run_script_deconfig(void)
{
	d4_run_script(NULL, "deconfig");
}

/*** Sending/receiving packets ***/

static ALWAYS_INLINE uint32_t random_xid(void)
{
	return rand();
}

/* Initialize the packet with the proper defaults */
static void init_packet(struct dhcp_packet *packet, char type)
{
	unsigned secs;

	/* Fill in: op, htype, hlen, cookie fields; message type option: */
	udhcp_init_header(packet, type);

	packet->xid = client_data.xid;

	client_data.last_secs = monotonic_sec();
	if (client_data.first_secs == 0)
		client_data.first_secs = client_data.last_secs;
	secs = client_data.last_secs - client_data.first_secs;
	packet->secs = (secs < 0xffff) ? htons(secs) : 0xffff;

	memcpy(packet->chaddr, client_data.client_mac, 6);
}

static void add_client_options(struct dhcp_packet *packet)
{
	int i, end, len;

	udhcp_add_simple_option(packet, DHCP_MAX_SIZE, htons(IP_UDP_DHCP_SIZE));

	/* Add a "param req" option with the list of options we'd like to have
	 * from stubborn DHCP servers. Pull the data from the struct in common.c.
	 * No bounds checking because it goes towards the head of the packet. */
	end = udhcp_end_option(packet->options);
	len = 0;
	for (i = 1; i < DHCP_END; i++) {
		if (client_data.opt_mask[i >> 3] & (1 << (i & 7))) {
			packet->options[end + OPT_DATA + len] = i;
			len++;
		}
	}
	if (len) {
		packet->options[end + OPT_CODE] = DHCP_PARAM_REQ;
		packet->options[end + OPT_LEN] = len;
		packet->options[end + OPT_DATA + len] = DHCP_END;
	}

	/* Request broadcast replies if we have no IP addr */
	if ((option_mask32 & OPT_B) && packet->ciaddr == 0)
		packet->flags |= htons(BROADCAST_FLAG);

	/* Add -x options if any */
	{
		struct option_set *curr = client_data.options;
		while (curr) {
			udhcp_add_binary_option(packet, curr->data);
			curr = curr->next;
		}
//		if (client_data.sname)
//			strncpy((char*)packet->sname, client_data.sname, sizeof(packet->sname) - 1);
//		if (client_data.boot_file)
//			strncpy((char*)packet->file, client_data.boot_file, sizeof(packet->file) - 1);
	}

	// This will be needed if we remove -V VENDOR_STR in favor of
	// -x vendor:VENDOR_STR
	//if (!udhcp_find_option(packet.options, DHCP_VENDOR, /*dhcpv6:*/ 0))
	//	/* not set, set the default vendor ID */
	//	...add (DHCP_VENDOR, "udhcp "BB_VER) opt...
}

static void add_serverid_and_clientid_options(struct dhcp_packet *packet, uint32_t server)
{
	struct option_set *ci;

	udhcp_add_simple_option(packet, DHCP_SERVER_ID, server);

	/* RFC 2131 section 2:
	 * If the client uses a 'client identifier' in one message,
	 * it MUST use that same identifier in all subsequent messages.
	 * section 3.1.6:
	 * If the client used a 'client identifier' when it obtained the lease,
	 * it MUST use the same 'client identifier' in the DHCPRELEASE message.
	 */
	ci = udhcp_find_option(client_data.options, DHCP_CLIENT_ID, /*dhcpv6:*/ 0);
	if (ci)
		udhcp_add_binary_option(packet, ci->data);
}

/* RFC 2131
 * 4.4.4 Use of broadcast and unicast
 *
 * The DHCP client broadcasts DHCPDISCOVER, DHCPREQUEST and DHCPINFORM
 * messages, unless the client knows the address of a DHCP server.
 * The client unicasts DHCPRELEASE messages to the server. Because
 * the client is declining the use of the IP address supplied by the server,
 * the client broadcasts DHCPDECLINE messages.
 *
 * When the DHCP client knows the address of a DHCP server, in either
 * INIT or REBOOTING state, the client may use that address
 * in the DHCPDISCOVER or DHCPREQUEST rather than the IP broadcast address.
 * The client may also use unicast to send DHCPINFORM messages
 * to a known DHCP server. If the client receives no response to DHCP
 * messages sent to the IP address of a known DHCP server, the DHCP
 * client reverts to using the IP broadcast address.
 */

static int raw_bcast_from_client_data_ifindex(struct dhcp_packet *packet, uint32_t src_nip)
{
	return udhcp_send_raw_packet(packet,
		/*src*/ src_nip, CLIENT_PORT,
		/*dst*/ INADDR_BROADCAST, SERVER_PORT, MAC_BCAST_ADDR,
		client_data.ifindex);
}

static int bcast_or_ucast(struct dhcp_packet *packet, uint32_t ciaddr, uint32_t server)
{
	if (server)
		return udhcp_send_kernel_packet(packet,
			ciaddr, CLIENT_PORT,
			server, SERVER_PORT,
			client_data.interface);
	return raw_bcast_from_client_data_ifindex(packet, ciaddr);
}

/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
/* NOINLINE: limit stack usage in caller */
static NOINLINE int send_discover(uint32_t requested)
{
	struct dhcp_packet packet;

	/* Fill in: op, htype, hlen, cookie, chaddr fields,
	 * xid field, message type option:
	 */
	init_packet(&packet, DHCPDISCOVER);

	if (requested)
		udhcp_add_simple_option(&packet, DHCP_REQUESTED_IP, requested);

	/* Add options: maxsize,
	 * "param req" option according to -O, options specified with -x
	 */
	add_client_options(&packet);

	bb_simple_info_msg("broadcasting discover");
	return raw_bcast_from_client_data_ifindex(&packet, INADDR_ANY);
}

/* Broadcast a DHCP request message */
/* RFC 2131 3.1 paragraph 3:
 * "The client _broadcasts_ a DHCPREQUEST message..."
 */
/* NOINLINE: limit stack usage in caller */
static NOINLINE int send_select(uint32_t server, uint32_t requested)
{
	struct dhcp_packet packet;
	struct in_addr temp_addr;
	char server_str[sizeof("255.255.255.255")];

/*
 * RFC 2131 4.3.2 DHCPREQUEST message
 * ...
 * If the DHCPREQUEST message contains a 'server identifier'
 * option, the message is in response to a DHCPOFFER message.
 * Otherwise, the message is a request to verify or extend an
 * existing lease. If the client uses a 'client identifier'
 * in a DHCPREQUEST message, it MUST use that same 'client identifier'
 * in all subsequent messages. If the client included a list
 * of requested parameters in a DHCPDISCOVER message, it MUST
 * include that list in all subsequent messages.
 */
	/* Fill in: op, htype, hlen, cookie, chaddr fields,
	 * xid field, message type option:
	 */
	init_packet(&packet, DHCPREQUEST);

	udhcp_add_simple_option(&packet, DHCP_REQUESTED_IP, requested);

	udhcp_add_simple_option(&packet, DHCP_SERVER_ID, server);

	/* Add options: maxsize,
	 * "param req" option according to -O, options specified with -x
	 */
	add_client_options(&packet);

	temp_addr.s_addr = server;
	strcpy(server_str, inet_ntoa(temp_addr));
	temp_addr.s_addr = requested;
	bb_info_msg("broadcasting select for %s, server %s",
			inet_ntoa(temp_addr),
			server_str
	);
	return raw_bcast_from_client_data_ifindex(&packet, INADDR_ANY);
}

/* Unicast or broadcast a DHCP renew message */
/* NOINLINE: limit stack usage in caller */
static NOINLINE int send_renew(uint32_t server, uint32_t ciaddr)
{
	struct dhcp_packet packet;

/*
 * RFC 2131 4.3.2 DHCPREQUEST message
 * ...
 * DHCPREQUEST generated during RENEWING state:
 *
 * 'server identifier' MUST NOT be filled in, 'requested IP address'
 * option MUST NOT be filled in, 'ciaddr' MUST be filled in with
 * client's IP address. In this situation, the client is completely
 * configured, and is trying to extend its lease. This message will
 * be unicast, so no relay agents will be involved in its
 * transmission.  Because 'giaddr' is therefore not filled in, the
 * DHCP server will trust the value in 'ciaddr', and use it when
 * replying to the client.
 */
	/* Fill in: op, htype, hlen, cookie, chaddr fields,
	 * xid field, message type option:
	 */
	init_packet(&packet, DHCPREQUEST);

	packet.ciaddr = ciaddr;

	/* Add options: maxsize,
	 * "param req" option according to -O, options specified with -x
	 */
	add_client_options(&packet);

	if (server) {
		struct in_addr temp_addr;
		temp_addr.s_addr = server;
		bb_info_msg("sending renew to server %s", inet_ntoa(temp_addr));
	} else {
		bb_simple_info_msg("broadcasting renew");
	}

	return bcast_or_ucast(&packet, ciaddr, server);
}

#if ENABLE_FEATURE_UDHCPC_ARPING
/* Broadcast a DHCP decline message */
/* NOINLINE: limit stack usage in caller */
static NOINLINE int send_decline(uint32_t server, uint32_t requested)
{
	struct dhcp_packet packet;

	/* Fill in: op, htype, hlen, cookie, chaddr, random xid fields,
	 * message type option:
	 */
	init_packet(&packet, DHCPDECLINE);

	/* DHCPDECLINE uses "requested ip", not ciaddr, to store offered IP */
	udhcp_add_simple_option(&packet, DHCP_REQUESTED_IP, requested);

	add_serverid_and_clientid_options(&packet, server);

	bb_simple_info_msg("broadcasting decline");
	return raw_bcast_from_client_data_ifindex(&packet, INADDR_ANY);
}
#endif

/* Unicast a DHCP release message */
static
ALWAYS_INLINE /* one caller, help compiler to use this fact */
int send_release(uint32_t server, uint32_t ciaddr)
{
	struct dhcp_packet packet;

	/* Fill in: op, htype, hlen, cookie, chaddr, random xid fields,
	 * message type option:
	 */
	init_packet(&packet, DHCPRELEASE);

	/* DHCPRELEASE uses ciaddr, not "requested ip", to store IP being released */
	packet.ciaddr = ciaddr;

	add_serverid_and_clientid_options(&packet, server);

	bb_info_msg("sending %s", "release");
	/* Note: normally we unicast here since "server" is not zero.
	 * However, there _are_ people who run "address-less" DHCP servers,
	 * and reportedly ISC dhcp client and Windows allow that.
	 */
	return bcast_or_ucast(&packet, ciaddr, server);
}

/* Returns -1 on errors that are fatal for the socket, -2 for those that aren't */
/* NOINLINE: limit stack usage in caller */
static NOINLINE int d4_recv_raw_packet(struct dhcp_packet *dhcp_pkt, int fd)
{
	int bytes;
	struct ip_udp_dhcp_packet packet;
	uint16_t check;
	unsigned char cmsgbuf[CMSG_LEN(sizeof(struct tpacket_auxdata))];
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;

	/* used to use just safe_read(fd, &packet, sizeof(packet))
	 * but we need to check for TP_STATUS_CSUMNOTREADY :(
	 */
	iov.iov_base = &packet;
	iov.iov_len = sizeof(packet);
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	for (;;) {
		bytes = recvmsg(fd, &msg, 0);
		if (bytes < 0) {
			if (errno == EINTR)
				continue;
			log1s("packet read error, ignoring");
			/* NB: possible down interface, etc. Caller should pause. */
			return bytes; /* returns -1 */
		}
		break;
	}

	if (bytes < (int) (sizeof(packet.ip) + sizeof(packet.udp))) {
		log1s("packet is too short, ignoring");
		return -2;
	}

	if (bytes < ntohs(packet.ip.tot_len)) {
		/* packet is bigger than sizeof(packet), we did partial read */
		log1s("oversized packet, ignoring");
		return -2;
	}

	/* ignore any extra garbage bytes */
	bytes = ntohs(packet.ip.tot_len);

	/* make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP
	 || packet.ip.version != IPVERSION
	 || packet.ip.ihl != (sizeof(packet.ip) >> 2)
	 || packet.udp.dest != htons(CLIENT_PORT)
	/* || bytes > (int) sizeof(packet) - can't happen */
	 || ntohs(packet.udp.len) != (uint16_t)(bytes - sizeof(packet.ip))
	) {
		log1s("unrelated/bogus packet, ignoring");
		return -2;
	}

	/* verify IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != inet_cksum(&packet.ip, sizeof(packet.ip))) {
		log1s("bad IP header checksum, ignoring");
		return -2;
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_PACKET
		 && cmsg->cmsg_type == PACKET_AUXDATA
		) {
			/* some VMs don't checksum UDP and TCP data
			 * they send to the same physical machine,
			 * here we detect this case:
			 */
			struct tpacket_auxdata *aux = (void *)CMSG_DATA(cmsg);
			if (aux->tp_status & TP_STATUS_CSUMNOTREADY)
				goto skip_udp_sum_check;
		}
	}

	/* verify UDP checksum. IP header has to be modified for this */
	memset(&packet.ip, 0, offsetof(struct iphdr, protocol));
	/* ip.xx fields which are not memset: protocol, check, saddr, daddr */
	packet.ip.tot_len = packet.udp.len; /* yes, this is needed */
	check = packet.udp.check;
	packet.udp.check = 0;
	if (check && check != inet_cksum(&packet, bytes)) {
		log1s("packet with bad UDP checksum, ignoring");
		return -2;
	}
 skip_udp_sum_check:

	if (packet.data.cookie != htonl(RFC1048_MAGIC)) {
		log1s("packet with bad magic, ignoring");
		return -2;
	}

	log2("received %s", "a packet");
	/* log2 because more informative msg for valid packets is printed later at log1 level */
	udhcp_dump_packet(&packet.data);

	bytes -= sizeof(packet.ip) + sizeof(packet.udp);
	memset(dhcp_pkt, 0, sizeof(*dhcp_pkt));
	memcpy(dhcp_pkt, &packet.data, bytes);
	return bytes;
}


/*** Main ***/

/* Values for client_data.listen_mode */
#define LISTEN_NONE   0
#define LISTEN_KERNEL 1
#define LISTEN_RAW    2

/* Values for client_data.state */
/* initial state: (re)start DHCP negotiation */
#define INIT_SELECTING  0
/* discover was sent, DHCPOFFER reply received */
#define REQUESTING      1
/* select/renew was sent, DHCPACK reply received */
#define BOUND           2
/* half of lease passed, want to renew it by sending unicast renew requests */
#define RENEWING        3
/* renew requests were not answered, lease is almost over, send broadcast renew */
#define REBINDING       4
/* manually requested renew (SIGUSR1) */
#define RENEW_REQUESTED 5
/* release, possibly manually requested (SIGUSR2) */
#define RELEASED        6

static int udhcp_raw_socket(int ifindex)
{
	int fd;
	struct sockaddr_ll sock;

	log2("opening raw socket on ifindex %d", ifindex);

	fd = xsocket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	/* ^^^^^
	 * SOCK_DGRAM: remove link-layer headers on input (SOCK_RAW keeps them)
	 * ETH_P_IP: want to receive only packets with IPv4 eth type
	 */

	memset(&sock, 0, sizeof(sock)); /* let's be deterministic */
	sock.sll_family = AF_PACKET;
	sock.sll_protocol = htons(ETH_P_IP);
	sock.sll_ifindex = ifindex;
	/*sock.sll_hatype = ARPHRD_???;*/
	/*sock.sll_pkttype = PACKET_???;*/
	/*sock.sll_halen = ???;*/
	/*sock.sll_addr[8] = ???;*/
	xbind(fd, (struct sockaddr *) &sock, sizeof(sock));

#if 0 /* Several users reported breakage when BPF filter is used */
	if (CLIENT_PORT == 68) {
		/* Use only if standard port is in use */
		/*
		 *	I've selected not to see LL header, so BPF doesn't see it, too.
		 *	The filter may also pass non-IP and non-ARP packets, but we do
		 *	a more complete check when receiving the message in userspace.
		 *
		 * and filter shamelessly stolen from:
		 *
		 *	http://www.flamewarmaster.de/software/dhcpclient/
		 *
		 * There are a few other interesting ideas on that page (look under
		 * "Motivation").  Use of netlink events is most interesting.  Think
		 * of various network servers listening for events and reconfiguring.
		 * That would obsolete sending HUP signals and/or make use of restarts.
		 *
		 * Copyright: 2006, 2007 Stefan Rompf <sux@loplof.de>.
		 * License: GPL v2.
		 */
		static const struct sock_filter filter_instr[] = {
			/* load 9th byte (protocol) */
			BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 9),
			/* jump to L1 if it is IPPROTO_UDP, else to L4 */
			BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_UDP, 0, 6),
			/* L1: load halfword from offset 6 (flags and frag offset) */
			BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 6),
			/* jump to L4 if any bits in frag offset field are set, else to L2 */
			BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K, 0x1fff, 4, 0),
			/* L2: skip IP header (load index reg with header len) */
			BPF_STMT(BPF_LDX|BPF_B|BPF_MSH, 0),
			/* load udp destination port from halfword[header_len + 2] */
			BPF_STMT(BPF_LD|BPF_H|BPF_IND, 2),
			/* jump to L3 if udp dport is CLIENT_PORT, else to L4 */
			BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 68, 0, 1),
			/* L3: accept packet ("accept 0x7fffffff bytes") */
			/* Accepting 0xffffffff works too but kernel 2.6.19 is buggy */
			BPF_STMT(BPF_RET|BPF_K, 0x7fffffff),
			/* L4: discard packet ("accept zero bytes") */
			BPF_STMT(BPF_RET|BPF_K, 0),
		};
		static const struct sock_fprog filter_prog = {
			.len = sizeof(filter_instr) / sizeof(filter_instr[0]),
			/* casting const away: */
			.filter = (struct sock_filter *) filter_instr,
		};
		/* Ignoring error (kernel may lack support for this) */
		if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter_prog,
				sizeof(filter_prog)) >= 0)
			log1("attached filter to raw socket fd"); // log?
	}
#endif

	if (setsockopt_1(fd, SOL_PACKET, PACKET_AUXDATA) != 0) {
		if (errno != ENOPROTOOPT)
			log1s("can't set PACKET_AUXDATA on raw socket");
	}

	return fd;
}

static void change_listen_mode(int new_mode)
{
	log1("entering listen mode: %s",
		new_mode != LISTEN_NONE
			? (new_mode == LISTEN_KERNEL ? "kernel" : "raw")
			: "none"
	);

	client_data.listen_mode = new_mode;
	if (client_data.sockfd >= 0) {
		close(client_data.sockfd);
		client_data.sockfd = -1;
	}
	if (new_mode == LISTEN_KERNEL)
		client_data.sockfd = udhcp_listen_socket(/*INADDR_ANY,*/ CLIENT_PORT, client_data.interface);
	else if (new_mode != LISTEN_NONE)
		client_data.sockfd = udhcp_raw_socket(client_data.ifindex);
	/* else LISTEN_NONE: client_data.sockfd stays closed */
}

static void perform_release(uint32_t server_id, uint32_t requested_ip)
{
	char buffer[sizeof("255.255.255.255")];
	struct in_addr temp_addr;

	change_listen_mode(LISTEN_NONE);

	/* send release packet */
	if (client_data.state == BOUND
	 || client_data.state == RENEWING
	 || client_data.state == REBINDING
	 || client_data.state == RENEW_REQUESTED
	) {
		temp_addr.s_addr = server_id;
		strcpy(buffer, inet_ntoa(temp_addr));
		temp_addr.s_addr = requested_ip;
		bb_info_msg("unicasting a release of %s to %s",
				inet_ntoa(temp_addr), buffer);
		client_data.xid = random_xid(); //TODO: can omit?
		send_release(server_id, requested_ip); /* unicast */
	}
	bb_simple_info_msg("entering released state");
/*
 * We can be here on: SIGUSR2,
 * or on exit (SIGTERM) and -R "release on quit" is specified.
 * Users requested to be notified in all cases, even if not in one
 * of the states above.
 */
	d4_run_script_deconfig();
	client_data.state = RELEASED;
}

#if BB_MMU
static void client_background(void)
{
	bb_daemonize(0);
	logmode &= ~LOGMODE_STDIO;
	/* rewrite pidfile, as our pid is different now */
	write_pidfile(client_data.pidfile);
}
#endif

//usage:#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
//usage:# define IF_UDHCP_VERBOSE(...) __VA_ARGS__
//usage:#else
//usage:# define IF_UDHCP_VERBOSE(...)
//usage:#endif
//usage:#define udhcpc_trivial_usage
//usage:       "[-fbq"IF_UDHCP_VERBOSE("v")"RB]"IF_FEATURE_UDHCPC_ARPING(" [-a[MSEC]]")" [-t N] [-T SEC] [-A SEC|-n]\n"
//usage:       "	[-i IFACE]"IF_FEATURE_UDHCP_PORT(" [-P PORT]")" [-s PROG] [-p PIDFILE]\n"
//usage:       "	[-oC] [-r IP] [-V VENDOR] [-F NAME] [-x OPT:VAL]... [-O OPT]..."
//usage:#define udhcpc_full_usage "\n"
//usage:     "\n	-i IFACE	Interface to use (default "CONFIG_UDHCPC_DEFAULT_INTERFACE")"
//usage:	IF_FEATURE_UDHCP_PORT(
//usage:     "\n	-P PORT		Use PORT (default 68)"
//usage:	)
//usage:     "\n	-s PROG		Run PROG at DHCP events (default "CONFIG_UDHCPC_DEFAULT_SCRIPT")"
//usage:     "\n	-p FILE		Create pidfile"
//usage:     "\n	-B		Request broadcast replies"
//usage:     "\n	-t N		Send up to N discover packets (default 3)"
//usage:     "\n	-T SEC		Pause between packets (default 3)"
//usage:     "\n	-A SEC		Wait if lease is not obtained (default 20)"
//usage:	USE_FOR_MMU(
//usage:     "\n	-b		Background if lease is not obtained"
//usage:	)
//usage:     "\n	-n		Exit if lease is not obtained"
//usage:     "\n	-q		Exit after obtaining lease"
//usage:     "\n	-R		Release IP on exit"
//usage:     "\n	-f		Run in foreground"
//usage:     "\n	-S		Log to syslog too"
//usage:	IF_FEATURE_UDHCPC_ARPING(
//usage:     "\n	-a[MSEC]	Validate offered address with ARP ping"
//usage:	)
//usage:     "\n	-r IP		Request this IP address"
//usage:     "\n	-o		Don't request any options (unless -O is given)"
//usage:     "\n	-O OPT		Request option OPT from server (cumulative)"
//usage:     "\n	-x OPT:VAL	Include option OPT in sent packets (cumulative)"
//usage:     "\n			Examples of string, numeric, and hex byte opts:"
//usage:     "\n			-x hostname:bbox - option 12"
//usage:     "\n			-x lease:3600 - option 51 (lease time)"
//usage:     "\n			-x 0x3d:0100BEEFC0FFEE - option 61 (client id)"
//usage:     "\n			-x 14:'\"dumpfile\"' - option 14 (shell-quoted)"
//usage:     "\n	-F NAME		Ask server to update DNS mapping for NAME"
//usage:     "\n	-V VENDOR	Vendor identifier (default 'udhcp VERSION')"
//usage:     "\n	-C		Don't send MAC as client identifier"
//usage:	IF_UDHCP_VERBOSE(
//usage:     "\n	-v		Verbose"
//usage:	)
//usage:     "\nSignals:"
//usage:     "\n	USR1	Renew lease"
//usage:     "\n	USR2	Release lease"

int udhcpc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int udhcpc_main(int argc UNUSED_PARAM, char **argv)
{
	uint8_t *message;
	const char *str_V, *str_F, *str_r;
	IF_FEATURE_UDHCPC_ARPING(const char *str_a = "2000";)
	IF_FEATURE_UDHCP_PORT(char *str_P;)
	uint8_t *clientid_mac_ptr;
	llist_t *list_O = NULL;
	llist_t *list_x = NULL;
	int tryagain_timeout = 20;
	int discover_timeout = 3;
	int discover_retries = 3;
	uint32_t server_id = server_id; /* for compiler */
	uint32_t requested_ip = 0;
	int packet_num;
	int timeout; /* must be signed */
	int lease_remaining; /* must be signed */
	unsigned opt;
	IF_FEATURE_UDHCPC_ARPING(unsigned arpping_ms;)
	int retval;

	setup_common_bufsiz();

	/* Default options */
	IF_FEATURE_UDHCP_PORT(SERVER_PORT = 67;)
	IF_FEATURE_UDHCP_PORT(CLIENT_PORT = 68;)
	client_data.interface = CONFIG_UDHCPC_DEFAULT_INTERFACE;
	client_data.script = CONFIG_UDHCPC_DEFAULT_SCRIPT;
	client_data.sockfd = -1;
	str_V = "udhcp "BB_VER;

	/* Make sure fd 0,1,2 are open */
	/* Set up the signal pipe on fds 3,4 - must be before openlog() */
	udhcp_sp_setup();

	/* Parse command line */
	opt = getopt32long(argv, "^"
		/* O,x: list; -T,-t,-A take numeric param */
		"CV:F:i:np:qRr:s:T:+t:+SA:+O:*ox:*fB"
		USE_FOR_MMU("b")
		IF_FEATURE_UDHCPC_ARPING("a::")
		IF_FEATURE_UDHCP_PORT("P:")
		"v"
		"\0" IF_UDHCP_VERBOSE("vv") /* -v is a counter */
		, udhcpc_longopts
		, &str_V, &str_F
		, &client_data.interface, &client_data.pidfile /* i,p */
		, &str_r /* r */
		, &client_data.script /* s */
		, &discover_timeout, &discover_retries, &tryagain_timeout /* T,t,A */
		, &list_O
		, &list_x
		IF_FEATURE_UDHCPC_ARPING(, &str_a)
		IF_FEATURE_UDHCP_PORT(, &str_P)
		IF_UDHCP_VERBOSE(, &dhcp_verbose)
	);
	if (opt & OPT_F) {
		char *p;
		unsigned len;
		/* FQDN option format: [0x51][len][flags][0][0]<fqdn> */
		len = strlen(str_F);
		p = udhcp_insert_new_option(
				&client_data.options, DHCP_FQDN,
				len + 3, /*dhcp6:*/ 0);
		/* Flag bits: 0000NEOS
		 * S: 1 = Client requests server to update A RR in DNS as well as PTR
		 * O: 1 = Server indicates to client that DNS has been updated regardless
		 * E: 1 = Name is in DNS format, i.e. <4>host<6>domain<3>com<0>,
		 *    not "host.domain.com". Format 0 is obsolete.
		 * N: 1 = Client requests server to not update DNS (S must be 0 then)
		 * Two [0] bytes which follow are deprecated and must be 0.
		 */
		p[OPT_DATA + 0] = 0x1;
		/*p[OPT_DATA + 1] = 0; - xzalloc did it */
		/*p[OPT_DATA + 2] = 0; */
		memcpy(p + OPT_DATA + 3, str_F, len); /* do not store NUL byte */
	}
	if (opt & OPT_r)
		if (!inet_aton(str_r, (void*)&requested_ip))
			bb_show_usage();
#if ENABLE_FEATURE_UDHCP_PORT
	if (opt & OPT_P) {
		CLIENT_PORT = xatou16(str_P);
		SERVER_PORT = CLIENT_PORT - 1;
	}
#endif
	IF_FEATURE_UDHCPC_ARPING(arpping_ms = xatou(str_a);)
	while (list_O) {
		char *optstr = llist_pop(&list_O);
		unsigned n = bb_strtou(optstr, NULL, 0);
		if (errno || n > 254) {
			n = udhcp_option_idx(optstr, dhcp_option_strings);
			n = dhcp_optflags[n].code;
		}
		client_data.opt_mask[n >> 3] |= 1 << (n & 7);
	}
	if (!(opt & OPT_o)) {
		unsigned i, n;
		for (i = 0; (n = dhcp_optflags[i].code) != 0; i++) {
			if (dhcp_optflags[i].flags & OPTION_REQ) {
				client_data.opt_mask[n >> 3] |= 1 << (n & 7);
			}
		}
	}
	while (list_x) {
		char *optstr = xstrdup(llist_pop(&list_x));
		udhcp_str2optset(optstr, &client_data.options,
				dhcp_optflags, dhcp_option_strings,
				/*dhcpv6:*/ 0
		);
		free(optstr);
	}
	if (str_V[0] != '\0') {
		char *p;
		unsigned len = strnlen(str_V, 254);
		p = udhcp_insert_new_option(
				&client_data.options, DHCP_VENDOR,
				len, /*dhcp6:*/ 0);
		memcpy(p + OPT_DATA, str_V, len); /* do not store NUL byte */
	}

	clientid_mac_ptr = NULL;
	if (!(opt & OPT_C) && !udhcp_find_option(client_data.options, DHCP_CLIENT_ID, /*dhcpv6:*/ 0)) {
		/* not suppressed and not set, create default client ID */
		clientid_mac_ptr = udhcp_insert_new_option(
				&client_data.options, DHCP_CLIENT_ID,
				1 + 6, /*dhcp6:*/ 0);
		clientid_mac_ptr[OPT_DATA] = 1; /* type: ethernet */
		clientid_mac_ptr += OPT_DATA + 1; /* skip option code, len, ethernet */
	}

	/* Not really necessary (we redo it on every iteration)
	 * but allows early (before daemonization) detection
	 * of bad interface name.
	 */
	if (udhcp_read_interface(client_data.interface,
			&client_data.ifindex,
			NULL,
			client_data.client_mac)
	) {
		return 1;
	}

#if !BB_MMU
	/* on NOMMU reexec (i.e., background) early */
	if (!(opt & OPT_f)) {
		bb_daemonize_or_rexec(0 /* flags */, argv);
		logmode = LOGMODE_NONE;
	}
#endif
	if (opt & OPT_S) {
		openlog(applet_name, LOG_PID, LOG_DAEMON);
		logmode |= LOGMODE_SYSLOG;
	}

	/* Create pidfile */
	write_pidfile(client_data.pidfile);
	/* Goes to stdout (unless NOMMU) and possibly syslog */
	bb_simple_info_msg("started, v"BB_VER);
	/* We want random_xid to be random... */
	srand(monotonic_us());

	client_data.state = INIT_SELECTING;
	d4_run_script_deconfig();
	packet_num = 0;
	timeout = 0;
	lease_remaining = 0;

	/* Main event loop. select() waits on signal pipe and possibly
	 * on sockfd.
	 * "continue" statements in code below jump to the top of the loop.
	 */
	for (;;) {
		struct pollfd pfds[2];
		struct dhcp_packet packet;

		//bb_error_msg("sockfd:%d, listen_mode:%d", client_data.sockfd, client_data.listen_mode);

		/* Was opening raw or udp socket here
		 * if (client_data.listen_mode != LISTEN_NONE && client_data.sockfd < 0),
		 * but on fast network renew responses return faster
		 * than we open sockets. Thus this code is moved
		 * to change_listen_mode(). Thus we open listen socket
		 * BEFORE we send renew request (see "case BOUND:"). */

		udhcp_sp_fd_set(pfds, client_data.sockfd);

		retval = 0;
		/* If we already timed out, fall through with retval = 0, else... */
		if (timeout > 0) {
			unsigned diff;

			if (timeout > INT_MAX/1000)
				timeout = INT_MAX/1000;
			log1("waiting %u seconds", timeout);
			diff = (unsigned)monotonic_sec();
			retval = poll(pfds, 2, timeout * 1000);
			diff = (unsigned)monotonic_sec() - diff;
			lease_remaining -= diff;
			if (lease_remaining < 0)
				lease_remaining = 0;
			timeout -= diff;
			if (timeout < 0)
				timeout = 0;

			if (retval < 0) {
				/* EINTR? A signal was caught, don't panic */
				if (errno == EINTR) {
					continue;
				}
				/* Else: an error occurred, panic! */
				bb_simple_perror_msg_and_die("poll");
			}
		}

		/* If timeout dropped to zero, time to become active:
		 * resend discover/renew/whatever
		 */
		if (retval == 0) {
			/* When running on a bridge, the ifindex may have changed
			 * (e.g. if member interfaces were added/removed
			 * or if the status of the bridge changed).
			 * Refresh ifindex and client_mac:
			 */
			if (udhcp_read_interface(client_data.interface,
					&client_data.ifindex,
					NULL,
					client_data.client_mac)
			) {
				goto ret0; /* iface is gone? */
			}
			if (clientid_mac_ptr)
				memcpy(clientid_mac_ptr, client_data.client_mac, 6);

			switch (client_data.state) {
			case INIT_SELECTING:
				if (!discover_retries || packet_num < discover_retries) {
					if (packet_num == 0) {
						change_listen_mode(LISTEN_RAW);
						client_data.xid = random_xid();
					}
					/* broadcast */
					send_discover(requested_ip);
					timeout = discover_timeout;
					packet_num++;
					continue;
				}
 leasefail:
				change_listen_mode(LISTEN_NONE);
				d4_run_script(NULL, "leasefail");
#if BB_MMU /* -b is not supported on NOMMU */
				if (opt & OPT_b) { /* background if no lease */
					bb_simple_info_msg("no lease, forking to background");
					client_background();
					/* do not background again! */
					opt = ((opt & ~(OPT_b|OPT_n)) | OPT_f);
					/* ^^^ also disables -n (-b takes priority over -n):
					 * ifup's default udhcpc options are -R -n,
					 * and users want to be able to add -b
					 * (in a config file) to make it background
					 * _and not exit_.
					 */
				} else
#endif
				if (opt & OPT_n) { /* abort if no lease */
					bb_simple_info_msg("no lease, failing");
					retval = 1;
					goto ret;
				}
				/* Wait before trying again */
				timeout = tryagain_timeout;
				packet_num = 0;
				continue;
			case REQUESTING:
				if (packet_num < 3) {
					/* send broadcast select packet */
					send_select(server_id, requested_ip);
					timeout = discover_timeout;
					packet_num++;
					continue;
				}
				/* Timed out, go back to init state.
				 * "discover...select...discover..." loops
				 * were seen in the wild. Treat them similarly
				 * to "no response to discover" case */
				client_data.state = INIT_SELECTING;
				goto leasefail;
			case BOUND:
				/* 1/2 lease passed, enter renewing state */
				client_data.state = RENEWING;
				client_data.first_secs = 0; /* make secs field count from 0 */
			got_SIGUSR1:
				log1s("entering renew state");
				change_listen_mode(LISTEN_KERNEL);
				/* fall right through */
			case RENEW_REQUESTED: /* in manual (SIGUSR1) renew */
			case RENEWING:
				if (packet_num == 0) {
					/* Send an unicast renew request */
			/* Sometimes observed to fail (EADDRNOTAVAIL) to bind
			 * a new UDP socket for sending inside send_renew.
			 * I hazard to guess existing listening socket
			 * is somehow conflicting with it, but why is it
			 * not deterministic then?! Strange.
			 * Anyway, it does recover by eventually failing through
			 * into INIT_SELECTING state.
			 */
					if (send_renew(server_id, requested_ip) >= 0) {
						timeout = discover_timeout;
						packet_num++;
						continue;
					}
					/* else: error sending.
					 * example: ENETUNREACH seen with server
					 * which gave us bogus server ID 1.1.1.1
					 * which wasn't reachable (and probably did not exist).
					 */
				} /* else: we had sent one packet, but got no reply */
				log1s("no response to renew");
				if (lease_remaining > 30) {
					/* Some lease time remains, try to renew later */
					change_listen_mode(LISTEN_NONE);
					goto BOUND_for_half_lease;
				}
				/* Enter rebinding state */
				client_data.state = REBINDING;
				log1s("entering rebinding state");
				/* Switch to bcast receive */
				change_listen_mode(LISTEN_RAW);
				packet_num = 0;
				/* fall right through */
			case REBINDING:
				/* Lease is *really* about to run out,
				 * try to find DHCP server using broadcast */
				if (lease_remaining > 0 && packet_num < 3) {
					/* send a broadcast renew request */
					send_renew(0 /*INADDR_ANY*/, requested_ip);
					timeout = discover_timeout;
					packet_num++;
					continue;
				}
				/* Timed out, enter init state */
				change_listen_mode(LISTEN_NONE);
				bb_simple_info_msg("lease lost, entering init state");
				d4_run_script_deconfig();
				client_data.state = INIT_SELECTING;
				client_data.first_secs = 0; /* make secs field count from 0 */
				timeout = 0;
				packet_num = 0;
				continue;
			/* case RELEASED: */
			}
			/* RELEASED state (when we got SIGUSR2) ends up here.
			 * (wait for SIGUSR1 to re-init, or for TERM, etc)
			 */
			timeout = INT_MAX;
			continue; /* back to main loop */
		} /* if poll timed out */

		/* poll() didn't timeout, something happened */

		/* Is it a signal? */
		switch (udhcp_sp_read()) {
		case SIGUSR1:
			if (client_data.state <= REQUESTING)
				/* Initial negotiations in progress, do not disturb */
				break;
			if (client_data.state == REBINDING)
				/* Do not go back from rebind to renew state */
				break;

			if (lease_remaining > 30) /* if renew fails, do not go back to BOUND */
				lease_remaining = 30;
			client_data.first_secs = 0; /* make secs field count from 0 */
			packet_num = 0;

			switch (client_data.state) {
			case BOUND:
			case RENEWING:
				/* Try to renew/rebind */
				client_data.state = RENEW_REQUESTED;
				goto got_SIGUSR1;

			case RENEW_REQUESTED:
				/* Two SIGUSR1 received, start things over */
				change_listen_mode(LISTEN_NONE);
				d4_run_script_deconfig();

			default:
			/* case RELEASED: */
				/* Wake from SIGUSR2-induced deconfigured state */
				change_listen_mode(LISTEN_NONE);
			}
			client_data.state = INIT_SELECTING;
			/* Kill any timeouts, user wants this to hurry along */
			timeout = 0;
			continue;
		case SIGUSR2:
			perform_release(server_id, requested_ip);
			/* ^^^ switches to LISTEN_NONE */
			timeout = INT_MAX;
			continue;
		case SIGTERM:
			bb_info_msg("received %s", "SIGTERM");
			goto ret0;
		}

		/* Is it a packet? */
		if (!pfds[1].revents)
			continue; /* no */

		{
			int len;

			/* A packet is ready, read it */
			if (client_data.listen_mode == LISTEN_KERNEL)
				len = udhcp_recv_kernel_packet(&packet, client_data.sockfd);
			else
				len = d4_recv_raw_packet(&packet, client_data.sockfd);
			if (len == -1) {
				/* Error is severe, reopen socket */
				bb_error_msg("read error: "STRERROR_FMT", reopening socket" STRERROR_ERRNO);
				sleep(discover_timeout); /* 3 seconds by default */
				change_listen_mode(client_data.listen_mode); /* just close and reopen */
			}
			if (len < 0)
				continue;
		}

		if (packet.xid != client_data.xid) {
			log1("xid %x (our is %x)%s",
				(unsigned)packet.xid, (unsigned)client_data.xid,
				", ignoring packet"
			);
			continue;
		}

		/* Ignore packets that aren't for us */
		if (packet.hlen != 6
		 || memcmp(packet.chaddr, client_data.client_mac, 6) != 0
		) {
//FIXME: need to also check that last 10 bytes are zero
			log1("chaddr does not match%s", ", ignoring packet");
			continue;
		}

		message = udhcp_get_option(&packet, DHCP_MESSAGE_TYPE);
		if (message == NULL) {
			log1("no message type option%s", ", ignoring packet");
			continue;
		}

		switch (client_data.state) {
		case INIT_SELECTING:
			/* Must be a DHCPOFFER */
			if (*message == DHCPOFFER) {
				struct in_addr temp_addr;
				uint8_t *temp;

/* What exactly is server's IP? There are several values.
 * Example DHCP offer captured with tchdump:
 *
 * 10.34.25.254:67 > 10.34.25.202:68 // IP header's src
 * BOOTP fields:
 * Your-IP 10.34.25.202
 * Server-IP 10.34.32.125   // "next server" IP
 * Gateway-IP 10.34.25.254  // relay's address (if DHCP relays are in use)
 * DHCP options:
 * DHCP-Message Option 53, length 1: Offer
 * Server-ID Option 54, length 4: 10.34.255.7       // "server ID"
 * Default-Gateway Option 3, length 4: 10.34.25.254 // router
 *
 * We think that real server IP (one to use in renew/release)
 * is one in Server-ID option. But I am not 100% sure.
 * IP header's src and Gateway-IP (same in this example)
 * might work too.
 * "Next server" and router are definitely wrong ones to use, though...
 */
/* We used to ignore packets without DHCP_SERVER_ID.
 * I've got user reports from people who run "address-less" servers.
 * They either supply DHCP_SERVER_ID of 0.0.0.0 or don't supply it at all.
 * They say ISC DHCP client supports this case.
 */
				server_id = 0;
				temp = udhcp_get_option32(&packet, DHCP_SERVER_ID);
				if (!temp) {
					bb_simple_info_msg("no server ID, using 0.0.0.0");
				} else {
					/* it IS unaligned sometimes, don't "optimize" */
					move_from_unaligned32(server_id, temp);
				}
				/*xid = packet.xid; - already is */
				temp_addr.s_addr = requested_ip = packet.yiaddr;
				log1("received offer of %s", inet_ntoa(temp_addr));

				/* enter requesting state */
				client_data.state = REQUESTING;
				timeout = 0;
				packet_num = 0;
			}
			continue;
		case REQUESTING:
		case RENEWING:
		case RENEW_REQUESTED:
		case REBINDING:
			if (*message == DHCPACK) {
				unsigned start;
				struct in_addr temp_addr;
				char server_str[sizeof("255.255.255.255")];
				uint8_t *temp;

				change_listen_mode(LISTEN_NONE);

				temp_addr.s_addr = server_id;
				strcpy(server_str, inet_ntoa(temp_addr));
				temp_addr.s_addr = packet.yiaddr;

				lease_remaining = 60 * 60;
				temp = udhcp_get_option32(&packet, DHCP_LEASE_TIME);
				if (temp) {
					uint32_t lease;
					/* it IS unaligned sometimes, don't "optimize" */
					move_from_unaligned32(lease, temp);
					lease_remaining = ntohl(lease);
				}
				/* Log message _before_ we sanitize lease */
				bb_info_msg("lease of %s obtained from %s, lease time %u%s",
					inet_ntoa(temp_addr), server_str, (unsigned)lease_remaining,
					temp ? "" : " (default)"
				);
				/* paranoia: must not be too small and not prone to overflows */
				/* NB: 60s leases _are_ used in real world
				 * (temporary IPs while ISP modem initializes)
				 * do not break this case by bumping it up.
				 */
				if (lease_remaining < 0) /* signed overflow? */
					lease_remaining = INT_MAX;
				if (lease_remaining < 30)
					lease_remaining = 30;
				requested_ip = packet.yiaddr;
#if ENABLE_FEATURE_UDHCPC_ARPING
				if (opt & OPT_a) {
/* RFC 2131 3.1 paragraph 5:
 * "The client receives the DHCPACK message with configuration
 * parameters. The client SHOULD perform a final check on the
 * parameters (e.g., ARP for allocated network address), and notes
 * the duration of the lease specified in the DHCPACK message. At this
 * point, the client is configured. If the client detects that the
 * address is already in use (e.g., through the use of ARP),
 * the client MUST send a DHCPDECLINE message to the server and restarts
 * the configuration process..." */
					if (!arpping(requested_ip,
							NULL,
							(uint32_t) 0,
							client_data.client_mac,
							client_data.interface,
							arpping_ms)
					) {
						bb_simple_info_msg("offered address is in use "
							"(got ARP reply), declining");
						client_data.xid = random_xid(); //TODO: can omit?
						send_decline(server_id, packet.yiaddr);

						if (client_data.state != REQUESTING)
							d4_run_script_deconfig();
						client_data.state = INIT_SELECTING;
						client_data.first_secs = 0; /* make secs field count from 0 */
						requested_ip = 0;
						timeout = tryagain_timeout;
						packet_num = 0;
						continue; /* back to main loop */
					}
				}
#endif
				/* enter bound state */
				start = monotonic_sec();
				d4_run_script(&packet, client_data.state == REQUESTING ? "bound" : "renew");
				lease_remaining -= (unsigned)monotonic_sec() - start;
				if (lease_remaining < 0)
					lease_remaining = 0;
				if (opt & OPT_q) { /* quit after lease */
					goto ret0;
				}
				/* future renew failures should not exit (JM) */
				opt &= ~OPT_n;
#if BB_MMU /* NOMMU case backgrounded earlier */
				if (!(opt & OPT_f)) {
					client_background();
					/* do not background again! */
					opt = ((opt & ~OPT_b) | OPT_f);
				}
#endif
 BOUND_for_half_lease:
				timeout = (unsigned)lease_remaining / 2;
				client_data.state = BOUND;
				/* make future renew packets use different xid */
				/* client_data.xid = random_xid(); ...but why bother? */
				packet_num = 0;
				continue; /* back to main loop */
			}
			if (*message == DHCPNAK) {
				/* If network has more than one DHCP server,
				 * "wrong" server can reply first, with a NAK.
				 * Do not interpret it as a NAK from "our" server.
				 */
				uint32_t svid = 0; /* we treat no server id as 0.0.0.0 */
				uint8_t *temp = udhcp_get_option32(&packet, DHCP_SERVER_ID);
				if (temp)
					move_from_unaligned32(svid, temp);
				if (svid != server_id) {
					log1("received DHCP NAK with wrong"
						" server ID%s", ", ignoring packet");
					continue;
				}
				/* return to init state */
				change_listen_mode(LISTEN_NONE);
				bb_info_msg("received %s", "DHCP NAK");
				d4_run_script(&packet, "nak");
				if (client_data.state != REQUESTING)
					d4_run_script_deconfig();
				sleep(3); /* avoid excessive network traffic */
				client_data.state = INIT_SELECTING;
				client_data.first_secs = 0; /* make secs field count from 0 */
				requested_ip = 0;
				timeout = 0;
				packet_num = 0;
			}
			continue;
		/* case BOUND: - ignore all packets */
		/* case RELEASED: - ignore all packets */
		}
		/* back to main loop */
	} /* for (;;) - main loop ends */

 ret0:
	if (opt & OPT_R) /* release on quit */
		perform_release(server_id, requested_ip);
	retval = 0;
 ret:
	/*if (client_data.pidfile) - remove_pidfile has its own check */
		remove_pidfile(client_data.pidfile);
	return retval;
}
