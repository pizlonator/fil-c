/* tftp.c - TFTP client.
 *
 * Copyright 2012 Madhur Verma <mad.flexi@gmail.com>
 * Copyright 2015 Sameer Prakash Pradhan <sameer.p.pradhan@gmail.com>
 *
 * No Standard.

USE_TFTP(NEWTOY(tftp, "<1b#<8>65464r:l:g|p|[!gp]", TOYFLAG_USR|TOYFLAG_BIN))

config TFTP
  bool "tftp"
  default n
  help
    usage: tftp [OPTIONS] HOST [PORT]

    Transfer file from/to tftp server.

    -l FILE Local FILE
    -r FILE Remote FILE
    -g    Get file
    -p    Put file
    -b SIZE Transfer blocks of SIZE octets(8 <= SIZE <= 65464)
*/
#define FOR_tftp
#include "toys.h"

GLOBALS(
  char *local_file;
  char *remote_file;
  long block_size;

  struct sockaddr_storage inaddr;
  int af;
)

#define TFTP_BLKSIZE    512
#define TFTP_RETRIES    3
#define TFTP_DATAHEADERSIZE 4
#define TFTP_MAXPACKETSIZE  (TFTP_DATAHEADERSIZE + TFTP_BLKSIZE)
#define TFTP_PACKETSIZE    TFTP_MAXPACKETSIZE
#define TFTP_DATASIZE    (TFTP_PACKETSIZE-TFTP_DATAHEADERSIZE)
#define TFTP_IOBUFSIZE    (TFTP_PACKETSIZE+8)

#define TFTP_OP_RRQ      1  /* Read Request      RFC 1350, RFC 2090 */
#define TFTP_OP_WRQ      2  /* Write Request     RFC 1350 */
#define TFTP_OP_DATA    3  /* Data chunk      RFC 1350 */
#define TFTP_OP_ACK      4  /* Acknowledgement     RFC 1350 */
#define TFTP_OP_ERR      5  /* Error Message     RFC 1350 */
#define TFTP_OP_OACK    6  /* Option acknowledgment RFC 2347 */

#define TFTP_ER_ILLEGALOP  4  /* Illegal TFTP operation */
#define TFTP_ER_UNKID    5  /* Unknown transfer ID */

#define TFTP_ES_NOSUCHFILE  "File not found"
#define TFTP_ES_ACCESS    "Access violation"
#define TFTP_ES_FULL    "Disk full or allocation exceeded"
#define TFTP_ES_ILLEGALOP  "Illegal TFTP operation"
#define TFTP_ES_UNKID    "Unknown transfer ID"
#define TFTP_ES_EXISTS    "File already exists"
#define TFTP_ES_UNKUSER    "No such user"
#define TFTP_ES_NEGOTIATE  "Terminate transfer due to option negotiation"

// Initializes SERVER with ADDR and returns socket.
static int init_tftp(struct sockaddr_storage *server)
{
  struct timeval to = { .tv_sec = 10, //Time out
                        .tv_usec = 0 };
  const int set = 1;
  int port = 69, sd = xsocket(TT.af, SOCK_DGRAM, IPPROTO_UDP);

  xsetsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (void *)&to, sizeof(struct timeval));
  xsetsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void *)&set, sizeof(set));

  if(toys.optc == 2) port = atolx_range(toys.optargs[1], 1, 65535);
  memset(server, 0, sizeof(struct sockaddr_storage));
  if (TT.af == AF_INET6) {
      ((struct sockaddr_in6 *)server)->sin6_family = AF_INET6;
      ((struct sockaddr_in6 *)server)->sin6_addr =
        ((struct sockaddr_in6 *)&TT.inaddr)->sin6_addr;
      ((struct sockaddr_in6 *)server)->sin6_port = htons(port);
  }
  else {
      ((struct sockaddr_in *)server)->sin_family = AF_INET;
      ((struct sockaddr_in *)server)->sin_addr.s_addr =
        ((struct sockaddr_in *)&TT.inaddr)->sin_addr.s_addr;
      ((struct sockaddr_in *)server)->sin_port = htons(port);
  }
  return sd;
}

/*
 * Makes a request packet in BUFFER with OPCODE and file PATH of MODE
 * and returns length of packet.
 */
static int mkpkt_request(uint8_t *buffer, int opcode, char *path, int mode)
{
  buffer[0] = opcode >> 8;
  buffer[1] = opcode & 0xff;
  if(strlen(path) > TFTP_BLKSIZE) error_exit("path too long");
  return sprintf((char*) &buffer[2], "%s%c%s", path, 0,
    (mode ? "octet" : "netascii")) + 3;
}

/*
 * Makes an acknowledgement packet in BUFFER of BLOCNO
 * and returns packet length.
 */
static int mkpkt_ack(uint8_t *buffer, uint16_t blockno)
{
  buffer[0] = TFTP_OP_ACK >> 8;
  buffer[1] = TFTP_OP_ACK & 0xff;
  buffer[2] = blockno >> 8;
  buffer[3] = blockno & 0xff;
  return 4;
}

/*
 * Makes an error packet in BUFFER with ERRORCODE and ERRORMSG.
 * and returns packet length.
 */
static int mkpkt_err(uint8_t *buffer, uint16_t errorcode, char *errormsg)
{
  buffer[0] = TFTP_OP_ERR >> 8;
  buffer[1] = TFTP_OP_ERR & 0xff;
  buffer[2] = errorcode >> 8;
  buffer[3] = errorcode & 0xff;
  strcpy((char*) &buffer[4], errormsg);
  return strlen(errormsg) + 5;
}

/*
 * Recieves data from server in BUFF with socket SD and updates FROM
 * and returns read length.
 */
static int read_server(int sd, void *buf, int len,
  struct sockaddr_storage *from)
{
  socklen_t alen;
  ssize_t nb;

  for (;;) {
    memset(buf, 0, len);
    alen = sizeof(struct sockaddr_storage);
    nb = recvfrom(sd, buf, len, 0, (struct sockaddr *) from, &alen);
    if (nb < 0) {
      if (errno == EAGAIN) {
        perror_msg("server read timed out");
        return nb;
      }else if (errno != EINTR) {
        perror_msg("server read failed");
        return nb;
      }
    }else return nb;
  }
  return nb;
}

/*
 * sends data to server TO from BUFF of length LEN through socket SD
 * and returns successfully send bytes number.
 */
static ssize_t write_server(int sd, void *buf, size_t len,
  struct sockaddr_storage *to)
{
  ssize_t nb;

  for (;;) {
    nb = sendto(sd, buf, len, 0, (struct sockaddr *)to,
            sizeof(struct sockaddr_storage));
    if (nb < 0) {
      if (errno != EINTR) {
        perror_msg("server write failed");
        return nb;
      }
    } else return nb;
  }
  return nb;
}

// checks packet for data and updates block no
static inline int check_data( uint8_t *packet, uint16_t *opcode,
  uint16_t *blockno)
{
  *opcode = (uint16_t) packet[0] << 8 | (uint16_t) packet[1];
  if (*opcode == TFTP_OP_DATA) {
    *blockno = (uint16_t) packet[2] << 8 | (uint16_t) packet[3];
    return 0;
  }
  return -1;
}

// Makes data packet through FD from file OFFSET in buffer PACKET of BLOCKNO
static int mkpkt_data(int fd, off_t offset, uint8_t *packet, uint16_t blockno)
{
  off_t tmp;
  int nbytesread;

  packet[0] = TFTP_OP_DATA >> 8;
  packet[1] = TFTP_OP_DATA & 0xff;
  packet[2] = blockno >> 8;
  packet[3] = blockno & 0xff;
  tmp = lseek(fd, offset, SEEK_SET);
  if (tmp == (off_t) -1) {
    perror_msg("lseek failed");
    return -1;
  }
  nbytesread = readall(fd, &packet[TFTP_DATAHEADERSIZE], TFTP_DATASIZE);
  if (nbytesread < 0) return -1;
  return nbytesread + TFTP_DATAHEADERSIZE;
}

// Receives ACK responses from server and updates blockno
static int read_ack(int sd, uint8_t *packet, struct sockaddr_storage *server,
  uint16_t *port, uint16_t *blockno)
{
  struct sockaddr_storage from;
  int nbytes;
  uint16_t opcode, rblockno;
  int packetlen, retry;

  for (retry = 0; retry < TFTP_RETRIES; retry++) {
    for (;;) {
      nbytes = read_server(sd, packet, TFTP_IOBUFSIZE, &from);
      if (nbytes < 4) { // Ack headersize = 4
        if (nbytes == 0) error_msg("Connection lost.");
        else if (nbytes > 0) error_msg("Short packet: %d bytes", nbytes);
        else error_msg("Server read ACK failure.");
        break;
      } else {
        if (!*port) {
          *port = ((struct sockaddr_in *)&from)->sin_port;
          ((struct sockaddr_in *)server)->sin_port =
                  ((struct sockaddr_in *)&from)->sin_port;
        }
        if (((struct sockaddr_in *)server)->sin_addr.s_addr !=
                ((struct sockaddr_in *)&from)->sin_addr.s_addr) {
          error_msg("Invalid address in DATA.");
          continue;
        }
        if (*port != ((struct sockaddr_in *)server)->sin_port) {
          error_msg("Invalid port in DATA.");
          packetlen = mkpkt_err(packet, TFTP_ER_UNKID, TFTP_ES_UNKID);
          (void) write_server(sd, packet, packetlen, server);
          continue;
        }
        opcode = (uint16_t) packet[0] << 8 | (uint16_t) packet[1];
        rblockno = (uint16_t) packet[2] << 8 | (uint16_t) packet[3];

        if (opcode != TFTP_OP_ACK) {
          error_msg("Bad opcode.");
          if (opcode > 5) {
            packetlen = mkpkt_err(packet, TFTP_ER_ILLEGALOP, TFTP_ES_ILLEGALOP);
            (void) write_server(sd, packet, packetlen, server);
          }
          break;
        }
        if (blockno) *blockno = rblockno;
        return 0;
      }
    }
  }
  error_msg("Timeout, Waiting for ACK.");
  return -1;
}

// receives file from server.
static int file_get(void)
{
  struct sockaddr_storage server, from;
  uint8_t *packet;
  uint16_t blockno = 0, opcode, rblockno = 0;
  int len, sd, fd, retry, nbytesrecvd = 0, ndatabytes, ret, result = -1;

  sd = init_tftp(&server);

  packet = (uint8_t*) xzalloc(TFTP_IOBUFSIZE);
  fd = xcreate(TT.local_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  len = mkpkt_request(packet, TFTP_OP_RRQ, TT.remote_file, 1);
  ret = write_server(sd, packet, len, &server);
  if (ret != len){
    unlink(TT.local_file);
    goto errout_with_sd;
  }
  if (TT.af == AF_INET6) ((struct sockaddr_in6 *)&server)->sin6_port = 0;
  else ((struct sockaddr_in *)&server)->sin_port = 0;

  do {
    blockno++;
    for (retry = 0 ; retry < TFTP_RETRIES; retry++) {
      nbytesrecvd = read_server(sd, packet, TFTP_IOBUFSIZE, &from);
      if (nbytesrecvd > 0) {
        if ( ((TT.af == AF_INET) &&
                memcmp(&((struct sockaddr_in *)&server)->sin_addr,
                &((struct sockaddr_in *)&from)->sin_addr,
                sizeof(struct in_addr))) ||
             ((TT.af == AF_INET6) &&
                memcmp(&((struct sockaddr_in6 *)&server)->sin6_addr,
                &((struct sockaddr_in6 *)&from)->sin6_addr,
                sizeof(struct in6_addr)))) {
          error_msg("Invalid address in DATA.");
          retry--;
          continue;
        }
        if ( ((TT.af == AF_INET) && ((struct sockaddr_in *)&server)->sin_port
                && (((struct sockaddr_in *)&server)->sin_port !=
                ((struct sockaddr_in *)&from)->sin_port)) ||
             ((TT.af == AF_INET6) && ((struct sockaddr_in6 *)&server)->sin6_port
                && (((struct sockaddr_in6 *)&server)->sin6_port !=
                ((struct sockaddr_in6 *)&from)->sin6_port))) {
          error_msg("Invalid port in DATA.");
          len = mkpkt_err(packet, TFTP_ER_UNKID, TFTP_ES_UNKID);
          ret = write_server(sd, packet, len, &from);
          retry--;
          continue;
        }
        if (nbytesrecvd < TFTP_DATAHEADERSIZE) {
          error_msg("Tiny data packet ignored.");
          continue;
        }
        if (check_data(packet, &opcode, &rblockno) != 0
            || blockno != rblockno) {

        if (opcode == TFTP_OP_ERR) {
          char *message = "DATA Check failure.";
            char *arr[] = {TFTP_ES_NOSUCHFILE, TFTP_ES_ACCESS,
              TFTP_ES_FULL, TFTP_ES_ILLEGALOP,
              TFTP_ES_UNKID, TFTP_ES_EXISTS,
              TFTP_ES_UNKUSER, TFTP_ES_NEGOTIATE};
            if (rblockno && (rblockno < 9)) message = arr[rblockno - 1];
            error_msg_raw(message);
        }
        else if (blockno == 1 && opcode == TFTP_OP_OACK) {
          len = mkpkt_ack(packet, 0);
          ret = write_server(sd, packet, len, &from);
          if (ret != len){
            unlink(TT.local_file);
            goto errout_with_sd;
          }
        }
        else if (opcode > 5) {
          len = mkpkt_err(packet, TFTP_ER_ILLEGALOP, TFTP_ES_ILLEGALOP);
          ret = write_server(sd, packet, len, &from);
        }
        continue;
        }
        if ((TT.af == AF_INET6) && !((struct sockaddr_in6 *)&server)->sin6_port)
          ((struct sockaddr_in6 *)&server)->sin6_port =
            ((struct sockaddr_in6 *)&from)->sin6_port;
        else if ((TT.af == AF_INET) && !((struct sockaddr_in *)&server)->sin_port)
          ((struct sockaddr_in *)&server)->sin_port =
            ((struct sockaddr_in *)&from)->sin_port;
        break;
      }
    }
    if (retry == TFTP_RETRIES) {
      error_msg("Retry limit exceeded.");
      unlink(TT.local_file);
      goto errout_with_sd;
    }
    ndatabytes = nbytesrecvd - TFTP_DATAHEADERSIZE;
    if (writeall(fd, packet + TFTP_DATAHEADERSIZE, ndatabytes) < 0){
      unlink(TT.local_file);
      goto errout_with_sd;
    }
    len = mkpkt_ack(packet, blockno);
    ret = write_server(sd, packet, len, &server);
    if (ret != len){
      unlink(TT.local_file);
      goto errout_with_sd;
    }
  } while (ndatabytes >= TFTP_DATASIZE);

  result = 0;

errout_with_sd: xclose(sd);
  free(packet);
  return result;
}

// Sends file to server.
int file_put(void)
{
  struct sockaddr_storage server;
  uint8_t *packet;
  off_t offset = 0;
  uint16_t blockno = 1, rblockno, port = 0;
  int packetlen, sd, fd, retry = 0, ret, result = -1;

  sd = init_tftp(&server);
  packet = (uint8_t*)xzalloc(TFTP_IOBUFSIZE);
  fd = xopenro(TT.local_file);

  for (;;) {  //first loop for request send and confirmation from server.
    packetlen = mkpkt_request(packet, TFTP_OP_WRQ, TT.remote_file, 1);
    ret = write_server(sd, packet, packetlen, &server);
    if (ret != packetlen) goto errout_with_sd;
    if (read_ack(sd, packet, &server, &port, NULL) == 0) break;
    if (++retry > TFTP_RETRIES) {
      error_msg("Retry count exceeded.");
      goto errout_with_sd;
    }
  }
  for (;;) {  // loop for data sending and receving ack from server.
    packetlen = mkpkt_data(fd, offset, packet, blockno);
    if (packetlen < 0) goto errout_with_sd;

    ret = write_server(sd, packet, packetlen, &server);
    if (ret != packetlen) goto errout_with_sd;

    if (read_ack(sd, packet, &server, &port, &rblockno) == 0) {
      if (rblockno == blockno) {
        if (packetlen < TFTP_PACKETSIZE) break;
        blockno++;
        offset += TFTP_DATASIZE;
        retry = 0;
        continue;
      }
    }
    if (++retry > TFTP_RETRIES) {
      error_msg("Retry count exceeded.");
      goto errout_with_sd;
    }
  }
  result = 0;

errout_with_sd: close(sd);
  free(packet);
  return result;
}

void tftp_main(void)
{
  struct addrinfo *info, rp, *res=0;
  int ret;

  if (FLAG(r)) {
    if (!FLAG(l)) {
      char *slash = strrchr(TT.remote_file, '/');
      TT.local_file = (slash) ? slash + 1 : TT.remote_file;
    }
  } else if (FLAG(l)) TT.remote_file = TT.local_file;
  else error_exit("Please provide some files.");

  memset(&rp, 0, sizeof(rp));
  rp.ai_family = AF_UNSPEC;
  rp.ai_socktype = SOCK_STREAM;
  ret = getaddrinfo(toys.optargs[0], toys.optargs[1], &rp, &info);
  if (!ret) {
    for (res = info; res; res = res->ai_next)
    if ( (res->ai_family == AF_INET) || (res->ai_family == AF_INET6)) break;
  }
  if (!res)
    error_exit("bad address '%s' : %s", toys.optargs[0], gai_strerror(ret));
  TT.af = info->ai_family;

  memcpy((void *)&TT.inaddr, info->ai_addr, info->ai_addrlen);
  freeaddrinfo(info);

  if (FLAG(g)) file_get();
  if (FLAG(p)) file_put();
}
