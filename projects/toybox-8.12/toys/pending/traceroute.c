/* traceroute - trace the route to "host".
 *
 * Copyright 2012 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 * Copyright 2013 Bilal Qureshi <bilal.jmi@gmail.com>
 * Copyright 2013 Ashwini Kumar <ak.ashwini1981@gmail.com>
 *
 * No Standard

USE_TRACEROUTE(NEWTOY(traceroute, "<1>2i:f#<1>255=1z#<0>86400=0g*w#<0>86400=5t#<0>255=0s:q#<1>255=3p#<1>65535=33434m#<1>255=30rvndlIUF64", TOYFLAG_STAYROOT|TOYFLAG_USR|TOYFLAG_BIN))
USE_TRACEROUTE(OLDTOY(traceroute6,traceroute, TOYFLAG_STAYROOT|TOYFLAG_USR|TOYFLAG_BIN))
config TRACEROUTE
  bool "traceroute"
  default n
  help
    usage: traceroute [-46FUIldnvr] [-f 1ST_TTL] [-m MAXTTL] [-p PORT] [-q PROBES]
    [-s SRC_IP] [-t TOS] [-w WAIT_SEC] [-g GATEWAY] [-i IFACE] [-z PAUSE_MSEC] HOST [BYTES]

    traceroute6 [-dnrv] [-m MAXTTL] [-p PORT] [-q PROBES][-s SRC_IP] [-t TOS] [-w WAIT_SEC]
      [-i IFACE] HOST [BYTES]

    Trace the route to HOST

    -4,-6 Force IP or IPv6 name resolution
    -F    Set the don't fragment bit (supports IPV4 only)
    -U    Use UDP datagrams instead of ICMP ECHO (supports IPV4 only)
    -I    Use ICMP ECHO instead of UDP datagrams (supports IPV4 only)
    -l    Display the TTL value of the returned packet (supports IPV4 only)
    -d    Set SO_DEBUG options to socket
    -n    Print numeric addresses
    -v    verbose
    -r    Bypass routing tables, send directly to HOST
    -m    Max time-to-live (max number of hops)(RANGE 1 to 255)
    -p    Base UDP port number used in probes(default 33434)(RANGE 1 to 65535)
    -q    Number of probes per TTL (default 3)(RANGE 1 to 255)
    -s    IP address to use as the source address
    -t    Type-of-service in probe packets (default 0)(RANGE 0 to 255)
    -w    Time in seconds to wait for a response (default 3)(RANGE 0 to 86400)
    -g    Loose source route gateway (8 max) (supports IPV4 only)
    -z    Pause Time in ms (default 0)(RANGE 0 to 86400) (supports IPV4 only)
    -f    Start from the 1ST_TTL hop (instead from 1)(RANGE 1 to 255) (supports IPV4 only)
    -i    Specify a network interface to operate with
*/
#define FOR_traceroute
#include "toys.h"
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

GLOBALS(
  long max_ttl;
  long port;
  long ttl_probes;
  char *src_ip;
  long tos;
  long wait_time;
  struct arg_list *loose_source;
  long pause_time;
  long first_ttl;
  char *iface;

  uint32_t gw_list[9];
  int recv_sock;
  int snd_sock;
  unsigned msg_len;
  char *packet;
  uint32_t ident;
  int istraceroute6;
)

#ifndef SOL_IPV6
# define SOL_IPV6 IPPROTO_IPV6
#endif

#define ICMP_HD_SIZE4  8
#define USEC           1000000ULL

struct payload_s {
  uint32_t seq;
  uint32_t ident;
};

char addr_str[INET6_ADDRSTRLEN];
struct sockaddr_storage dest;

//Compute checksum SUM of buffer P of length LEN
static u_int16_t in_cksum(u_int16_t *p, u_int len)
{
  u_int32_t sum = 0;
  int nwords = len >> 1;

  while (nwords-- != 0) sum += *p++;
  if (len & 1) {
    union {
      u_int16_t w;
      u_int8_t c[2];
    } u;
    u.c[0] = *(u_char *) p;
    u.c[1] = 0;
    sum += u.w;
  }
  // end-around-carry
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (~sum);
}

//sends a single probe packet with sequence(SEQ) and time-to-live(TTL)
static void send_probe4(int seq, int ttl)
{
  int res, len;
  void *out;
  struct payload_s *send_data4 = (struct payload_s *)(TT.packet);
  struct icmp *send_icmp4 = (struct icmp *)(TT.packet);

  if (toys.optflags & FLAG_U) {
    send_data4->seq = seq;
    send_data4->ident = TT.ident;
    ((struct sockaddr_in *)&dest)->sin_port = TT.port + seq;
    out = send_data4;
  } else {
    send_icmp4->icmp_type = ICMP_ECHO;
    send_icmp4->icmp_id = htons(TT.ident);
    send_icmp4->icmp_seq = htons(seq);
    send_icmp4->icmp_cksum = 0;
    send_icmp4->icmp_cksum = in_cksum((uint16_t *) send_icmp4, TT.msg_len);
    if (send_icmp4->icmp_cksum == 0) send_icmp4->icmp_cksum = 0xffff;
    out = send_icmp4;
  }

  res = setsockopt(TT.snd_sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
  if (res < 0) perror_exit("setsockopt ttl %d", ttl);

  len = TT.msg_len;
  res = sendto(TT.snd_sock, out, len, 0, (struct sockaddr *) &dest,
      sizeof(struct sockaddr_in));
  if (res != len) perror_exit(" sendto");
}

//sends a single probe packet with sequence(SEQ) and time-to-live(TTL)
static void send_probe6(int seq, int ttl)
{
  void *out;
  struct payload_s *send_data6 = (struct payload_s *) (TT.packet);

  send_data6->seq = seq;
  send_data6->ident = TT.ident;
  ((struct sockaddr_in6 *)&dest)->sin6_port = TT.port;

  if (setsockopt(TT.snd_sock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl,
        sizeof(ttl)) < 0) error_exit("setsockopt ttl %d", ttl);

  out = send_data6;

  if (sendto(TT.snd_sock, out, TT.msg_len, 0,
        (struct sockaddr *) &dest, sizeof(struct sockaddr_in6)) < 0)
    perror_exit("sendto");
}

static void set_flag_dr(int sock)
{
  int set = 1;
  if ((toys.optflags & FLAG_d) && (setsockopt(sock,SOL_SOCKET, SO_DEBUG,
          &set, sizeof(set)) < 0)) perror_exit("SO_DEBUG failed ");

  if ((toys.optflags & FLAG_r) && (setsockopt(sock, SOL_SOCKET, SO_DONTROUTE,
          &set, sizeof(set)) < 0)) perror_exit("SO_DONTROUTE failed ");
}

static void bind_to_interface(int sock)
{
  struct ifreq ifr;

  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", TT.iface);
  if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)))
      perror_msg("can't bind to interface %s", TT.iface);
}

static void resolve_addr(char *host, int family, int type, int proto, void *sock)
{
  struct addrinfo *info, hint;
  int ret;

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = family;
  hint.ai_socktype = type;
  hint.ai_protocol = proto;

  ret = getaddrinfo(host, NULL, &hint, &info);
  if (ret || !info) error_exit("bad address:  %s ", host);

  memcpy(sock, info->ai_addr, info->ai_addrlen);
  freeaddrinfo(info);
}

static void do_trace()
{
  int seq, fexit, ttl, tv = TT.wait_time * 1000;
  struct pollfd pfd[1];
  struct sockaddr_storage from;

  memset(&from, 0, sizeof(from));
  pfd[0].fd = TT.recv_sock;
  pfd[0].events = POLLIN;

  for (ttl = TT.first_ttl; ttl <= TT.max_ttl; ++ttl) {
    int probe, dest_reach = 0, print_verbose = 1;
    struct timeval t1, t2;
    struct sockaddr_storage last_addr;

    memset(&last_addr, 0, sizeof(last_addr));
    fexit = 0;
    xprintf("%2d", ttl);

    for (probe = 0, seq = 0; probe < TT.ttl_probes; ++probe) {
      int res = 0, tleft;

      fflush(NULL);
      if (!TT.istraceroute6)
        if (probe && (toys.optflags & FLAG_z)) msleep(TT.pause_time);

      if (!TT.istraceroute6) send_probe4(++seq, ttl);
      else send_probe6(++seq, ttl);
      gettimeofday(&t1, NULL);
      t2 = t1;

      while ((tleft = (int)(tv - ((unsigned long long)(t2.tv_sec * 1000ULL
                  + t2.tv_usec/1000) - (unsigned long long)(t1.tv_sec * 1000ULL
                    + t1.tv_usec/1000)))) >= 0) {
        unsigned delta = 0;
        if (!(res = poll(pfd, 1, tleft))) {
          xprintf("  *");
          break;
        }
        gettimeofday(&t2, NULL);
        if (res < 0) {
          if (errno != EINTR) perror_exit("poll");
          continue;
        }
        delta = (t2.tv_sec * USEC + t2.tv_usec)
          - (t1.tv_sec * USEC + t1.tv_usec);

        if (pfd[0].revents) {
          socklen_t addrlen = sizeof(struct sockaddr_storage);
          int rcv_len, icmp_res = 0;

          rcv_len = recvfrom(TT.recv_sock, toybuf, sizeof(toybuf),
              MSG_DONTWAIT, (struct sockaddr *) &from, &addrlen);
          if (rcv_len <= 0) continue;

          if (!TT.istraceroute6) {
            int pmtu = 0;
            struct ip *rcv_pkt = (struct ip*) toybuf;
            struct icmp *ricmp;

            ricmp = (struct icmp *) ((char*)rcv_pkt + (rcv_pkt->ip_hl << 2));
            if (ricmp->icmp_code == ICMP_UNREACH_NEEDFRAG)
              pmtu = ntohs(ricmp->icmp_nextmtu);

            if ((ricmp->icmp_type == ICMP_TIMXCEED
                  && ricmp->icmp_code == ICMP_TIMXCEED_INTRANS)
                || ricmp->icmp_type == ICMP_UNREACH
                || ricmp->icmp_type == ICMP_ECHOREPLY) {

              struct udphdr *hudp;
              struct icmp *hicmp;
              struct ip *hip = &ricmp->icmp_ip;

              if (toys.optflags & FLAG_U) {
                hudp = (struct udphdr*) ((char*)hip + (hip->ip_hl << 2));
                if ((hip->ip_hl << 2) + 12 <=(rcv_len - (rcv_pkt->ip_hl << 2))
                    && hip->ip_p == IPPROTO_UDP
                    && hudp->dest == (TT.port + seq))
                  icmp_res = (ricmp->icmp_type == ICMP_TIMXCEED ? -1 :
                      ricmp->icmp_code);
              } else {
                hicmp = (struct icmp *) ((char*)hip + (hip->ip_hl << 2));
                if (ricmp->icmp_type == ICMP_ECHOREPLY
                    && ricmp->icmp_id == ntohs(TT.ident)
                    && ricmp->icmp_seq == ntohs(seq))
                  icmp_res = ICMP_UNREACH_PORT;
                else if ((hip->ip_hl << 2) + ICMP_HD_SIZE4
                    <= (rcv_len - (rcv_pkt->ip_hl << 2))
                    && hip->ip_p == IPPROTO_ICMP
                    && hicmp->icmp_id == htons(TT.ident)
                    && hicmp->icmp_seq == htons(seq))
                  icmp_res = (ricmp->icmp_type == ICMP_TIMXCEED ? -1 :
                      ricmp->icmp_code);
              }
            }
            if (!icmp_res) continue;

            if (addrlen > 0) {
              if (memcmp(&((struct sockaddr_in *)&last_addr)->sin_addr,
                    &((struct sockaddr_in *)&from)->sin_addr,
                    sizeof(struct in_addr))) {
                if (!(toys.optflags & FLAG_n)) {
                  char host[NI_MAXHOST];
                  if (!getnameinfo((struct sockaddr *) &from,
                        sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, 0))
                    xprintf("  %s (", host);
                  else xprintf(" %s (", inet_ntoa(
                          ((struct sockaddr_in *)&from)->sin_addr));
                }
                xprintf(" %s", inet_ntoa(
                      ((struct sockaddr_in *)&from)->sin_addr));
                if (!(toys.optflags & FLAG_n)) xprintf(")");
                memcpy(&last_addr, &from, sizeof(from));
              }
              xprintf("  %u.%03u ms", delta / 1000, delta % 1000);
              if (toys.optflags & FLAG_l) xprintf(" (%d)", rcv_pkt->ip_ttl);
              if (toys.optflags & FLAG_v) {
                xprintf(" %d bytes from %s : icmp type %d code %d\t",
                    rcv_len, inet_ntoa(((struct sockaddr_in *)&from)->sin_addr),
                    ricmp->icmp_type, ricmp->icmp_code);
              }
            } else xprintf("\t!H");

            switch (icmp_res) {
              case ICMP_UNREACH_PORT:
                if (rcv_pkt->ip_ttl <= 1) xprintf(" !");
                dest_reach = 1;
                break;
              case ICMP_UNREACH_NET:
                xprintf(" !N");
                ++fexit;
                break;
              case ICMP_UNREACH_HOST:
                xprintf(" !H");
                ++fexit;
                break;
              case ICMP_UNREACH_PROTOCOL:
                xprintf(" !P");
                dest_reach = 1;
                break;
              case ICMP_UNREACH_NEEDFRAG:
                xprintf(" !F-%d", pmtu);
                ++fexit;
                break;
              case ICMP_UNREACH_SRCFAIL:
                xprintf(" !S");
                ++fexit;
                break;
              case ICMP_UNREACH_FILTER_PROHIB:
              case ICMP_UNREACH_NET_PROHIB:
                xprintf(" !A");
                ++fexit;
                break;
              case ICMP_UNREACH_HOST_PROHIB:
                xprintf(" !C");
                ++fexit;
                break;
              case ICMP_UNREACH_HOST_PRECEDENCE:
                xprintf(" !V");
                ++fexit;
                break;
              case ICMP_UNREACH_PRECEDENCE_CUTOFF:
                xprintf(" !C");
                ++fexit;
                break;
              case ICMP_UNREACH_NET_UNKNOWN:
              case ICMP_UNREACH_HOST_UNKNOWN:
                xprintf(" !U");
                ++fexit;
                break;
              case ICMP_UNREACH_ISOLATED:
                xprintf(" !I");
                ++fexit;
                break;
              case ICMP_UNREACH_TOSNET:
              case ICMP_UNREACH_TOSHOST:
                xprintf(" !T");
                ++fexit;
                break;
              default:
                break;
            }
            break;
          } else {
            struct icmp6_hdr *ricmp  = (struct icmp6_hdr *) toybuf;

            if ((ricmp->icmp6_type == ICMP6_TIME_EXCEEDED
                  && ricmp->icmp6_code == ICMP6_TIME_EXCEED_TRANSIT)
                || ricmp->icmp6_type == ICMP6_DST_UNREACH
                || ricmp->icmp6_type == ICMP6_ECHO_REPLY) {

              struct ip6_hdr *hip;
              struct udphdr *hudp;
              int hdr_next;

              hip = (struct ip6_hdr *)(ricmp + 1);
              hudp = (struct udphdr*) (hip + 1);
              hdr_next = hip->ip6_nxt;
              if (hdr_next == IPPROTO_FRAGMENT) {
                hdr_next = *(unsigned char*)hudp;
                hudp++;
              }

              if (hdr_next == IPPROTO_UDP) {
                struct payload_s *pkt = (struct payload_s*)(hudp + 1);
                if ((pkt->ident == TT.ident) && (pkt->seq == seq))
                  icmp_res = (ricmp->icmp6_type == ICMP6_TIME_EXCEEDED) ? -1 :
                    ricmp->icmp6_code;
              }
            }

            if (!icmp_res) continue;
            if (addrlen > 0) {
              if (memcmp(&((struct sockaddr_in6 *)&last_addr)->sin6_addr,
                    &((struct sockaddr_in6 *)&from)->sin6_addr,
                    sizeof(struct in6_addr))) {
                if (!(toys.optflags & FLAG_n)) {
                  char host[NI_MAXHOST];
                  if (!getnameinfo((struct sockaddr *) &from,
                        sizeof(from), host, sizeof(host), NULL, 0, 0))
                    xprintf("  %s (", host);
                }
                memset(addr_str, '\0', INET6_ADDRSTRLEN);
                inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&from)->sin6_addr,
                    addr_str, INET6_ADDRSTRLEN);
                xprintf(" %s", addr_str);

                if (!(toys.optflags & FLAG_n)) xprintf(")");
                memcpy(&last_addr,&from,sizeof(from));
              }

              if (toys.optflags & FLAG_v) {
                if(print_verbose){
                  memset(addr_str, '\0', INET6_ADDRSTRLEN);
                  inet_ntop(AF_INET6, &((struct sockaddr_in6 *)
                        &from)->sin6_addr, addr_str, INET6_ADDRSTRLEN);
                  rcv_len -= sizeof(struct ip6_hdr);
                  xprintf(" %d bytes to %s ", rcv_len, addr_str);
                }
              }
              xprintf("  %u.%03u ms", delta / 1000, delta % 1000);
              delta = 0;

            } else xprintf("\t!H");

            switch (icmp_res) {
              case ICMP6_DST_UNREACH_NOPORT:
                ++fexit;
                dest_reach = 1;
                break;
              case ICMP6_DST_UNREACH_NOROUTE:
                xprintf(" !N");
                ++fexit;
                break;
              case ICMP6_DST_UNREACH_ADDR:
                xprintf(" !H");
                ++fexit;
                break;
              case ICMP6_DST_UNREACH_ADMIN:
                xprintf(" !S");
                ++fexit;
                break;
              default:
                break;
            }
            break;
          }
        } //revents
      }
      print_verbose = 0;
    }
    xputc('\n');
    if(!TT.istraceroute6) {
      if (!memcmp(&((struct sockaddr_in *)&from)->sin_addr,
            &((struct sockaddr_in *)&dest)->sin_addr, sizeof(struct in_addr))
          || dest_reach || (fexit && fexit >= TT.ttl_probes -1))
        break;
    } else if (dest_reach || (fexit > 0 && fexit >= TT.ttl_probes -1)) break;
  }
}

void traceroute_main(void)
{
  unsigned pack_size = 0, tyser = 0;
  int lsrr = 0, set = 1;

  if(!(toys.optflags & FLAG_4) &&
      (inet_pton(AF_INET6, toys.optargs[0], &dest)))
    toys.optflags |= FLAG_6;

  memset(&dest, 0, sizeof(dest));
  if (toys.optflags & FLAG_6) TT.istraceroute6 = 1;
  else TT.istraceroute6 = toys.which->name[10] == '6';

  if(!TT.istraceroute6 && (toys.optflags & FLAG_g)) {
      struct arg_list *node;

      for (node = TT.loose_source; node; node = node->next, lsrr++) {
        struct sockaddr_in sin;

        memset( &sin, 0, sizeof(sin));
        if (lsrr >= 8) error_exit("no more than 8 gateways"); // NGATEWAYS
        resolve_addr(node->arg, AF_INET, SOCK_STREAM, 0, &sin);
        TT.gw_list[lsrr] = sin.sin_addr.s_addr;
      }
  } else TT.first_ttl = 1;

  TT.msg_len = pack_size = ICMP_HD_SIZE4; //udp payload is also 8bytes
  if (toys.optargs[1])
    TT.msg_len = atolx_range(toys.optargs[1], pack_size, 32768);//max packet size

  TT.recv_sock = xsocket((TT.istraceroute6 ? AF_INET6 : AF_INET), SOCK_RAW,
      (TT.istraceroute6 ? IPPROTO_ICMPV6 : IPPROTO_ICMP));

  if (TT.istraceroute6) {
    int two = 2;
#ifdef IPV6_RECVPKTINFO
    setsockopt(TT.recv_sock, SOL_IPV6, IPV6_RECVPKTINFO, &set,
        sizeof(set));
    setsockopt(TT.recv_sock, SOL_IPV6, IPV6_2292PKTINFO, &set,
        sizeof(set));
#else
    setsockopt(TT.recv_sock, SOL_IPV6, IPV6_PKTINFO, &set, sizeof(set));
#endif

    if (setsockopt(TT.recv_sock, SOL_RAW, IPV6_CHECKSUM, &two,
          sizeof(two)) < 0)  perror_exit("setsockopt RAW_CHECKSUM");
  }

  set_flag_dr(TT.recv_sock);

  if (!TT.istraceroute6) {
    if (toys.optflags & FLAG_U)
      TT.snd_sock = xsocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    else TT.snd_sock = xsocket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    if (toys.optflags & FLAG_i) bind_to_interface(TT.snd_sock);

    resolve_addr(toys.optargs[0], AF_INET, ((toys.optflags & FLAG_U) ?
          SOCK_DGRAM : SOCK_RAW), ((toys.optflags & FLAG_U) ? IPPROTO_UDP :
            IPPROTO_ICMP), &dest);
    if (lsrr > 0) {
      unsigned char optlist[MAX_IPOPTLEN];
      unsigned size;

      TT.gw_list[lsrr] = ((struct sockaddr_in *)&dest)->sin_addr.s_addr;
      ++lsrr;

      optlist[0] = IPOPT_NOP;
      optlist[1] = IPOPT_LSRR;// loose source route option
      size = lsrr * sizeof(TT.gw_list[0]);
      optlist[2] = size + 3;
      optlist[3] = IPOPT_MINOFF;
      memcpy(optlist + 4, TT.gw_list, size);

      if (setsockopt(TT.snd_sock, IPPROTO_IP, IP_OPTIONS,
            (char *)optlist, size + sizeof(TT.gw_list[0])) < 0)
        perror_exit("LSRR IP_OPTIONS");
    }
  } else TT.snd_sock = xsocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

  if (setsockopt(TT.snd_sock, SOL_SOCKET, SO_SNDBUF, &TT.msg_len,
        sizeof(TT.msg_len)) < 0) perror_exit("SO_SNDBUF failed ");

  if (!TT.istraceroute6) {
    if ((toys.optflags & FLAG_t) &&
        setsockopt(TT.snd_sock, IPPROTO_IP, IP_TOS, &tyser, sizeof(tyser)) < 0)
      perror_exit("IP_TOS %ld failed ", TT.tos);

#ifdef IP_DONTFRAG
    if ((toys.optflags & FLAG_F) &&
        (setsockopt(TT.snd_sock, IPPROTO_IP, IP_DONTFRAG, &set,
                    sizeof(set)) < 0)) perror_exit("IP_DONTFRAG failed ");
#endif
  } else if (setsockopt(TT.snd_sock, IPPROTO_IPV6, IPV6_TCLASS, &TT.tos,
        sizeof(TT.tos)) < 0) perror_exit("IPV6_TCLASS %ld failed ", TT.tos);

  set_flag_dr(TT.snd_sock);
  TT.packet = xzalloc(TT.msg_len);
  TT.ident = getpid();

  if (!TT.istraceroute6) {
    if (!(toys.optflags & FLAG_U)) TT.ident |= 0x8000;
    if (toys.optflags & FLAG_s) {
      struct sockaddr_in source;

      memset(&source, 0, sizeof(source));
      if (!inet_aton(TT.src_ip, &(source.sin_addr)))
        error_exit("bad address: %s", TT.src_ip);
      if (setsockopt(TT.snd_sock, IPPROTO_IP, IP_MULTICAST_IF,
            (struct sockaddr*)&source, sizeof(struct sockaddr_in)))
        perror_exit("can't set multicast source interface");
      xbind(TT.snd_sock,(struct sockaddr*)&source, sizeof(struct sockaddr_in));
    }

    if(TT.first_ttl > TT.max_ttl)
      error_exit("ERROR :Range for -f is 1 to %ld (max ttl)", TT.max_ttl);

    xprintf("traceroute to %s(%s)", toys.optargs[0],
           inet_ntoa(((struct sockaddr_in *)&dest)->sin_addr));
  } else {
    if (toys.optflags & FLAG_i) bind_to_interface(TT.snd_sock);

    resolve_addr(toys.optargs[0], AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &dest);
    if (toys.optflags & FLAG_s) {
      struct sockaddr_in6 source;

      memset(&source, 0, sizeof(source));
      if(inet_pton(AF_INET6, TT.src_ip, &(source.sin6_addr)) <= 0)
        error_exit("bad address: %s", TT.src_ip);

      xbind(TT.snd_sock,(struct sockaddr*)&source, sizeof(struct sockaddr_in6));
    } else {
      struct sockaddr_in6 prb;
      socklen_t len = sizeof(prb);
      int p_fd = xsocket(AF_INET6, SOCK_DGRAM, 0);
      if (toys.optflags & FLAG_i) bind_to_interface(p_fd);

      ((struct sockaddr_in6 *)&dest)->sin6_port = htons(1025);
      xconnect(p_fd, (struct sockaddr *)&dest, sizeof(struct sockaddr_in6));
      if(getsockname(p_fd, (struct sockaddr *)&prb, &len))
        error_exit("probe addr failed");
      close(p_fd);
      prb.sin6_port = 0;
      xbind(TT.snd_sock, (struct sockaddr*)&prb, sizeof(struct sockaddr_in6));
      xbind(TT.recv_sock, (struct sockaddr*)&prb, sizeof(struct sockaddr_in6));
    }

    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&dest)->sin6_addr,
              addr_str, INET6_ADDRSTRLEN);
    xprintf("traceroute6 to %s(%s)", toys.optargs[0], addr_str);
  }

  if (toys.optflags & FLAG_s) xprintf(" from %s",TT.src_ip);
  xprintf(", %ld hops max, %u byte packets\n", TT.max_ttl, TT.msg_len);

  do_trace();
}
