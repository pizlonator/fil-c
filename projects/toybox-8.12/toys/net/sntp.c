/* sntp.c - sntp client and server
 *
 * Copyright 2019 Rob Landley <rob@landley.net>
 *
 * See https://www.ietf.org/rfc/rfc4330.txt

  modes: oneshot display, oneshot set, persist, serve, multi

USE_SNTP(NEWTOY(sntp, ">1M :m :Sp:t#<0=1>16asdDqr#<4>17=10[!as]", TOYFLAG_USR|TOYFLAG_BIN))

config SNTP
  bool "sntp"
  default y
  help
    usage: sntp [-saSdDq] [-r SHIFT] [-mM[ADDRESS]] [-p PORT] [SERVER]

    Simple Network Time Protocol client. Query SERVER and display time.

    -p	Use PORT (default 123)
    -s	Set system clock suddenly
    -a	Adjust system clock gradually
    -S	Serve time instead of querying (bind to SERVER address if specified)
    -m	Wait for updates from multicast ADDRESS (RFC 4330 suggests 224.0.1.1)
    -M	Multicast server on ADDRESS (RFC 4330 suggests 224.0.1.1)
    -t	TTL (multicast only, default 1)
    -d	Daemonize (run in background re-querying)
    -D	Daemonize but stay in foreground: re-query time every 1000 seconds
    -r	Retry shift (every 1<<SHIFT seconds)
    -q	Quiet (don't display time)
*/

#define FOR_sntp
#include "toys.h"
#include <sys/timex.h>

GLOBALS(
  long r, t;
  char *p, *m, *M;
)

// Seconds from 1900 to 1970, including appropriate leap days
#define SEVENTIES 2208988800ULL

// Get time and return ntptime (saving timespec in pointer if not null)
// NTP time is high 32 bits = seconds since 1970 (blame RFC 868), low 32 bits
// fraction of a second.
// diff is how far off we think our clock is from reality (in nanoseconds)
static unsigned long long lunchtime(struct timespec *television, long long diff)
{
  struct timespec tv;

  clock_gettime(CLOCK_REALTIME, &tv);
  if (diff) nanomove(&tv, diff);

  if (television) *television = tv;

  // Unix time is 1970 but RFCs 868 and 958 said 1900 so add seconds 1900->1970
  // If they'd done a 34/30 bit split the Y2036 problem would be centuries
  // from now and still give us nanosecond accuracy, but no...
  return ((tv.tv_sec+SEVENTIES)<<32)+(((long long)tv.tv_nsec)<<32)/1000000000;
}

// convert ntptime back to struct timespec.
static void doublyso(unsigned long long now, struct timespec *tv)
{
  // Y2036 fixup: if time wrapped, it's in the future
  tv->tv_sec = (now>>32) + (1LL<<32)*!(now&(1LL<<63));
  tv->tv_sec -= SEVENTIES; // Force signed math for Y2038 fixup
  tv->tv_nsec = ((now&0xFFFFFFFF)*1000000000)>>32;
}

void sntp_main(void)
{
  struct timespec tv, tv2;
  unsigned long long *pktime = (void *)toybuf, now, then, before QUIET;
  long long diff = 0;
  struct addrinfo *ai;
  union socksaddr sa;
  int fd, tries = 0;

  if (FLAG(d)) xvdaemon();

  if (FLAG(M)) toys.optflags |= FLAG_S;
  if (!(FLAG(S)||FLAG(m)) && !*toys.optargs)
    error_exit("Need -SMm or SERVER address");

  // Lookup address and open server or client UDP socket
  if (!TT.p || !*TT.p) TT.p = "123";
  ai = xgetaddrinfo(*toys.optargs, TT.p, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP,
    AI_PASSIVE*!*toys.optargs);

  // Act as server if necessary
  if (FLAG(S)||FLAG(m)) {
    fd = xbindany(ai);
    if (TT.m || TT.M) {
      struct ip_mreq group;
      int t = 0;

      // subscribe to multicast group
      memset(&group, 0, sizeof(group));
      group.imr_multiaddr.s_addr = inet_addr(TT.m ? TT.m : TT.M);
      xsetsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));
      xsetsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &t, 4);
      t = TT.t;
      xsetsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &t, 4);
    }
  } else fd = xsocket(ai->ai_family, SOCK_DGRAM, IPPROTO_UDP);

  // -Sm = loop waiting for input
  // -Dd = loop polling time and waiting until next poll period
  // Otherwise poll up to 3 times to get 2 responses, then exit

  // loop sending/receiving packets
  for (;;) {
    now = millitime();

    // If we're in server or multicast client mode, don't poll
    if (FLAG(m) || FLAG(S)) then = -1;

    // daemon and oneshot modes send a packet each time through outer loop
    else {
      then = now + 3000;
      if (FLAG(d)||FLAG(D)||FLAG(M)) then = now + (1<<TT.r)*1000;

      // Send NTP query packet
      memset(toybuf, 0, 48);
      *toybuf = 0xe3; // li = 3 (unsynchronized), version = 4, mode = 3 (client)
      toybuf[2] = 8;  // poll frequency 1<<8 = 256 seconds
      pktime[5] = SWAP_BE64(before = lunchtime(&tv, diff));
      xsendto(fd, toybuf, 48, ai->ai_addr);
    }

    // Loop receiving packets until it's time to send the next one.
    for (;;) {
      int strike;

      // Wait to receive a packet

      if (then>0 && then<(now = millitime())) break;;
      strike = xrecvwait(fd, toybuf, sizeof(toybuf), &sa, then-now);
      if (strike<1) {
        if (!(FLAG(S)||FLAG(m)||FLAG(D)||FLAG(d)) && ++tries == 3)
          error_exit("no reply from %s", *toys.optargs);
        break;
      }
      if (strike<48) continue;

      // Validate packet
      if (!FLAG(S) || FLAG(m)) {
        char buf[128];
        int mode = 7&*toybuf;

        // Is source address what we expect?
        xstrncpy(buf, ntop(ai->ai_addr), 128);
        strike = strcmp(buf, ntop((void *)&sa));
        // Does this reply's originate timestamp match the packet we sent?
        if (!FLAG(S) && !FLAG(m) && before != SWAP_BE64(pktime[3])) continue;
        // Ignore packets from wrong address or with wrong mode
        if (strike && !FLAG(S)) continue;
        if (!((FLAG(m) && mode==5) || (FLAG(S) && mode==3) ||
            (!FLAG(m) && !FLAG(S) && mode==4))) continue;
      }

      // If received a -S request packet, send server packet
      if (strike) {
        char *buf = toybuf+48;

        *buf = 0x24;  // LI 0 VN 4 mode 4.
        buf[1] = 3;   // stratum 3
        buf[2] = 10;  // recommended retry every 1<<10=1024 seconds
        buf[3] = 250; // precision -6, minimum allowed
        strcpy(buf+12, "LOCL");
        pktime[6+3] = pktime[5]; // send back reference time they sent us
        // everything else is current time
        pktime[6+2] = pktime[6+4] = pktime[6+5] = SWAP_BE64(lunchtime(0, 0));
        xsendto(fd, buf, 48, (void *)&sa);

      // Got a time packet from a recognized server
      } else {
        int unset = !diff;

        // First packet: figure out how far off our clock is from what server
        // said and try again. Don't set clock, just record offset to use
        // generating second request. (We know this time is in the past
        // because transmission took time, but it's a start. And if time is
        // miraculously exact, don't loop.)

        lunchtime(&tv2, diff);
        diff = nanodiff(&tv, &tv2);
        if (unset && diff) break;

        // Second packet: determine midpoint of packet transit time according
        // to local clock, assuming each direction took same time so midpoint
        // is time server reported. The first tv was the adjusted time
        // we sent the packet at, tv2 is what server replied, so now diff
        // is round trip time.

        // What time did the server say and how far off are we?
        nanomove(&tv, diff/2);
        doublyso(SWAP_BE64(pktime[5]), &tv2);
        diff = nanodiff(&tv, &tv2);

        if (FLAG(s)) {
          // Do read/adjust/set to lose as little time as possible.
          clock_gettime(CLOCK_REALTIME, &tv2);
          nanomove(&tv2, diff);
          if (clock_settime(CLOCK_REALTIME, &tv2))
            perror_exit("clock_settime");
        } else if (FLAG(a)) {
          struct timex tx;

          // call adjtimex() to move the clock gradually
          nanomove(&tv2, diff);
          memset(&tx, 0, sizeof(struct timex));
          tx.offset = tv2.tv_sec*1000000+tv2.tv_nsec/1000;
          tx.modes = ADJ_OFFSET_SINGLESHOT;
          if (adjtimex(&tx) == -1) perror_exit("adjtimex");
        }

        // Display the time and offset
        if (!FLAG(q)) {
          format_iso_time(toybuf, sizeof(toybuf)-1, &tv2);
          printf("%s offset %c%lld.%09lld secs\n", toybuf, (diff<0) ? '-' : '+',
            llabs(diff/1000000000), llabs(diff%1000000000));
        }

        // If we're not in daemon mode, we're done. (Can't get here for -S.)
        if (!FLAG(d) && !FLAG(D)) return;
      }
    }
  }
}
