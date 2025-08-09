/* ftpget.c - Fetch file(s) from ftp server
 *
 * Copyright 2016 Rob Landley <rob@landley.net>
 *
 * No standard for the command, but see https://www.ietf.org/rfc/rfc959.txt
 * TODO: local can be -
 * TEST: -g -s (when local and remote exist) -gc, -sc
 * zero length file

USE_FTPGET(NEWTOY(ftpget, "<2>3P:cp:u:vgslLmMdD[-gs][!gslLmMdD][!clL]", TOYFLAG_USR|TOYFLAG_BIN))
USE_FTPPUT(OLDTOY(ftpput, ftpget, TOYFLAG_USR|TOYFLAG_BIN))

config FTPGET
  bool "ftpget"
  default y
  help
    usage: ftpget [-cvgslLmMdD] [-P PORT] [-p PASSWORD] [-u USER] HOST [LOCAL] REMOTE

    Talk to ftp server. By default get REMOTE file via passive anonymous
    transfer, optionally saving under a LOCAL name. Can also send, list, etc.

    -c	Continue partial transfer
    -p	Use PORT instead of "21"
    -P	Use PASSWORD instead of "ftpget@"
    -u	Use USER instead of "anonymous"
    -v	Verbose

    Ways to interact with FTP server:
    -d	Delete file
    -D	Remove directory
    -g	Get file (default)
    -l	List directory
    -L	List (filenames only)
    -m	Move file on server from LOCAL to REMOTE
    -M	mkdir
    -s	Send file

config FTPPUT
  bool "ftpput"
  default y
  help
    An ftpget that defaults to -s instead of -g
*/

#define FOR_ftpget
#include "toys.h"

GLOBALS(
  char *u, *p, *P;

  int fd;
)

// we should get one line of data, but it may be in multiple chunks
static int xread2line(int fd, char *buf, int len)
{
  int i, total = 0;

  len--;
  while (total<len && (i = xread(fd, buf, len-total))) {
    total += i;
    if (buf[total-1] == '\n') break;
  }
  if (total>=len) error_exit("overflow");
  while (total--)
    if (buf[total]=='\r' || buf[total]=='\n') buf[total] = 0;
    else break;
  if (FLAG(v)) fprintf(stderr, "%s\n", toybuf);

  return total+1;
}

static int ftp_line(char *cmd, char *arg, int must)
{
  int rc = 0;

  if (cmd) {
    char *s = "%s %s\r\n"+3*(!arg);
    if (FLAG(v)) fprintf(stderr, s, cmd, arg);
    dprintf(TT.fd, s, cmd, arg);
  }
  if (must>=0) {
    xread2line(TT.fd, toybuf, sizeof(toybuf));
    if (!sscanf(toybuf, "%d", &rc) || (must && rc != must))
      error_exit_raw(toybuf);
  }

  return rc;
}

void ftpget_main(void)
{
  struct sockaddr_in6 si6;
  int rc, ii = 1, port = 0;
  socklen_t sl = sizeof(si6);
  char *s, *remote = toys.optargs[2];
  unsigned long long lenl = 0, lenr;

  if (!(toys.optflags&(FLAG_v-1)))
    toys.optflags |= (toys.which->name[3]=='g') ? FLAG_g : FLAG_s;

  if (!TT.u) TT.u = "anonymous";
  if (!TT.P) TT.P = "ftpget@";
  if (!TT.p) TT.p = "21";
  if (!remote) remote = toys.optargs[1];

  // connect
  TT.fd = xconnectany(xgetaddrinfo(*toys.optargs, TT.p, 0, SOCK_STREAM, 0,
    AI_ADDRCONFIG));
  if (getpeername(TT.fd, (void *)&si6, &sl)) perror_exit("getpeername");

  // Login
  ftp_line(0, 0, 220);
  rc = ftp_line("USER", TT.u, 0);
  if (rc == 331) rc = ftp_line("PASS", TT.P, 0);
  if (rc != 230) error_exit_raw(toybuf);

  if (FLAG(m)) {
    if (toys.optc != 3) error_exit("-m FROM TO");
    ftp_line("RNFR", toys.optargs[1], 350);
    ftp_line("RNTO", toys.optargs[2], 250);
  } else if (FLAG(M)) ftp_line("MKD", toys.optargs[1], 257);
  else if (FLAG(d)) ftp_line("DELE", toys.optargs[1], 250);
  else if (FLAG(D)) ftp_line("RMD", toys.optargs[1], 250);
  else {
    int get = !FLAG(s), cnt = FLAG(c);
    char *cmd;

    // Only do passive binary transfers
    ftp_line("TYPE", "I", 0);
    rc = ftp_line("PASV", 0, 0);

    // PASV means the server opens a port you connect to instead of the server
    // dialing back to the client. (Still insane, but less so.) So need port #

    // PASV output is "227 PASV ok (x,x,x,x,p1,p2)" where x,x,x,x is the IP addr
    // (must match the server you're talking to???) and port is (256*p1)+p2
    s = 0;
    if (rc==227) for (s = toybuf; (s = strchr(s, ',')); s++) {
      int p1, got = 0;

      sscanf(s, ",%u,%u)%n", &p1, &port, &got);
      if (!got) continue;
      port += 256*p1;
      break;
    }
    if (!s || port<1 || port>65535) error_exit_raw(toybuf);
    si6.sin6_port = SWAP_BE16(port); // same field size/offset for v4 and v6
    port = xsocket(si6.sin6_family, SOCK_STREAM, 0);
    xconnect(port, (void *)&si6, sizeof(si6));

    // RETR blocks until file data read from data port, so use SIZE to check
    // if file exists before creating local copy
    lenr = 0;
    if (FLAG(s)|FLAG(g)) {
      if (ftp_line("SIZE", remote, 0) == 213)
        sscanf(toybuf, "%*u %llu", &lenr);
      else if (get) error_exit("no %s", remote);

      // Open file for reading or writing
      if (strcmp(toys.optargs[1], "-"))
        ii = xcreate(toys.optargs[1],
          get ? (cnt ? O_APPEND : O_TRUNC)|O_CREAT|O_WRONLY : O_RDONLY, 0666);
      lenl = fdlength(ii);
    }
    if (get) {
      cmd = "RETR";
      if (FLAG(l)) cmd = "LIST";
      if (FLAG(L)) cmd = "NLST";
      if (cnt) {
        char buf[32];

        if (lenl>=lenr) goto done;
        sprintf(buf, "%llu", lenl);
        ftp_line("REST", buf, 350);
      } else lenl = 0;

      ftp_line(cmd, remote, -1);
      lenl += xsendfile(port, ii);
      ftp_line(0, 0, FLAG(g) ? 226 : 150);
    } else if (FLAG(s)) {
      cmd = "STOR";
      if (cnt && lenr) {
        cmd = "APPE";
        xlseek(ii, lenl, SEEK_SET);
      } else lenr = 0;
      ftp_line(cmd, remote, 150);
      lenr += xsendfile(ii, port);
      close(port);
    }
    if (FLAG(s)|FLAG(g))
      if (lenl != lenr) error_exit("short %lld/%lld", lenl, lenr);
  }
  ftp_line("QUIT", 0, 0);

done:
  if (CFG_TOYBOX_FREE) {
    if (ii!=1) xclose(ii);
    xclose(port);
    xclose(TT.fd);
  }
}
