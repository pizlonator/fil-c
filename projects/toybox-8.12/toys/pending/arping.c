/* arping - send ARP REQUEST to a neighbour host.
 *
 * Copyright 2013 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.

USE_ARPING(NEWTOY(arping, "<1>1s:I:w#<0c#<0AUDbqf[+AU][+Df]", TOYFLAG_USR|TOYFLAG_SBIN))

config ARPING
  bool "arping"
  default n
  help
    usage: arping [-fqbDUA] [-c CNT] [-w TIMEOUT] [-I IFACE] [-s SRC_IP] DST_IP

    Send ARP requests/replies

    -f         Quit on first ARP reply
    -q         Quiet
    -b         Keep broadcasting, don't go unicast
    -D         Duplicated address detection mode
    -U         Unsolicited ARP mode, update your neighbors
    -A         ARP answer mode, update your neighbors
    -c N       Stop after sending N ARP requests
    -w TIMEOUT Time to wait for ARP reply, seconds
    -I IFACE   Interface to use (default eth0)
    -s SRC_IP  Sender IP address
    DST_IP     Target IP address
*/
#define FOR_arping
#include "toys.h"
#include <netinet/ether.h>
#include <netpacket/packet.h>

GLOBALS(
    long count;
    unsigned long time_out;
    char *iface;
    char *src_ip;

    int sockfd;
    unsigned long start, end;
    unsigned sent_at, sent_nr, rcvd_nr, brd_sent, rcvd_req, brd_rcv,
             unicast_flag;
)

struct sockaddr_ll src_pk, dst_pk;
struct in_addr src_addr, dest_addr;
extern void *mempcpy(void *dest, const void *src, size_t n);

// Gets information of INTERFACE and updates IFINDEX, MAC and IP.
static void get_interface(char *interface, int *ifindex, uint32_t *oip,
    uint8_t *mac)
{
  struct ifreq req;
  struct sockaddr_in *ip;
  int fd = xsocket(AF_INET, SOCK_RAW, IPPROTO_RAW);

  req.ifr_addr.sa_family = AF_INET;
  xstrncpy(req.ifr_name, interface, IFNAMSIZ);
  req.ifr_name[IFNAMSIZ-1] = '\0';

  xioctl(fd, SIOCGIFFLAGS, &req);
  if (!(req.ifr_flags & IFF_UP)) return;

  if (oip) {
    xioctl(fd, SIOCGIFADDR, &req);
    ip = (struct sockaddr_in*) &req.ifr_addr;
    *oip = ntohl(ip->sin_addr.s_addr);
  }
  if (ifindex) {
    xioctl(fd, SIOCGIFINDEX, &req);
    *ifindex = req.ifr_ifindex;
  }
  if (mac) {
    xioctl(fd, SIOCGIFHWADDR, &req);
    memcpy(mac, req.ifr_hwaddr.sa_data, 6);
  }
  xclose(fd);
}

// SIGINT handler, Print Number of Packets send or receive details.
static void done(int sig)
{
  if (!(toys.optflags & FLAG_q)) {
    xprintf("Sent %u probe(s) (%u broadcast(s))\n", TT.sent_nr, TT.brd_sent);
    xprintf("Received %u repl%s (%u request(s), %u broadcast(s))\n",
        TT.rcvd_nr, TT.rcvd_nr == 1 ? "y":"ies", TT.rcvd_req, TT.brd_rcv);
  }
  if (toys.optflags & FLAG_D) exit(!!TT.rcvd_nr);
  //In -U mode, No reply is expected.
  if (toys.optflags & FLAG_U) exit(EXIT_SUCCESS);
  exit(!TT.rcvd_nr);
}

// Create and Send Packet
static void send_packet()
{
  int ret;
  unsigned char sbuf[256] = {0,};
  struct arphdr *arp_h = (struct arphdr *) sbuf;
  unsigned char *ptr = (unsigned char *)(arp_h + 1);

  arp_h->ar_hrd = htons(ARPHRD_ETHER);
  arp_h->ar_pro = htons(ETH_P_IP);
  arp_h->ar_hln = src_pk.sll_halen;
  arp_h->ar_pln = 4;
  arp_h->ar_op = (toys.optflags & FLAG_A) ? htons(ARPOP_REPLY)
    : htons(ARPOP_REQUEST);

  ptr = mempcpy(ptr, &src_pk.sll_addr, src_pk.sll_halen);
  ptr = mempcpy(ptr, &src_addr, 4);
  ptr = mempcpy(ptr,
                (toys.optflags & FLAG_A) ? &src_pk.sll_addr : &dst_pk.sll_addr,
                src_pk.sll_halen);
  ptr = mempcpy(ptr, &dest_addr, 4);

  ret = sendto(TT.sockfd, sbuf, ptr - sbuf, 0,
      (struct sockaddr *)&dst_pk, sizeof(dst_pk));
  if (ret == ptr - sbuf) {
    struct timeval tval;

    gettimeofday(&tval, NULL);
    TT.sent_at = tval.tv_sec * 1000000ULL + tval.tv_usec;
    TT.sent_nr++;
    if (!TT.unicast_flag) TT.brd_sent++;
  }
}

// Receive Packet and filter with valid checks.
static void recv_from(struct sockaddr_ll *from, int *recv_len)
{
  struct in_addr s_ip, d_ip;
  struct arphdr *arp_hdr = (struct arphdr *)toybuf;
  unsigned char *p = (unsigned char *)(arp_hdr + 1);

  if (arp_hdr->ar_op != htons(ARPOP_REQUEST) &&
      arp_hdr->ar_op != htons(ARPOP_REPLY)) return;

  if (from->sll_pkttype != PACKET_HOST && from->sll_pkttype != PACKET_BROADCAST
      && from->sll_pkttype != PACKET_MULTICAST) return;

  if (arp_hdr->ar_pro != htons(ETH_P_IP) || (arp_hdr->ar_pln != 4)
      || (arp_hdr->ar_hln != src_pk.sll_halen)
      || (*recv_len < (int)(sizeof(*arp_hdr) + 2 * (4 + arp_hdr->ar_hln))))
    return;

  memcpy(&s_ip.s_addr, p + arp_hdr->ar_hln, 4);
  memcpy(&d_ip.s_addr, p + arp_hdr->ar_hln + 4 + arp_hdr->ar_hln, 4);

  if (dest_addr.s_addr != s_ip.s_addr) return;
  if (toys.optflags & FLAG_D) {
    if (src_addr.s_addr && src_addr.s_addr != d_ip.s_addr) return;
    if (!memcmp(p, &src_pk.sll_addr, src_pk.sll_halen)) return;
  } else if (src_addr.s_addr != d_ip.s_addr ) return;

  if (!(toys.optflags & FLAG_q)) {
    printf("%scast re%s from %s [%s]",
        from->sll_pkttype == PACKET_HOST ? "Uni" : "Broad",
        arp_hdr->ar_op == htons(ARPOP_REPLY) ? "ply" : "quest",
        inet_ntoa(s_ip), ether_ntoa((struct ether_addr *) p));
    if (TT.sent_at) {
      unsigned delta;
      struct timeval tval;

      gettimeofday(&tval, NULL);
      delta = (tval.tv_sec * 1000000ULL + (tval.tv_usec)) - TT.sent_at;
      xprintf(" %u.%03ums\n", delta / 1000, delta % 1000);
    }
  }
  TT.rcvd_nr++;
  if (from->sll_pkttype != PACKET_HOST) TT.brd_rcv++;
  if (arp_hdr->ar_op == htons(ARPOP_REQUEST)) TT.rcvd_req++;
  if (toys.optflags & FLAG_f) done(0);
  if (!(toys.optflags & FLAG_b)) {
    memcpy(dst_pk.sll_addr, p, src_pk.sll_halen);
    TT.unicast_flag = 1;
  }
}

// Alarm signal Handle, send packets in one second interval.
static void send_signal(int sig)
{
  struct timeval start;

  gettimeofday(&start, NULL);
  if (!TT.start)
    TT.end = TT.start = start.tv_sec * 1000 + start.tv_usec / 1000;
  else TT.end = start.tv_sec*1000 + start.tv_usec / 1000;
  if (toys.optflags & FLAG_c) {
    if (!TT.count) done(0);
    TT.count--;
  }
  if ((toys.optflags & FLAG_w) && ((TT.end - TT.start) >
        ((TT.time_out)*1000))) done(0);
  send_packet();
  alarm(1);
}

void arping_main(void)
{
  struct ifreq ifr;
  struct sockaddr_ll from;
  socklen_t len;
  int if_index, recv_len;

  if (!(toys.optflags & FLAG_I)) TT.iface = "eth0";
  TT.sockfd = xsocket(AF_PACKET, SOCK_DGRAM, 0);

  memset(&ifr, 0, sizeof(ifr));
  xstrncpy(ifr.ifr_name, TT.iface, IFNAMSIZ);
  get_interface(TT.iface, &if_index, NULL, NULL);
  src_pk.sll_ifindex = if_index;

  xioctl(TT.sockfd, SIOCGIFFLAGS, (char*)&ifr);
  if (!(ifr.ifr_flags & IFF_UP) && !(toys.optflags & FLAG_q))
    error_exit("Interface \"%s\" is down", TT.iface);
  if ((ifr.ifr_flags & (IFF_NOARP | IFF_LOOPBACK))
      && !(toys.optflags & FLAG_q)) {
    xprintf("Interface \"%s\" is not ARPable\n", TT.iface);
    toys.exitval = (toys.optflags & FLAG_D) ? 0 : 2;
    return;
  }
  if (!inet_aton(*toys.optargs, &dest_addr)) {
    struct hostent *hp = gethostbyname2(*toys.optargs, AF_INET);

    if (!hp) perror_exit("bad address '%s'", *toys.optargs);
    memcpy(&dest_addr, hp->h_addr, 4);
  }
  if ((toys.optflags & FLAG_s) && !(inet_aton(TT.src_ip, &src_addr)))
    perror_exit("invalid source address '%s'",TT.src_ip);
  if (!(toys.optflags & FLAG_D) && (toys.optflags & FLAG_U)
      && !src_addr.s_addr) src_addr = dest_addr;
  if (!(toys.optflags & FLAG_D) || src_addr.s_addr) {
    struct sockaddr_in saddr;
    int p_fd = xsocket(AF_INET, SOCK_DGRAM, 0);

    if (setsockopt(p_fd, SOL_SOCKET, SO_BINDTODEVICE, TT.iface,
          strlen(TT.iface))) perror_exit("setsockopt");

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    if (src_addr.s_addr) {
      saddr.sin_addr = src_addr;
      xbind(p_fd, (struct sockaddr*)&saddr, sizeof(saddr));
    } else {
      uint32_t oip;

      saddr.sin_port = htons(1025);
      saddr.sin_addr = dest_addr;
      xconnect(p_fd, (struct sockaddr *) &saddr, sizeof(saddr));
      get_interface(TT.iface, NULL, &oip, NULL);
      src_addr.s_addr = htonl(oip);
    }
    xclose(p_fd);
  }

  src_pk.sll_family = AF_PACKET;
  src_pk.sll_protocol = htons(ETH_P_ARP);
  xbind(TT.sockfd, (struct sockaddr *)&src_pk, sizeof(src_pk));

  socklen_t alen = sizeof(src_pk);
  getsockname(TT.sockfd, (struct sockaddr *)&src_pk, &alen);
  if (!src_pk.sll_halen) {
    perror_msg("src is not arpable");
    toys.exitval = (toys.optflags & FLAG_D) ? 0 : 2;
    return;
  }
  if (!(toys.optflags & FLAG_q)) {
    xprintf("ARPING to %s", inet_ntoa(dest_addr));
    xprintf(" from %s via %s\n", inet_ntoa(src_addr), TT.iface);
  }

  dst_pk = src_pk;
  //First packet always broadcasts.
  memset(dst_pk.sll_addr, -1, dst_pk.sll_halen);
  signal(SIGINT, done);
  signal(SIGALRM, send_signal);

  send_signal(0); // Send first Broadcast message.
  while (1) {
    len = sizeof(from);
    recv_len = recvfrom(TT.sockfd, toybuf, 4096, 0,
        (struct sockaddr *)&from, &len);
    if (recv_len < 0) continue;
    recv_from(&from, &recv_len);
  }
}
