/* Tests for the support_format_* functions.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <support/check.h>
#include <support/check_nss.h>
#include <support/format_nss.h>

#include <aliases.h>
#include <arpa/inet.h>
#include <grp.h>
#include <gshadow.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <pwd.h>
#include <rpc/netdb.h>
#include <shadow.h>

static void
test_format_address_family (void)
{
  support_check_nss ("AF_INET", "address_family",
                     support_format_address_family (AF_INET),
                     "INET");
  support_check_nss ("AF_INET6", "address_family",
                     support_format_address_family (AF_INET6),
                     "INET6");
  support_check_nss ("AF_LOCAL", "address_family",
                     support_format_address_family (AF_LOCAL),
                     "LOCAL");
  support_check_nss ("AF_UNSPEC", "address_family",
                     support_format_address_family (AF_UNSPEC),
                     "UNSPEC");
  support_check_nss ("unknown family", "address_family",
                     support_format_address_family (12345),
                     "<unknown address family 12345>");
}

static void
test_format_herrno (void)
{
  support_check_nss ("HOST_NOT_FOUND", "herrno",
                     support_format_herrno (HOST_NOT_FOUND),
                     "HOST_NOT_FOUND");
  support_check_nss ("NO_ADDRESS", "herrno",
                     support_format_herrno (NO_ADDRESS),
                     "NO_ADDRESS");
  support_check_nss ("NO_RECOVERY", "herrno",
                     support_format_herrno (NO_RECOVERY),
                     "NO_RECOVERY");
  support_check_nss ("TRY_AGAIN", "herrno",
                     support_format_herrno (TRY_AGAIN),
                     "TRY_AGAIN");
  support_check_nss ("invalid herrno", "herrno",
                     support_format_herrno (12345),
                     "<invalid h_errno value 12345>\n");
}

static void
test_format_passwd (void)
{
  struct passwd p =
    {
      .pw_name = (char *) "testuser",
      .pw_passwd = (char *) "x",
      .pw_uid = 1000,
      .pw_gid = 1000,
      .pw_gecos = (char *) "Test User",
      .pw_dir = (char *) "/home/testuser",
      .pw_shell = (char *) "/bin/bash"
    };
  support_check_nss ("passwd", "passwd",
                     support_format_passwd (&p),
                     "name: testuser\n"
                     "passwd: x\n"
                     "uid: 1000\n"
                     "gid: 1000\n"
                     "gecos: Test User\n"
                     "dir: /home/testuser\n"
                     "shell: /bin/bash\n");
}

static void
test_format_group (void)
{
  char *members[] = { (char *) "user1", (char *) "user2", NULL };
  struct group g =
    {
      .gr_name = (char *) "testgroup",
      .gr_passwd = (char *) "x",
      .gr_gid = 100,
      .gr_mem = members
    };
  support_check_nss ("group with members", "group",
                     support_format_group (&g),
                     "name: testgroup\n"
                     "passwd: x\n"
                     "gid: 100\n"
                     "member: user1\n"
                     "member: user2\n");

  char *no_members[] = { NULL };
  struct group g_empty =
    {
      .gr_name = (char *) "emptygroup",
      .gr_passwd = (char *) "",
      .gr_gid = 200,
      .gr_mem = no_members
    };
  support_check_nss ("group without members", "group",
                     support_format_group (&g_empty),
                     "name: emptygroup\n"
                     "passwd: \n"
                     "gid: 200\n");
}

static void
test_format_spwd (void)
{
  struct spwd s =
    {
      .sp_namp = (char *) "testuser",
      .sp_pwdp = (char *) "$6$rounds=5000$hash",
      .sp_lstchg = 19000,
      .sp_min = 0,
      .sp_max = 99999,
      .sp_warn = 7,
      .sp_inact = -1,
      .sp_expire = -1,
      .sp_flag = 0
    };
  support_check_nss ("spwd", "spwd",
                     support_format_spwd (&s),
                     "sp_namp: testuser\n"
                     "sp_pwdp: $6$rounds=5000$hash\n"
                     "sp_lstchg: 19000\n"
                     "sp_min: 0\n"
                     "sp_max: 99999\n"
                     "sp_warn: 7\n"
                     "sp_inact: -1\n"
                     "sp_expire: -1\n"
                     "sp_flag: 0\n");
}

static void
test_format_sgrp (void)
{
  char *admins[] = { (char *) "admin1", NULL };
  char *members[] = { (char *) "user1", (char *) "user2", NULL };
  struct sgrp s =
    {
      .sg_namp = (char *) "testgroup",
      .sg_passwd = (char *) "!",
      .sg_adm = admins,
      .sg_mem = members
    };
  support_check_nss ("sgrp", "sgrp",
                     support_format_sgrp (&s),
                     "name: testgroup\n"
                     "passwd: !\n"
                     "admin: admin1\n"
                     "member: user1\n"
                     "member: user2\n");

  char *no_admins[] = { NULL };
  char *no_members[] = { NULL };
  struct sgrp s_empty =
    {
      .sg_namp = (char *) "emptygroup",
      .sg_passwd = (char *) "",
      .sg_adm = no_admins,
      .sg_mem = no_members
    };
  support_check_nss ("sgrp empty", "sgrp",
                     support_format_sgrp (&s_empty),
                     "name: emptygroup\n"
                     "passwd: \n");
}

static void
test_format_protoent (void)
{
  char *aliases[] = { (char *) "ICMP", NULL };
  struct protoent p =
    {
      .p_name = (char *) "icmp",
      .p_aliases = aliases,
      .p_proto = 1
    };
  support_check_nss ("protoent with alias", "protoent",
                     support_format_protoent (&p),
                     "name: icmp\n"
                     "alias: ICMP\n"
                     "proto: 1\n");

  char *no_aliases[] = { NULL };
  struct protoent p_no_alias =
    {
      .p_name = (char *) "tcp",
      .p_aliases = no_aliases,
      .p_proto = 6
    };
  support_check_nss ("protoent without alias", "protoent",
                     support_format_protoent (&p_no_alias),
                     "name: tcp\n"
                     "proto: 6\n");
}

static void
test_format_servent (void)
{
  char *aliases[] = { (char *) "WWW", (char *) "www-http", NULL };
  struct servent s =
    {
      .s_name = (char *) "http",
      .s_aliases = aliases,
      .s_port = htons (80),
      .s_proto = (char *) "tcp"
    };
  support_check_nss ("servent", "servent",
                     support_format_servent (&s),
                     "name: http\n"
                     "alias: WWW\n"
                     "alias: www-http\n"
                     "port: 80\n"
                     "proto: tcp\n");
}

static void
test_format_rpcent (void)
{
  char *aliases[] = { (char *) "portmap", (char *) "sunrpc", NULL };
  struct rpcent r =
    {
      .r_name = (char *) "portmapper",
      .r_aliases = aliases,
      .r_number = 100000
    };
  support_check_nss ("rpcent", "rpcent",
                     support_format_rpcent (&r),
                     "name: portmapper\n"
                     "number: 100000\n"
                     "alias: portmap\n"
                     "alias: sunrpc\n");
}

static void
test_format_aliasent (void)
{
  char *members[] =
    { (char *) "user1@example.com", (char *) "user2@example.com" };
  struct aliasent a =
    {
      .alias_name = (char *) "staff",
      .alias_members_len = 2,
      .alias_members = members,
      .alias_local = 0
    };
  support_check_nss ("aliasent", "aliasent",
                     support_format_aliasent (&a),
                     "name: staff\n"
                     "member: user1@example.com\n"
                     "member: user2@example.com\n"
                     "local: 0\n");
}

static void
test_format_ether_addr (void)
{
  struct ether_addr e = { { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 } };
  support_check_nss ("ether_addr", "ether_addr",
                     support_format_ether_addr (&e),
                     "address: 0:11:22:33:44:55\n");
}

static void
test_format_hostent (void)
{
  char addr1[4] = { 192, 0, 2, 1 };
  char addr2[4] = { 192, 0, 2, 2 };
  char *addr_list[] = { addr1, addr2, NULL };
  char *aliases[] = { (char *) "www.example", NULL };
  struct hostent h =
    {
      .h_name = (char *) "example.com",
      .h_aliases = aliases,
      .h_addrtype = AF_INET,
      .h_length = 4,
      .h_addr_list = addr_list
    };
  support_check_nss ("hostent IPv4", "hostent",
                     support_format_hostent (&h),
                     "name: example.com\n"
                     "alias: www.example\n"
                     "address: 192.0.2.1\n"
                     "address: 192.0.2.2\n");

  char addr6[16] = { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 1 };
  char *addr6_list[] = { addr6, NULL };
  char *no_aliases[] = { NULL };
  struct hostent h6 =
    {
      .h_name = (char *) "ipv6.example.com",
      .h_aliases = no_aliases,
      .h_addrtype = AF_INET6,
      .h_length = 16,
      .h_addr_list = addr6_list
    };
  support_check_nss ("hostent IPv6", "hostent",
                     support_format_hostent (&h6),
                     "name: ipv6.example.com\n"
                     "address: 2001:db8::1\n");
}

static void
test_format_netent (void)
{
  char *aliases[] = { (char *) "localnet", NULL };
  struct netent n =
    {
      .n_name = (char *) "loopback",
      .n_aliases = aliases,
      .n_addrtype = AF_INET,
      .n_net = 0x7f000000
    };
  support_check_nss ("netent", "netent",
                     support_format_netent (&n),
                     "name: loopback\n"
                     "alias: localnet\n"
                     "net: 0x7f000000\n");
}

static void
test_format_addrinfo (void)
{
  /* Test successful result with IPv4 address.  */
  struct sockaddr_in sin =
    {
      .sin_family = AF_INET,
      .sin_port = htons (80),
      .sin_addr = { htonl (0xc0000201) }  /* 192.0.2.1 */
    };
  struct addrinfo ai =
    {
      .ai_flags = 0,
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
      .ai_protocol = IPPROTO_TCP,
      .ai_addrlen = sizeof (sin),
      .ai_addr = (struct sockaddr *) &sin,
      .ai_canonname = NULL,
      .ai_next = NULL
    };
  support_check_nss ("addrinfo IPv4 STREAM/TCP", "addrinfo",
                     support_format_addrinfo (&ai, 0),
                     "address: STREAM/TCP 192.0.2.1 80\n");

  /* Test with canonname.  */
  struct addrinfo ai_canon =
    {
      .ai_flags = AI_CANONNAME,
      .ai_family = AF_INET,
      .ai_socktype = SOCK_DGRAM,
      .ai_protocol = IPPROTO_UDP,
      .ai_addrlen = sizeof (sin),
      .ai_addr = (struct sockaddr *) &sin,
      .ai_canonname = (char *) "canonical.example.com",
      .ai_next = NULL
    };
  support_check_nss ("addrinfo with canonname", "addrinfo",
                     support_format_addrinfo (&ai_canon, 0),
                     "flags: AI_CANONNAME\n"
                     "canonname: canonical.example.com\n"
                     "address: DGRAM/UDP 192.0.2.1 80\n");

  /* Test error case.  */
  support_check_nss ("addrinfo EAI_NONAME", "addrinfo",
                     support_format_addrinfo (NULL, EAI_NONAME),
                     "error: Name or service not known\n");
}

static int
do_test (void)
{
  test_format_address_family ();
  test_format_herrno ();
  test_format_passwd ();
  test_format_group ();
  test_format_spwd ();
  test_format_sgrp ();
  test_format_protoent ();
  test_format_servent ();
  test_format_rpcent ();
  test_format_aliasent ();
  test_format_ether_addr ();
  test_format_hostent ();
  test_format_netent ();
  test_format_addrinfo ();
  return 0;
}

#include <support/test-driver.c>
