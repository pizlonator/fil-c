/* host.c - DNS lookup utility
 *
 * Copyright 2014 Rich Felker <dalias@aerifal.cx>
 *
 * No standard, but there's a version in bind9
 * See https://www.ietf.org/rfc/rfc1035.txt
 * See https://www.ietf.org/rfc/rfc3596.txt

USE_HOST(NEWTOY(host, "<1>2avt:", TOYFLAG_USR|TOYFLAG_BIN))

config HOST
  bool "host"
  default y
  help
    usage: host [-v] [-t TYPE] NAME [SERVER]

    Look up DNS records for NAME, either domain name or IPv4/IPv6 address to
    reverse lookup, from SERVER or default DNS server(s).

    -a	All records
    -t TYPE	Record TYPE (number or ANY A AAAA CNAME MX NS PTR SOA SRV TXT)
    -v	Verbose
*/

#define FOR_host
#include "toys.h"
#include <resolv.h>

GLOBALS(
  char *t;

  char **nsname;
  unsigned nslen;
)

static const struct rrt {
  char *name, *msg;
  int type;
} rrt[] = { { "A", "has address", 1 }, { "NS", "name server", 2 },
  { "CNAME", "is a nickname for", 5 }, { "SOA", "start of authority", 6 },
  { "PTR", "domain name pointer", 12 }, { "HINFO", "host information", 13 },
  { "MX", "mail is handled", 15 }, { "TXT", "descriptive text", 16 },
  { "AAAA", "has IPv6 address", 28 }, { "SRV", "has SRV record", 33 }
};

int xdn_expand(char *packet, char *endpkt, char *comp, char *expand, int elen)
{
  int i = dn_expand(packet, endpkt, comp, expand, elen);

  if (i<1) error_exit("bad dn_expand");

  return i;
}

// Fetch "nameserve" lines from /etc/resolv.conf. Ignores 'options' lines
static void get_nsname(char **pline, long len)
{
  char *line, *p;

  if (!len) return;
  line = *pline;
  if (strstart(&line, "nameserver") && isspace(*line)) {
    while (isspace(*line)) line++;
    for (p = line; *p && !isspace(*p) && *p!='#'; p++);
    if (p == line) return;
    *p = 0;
    if (!(TT.nslen&8))
      TT.nsname = xrealloc(TT.nsname, (TT.nslen+8)*sizeof(void *));
    TT.nsname[TT.nslen++] = xstrdup(line);
  }
}

void host_main(void)
{
  int verbose = FLAG(a)||FLAG(v), type, abuf_len = 65536, //Largest TCP response
      i, j, sec, rcode, qlen, alen QUIET, pllen = 0, t2len = 2048;
  unsigned count, ttl;
  char *abuf = xmalloc(abuf_len), *name = *toys.optargs, *p, *ss,
       *t2 = toybuf+t2len;
  struct addrinfo *ai;

  // What kind of query are we doing?
  if (!TT.t && FLAG(a)) TT.t = "255";
  if (!getaddrinfo(name, 0,&(struct addrinfo){.ai_flags=AI_NUMERICHOST}, &ai)) {
    name = toybuf;
    if (ai->ai_family == AF_INET) {
      p = (void *)&((struct sockaddr_in *)ai->ai_addr)->sin_addr;
      sprintf(name, "%d.%d.%d.%d.in-addr.arpa", p[3], p[2], p[1], p[0]);
    } else if (ai->ai_family == AF_INET6) {
      p = (void *)&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
      for (j = 0, i = 15; i>=0; i--)
        j += sprintf(name+j, "%x.%x.", p[i]&15, p[i]>>4);
      strcpy(name+j, "ip6.arpa");
    }
    if (!TT.t) TT.t = "12";
  } else if (!TT.t) TT.t = "1";

  // Prepare query packet of appropriate type
  if (TT.t[0]-'0'<10) type = atoi(TT.t); // TODO
  else if (!strcasecmp(TT.t, "any") || !strcmp(TT.t, "*")) type = 255;
  else {
    for (i = 0; i<ARRAY_LEN(rrt); i++) if (!strcasecmp(TT.t, rrt[i].name)) {
      type = rrt[i].type;
      break;
    }
    if (i == ARRAY_LEN(rrt)) error_exit("bad -t: %s", TT.t);
  }
  qlen = res_mkquery(0, name, 1, type, 0, 0, 0, t2, t2len);
  if (qlen<0) error_exit("bad NAME: %s", name);

  // Grab nameservers
  if (toys.optargs[1]) TT.nsname = toys.optargs+1;
  else do_lines(xopen("/etc/resolv.conf", O_RDONLY), '\n', get_nsname);
  if (!TT.nsname) error_exit("No nameservers");

  // Send one query packet to each server until we receive response
  while (*TT.nsname) {
    if (verbose) printf("Using domain server %s:\n", *TT.nsname);
    ai = xgetaddrinfo(*TT.nsname, "53", 0, SOCK_DGRAM, 0, 0);
    i = xsocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    xconnect(i, ai->ai_addr, ai->ai_addrlen);
    setsockopt(i, SOL_SOCKET, SO_RCVTIMEO, &(struct timeval){ .tv_sec = 5 },
      sizeof(struct timeval));
    send(i, t2, qlen, 0);
    alen = recv(i, abuf, abuf_len, 0);
    close(i);
    if (16<alen) break;
    if (!*++TT.nsname) error_exit("Host not found.");
  }

  // Did it error?
  rcode = abuf[3]&7;
  if (verbose) {
    printf("rcode = %d, ancount = %d\n", rcode, (int)peek_be(abuf+6, 2));
    if (!(abuf[2]&4)) puts("The following answer is not authoritative:");
  }
  if (rcode) error_exit("Host not found: %s",
    (char *[]){ "Format error", "Server failure",
    "Non-existant domain", "Not implemented", "Refused", ""}[rcode-1]);
  if (abuf[2]&2) puts("Truncated");

  // Print the result
  p = abuf + 12;
  qlen = 0;
  for (sec = 0; sec<(2<<verbose); sec++) {
    count = peek_be(abuf+4+2*sec, 2);
    if (verbose && count && sec>1)
      puts(sec==2 ? "For authoritative answers, see:"
        : "Additional information:");

    for (; count--; p += pllen) {
      p += xdn_expand(abuf, abuf+alen, p, toybuf, sizeof(toybuf)-t2len);
      if (alen-(p-abuf)<10) error_exit("bad header");
      type = peek_be(p, 2);
      p += 4;
      if (!sec) continue;
      ttl = peek_be(p, 4);
      p += 4;
      pllen = peek_be(p, 2);
      p += 2;
      if ((p-abuf)+pllen>alen) error_exit("bad header");
      if (type==1 || type == 28)
        inet_ntop(type==1 ? AF_INET : AF_INET6, p, t2, t2len);
      else if (type==2 || type==5) xdn_expand(abuf, abuf+alen, p, t2, t2len);
      else if (type==13 || type==16) {
        if (pllen && pllen-1==*p) p++, pllen--;
        sprintf(t2, "\"%.*s\"", minof(pllen, t2len), p);
      } else if (type==6) {
        ss = p+xdn_expand(abuf, abuf+alen, p, t2, t2len-1);
        j = strlen(t2);
        t2[j++] = ' ';
        ss += xdn_expand(abuf, abuf+alen, ss, t2+j, t2len-j);
        j += strlen(t2+j);
        snprintf(t2+j, t2len-j, "(\n\t\t%u\t;serial (version)\n\t\t%u\t"
          ";refresh period\n\t\t%u\t;retry interval\n\t\t%u\t;expire time\n"
          "\t\t%u\t;default ttl\n\t\t)", (unsigned)peek_be(ss, 4),
          (unsigned)peek_be(ss+4, 4), (unsigned)peek_be(ss+8, 4),
          (unsigned)peek_be(ss+12, 4), (unsigned)peek_be(ss+16, 4));
      } else if (type==15) {
        j = peek_be(p, 2);
        j = sprintf(t2, verbose ? "%d " : "(pri=%d) by ", j);
        xdn_expand(abuf, abuf+alen, p+2, t2+j, t2len-j);
      } else if (type==33) {
        j = sprintf(t2, "%u %u %u ", (int)peek_be(p, 2), (int)peek_be(p+2, 2),
          (int)peek_be(p+4, 2));
        xdn_expand(abuf, abuf+alen, p+6, t2+j, t2len-j);
      } else {
        printf("%s unsupported RR type %u\n", toybuf, type);
        continue;
      }
      for (i = 0; rrt[i].type != type; i++);
      if (verbose) printf("%s\t%u\tIN %s\t%s\n", toybuf, ttl, rrt[i].name, t2);
      else printf("%s %s %s\n", toybuf, rrt[i].msg, t2);
      qlen++;
    }
  }
  if (TT.t && !qlen) printf("%s has no %s record\n", *toys.optargs, TT.t);

  if (CFG_TOYBOX_FREE) free(abuf);
  toys.exitval = rcode;
}
