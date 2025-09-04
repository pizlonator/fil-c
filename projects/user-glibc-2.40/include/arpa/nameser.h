#ifndef _ARPA_NAMESER_H_

#include <resolv/arpa/nameser.h>

# ifndef _ISOMAC

/* If the machine allows unaligned access we can do better than using
   the NS_GET16, NS_GET32, NS_PUT16, and NS_PUT32 macros from the
   installed header.  */
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>

extern const struct _ns_flagdata _ns_flagdata[] attribute_hidden;

extern unsigned int	__ns_get16 (const unsigned char *) __THROW;
extern unsigned long	__ns_get32 (const unsigned char *) __THROW;
int __ns_name_ntop (const unsigned char *, char *, size_t) __THROW;
int __ns_name_unpack (const unsigned char *, const unsigned char *,
		      const unsigned char *, unsigned char *, size_t) __THROW;

/* Like ns_samename, but for uncompressed binary names.  Return true
   if the two arguments compare are equal as case-insensitive domain
   names.  */
_Bool __ns_samebinaryname (const unsigned char *, const unsigned char *)
  attribute_hidden;

#define ns_msg_getflag(handle, flag) \
  (((handle)._flags & _ns_flagdata[flag].mask) >> _ns_flagdata[flag].shift)

libresolv_hidden_proto (ns_get16)
libresolv_hidden_proto (ns_get32)
libresolv_hidden_proto (ns_put16)
libresolv_hidden_proto (ns_put32)
libresolv_hidden_proto (ns_initparse)
libresolv_hidden_proto (ns_skiprr)
libresolv_hidden_proto (ns_parserr)
libresolv_hidden_proto (ns_sprintrr)
libresolv_hidden_proto (ns_sprintrrf)
libresolv_hidden_proto (ns_samedomain)
libresolv_hidden_proto (ns_format_ttl)

extern __typeof (ns_makecanon) __libc_ns_makecanon;
libc_hidden_proto (__libc_ns_makecanon)
extern __typeof (ns_name_compress) __ns_name_compress;
libc_hidden_proto (__ns_name_compress)
extern __typeof (ns_name_ntop) __ns_name_ntop;
libc_hidden_proto (__ns_name_ntop)
extern __typeof (ns_name_pack) __ns_name_pack;
libc_hidden_proto (__ns_name_pack)
extern __typeof (ns_name_pton) __ns_name_pton;
libc_hidden_proto (__ns_name_pton)
extern __typeof (ns_name_skip) __ns_name_skip;
libc_hidden_proto (__ns_name_skip)
extern __typeof (ns_name_uncompress) __ns_name_uncompress;
libc_hidden_proto (__ns_name_uncompress)
extern __typeof (ns_name_unpack) __ns_name_unpack;
libc_hidden_proto (__ns_name_unpack)
extern __typeof (ns_samename) __libc_ns_samename;
libc_hidden_proto (__libc_ns_samename)

/* Packet parser helper functions.  */

/* Verify that P points to an uncompressed domain name in wire format.
   On success, return the length of the encoded name, including the
   terminating null byte.  On failure, return -1 and set errno.  EOM
   must point one past the last byte in the packet.  */
int __ns_name_length_uncompressed (const unsigned char *p,
				   const unsigned char *eom) attribute_hidden;

/* Iterator over the resource records in a DNS packet.  */
struct ns_rr_cursor
{
  /* These members are not changed after initialization.  */
  const unsigned char *begin;	/* First byte of packet.  */
  const unsigned char *end;	/* One past the last byte of the packet.  */
  const unsigned char *first_rr; /* First resource record (or packet end).  */

  /* Advanced towards the end while reading the packet.  */
  const unsigned char *current;
};

/* Returns the RCODE field from the DNS header.  */
static inline int
ns_rr_cursor_rcode (const struct ns_rr_cursor *c)
{
  return c->begin[3] & 0x0f;	/* Lower 4 bits at offset 3.  */
}

/* Returns the length of the answer section according to the DNS header.  */
static inline int
ns_rr_cursor_ancount (const struct ns_rr_cursor *c)
{
  return c->begin[6] * 256 + c->begin[7]; /* 16 bits at offset 6.  */
}

/* Returns the length of the authority (name server) section according
   to the DNS header.  */
static inline int
ns_rr_cursor_nscount (const struct ns_rr_cursor *c)
{
  return c->begin[8] * 256 + c->begin[9]; /* 16 bits at offset 8.  */
}

/* Returns the length of the additional data section according to the
   DNS header.  */
static inline int
ns_rr_cursor_adcount (const struct ns_rr_cursor *c)
{
  return c->begin[10] * 256 + c->begin[11]; /* 16 bits at offset 10.  */
}

/* Returns a pointer to the uncompressed question name in wire
   format.  */
static inline const unsigned char *
ns_rr_cursor_qname (const struct ns_rr_cursor *c)
{
  return c->begin + 12;		/* QNAME starts right after the header.  */
}

/* Returns the question type of the first and only question.  */
static inline const int
ns_rr_cursor_qtype (const struct ns_rr_cursor *c)
{
  /* 16 bits 4 bytes back from the first RR header start.  */
  return c->first_rr[-4] * 256 + c->first_rr[-3];
}

/* Returns the clss of the first and only question (usually C_IN).  */
static inline const int
ns_rr_cursor_qclass (const struct ns_rr_cursor *c)
{
  /* 16 bits 2 bytes back from the first RR header start.  */
  return c->first_rr[-2] * 256 + c->first_rr[-1];
}

/* Initializes *C to cover the packet [BUF, BUF+LEN).  Returns false
   if LEN is less than sizeof (*HD), if the packet does not contain a
   full (uncompressed) question, or if the question count is not 1.  */
_Bool __ns_rr_cursor_init (struct ns_rr_cursor *c,
			   const unsigned char *buf, size_t len)
  attribute_hidden;

/* Like ns_rr, but the record owner name is not decoded into text format.  */
struct ns_rr_wire
{
  unsigned char rname[NS_MAXCDNAME]; /* Owner name of the record.  */
  uint16_t rtype;		/* Resource record type (T_*).  */
  uint16_t rclass;		/* Resource record class (C_*).  */
  uint32_t ttl;			/* Time-to-live field.  */
  const unsigned char *rdata;	/* Start of resource record data.  */
  uint16_t rdlength;		/* Length of the data at rdata, in bytes.  */
};

/* Attempts to parse the record at C into *RR.  On success, return
   true, and C is advanced past the record, and RR->rdata points to
   the record data.  On failure, errno is set to EMSGSIZE, and false
   is returned.  */
_Bool __ns_rr_cursor_next (struct ns_rr_cursor *c, struct ns_rr_wire *rr)
  attribute_hidden;

# endif /* !_ISOMAC */
#endif
