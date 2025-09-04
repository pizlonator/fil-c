/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config TLS
//config:	bool #No description makes it a hidden option
//config:	default n
//Note:
//Config.src also defines FEATURE_TLS_SHA1 option

//kbuild:lib-$(CONFIG_TLS) += tls.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_montgomery_reduce.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_mul_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_sqr_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_aes.o
//kbuild:lib-$(CONFIG_TLS) += tls_aesgcm.o
//kbuild:lib-$(CONFIG_TLS) += tls_rsa.o
//kbuild:lib-$(CONFIG_TLS) += tls_fe.o
//kbuild:lib-$(CONFIG_TLS) += tls_sp_c32.o

#include "tls.h"

// Usually enabled. You can disable some of them to force only
// specific ciphers to be advertized to server.
// (this would not exclude code to handle disabled ciphers, no code size win)
#define ALLOW_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256       1
#define ALLOW_ECDHE_RSA_WITH_AES_128_CBC_SHA256         1
#define ALLOW_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256       1
#define ALLOW_ECDHE_RSA_WITH_AES_128_GCM_SHA256         1
#define ALLOW_RSA_WITH_AES_128_CBC_SHA256       1
#define ALLOW_RSA_WITH_AES_256_CBC_SHA256       1
#define ALLOW_RSA_WITH_AES_128_GCM_SHA256       1
#define ALLOW_CURVE_P256        1
#define ALLOW_CURVE_X25519      1

// For testing (does everything except encrypting).
// works against "openssl s_server -cipher NULL"
// and against wolfssl-3.9.10-stable/examples/server/server.c:
#define ALLOW_RSA_NULL_SHA256                   0

#define TLS_DEBUG      0
#define TLS_DEBUG_HASH 0
#define TLS_DEBUG_DER  0
#define TLS_DEBUG_FIXED_SECRETS 0
#if 0
# define dump_raw_out(...) dump_hex(__VA_ARGS__)
#else
# define dump_raw_out(...) ((void)0)
#endif
#if 0
# define dump_raw_in(...) dump_hex(__VA_ARGS__)
#else
# define dump_raw_in(...) ((void)0)
#endif

#if TLS_DEBUG
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#if TLS_DEBUG_DER
# define dbg_der(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg_der(...) ((void)0)
#endif


//TLS 1.2
#define TLS_MAJ 3
#define TLS_MIN 3

#define RECORD_TYPE_CHANGE_CIPHER_SPEC  20 /* 0x14 */
#define RECORD_TYPE_ALERT               21 /* 0x15 */
#define RECORD_TYPE_HANDSHAKE           22 /* 0x16 */
#define RECORD_TYPE_APPLICATION_DATA    23 /* 0x17 */

#define HANDSHAKE_HELLO_REQUEST         0  /* 0x00 */
#define HANDSHAKE_CLIENT_HELLO          1  /* 0x01 */
#define HANDSHAKE_SERVER_HELLO          2  /* 0x02 */
#define HANDSHAKE_HELLO_VERIFY_REQUEST  3  /* 0x03 */
#define HANDSHAKE_NEW_SESSION_TICKET    4  /* 0x04 */
#define HANDSHAKE_CERTIFICATE           11 /* 0x0b */
#define HANDSHAKE_SERVER_KEY_EXCHANGE   12 /* 0x0c */
#define HANDSHAKE_CERTIFICATE_REQUEST   13 /* 0x0d */
#define HANDSHAKE_SERVER_HELLO_DONE     14 /* 0x0e */
#define HANDSHAKE_CERTIFICATE_VERIFY    15 /* 0x0f */
#define HANDSHAKE_CLIENT_KEY_EXCHANGE   16 /* 0x10 */
#define HANDSHAKE_FINISHED              20 /* 0x14 */

#define TLS_EMPTY_RENEGOTIATION_INFO_SCSV       0x00FF /* not a real cipher id... */

#define SSL_NULL_WITH_NULL_NULL                 0x0000
#define SSL_RSA_WITH_NULL_MD5                   0x0001
#define SSL_RSA_WITH_NULL_SHA                   0x0002
#define SSL_RSA_WITH_RC4_128_MD5                0x0004
#define SSL_RSA_WITH_RC4_128_SHA                0x0005
#define TLS_RSA_WITH_IDEA_CBC_SHA               0x0007  /* 7 */
#define SSL_RSA_WITH_3DES_EDE_CBC_SHA           0x000A  /* 10 */

#define SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA       0x0016  /* 22 */
#define SSL_DH_anon_WITH_RC4_128_MD5            0x0018  /* 24 */
#define SSL_DH_anon_WITH_3DES_EDE_CBC_SHA       0x001B  /* 27 */
#define TLS_RSA_WITH_AES_128_CBC_SHA            0x002F  /*SSLv3   Kx=RSA   Au=RSA   Enc=AES(128) Mac=SHA1 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA        0x0033  /* 51 */
#define TLS_DH_anon_WITH_AES_128_CBC_SHA        0x0034  /* 52 */
#define TLS_RSA_WITH_AES_256_CBC_SHA            0x0035  /* 53 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA        0x0039  /* 57 */
#define TLS_DH_anon_WITH_AES_256_CBC_SHA        0x003A  /* 58 */
#define TLS_RSA_WITH_NULL_SHA256                0x003B  /* 59 */
#define TLS_RSA_WITH_AES_128_CBC_SHA256         0x003C  /* 60 */
#define TLS_RSA_WITH_AES_256_CBC_SHA256         0x003D  /* 61 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256     0x0067  /* 103 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256     0x006B  /* 107 */
#define TLS_PSK_WITH_AES_128_CBC_SHA            0x008C  /* 140 */
#define TLS_PSK_WITH_AES_256_CBC_SHA            0x008D  /* 141 */
#define TLS_DHE_PSK_WITH_AES_128_CBC_SHA        0x0090  /* 144 */
#define TLS_DHE_PSK_WITH_AES_256_CBC_SHA        0x0091  /* 145 */
#define TLS_RSA_WITH_SEED_CBC_SHA               0x0096  /* 150 */
#define TLS_RSA_WITH_AES_128_GCM_SHA256         0x009C  /*TLSv1.2 Kx=RSA   Au=RSA   Enc=AESGCM(128) Mac=AEAD */
#define TLS_RSA_WITH_AES_256_GCM_SHA384         0x009D  /*TLSv1.2 Kx=RSA   Au=RSA   Enc=AESGCM(256) Mac=AEAD */
#define TLS_DHE_RSA_WITH_AES_128_GCM_SHA256     0x009E  /*TLSv1.2 Kx=DH    Au=RSA   Enc=AESGCM(128) Mac=AEAD */
#define TLS_DHE_RSA_WITH_AES_256_GCM_SHA384     0x009F  /*TLSv1.2 Kx=DH    Au=RSA   Enc=AESGCM(256) Mac=AEAD */
#define TLS_DH_anon_WITH_AES_128_GCM_SHA256     0x00A6  /* RFC 5288 */
#define TLS_DH_anon_WITH_AES_256_GCM_SHA384     0x00A7  /* RFC 5288 */
#define TLS_PSK_WITH_AES_128_CBC_SHA256         0x00AE  /* 174 */
#define TLS_PSK_WITH_AES_256_CBC_SHA384         0x00AF  /* 175 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA     0xC004  /* 49156 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA     0xC005  /* 49157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    0xC009  /*TLSv1   Kx=ECDH  Au=ECDSA Enc=AES(128) Mac=SHA1 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    0xC00A  /*TLSv1   Kx=ECDH  Au=ECDSA Enc=AES(256) Mac=SHA1 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA       0xC00E  /* 49166 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA       0xC00F  /* 49167 */
#define TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA     0xC012  /* 49170 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA      0xC013  /*TLSv1   Kx=ECDH  Au=RSA   Enc=AES(128) Mac=SHA1 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA      0xC014  /*TLSv1   Kx=ECDH  Au=RSA   Enc=AES(256) Mac=SHA1 */
#define TLS_ECDH_anon_WITH_AES_128_CBC_SHA      0xC018  /* RFC 4492 */
#define TLS_ECDH_anon_WITH_AES_256_CBC_SHA      0xC019  /* RFC 4492 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 0xC023  /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AES(128) Mac=SHA256 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 0xC024  /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AES(256) Mac=SHA384 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256  0xC025  /* 49189 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384  0xC026  /* 49190 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256   0xC027  /*TLSv1.2 Kx=ECDH  Au=RSA   Enc=AES(128) Mac=SHA256 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384   0xC028  /*TLSv1.2 Kx=ECDH  Au=RSA   Enc=AES(256) Mac=SHA384 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256    0xC029  /* 49193 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384    0xC02A  /* 49194 */
/* RFC 5288 "AES Galois Counter Mode (GCM) Cipher Suites for TLS" */
#define TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 0xC02B  /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AESGCM(128) Mac=AEAD */
#define TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 0xC02C  /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AESGCM(256) Mac=AEAD */
#define TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256  0xC02D  /* 49197 */
#define TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384  0xC02E  /* 49198 */
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   0xC02F  /*TLSv1.2 Kx=ECDH  Au=RSA   Enc=AESGCM(128) Mac=AEAD */
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   0xC030  /*TLSv1.2 Kx=ECDH  Au=RSA   Enc=AESGCM(256) Mac=AEAD */
#define TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256    0xC031  /* 49201 */
#define TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384    0xC032  /* 49202 */
#define TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA      0xC035
#define TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA      0xC036
#define TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256   0xC037
#define TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384   0xC038

/* From http://wiki.mozilla.org/Security/Server_Side_TLS */
/* and 'openssl ciphers -V -stdname' */
#define TLS_RSA_WITH_AES_128_CCM                      0xC09C /*TLSv1.2 Kx=RSA   Au=RSA   Enc=AESCCM(128) Mac=AEAD */
#define TLS_RSA_WITH_AES_256_CCM                      0xC09D /*TLSv1.2 Kx=RSA   Au=RSA   Enc=AESCCM(256) Mac=AEAD */
#define TLS_DHE_RSA_WITH_AES_128_CCM                  0xC09E /*TLSv1.2 Kx=DH    Au=RSA   Enc=AESCCM(128) Mac=AEAD */
#define TLS_DHE_RSA_WITH_AES_256_CCM                  0xC09F /*TLSv1.2 Kx=DH    Au=RSA   Enc=AESCCM(256) Mac=AEAD */
#define TLS_RSA_WITH_AES_128_CCM_8                    0xC0A0 /*TLSv1.2 Kx=RSA   Au=RSA   Enc=AESCCM8(128) Mac=AEAD */
#define TLS_RSA_WITH_AES_256_CCM_8                    0xC0A1 /*TLSv1.2 Kx=RSA   Au=RSA   Enc=AESCCM8(256) Mac=AEAD */
#define TLS_DHE_RSA_WITH_AES_128_CCM_8                0xC0A2 /*TLSv1.2 Kx=DH    Au=RSA   Enc=AESCCM8(128) Mac=AEAD */
#define TLS_DHE_RSA_WITH_AES_256_CCM_8                0xC0A3 /*TLSv1.2 Kx=DH    Au=RSA   Enc=AESCCM8(256) Mac=AEAD */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CCM              0xC0AC /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AESCCM(128) Mac=AEAD */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CCM              0xC0AD /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AESCCM(256) Mac=AEAD */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8            0xC0AE /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AESCCM8(128) Mac=AEAD */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8            0xC0AF /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=AESCCM8(256) Mac=AEAD */
#define TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256   0xCCA8 /*TLSv1.2 Kx=ECDH  Au=RSA   Enc=CHACHA20/POLY1305(256) Mac=AEAD */
#define TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 0xCCA9 /*TLSv1.2 Kx=ECDH  Au=ECDSA Enc=CHACHA20/POLY1305(256) Mac=AEAD */
#define TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256     0xCCAA /*TLSv1.2 Kx=DH    Au=RSA   Enc=CHACHA20/POLY1305(256) Mac=AEAD */

#define TLS_AES_128_GCM_SHA256                        0x1301 /*TLSv1.3 Kx=any   Au=any   Enc=AESGCM(128) Mac=AEAD */
#define TLS_AES_256_GCM_SHA384                        0x1302 /*TLSv1.3 Kx=any   Au=any   Enc=AESGCM(256) Mac=AEAD */
#define TLS_CHACHA20_POLY1305_SHA256                  0x1303 /*TLSv1.3 Kx=any   Au=any   Enc=CHACHA20/POLY1305(256) Mac=AEAD */
#define TLS_AES_128_CCM_SHA256                        0x1304 /*TLSv1.3 Kx=any   Au=any   Enc=AESCCM(128) Mac=AEAD */

/* Might go to libbb.h */
#define TLS_MAX_CRYPTBLOCK_SIZE 16
#define TLS_MAX_OUTBUF          (1 << 14)

enum {
	SHA_INSIZE     = 64,

	AES128_KEYSIZE = 16,
	AES256_KEYSIZE = 32,

	RSA_PREMASTER_SIZE = 48,

	RECHDR_LEN = 5,

	/* 8 = 3+5. 3 extra bytes result in record data being 32-bit aligned */
	OUTBUF_PFX = 8 + AES_BLOCK_SIZE, /* header + IV */
	OUTBUF_SFX = TLS_MAX_MAC_SIZE + TLS_MAX_CRYPTBLOCK_SIZE, /* MAC + padding */

	// RFC 5246:
	// | 6.2.1. Fragmentation
	// |  The record layer fragments information blocks into TLSPlaintext
	// |  records carrying data in chunks of 2^14 bytes or less.  Client
	// |  message boundaries are not preserved in the record layer (i.e.,
	// |  multiple client messages of the same ContentType MAY be coalesced
	// |  into a single TLSPlaintext record, or a single message MAY be
	// |  fragmented across several records)
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSPlaintext.fragment.
	// |    The length MUST NOT exceed 2^14.
	// |...
	// | 6.2.2. Record Compression and Decompression
	// |...
	// |  Compression must be lossless and may not increase the content length
	// |  by more than 1024 bytes.  If the decompression function encounters a
	// |  TLSCompressed.fragment that would decompress to a length in excess of
	// |  2^14 bytes, it MUST report a fatal decompression failure error.
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSCompressed.fragment.
	// |    The length MUST NOT exceed 2^14 + 1024.
	// |...
	// | 6.2.3.  Record Payload Protection
	// |  The encryption and MAC functions translate a TLSCompressed
	// |  structure into a TLSCiphertext.  The decryption functions reverse
	// |  the process.  The MAC of the record also includes a sequence
	// |  number so that missing, extra, or repeated messages are
	// |  detectable.
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSCiphertext.fragment.
	// |    The length MUST NOT exceed 2^14 + 2048.
	MAX_INBUF = RECHDR_LEN + (1 << 14) + 2048,

	/* Bits for tls->flags */
	NEED_EC_KEY            = 1 << 0,
	GOT_CERT_RSA_KEY_ALG   = 1 << 1,
	GOT_CERT_ECDSA_KEY_ALG = 1 << 2, // so far unused
	GOT_EC_KEY             = 1 << 3,
	GOT_EC_CURVE_X25519    = 1 << 4, // else P256
	ENCRYPTION_AESGCM      = 1 << 5, // else AES-SHA (or NULL-SHA if ALLOW_RSA_NULL_SHA256=1)
	ENCRYPT_ON_WRITE       = 1 << 6,
};

struct record_hdr {
	uint8_t type;
	uint8_t proto_maj, proto_min;
	uint8_t len16_hi, len16_lo;
};

struct tls_handshake_data {
	/* In bbox, md5/sha1/sha256 ctx's are the same structure */
	md5sha_ctx_t handshake_hash_ctx;

	uint8_t client_and_server_rand32[2 * 32];
	uint8_t master_secret[48];

//TODO: store just the DER key here, parse/use/delete it when sending client key
//this way it will stay key type agnostic here.
	psRsaKey_t server_rsa_pub_key;

	/* peer's elliptic curve key data */
	/* for x25519, it contains one point in first 32 bytes */
	/* for P256, it contains x,y point pair, each 32 bytes long */
	uint8_t ecc_pub_key32[2 * 32];

/* HANDSHAKE HASH: */
	//unsigned saved_client_hello_size;
	//uint8_t saved_client_hello[1];
};


static unsigned get24be(const uint8_t *p)
{
	return 0x100*(0x100*p[0] + p[1]) + p[2];
}

#if TLS_DEBUG
/* Nondestructively see the current hash value */
# if TLS_DEBUG_HASH
static unsigned sha_peek(md5sha_ctx_t *ctx, void *buffer)
{
	md5sha_ctx_t ctx_copy = *ctx; /* struct copy */
	return sha_end(&ctx_copy, buffer);
}
# endif

static void dump_hex(const char *fmt, const void *vp, int len)
{
	char hexbuf[32 * 1024 + 4];
	const uint8_t *p = vp;

	bin2hex(hexbuf, (void*)p, len)[0] = '\0';
	dbg(fmt, hexbuf);
}

static void dump_tls_record(const void *vp, int len)
{
	const uint8_t *p = vp;

	while (len > 0) {
		unsigned xhdr_len;
		if (len < RECHDR_LEN) {
			dump_hex("< |%s|\n", p, len);
			return;
		}
		xhdr_len = 0x100*p[3] + p[4];
		dbg("< hdr_type:%u ver:%u.%u len:%u", p[0], p[1], p[2], xhdr_len);
		p += RECHDR_LEN;
		len -= RECHDR_LEN;
		if (len >= 4 && p[-RECHDR_LEN] == RECORD_TYPE_HANDSHAKE) {
			unsigned len24 = get24be(p + 1);
			dbg(" type:%u len24:%u", p[0], len24);
		}
		if (xhdr_len > len)
			xhdr_len = len;
		dump_hex(" |%s|\n", p, xhdr_len);
		p += xhdr_len;
		len -= xhdr_len;
	}
}
#else
# define dump_hex(...) ((void)0)
# define dump_tls_record(...) ((void)0)
#endif

void FAST_FUNC tls_get_random(void *buf, unsigned len)
{
	if (len != open_read_close("/dev/urandom", buf, len))
		xfunc_die();
}

static void xorbuf3(void *dst, const void *src1, const void *src2, unsigned count)
{
	uint8_t *d = dst;
	const uint8_t *s1 = src1;
	const uint8_t* s2 = src2;
	while (count--)
		*d++ = *s1++ ^ *s2++;
}

void FAST_FUNC xorbuf(void *dst, const void *src, unsigned count)
{
	xorbuf3(dst, dst, src, count);
}

void FAST_FUNC xorbuf_aligned_AES_BLOCK_SIZE(void *dst, const void *src)
{
	unsigned long *d = dst;
	const unsigned long *s = src;
	d[0] ^= s[0];
#if ULONG_MAX <= 0xffffffffffffffff
	d[1] ^= s[1];
 #if ULONG_MAX == 0xffffffff
	d[2] ^= s[2];
	d[3] ^= s[3];
 #endif
#endif
}

#if !TLS_DEBUG_HASH
# define hash_handshake(tls, fmt, buffer, len) \
         hash_handshake(tls, buffer, len)
#endif
static void hash_handshake(tls_state_t *tls, const char *fmt, const void *buffer, unsigned len)
{
	md5sha_hash(&tls->hsd->handshake_hash_ctx, buffer, len);
#if TLS_DEBUG_HASH
	{
		uint8_t h[TLS_MAX_MAC_SIZE];
		dump_hex(fmt, buffer, len);
		dbg(" (%u bytes) ", (int)len);
		len = sha_peek(&tls->hsd->handshake_hash_ctx, h);
		if (ENABLE_FEATURE_TLS_SHA1 && len == SHA1_OUTSIZE)
			dump_hex("sha1:%s\n", h, len);
		else
		if (len == SHA256_OUTSIZE)
			dump_hex("sha256:%s\n", h, len);
		else
			dump_hex("sha???:%s\n", h, len);
	}
#endif
}

#if !ENABLE_FEATURE_TLS_SHA1
# define TLS_MAC_SIZE(tls) SHA256_OUTSIZE
#else
# define TLS_MAC_SIZE(tls) (tls)->MAC_size
#endif

// RFC 2104:
// HMAC(key, text) based on a hash H (say, sha256) is:
// ipad = [0x36 x INSIZE]
// opad = [0x5c x INSIZE]
// HMAC(key, text) = H((key XOR opad) + H((key XOR ipad) + text))
//
// H(key XOR opad) and H(key XOR ipad) can be precomputed
// if we often need HMAC hmac with the same key.
//
// text is often given in disjoint pieces.
typedef struct hmac_precomputed {
	md5sha_ctx_t hashed_key_xor_ipad;
	md5sha_ctx_t hashed_key_xor_opad;
} hmac_precomputed_t;

typedef void md5sha_begin_func(md5sha_ctx_t *ctx) FAST_FUNC;
#if !ENABLE_FEATURE_TLS_SHA1
#define hmac_begin(pre,key,key_size,begin) \
	hmac_begin(pre,key,key_size)
#define begin sha256_begin
#endif
static void hmac_begin(hmac_precomputed_t *pre, uint8_t *key, unsigned key_size, md5sha_begin_func *begin)
{
	uint8_t key_xor_ipad[SHA_INSIZE];
	uint8_t key_xor_opad[SHA_INSIZE];
//	uint8_t tempkey[SHA1_OUTSIZE < SHA256_OUTSIZE ? SHA256_OUTSIZE : SHA1_OUTSIZE];
	unsigned i;

	// "The authentication key can be of any length up to INSIZE, the
	// block length of the hash function.  Applications that use keys longer
	// than INSIZE bytes will first hash the key using H and then use the
	// resultant OUTSIZE byte string as the actual key to HMAC."
	if (key_size > SHA_INSIZE) {
		bb_simple_error_msg_and_die("HMAC key>64"); //does not happen (yet?)
//		md5sha_ctx_t ctx;
//		begin(&ctx);
//		md5sha_hash(&ctx, key, key_size);
//		key_size = sha_end(&ctx, tempkey);
//		//key = tempkey; - right? RIGHT? why does it work without this?
//		// because SHA_INSIZE is 64, but hmac() is always called with
//		// key_size = tls->MAC_size = SHA1/256_OUTSIZE (20 or 32),
//		// and prf_hmac_sha256() -> hmac_sha256() key sizes are:
//		// - RSA_PREMASTER_SIZE is 48
//		// - CURVE25519_KEYSIZE is 32
//		// - master_secret[] is 48
	}

	for (i = 0; i < key_size; i++) {
		key_xor_ipad[i] = key[i] ^ 0x36;
		key_xor_opad[i] = key[i] ^ 0x5c;
	}
	for (; i < SHA_INSIZE; i++) {
		key_xor_ipad[i] = 0x36;
		key_xor_opad[i] = 0x5c;
	}

	begin(&pre->hashed_key_xor_ipad);
	begin(&pre->hashed_key_xor_opad);
	md5sha_hash(&pre->hashed_key_xor_ipad, key_xor_ipad, SHA_INSIZE);
	md5sha_hash(&pre->hashed_key_xor_opad, key_xor_opad, SHA_INSIZE);
}
#undef begin

static unsigned hmac_sha_precomputed_v(
		hmac_precomputed_t *pre,
		uint8_t *out,
		va_list va)
{
	uint8_t *text;
	unsigned len;

	/* pre->hashed_key_xor_ipad contains unclosed "H((key XOR ipad) +" state */
	/* pre->hashed_key_xor_opad contains unclosed "H((key XOR opad) +" state */

	/* calculate out = H((key XOR ipad) + text) */
	while ((text = va_arg(va, uint8_t*)) != NULL) {
		unsigned text_size = va_arg(va, unsigned);
		md5sha_hash(&pre->hashed_key_xor_ipad, text, text_size);
	}
	len = sha_end(&pre->hashed_key_xor_ipad, out);

	/* out = H((key XOR opad) + out) */
	md5sha_hash(&pre->hashed_key_xor_opad, out, len);
	return sha_end(&pre->hashed_key_xor_opad, out);
}

static unsigned hmac_sha_precomputed(hmac_precomputed_t *pre_init, uint8_t *out, ...)
{
	hmac_precomputed_t pre;
	va_list va;
	unsigned len;

	va_start(va, out);
	pre = *pre_init; /* struct copy */
	len = hmac_sha_precomputed_v(&pre, out, va);
	va_end(va);
	return len;
}

#if !ENABLE_FEATURE_TLS_SHA1
#define hmac(tls,out,key,key_size,...) \
	hmac(out,key,key_size, __VA_ARGS__)
#endif
static unsigned hmac(tls_state_t *tls, uint8_t *out, uint8_t *key, unsigned key_size, ...)
{
	hmac_precomputed_t pre;
	va_list va;
	unsigned len;

	va_start(va, key_size);

	hmac_begin(&pre, key, key_size,
			(ENABLE_FEATURE_TLS_SHA1 && tls->MAC_size == SHA1_OUTSIZE)
				? sha1_begin
				: sha256_begin
	);
	len = hmac_sha_precomputed_v(&pre, out, va);

	va_end(va);
	return len;
}

// RFC 5246:
// 5.  HMAC and the Pseudorandom Function
//...
// In this section, we define one PRF, based on HMAC.  This PRF with the
// SHA-256 hash function is used for all cipher suites defined in this
// document and in TLS documents published prior to this document when
// TLS 1.2 is negotiated.
// ^^^^^^^^^^^^^ IMPORTANT!
//               PRF uses sha256 regardless of cipher for all ciphers
//               defined by RFC 5246. It's not sha1 for AES_128_CBC_SHA!
//               However, for _SHA384 ciphers, it's sha384. See RFC 5288,5289.
//...
//    P_hash(secret, seed) = HMAC_hash(secret, A(1) + seed) +
//                           HMAC_hash(secret, A(2) + seed) +
//                           HMAC_hash(secret, A(3) + seed) + ...
// where + indicates concatenation.
// A() is defined as:
//    A(0) = seed
//    A(1) = HMAC_hash(secret, A(0)) = HMAC_hash(secret, seed)
//    A(i) = HMAC_hash(secret, A(i-1))
// P_hash can be iterated as many times as necessary to produce the
// required quantity of data.  For example, if P_SHA256 is being used to
// create 80 bytes of data, it will have to be iterated three times
// (through A(3)), creating 96 bytes of output data; the last 16 bytes
// of the final iteration will then be discarded, leaving 80 bytes of
// output data.
//
// TLS's PRF is created by applying P_hash to the secret as:
//
//    PRF(secret, label, seed) = P_<hash>(secret, label + seed)
//
// The label is an ASCII string.
//
// RFC 5288:
// For cipher suites ending with _SHA256, the PRF is the TLS PRF
// with SHA-256 as the hash function.
// For cipher suites ending with _SHA384, the PRF is the TLS PRF
// with SHA-384 as the hash function.
static void prf_hmac_sha256(/*tls_state_t *tls,*/
		uint8_t *outbuf, unsigned outbuf_size,
		uint8_t *secret, unsigned secret_size,
		const char *label,
		uint8_t *seed, unsigned seed_size)
{
	hmac_precomputed_t pre;
	uint8_t a[TLS_MAX_MAC_SIZE];
	uint8_t *out_p = outbuf;
	unsigned label_size = strlen(label);
	unsigned MAC_size = SHA256_OUTSIZE;

	/* In P_hash() calculation, "seed" is "label + seed": */
#define SEED   label, label_size, seed, seed_size
#define A      a, MAC_size

	hmac_begin(&pre, secret, secret_size, sha256_begin);

	/* A(1) = HMAC_hash(secret, seed) */
	hmac_sha_precomputed(&pre, a, SEED, NULL);

	for (;;) {
		/* HMAC_hash(secret, A(1) + seed) */
		if (outbuf_size <= MAC_size) {
			/* Last, possibly incomplete, block */
			/* (use a[] as temp buffer) */
			hmac_sha_precomputed(&pre, a, A, SEED, NULL);
			memcpy(out_p, a, outbuf_size);
			return;
		}
		/* Not last block. Store directly to result buffer */
		hmac_sha_precomputed(&pre, out_p, A, SEED, NULL);
		out_p += MAC_size;
		outbuf_size -= MAC_size;
		/* A(2) = HMAC_hash(secret, A(1)) */
		hmac_sha_precomputed(&pre, a, A, NULL);
	}
#undef A
#undef SECRET
#undef SEED
}

static void bad_record_die(tls_state_t *tls, const char *expected, int len)
{
	bb_error_msg("got bad TLS record (len:%d) while expecting %s", len, expected);
	if (len > 0) {
		uint8_t *p = tls->inbuf;
		if (len > 99)
			len = 99; /* don't flood, a few lines should be enough */
		do {
			fprintf(stderr, " %02x", *p++);
			len--;
		} while (len != 0);
		fputc('\n', stderr);
	}
	xfunc_die();
}

static void tls_error_die(tls_state_t *tls, int line)
{
	dump_tls_record(tls->inbuf, tls->ofs_to_buffered + tls->buffered_size);
	bb_error_msg_and_die("tls error at line %d cipher:%04x", line, tls->cipher_id);
}
#define tls_error_die(tls) tls_error_die(tls, __LINE__)

#if 0 //UNUSED
static void tls_free_inbuf(tls_state_t *tls)
{
	if (tls->buffered_size == 0) {
		free(tls->inbuf);
		tls->inbuf_size = 0;
		tls->inbuf = NULL;
	}
}
#endif

static void tls_free_outbuf(tls_state_t *tls)
{
	free(tls->outbuf);
	tls->outbuf_size = 0;
	tls->outbuf = NULL;
}

static void *tls_get_outbuf(tls_state_t *tls, int len)
{
	if (len > TLS_MAX_OUTBUF)
		xfunc_die();
	len += OUTBUF_PFX + OUTBUF_SFX;
	if (tls->outbuf_size < len) {
		tls->outbuf_size = len;
		tls->outbuf = xrealloc(tls->outbuf, len);
	}
	return tls->outbuf + OUTBUF_PFX;
}

static void *tls_get_zeroed_outbuf(tls_state_t *tls, int len)
{
	void *record = tls_get_outbuf(tls, len);
	memset(record, 0, len);
	return record;
}

static void xwrite_encrypted_and_hmac_signed(tls_state_t *tls, unsigned size, unsigned type)
{
	uint8_t *buf = tls->outbuf + OUTBUF_PFX;
	struct record_hdr *xhdr;
	uint8_t padding_length;

	xhdr = (void*)(buf - RECHDR_LEN);
	if (!ALLOW_RSA_NULL_SHA256 /* if "no encryption" can't be selected */
	 || tls->cipher_id != TLS_RSA_WITH_NULL_SHA256 /* or if it wasn't selected */
	) {
		xhdr = (void*)(buf - RECHDR_LEN - AES_BLOCK_SIZE); /* place for IV */
	}

	xhdr->type = type;
	xhdr->proto_maj = TLS_MAJ;
	xhdr->proto_min = TLS_MIN;
	/* fake unencrypted record len for MAC calculation */
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;

	/* Calculate MAC signature */
	hmac(tls, buf + size, /* result */
		tls->client_write_MAC_key, TLS_MAC_SIZE(tls),
		&tls->write_seq64_be, sizeof(tls->write_seq64_be),
		xhdr, RECHDR_LEN,
		buf, size,
		NULL
	);
	tls->write_seq64_be = SWAP_BE64(1 + SWAP_BE64(tls->write_seq64_be));

	size += TLS_MAC_SIZE(tls);

	// RFC 5246:
	// 6.2.3.1.  Null or Standard Stream Cipher
	//
	// Stream ciphers (including BulkCipherAlgorithm.null; see Appendix A.6)
	// convert TLSCompressed.fragment structures to and from stream
	// TLSCiphertext.fragment structures.
	//
	//    stream-ciphered struct {
	//        opaque content[TLSCompressed.length];
	//        opaque MAC[SecurityParameters.mac_length];
	//    } GenericStreamCipher;
	//
	// The MAC is generated as:
	//    MAC(MAC_write_key, seq_num +
	//                          TLSCompressed.type +
	//                          TLSCompressed.version +
	//                          TLSCompressed.length +
	//                          TLSCompressed.fragment);
	// where "+" denotes concatenation.
	// seq_num
	//    The sequence number for this record.
	// MAC
	//    The MAC algorithm specified by SecurityParameters.mac_algorithm.
	//
	// Note that the MAC is computed before encryption.  The stream cipher
	// encrypts the entire block, including the MAC.
	//...
	// Appendix C.  Cipher Suite Definitions
	//...
	// MAC       Algorithm    mac_length  mac_key_length
	// --------  -----------  ----------  --------------
	// SHA       HMAC-SHA1       20            20
	// SHA256    HMAC-SHA256     32            32
	if (ALLOW_RSA_NULL_SHA256
	 && tls->cipher_id == TLS_RSA_WITH_NULL_SHA256
	) {
		/* No encryption, only signing */
		xhdr->len16_hi = size >> 8;
		xhdr->len16_lo = size & 0xff;
		dump_raw_out(">> %s\n", xhdr, RECHDR_LEN + size);
		xwrite(tls->ofd, xhdr, RECHDR_LEN + size);
		dbg("wrote %u bytes (NULL crypt, SHA256 hash)\n", size);
		return;
	}

	// 6.2.3.2.  CBC Block Cipher
	// For block ciphers (such as 3DES or AES), the encryption and MAC
	// functions convert TLSCompressed.fragment structures to and from block
	// TLSCiphertext.fragment structures.
	//    struct {
	//        opaque IV[SecurityParameters.record_iv_length];
	//        block-ciphered struct {
	//            opaque content[TLSCompressed.length];
	//            opaque MAC[SecurityParameters.mac_length];
	//            uint8 padding[GenericBlockCipher.padding_length];
	//            uint8 padding_length;
	//        };
	//    } GenericBlockCipher;
	//...
	// IV
	//    The Initialization Vector (IV) SHOULD be chosen at random, and
	//    MUST be unpredictable.  Note that in versions of TLS prior to 1.1,
	//    there was no IV field (...).  For block ciphers, the IV length is
	//    of length SecurityParameters.record_iv_length, which is equal to the
	//    SecurityParameters.block_size.
	// padding
	//    Padding that is added to force the length of the plaintext to be
	//    an integral multiple of the block cipher's block length.
	// padding_length
	//    The padding length MUST be such that the total size of the
	//    GenericBlockCipher structure is a multiple of the cipher's block
	//    length.  Legal values range from zero to 255, inclusive.
	//...
	// Appendix C.  Cipher Suite Definitions
	//...
	//                         Key      IV   Block
	// Cipher        Type    Material  Size  Size
	// ------------  ------  --------  ----  -----
	// AES_128_CBC   Block      16      16     16
	// AES_256_CBC   Block      32      16     16

	tls_get_random(buf - AES_BLOCK_SIZE, AES_BLOCK_SIZE); /* IV */
	dbg("before crypt: 5 hdr + %u data + %u hash bytes\n",
			size - TLS_MAC_SIZE(tls), TLS_MAC_SIZE(tls));

	/* Fill IV and padding in outbuf */
	// RFC is talking nonsense:
	//    "Padding that is added to force the length of the plaintext to be
	//    an integral multiple of the block cipher's block length."
	// WRONG. _padding+padding_length_, not just _padding_,
	// pads the data.
	// IOW: padding_length is the last byte of padding[] array,
	// contrary to what RFC depicts.
	//
	// What actually happens is that there is always padding.
	// If you need one byte to reach BLOCKSIZE, this byte is 0x00.
	// If you need two bytes, they are both 0x01.
	// If you need three, they are 0x02,0x02,0x02. And so on.
	// If you need no bytes to reach BLOCKSIZE, you have to pad a full
	// BLOCKSIZE with bytes of value (BLOCKSIZE-1).
	// It's ok to have more than minimum padding, but we do minimum.
	padding_length = (~size) & (AES_BLOCK_SIZE - 1);
	do {
		buf[size++] = padding_length; /* padding */
	} while ((size & (AES_BLOCK_SIZE - 1)) != 0);

	/* Encrypt content+MAC+padding in place */
	aes_cbc_encrypt(
		&tls->aes_encrypt, /* selects 128/256 */
		buf - AES_BLOCK_SIZE, /* IV */
		buf, size, /* plaintext */
		buf /* ciphertext */
	);

	/* Write out */
	dbg("writing 5 + %u IV + %u encrypted bytes, padding_length:0x%02x\n",
			AES_BLOCK_SIZE, size, padding_length);
	size += AES_BLOCK_SIZE;     /* + IV */
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;
	dump_raw_out(">> %s\n", xhdr, RECHDR_LEN + size);
	xwrite(tls->ofd, xhdr, RECHDR_LEN + size);
	dbg("wrote %u bytes\n", (int)RECHDR_LEN + size);
}

/* Example how GCM encryption combines nonce, aad, input and generates
 * "header | exp_nonce | encrypted output | tag":
 * nonce:0d 6a 26 31 00 00 00 00 00 00 00 01 (implicit 4 bytes (derived from master secret), then explicit 8 bytes)
 * aad:  00 00 00 00 00 00 00 01 17 03 03 00 1c
 * in:   47 45 54 20 2f 69 6e 64 65 78 2e 68 74 6d 6c 20 48 54 54 50 2f 31 2e 30 0d 0a 0d 0a "GET /index.html HTTP/1.0\r\n\r\n" (0x1c bytes)
 * out:  f7 8a b2 8f 78 0e f6 d5 76 17 2e b5 6d 46 59 56 8b 46 9f 0b d9 2c 35 28 13 66 19 be
 * tag:  c2 86 ce 4a 50 4a d0 aa 50 b3 76 5c 49 2a 3f 33
 * sent: 17 03 03 00 34|00 00 00 00 00 00 00 01|f7 8a b2 8f 78 0e f6 d5 76 17 2e b5 6d 46 59 56 8b 46 9f 0b d9 2c 35 28 13 66 19 be|c2 86 ce 4a 50 4a d0 aa 50 b3 76 5c 49 2a 3f 33
 * .............................................^^ buf points here
 */
static void xwrite_encrypted_aesgcm(tls_state_t *tls, unsigned size, unsigned type)
{
#define COUNTER(v) (*(uint32_t*)(v + 12))

	uint8_t aad[13 + 3] ALIGNED_long;   /* +3 creates [16] buffer, simplifying GHASH() */
	uint8_t nonce[12 + 4] ALIGNED_long; /* +4 creates space for AES block counter */
	uint8_t scratch[AES_BLOCK_SIZE] ALIGNED_long; //[16]
	uint8_t authtag[AES_BLOCK_SIZE] ALIGNED_long; //[16]
	uint8_t *buf;
	struct record_hdr *xhdr;
	unsigned remaining;
	unsigned cnt;
	uint64_t t64;

	buf = tls->outbuf + OUTBUF_PFX; /* see above for the byte it points to */
	dump_hex("xwrite_encrypted_aesgcm plaintext:%s\n", buf, size);

	xhdr = (void*)(buf - 8 - RECHDR_LEN);
	xhdr->type = type; /* do it here so that "type" param no longer used */

	aad[8] = type;
	aad[9] = TLS_MAJ;
	aad[10] = TLS_MIN;
	aad[11] = size >> 8;
	/* set aad[12], and clear aad[13..15] */
	COUNTER(aad) = SWAP_LE32(size & 0xff);

	memcpy(nonce, tls->client_write_IV, 4);
	t64 = tls->write_seq64_be;
	move_to_unaligned64(nonce + 4, t64);
	move_to_unaligned64(aad,       t64);
	move_to_unaligned64(buf - 8,   t64);
	/* seq64 is not used later in this func, can increment here */
	tls->write_seq64_be = SWAP_BE64(1 + SWAP_BE64(t64));

	cnt = 1;
	remaining = size;
	while (remaining != 0) {
		unsigned n;

		cnt++;
		COUNTER(nonce) = htonl(cnt); /* yes, first cnt here is 2 (!) */
		aes_encrypt_one_block(&tls->aes_encrypt, nonce, scratch);
		n = remaining > AES_BLOCK_SIZE ? AES_BLOCK_SIZE : remaining;
		xorbuf(buf, scratch, n);
		buf += n;
		remaining -= n;
	}

	aesgcm_GHASH(tls->H, aad, /*sizeof(aad),*/ tls->outbuf + OUTBUF_PFX, size, authtag /*, sizeof(authtag)*/);
	COUNTER(nonce) = htonl(1);
	aes_encrypt_one_block(&tls->aes_encrypt, nonce, scratch);
	xorbuf_aligned_AES_BLOCK_SIZE(authtag, scratch);

	memcpy(buf, authtag, sizeof(authtag));

	/* Write out */
	xhdr = (void*)(tls->outbuf + OUTBUF_PFX - 8 - RECHDR_LEN);
	size += 8 + sizeof(authtag);
	/*xhdr->type = type; - already is */
	xhdr->proto_maj = TLS_MAJ;
	xhdr->proto_min = TLS_MIN;
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;
	size += RECHDR_LEN;
	dump_raw_out(">> %s\n", xhdr, size);
	xwrite(tls->ofd, xhdr, size);
	dbg("wrote %u bytes\n", size);
#undef COUNTER
}

static void xwrite_encrypted(tls_state_t *tls, unsigned size, unsigned type)
{
	if (!(tls->flags & ENCRYPTION_AESGCM)) {
		xwrite_encrypted_and_hmac_signed(tls, size, type);
		return;
	}
	xwrite_encrypted_aesgcm(tls, size, type);
}

static void xwrite_handshake_record(tls_state_t *tls, unsigned size)
{
	uint8_t *buf = tls->outbuf + OUTBUF_PFX;
	struct record_hdr *xhdr = (void*)(buf - RECHDR_LEN);

	xhdr->type = RECORD_TYPE_HANDSHAKE;
	xhdr->proto_maj = TLS_MAJ;
	xhdr->proto_min = TLS_MIN;
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;
	dump_raw_out(">> %s\n", xhdr, RECHDR_LEN + size);
	xwrite(tls->ofd, xhdr, RECHDR_LEN + size);
	dbg("wrote %u bytes\n", (int)RECHDR_LEN + size);
}

static void xwrite_and_update_handshake_hash(tls_state_t *tls, unsigned size)
{
	if (!(tls->flags & ENCRYPT_ON_WRITE)) {
		uint8_t *buf;

		xwrite_handshake_record(tls, size);
		/* Handshake hash does not include record headers */
		buf = tls->outbuf + OUTBUF_PFX;
		hash_handshake(tls, ">> hash:%s", buf, size);
		return;
	}
	xwrite_encrypted(tls, size, RECORD_TYPE_HANDSHAKE);
}

static int tls_has_buffered_record(tls_state_t *tls)
{
	int buffered = tls->buffered_size;
	struct record_hdr *xhdr;
	int rec_size;

	if (buffered < RECHDR_LEN)
		return 0;
	xhdr = (void*)(tls->inbuf + tls->ofs_to_buffered);
	rec_size = RECHDR_LEN + (0x100 * xhdr->len16_hi + xhdr->len16_lo);
	if (buffered < rec_size)
		return 0;
	return rec_size;
}

static const char *alert_text(int code)
{
	//10 unexpected_message
	//20 bad_record_mac
	//21 decryption_failed
	//22 record_overflow
	//30 decompression_failure
	//40 handshake_failure
	//41 no_certificate
	//42 bad_certificate
	//43 unsupported_certificate
	//44 certificate_revoked
	//45 certificate_expired
	//46 certificate_unknown
	//47 illegal_parameter
	//48 unknown_ca
	//49 access_denied
	//50 decode_error
	//51 decrypt_error
	//52 too_many_cids_requested
	//60 export_restriction
	//70 protocol_version
	//71 insufficient_security
	//80 internal_error
	//86 inappropriate_fallback
	//90 user_canceled
	//100 no_renegotiation
	//109 missing_extension
	//110 unsupported_extension
	//111 certificate_unobtainable
	//112 unrecognized_name
	//113 bad_certificate_status_response
	//114 bad_certificate_hash_value
	//115 unknown_psk_identity
	//116 certificate_required
	//120 no_application_protocol
	switch (code) {
	case 20:  return "bad MAC";
	case 50:  return "decode error";
	case 40:  return "handshake failure";
	case 51:  return "decrypt error";
	case 80:  return "internal error";
	case 112: return "unrecognized name";
	}
	return itoa(code);
}

static void tls_aesgcm_decrypt(tls_state_t *tls, uint8_t *buf, int size)
{
#define COUNTER(v) (*(uint32_t*)(v + 12))

	//uint8_t aad[13 + 3] ALIGNED_long; /* +3 creates [16] buffer, simplifying GHASH() */
	uint8_t nonce[12 + 4] ALIGNED_long; /* +4 creates space for AES block counter */
	uint8_t scratch[AES_BLOCK_SIZE] ALIGNED_long; //[16]
	//uint8_t authtag[AES_BLOCK_SIZE] ALIGNED_long; //[16]
	unsigned remaining;
	unsigned cnt;

	//memcpy(aad, buf, 8);
	//aad[8] = type;
	//aad[9] = TLS_MAJ;
	//aad[10] = TLS_MIN;
	//aad[11] = size >> 8;
	///* set aad[12], and clear aad[13..15] */
	//COUNTER(aad) = SWAP_LE32(size & 0xff);

	memcpy(nonce,     tls->server_write_IV, 4);
	memcpy(nonce + 4, buf, 8);

	cnt = 1;
	remaining = size;
	while (remaining != 0) {
		unsigned n;

		cnt++;
		COUNTER(nonce) = htonl(cnt); /* yes, first cnt here is 2 (!) */
		aes_encrypt_one_block(&tls->aes_decrypt, nonce, scratch);
		n = remaining > AES_BLOCK_SIZE ? AES_BLOCK_SIZE : remaining;
		xorbuf3(buf, scratch, buf + 8, n);
		buf += n;
		remaining -= n;
	}

	//aesgcm_GHASH(tls->H, aad, tls->inbuf + RECHDR_LEN, size, authtag);
	//COUNTER(nonce) = htonl(1);
	//aes_encrypt_one_block(&tls->aes_encrypt, nonce, scratch);
	//xorbuf_aligned_AES_BLOCK_SIZE(authtag, scratch);

	//memcmp(buf, authtag, sizeof(authtag)) || DIE("HASH DOES NOT MATCH!");
#undef COUNTER
}

static int tls_xread_record(tls_state_t *tls, const char *expected)
{
	struct record_hdr *xhdr;
	int sz;
	int total;
	int target;

 again:
	dbg("ofs_to_buffered:%u buffered_size:%u\n", tls->ofs_to_buffered, tls->buffered_size);
	total = tls->buffered_size;
	if (total != 0) {
		memmove(tls->inbuf, tls->inbuf + tls->ofs_to_buffered, total);
		//dbg("<< remaining at %d [%d] ", tls->ofs_to_buffered, total);
		//dump_raw_in("<< %s\n", tls->inbuf, total);
	}
	errno = 0;
	target = MAX_INBUF;
	for (;;) {
		int rem;

		if (total >= RECHDR_LEN && target == MAX_INBUF) {
			xhdr = (void*)tls->inbuf;
			target = RECHDR_LEN + (0x100 * xhdr->len16_hi + xhdr->len16_lo);

			if (target > MAX_INBUF /* malformed input (too long) */
			 || xhdr->proto_maj != TLS_MAJ
			 || xhdr->proto_min != TLS_MIN
			) {
				sz = total < target ? total : target;
				bad_record_die(tls, expected, sz);
			}
			dbg("xhdr type:%d ver:%d.%d len:%d\n",
				xhdr->type, xhdr->proto_maj, xhdr->proto_min,
				0x100 * xhdr->len16_hi + xhdr->len16_lo
			);
		}
		/* if total >= target, we have a full packet (and possibly more)... */
		if (total - target >= 0)
			break;
		/* input buffer is grown only as needed */
		rem = tls->inbuf_size - total;
		if (rem == 0) {
			tls->inbuf_size += MAX_INBUF / 8;
			if (tls->inbuf_size > MAX_INBUF)
				tls->inbuf_size = MAX_INBUF;
			dbg("inbuf_size:%d\n", tls->inbuf_size);
			rem = tls->inbuf_size - total;
			tls->inbuf = xrealloc(tls->inbuf, tls->inbuf_size);
		}
		sz = safe_read(tls->ifd, tls->inbuf + total, rem);
		if (sz <= 0) {
			if (sz == 0 && total == 0) {
				/* "Abrupt" EOF, no TLS shutdown (seen from kernel.org) */
				dbg("EOF (without TLS shutdown) from peer\n");
				tls->buffered_size = 0;
				goto end;
			}
			bb_perror_msg_and_die("short read, have only %d", total);
		}
		dump_raw_in("<< %s\n", tls->inbuf + total, sz);
		total += sz;
	}
	tls->buffered_size = total - target;
	tls->ofs_to_buffered = target;
	//dbg("<< stashing at %d [%d] ", tls->ofs_to_buffered, tls->buffered_size);
	//dump_hex("<< %s\n", tls->inbuf + tls->ofs_to_buffered, tls->buffered_size);

	sz = target - RECHDR_LEN;

	/* Needs to be decrypted? */
	if (tls->min_encrypted_len_on_read != 0) {
		if (sz < (int)tls->min_encrypted_len_on_read)
			bb_error_msg_and_die("bad encrypted len:%u", sz);

		if (tls->flags & ENCRYPTION_AESGCM) {
			/* AESGCM */
			uint8_t *p = tls->inbuf + RECHDR_LEN;

			sz -= 8 + AES_BLOCK_SIZE; /* we will overwrite nonce, drop hash */
			tls_aesgcm_decrypt(tls, p, sz);
			dbg("encrypted size:%u\n", sz);
		} else
		if (tls->min_encrypted_len_on_read > TLS_MAC_SIZE(tls)) {
			/* AES+SHA */
			uint8_t *p = tls->inbuf + RECHDR_LEN;
			int padding_len;

			if (sz & (AES_BLOCK_SIZE-1))
				bb_error_msg_and_die("bad encrypted len:%u", sz);

			/* Decrypt content+MAC+padding, moving it over IV in the process */
			sz -= AES_BLOCK_SIZE; /* we will overwrite IV now */
			aes_cbc_decrypt(
				&tls->aes_decrypt, /* selects 128/256 */
				p, /* IV */
				p + AES_BLOCK_SIZE, sz, /* ciphertext */
				p /* plaintext */
			);
			padding_len = p[sz - 1];
			dbg("encrypted size:%u type:0x%02x padding_length:0x%02x\n", sz, p[0], padding_len);
			padding_len++;
			sz -= TLS_MAC_SIZE(tls) + padding_len; /* drop MAC and padding */
		} else {
			/* if nonzero, then it's TLS_RSA_WITH_NULL_SHA256: drop MAC */
			/* else: no encryption yet on input, subtract zero = NOP */
			sz -= tls->min_encrypted_len_on_read;
		}
	}
	if (sz < 0)
		bb_simple_error_msg_and_die("encrypted data too short");

	//dump_hex("<< %s\n", tls->inbuf, RECHDR_LEN + sz);

	xhdr = (void*)tls->inbuf;
	if (xhdr->type == RECORD_TYPE_ALERT && sz >= 2) {
		uint8_t *p = tls->inbuf + RECHDR_LEN;
		dbg("ALERT size:%d level:%d description:%d\n", sz, p[0], p[1]);
		if (p[0] == 2) { /* fatal */
			bb_error_msg_and_die("TLS %s from peer (alert code %d): %s",
				"error",
				p[1], alert_text(p[1])
			);
		}
		if (p[0] == 1) { /* warning */
			if (p[1] == 0) { /* "close_notify" warning: it's EOF */
				dbg("EOF (TLS encoded) from peer\n");
				sz = 0;
				goto end;
			}
//This possibly needs to be cached and shown only if
//a fatal alert follows
//			bb_error_msg("TLS %s from peer (alert code %d): %s",
//				"warning",
//				p[1], alert_text(p[1])
//			);
			/* discard it, get next record */
			goto again;
		}
		/* p[0] not 1 or 2: not defined in protocol */
		sz = 0;
		goto end;
	}

	/* RFC 5246 is not saying it explicitly, but sha256 hash
	 * in our FINISHED record must include data of incoming packets too!
	 */
	if (tls->inbuf[0] == RECORD_TYPE_HANDSHAKE
/* HANDSHAKE HASH: */
	// && do_we_know_which_hash_to_use /* server_hello() might not know it in the future! */
	) {
		hash_handshake(tls, "<< hash:%s", tls->inbuf + RECHDR_LEN, sz);
	}
 end:
	dbg("got block len:%u\n", sz);
	return sz;
}

static void binary_to_pstm(pstm_int *pstm_n, uint8_t *bin_ptr, unsigned len)
{
	pstm_init_for_read_unsigned_bin(/*pool:*/ NULL, pstm_n, len);
	pstm_read_unsigned_bin(pstm_n, bin_ptr, len);
	//return bin_ptr + len;
}

/*
 * DER parsing routines
 */
static unsigned get_der_len(uint8_t **bodyp, uint8_t *der, uint8_t *end)
{
	unsigned len, len1;

	if (end - der < 2)
		xfunc_die();
//	if ((der[0] & 0x1f) == 0x1f) /* not single-byte item code? */
//		xfunc_die();

	len = der[1]; /* maybe it's short len */
	if (len >= 0x80) {
		/* no, it's long */

		if (len == 0x80 || end - der < (int)(len - 0x7e)) {
			/* 0x80 is "0 bytes of len", invalid DER: must use short len if can */
			/* need 3 or 4 bytes for 81, 82 */
			xfunc_die();
		}

		len1 = der[2]; /* if (len == 0x81) it's "ii 81 xx", fetch xx */
		if (len > 0x82) {
			/* >0x82 is "3+ bytes of len", should not happen realistically */
			xfunc_die();
		}
		if (len == 0x82) { /* it's "ii 82 xx yy" */
			len1 = 0x100*len1 + der[3];
			der += 1; /* skip [yy] */
		}
		der += 1; /* skip [xx] */
		len = len1;
//		if (len < 0x80)
//			xfunc_die(); /* invalid DER: must use short len if can */
	}
	der += 2; /* skip [code]+[1byte] */

	if (end - der < (int)len)
		xfunc_die();
	*bodyp = der;

	return len;
}

static uint8_t *enter_der_item(uint8_t *der, uint8_t **endp)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, *endp);
	dbg_der("entered der @%p:0x%02x len:%u inner_byte @%p:0x%02x\n", der, der[0], len, new_der, new_der[0]);
	/* Move "end" position to cover only this item */
	*endp = new_der + len;
	return new_der;
}

static uint8_t *skip_der_item(uint8_t *der, uint8_t *end)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, end);
	/* Skip body */
	new_der += len;
	dbg_der("skipped der 0x%02x, next byte 0x%02x\n", der[0], new_der[0]);
	return new_der;
}

static void der_binary_to_pstm(pstm_int *pstm_n, uint8_t *der, uint8_t *end)
{
	uint8_t *bin_ptr;
	unsigned len = get_der_len(&bin_ptr, der, end);

	dbg_der("binary bytes:%u, first:0x%02x\n", len, bin_ptr[0]);
	binary_to_pstm(pstm_n, bin_ptr, len);
}

static void find_key_in_der_cert(tls_state_t *tls, uint8_t *der, int len)
{
/* Certificate is a DER-encoded data structure. Each DER element has a length,
 * which makes it easy to skip over large compound elements of any complexity
 * without parsing them. Example: partial decode of kernel.org certificate:
 *  SEQ 0x05ac/1452 bytes (Certificate): 308205ac
 *    SEQ 0x0494/1172 bytes (tbsCertificate): 30820494
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0] 3 bytes: a003
 *        INTEGER (version): 0201 02
 *      INTEGER 0x11 bytes (serialNumber): 0211 00 9f85bf664b0cddafca508679501b2be4
 *      //^^^^^^note: matrixSSL also allows [ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 2] = 0x82 type
 *      SEQ 0x0d bytes (signatureAlgo): 300d
 *        OID 9 bytes: 0609 2a864886f70d01010b (OID_SHA256_RSA_SIG 42.134.72.134.247.13.1.1.11)
 *        NULL: 0500
 *      SEQ 0x5f bytes (issuer): 305f
 *        SET 11 bytes: 310b
 *          SEQ 9 bytes: 3009
 *            OID 3 bytes: 0603 550406
 *            Printable string "FR": 1302 4652
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550408
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550407
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 55040a
 *            Printable string "Gandi": 1305 47616e6469
 *        SET 32 bytes: 3120
 *          SEQ 30 bytes: 301e
 *            OID 3 bytes: 0603 550403
 *            Printable string "Gandi Standard SSL CA 2": 1317 47616e6469205374616e646172642053534c2043412032
 *      SEQ 30 bytes (validity): 301e
 *        TIME "161011000000Z": 170d 3136313031313030303030305a
 *        TIME "191011235959Z": 170d 3139313031313233353935395a
 *      SEQ 0x5b/91 bytes (subject): 305b //I did not decode this
 *          3121301f060355040b1318446f6d61696e20436f
 *          6e74726f6c2056616c6964617465643121301f06
 *          0355040b1318506f73697469766553534c204d75
 *          6c74692d446f6d61696e31133011060355040313
 *          0a6b65726e656c2e6f7267
 *      SEQ 0x01a2/418 bytes (subjectPublicKeyInfo): 308201a2
 *        SEQ 13 bytes (algorithm): 300d
 *          OID 9 bytes: 0609 2a864886f70d010101 (OID_RSA_KEY_ALG 42.134.72.134.247.13.1.1.1)
 *          NULL: 0500
 *        BITSTRING 0x018f/399 bytes (publicKey): 0382018f
 *          ????: 00
 *          //after the zero byte, it appears key itself uses DER encoding:
 *          SEQ 0x018a/394 bytes: 3082018a
 *            INTEGER 0x0181/385 bytes (modulus): 02820181
 *                  00b1ab2fc727a3bef76780c9349bf3
 *                  ...24 more blocks of 15 bytes each...
 *                  90e895291c6bc8693b65
 *            INTEGER 3 bytes (exponent): 0203 010001
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0x3] 0x01e5 bytes (X509v3 extensions): a38201e5
 *        SEQ 0x01e1 bytes: 308201e1
 *        ...
 * Certificate is a sequence of three elements:
 *	tbsCertificate (SEQ)
 *	signatureAlgorithm (AlgorithmIdentifier)
 *	signatureValue (BIT STRING)
 *
 * In turn, tbsCertificate is a sequence of:
 *	version
 *	serialNumber
 *	signatureAlgo (AlgorithmIdentifier)
 *	issuer (Name, has complex structure)
 *	validity (Validity, SEQ of two Times)
 *	subject (Name)
 *	subjectPublicKeyInfo (SEQ)
 *	...
 *
 * subjectPublicKeyInfo is a sequence of:
 *	algorithm (AlgorithmIdentifier)
 *	publicKey (BIT STRING)
 *
 * We need Certificate.tbsCertificate.subjectPublicKeyInfo.publicKey
 *
 * Example of an ECDSA key:
 *      SEQ 0x59 bytes (subjectPublicKeyInfo): 3059
 *        SEQ 0x13 bytes (algorithm): 3013
 *          OID 7 bytes: 0607 2a8648ce3d0201   (OID_ECDSA_KEY_ALG 42.134.72.206.61.2.1)
 *          OID 8 bytes: 0608 2a8648ce3d030107 (OID_EC_prime256v1 42.134.72.206.61.3.1.7)
 *        BITSTRING 0x42 bytes (publicKey): 0342
 *          0004 53af f65e 50cc 7959 7e29 0171 c75c
 *          7335 e07d f45b 9750 b797 3a38 aebb 2ac6
 *          8329 2748 e77e 41cb d482 2ce6 05ec a058
 *          f3ab d561 2f4c d845 9ad3 7252 e3de bd3b
 *          9012
 */
	uint8_t *end = der + len;

	/* enter "Certificate" item: [der, end) will be only Cert */
	der = enter_der_item(der, &end);

	/* enter "tbsCertificate" item: [der, end) will be only tbsCert */
	der = enter_der_item(der, &end);

	/*
	 * Skip version field only if it is present. For a v1 certificate, the
	 * version field won't be present since v1 is the default value for the
	 * version field and fields with default values should be omitted (see
	 * RFC 5280 sections 4.1 and 4.1.2.1). If the version field is present
	 * it will have a tag class of 2 (context-specific), bit 6 as 1
	 * (constructed), and a tag number of 0 (see ITU-T X.690 sections 8.1.2
	 * and 8.14).
	 */
	/* bits 7-6: 10 */
	/* bit 5: 1 */
	/* bits 4-0: 00000 */
	if (der[0] == 0xa0)
		der = skip_der_item(der, end); /* version */

	/* skip up to subjectPublicKeyInfo */
	der = skip_der_item(der, end); /* serialNumber */
	der = skip_der_item(der, end); /* signatureAlgo */
	der = skip_der_item(der, end); /* issuer */
	der = skip_der_item(der, end); /* validity */
	der = skip_der_item(der, end); /* subject */

	/* enter subjectPublicKeyInfo */
	der = enter_der_item(der, &end);
	{ /* check subjectPublicKeyInfo.algorithm */
		static const uint8_t OID_RSA_KEY_ALG[] ALIGN1 = {
			0x30,0x0d, // SEQ 13 bytes
			0x06,0x09, 0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01, //OID_RSA_KEY_ALG 42.134.72.134.247.13.1.1.1
			//0x05,0x00, // NULL
		};
		static const uint8_t OID_ECDSA_KEY_ALG[] ALIGN1 = {
			0x30,0x13, // SEQ 0x13 bytes
			0x06,0x07, 0x2a,0x86,0x48,0xce,0x3d,0x02,0x01,      //OID_ECDSA_KEY_ALG 42.134.72.206.61.2.1
		//allow any curve code for now...
		//	0x06,0x08, 0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07, //OID_EC_prime256v1 42.134.72.206.61.3.1.7
			//RFC 3279:
			//42.134.72.206.61.3     is ellipticCurve
			//42.134.72.206.61.3.0   is c-TwoCurve
			//42.134.72.206.61.3.1   is primeCurve
			//42.134.72.206.61.3.1.7 is curve_secp256r1
		};
		if (memcmp(der, OID_RSA_KEY_ALG, sizeof(OID_RSA_KEY_ALG)) == 0) {
			dbg("RSA key\n");
			tls->flags |= GOT_CERT_RSA_KEY_ALG;
		} else
		if (memcmp(der, OID_ECDSA_KEY_ALG, sizeof(OID_ECDSA_KEY_ALG)) == 0) {
			dbg("ECDSA key\n");
			//UNUSED: tls->flags |= GOT_CERT_ECDSA_KEY_ALG;
		} else
			bb_simple_error_msg_and_die("not RSA or ECDSA cert");
	}

	if (tls->flags & GOT_CERT_RSA_KEY_ALG) {
		/* parse RSA key: */
	//based on getAsnRsaPubKey(), pkcs1ParsePrivBin() is also of note
		/* skip subjectPublicKeyInfo.algorithm */
		der = skip_der_item(der, end);
		/* enter subjectPublicKeyInfo.publicKey */
		//die_if_not_this_der_type(der, end, 0x03); /* must be BITSTRING */
		der = enter_der_item(der, &end);

		dbg("key bytes:%u, first:0x%02x\n", (int)(end - der), der[0]);
		if (end - der < 14)
			xfunc_die();
		/* example format:
		 * ignore bits: 00
		 * SEQ 0x018a/394 bytes: 3082018a
		 *   INTEGER 0x0181/385 bytes (modulus): 02820181 XX...XXX
		 *   INTEGER 3 bytes (exponent): 0203 010001
		 */
		if (*der != 0) /* "ignore bits", should be 0 */
			xfunc_die();
		der++;
		der = enter_der_item(der, &end); /* enter SEQ */
		/* memset(tls->hsd->server_rsa_pub_key, 0, sizeof(tls->hsd->server_rsa_pub_key)); - already is */
		der_binary_to_pstm(&tls->hsd->server_rsa_pub_key.N, der, end); /* modulus */
		der = skip_der_item(der, end);
		der_binary_to_pstm(&tls->hsd->server_rsa_pub_key.e, der, end); /* exponent */
		tls->hsd->server_rsa_pub_key.size = pstm_unsigned_bin_size(&tls->hsd->server_rsa_pub_key.N);
		dbg("server_rsa_pub_key.size:%d\n", tls->hsd->server_rsa_pub_key.size);
	}
	/* else: ECDSA key. It is not used for generating encryption keys,
	 * it is used only to sign the EC public key (which comes in ServerKey message).
	 * Since we do not verify cert validity, verifying signature on EC public key
	 * wouldn't add any security. Thus, we do nothing here.
	 */
}

/*
 * TLS Handshake routines
 */
static int tls_xread_handshake_block(tls_state_t *tls, int min_len)
{
	struct record_hdr *xhdr;
	int len = tls_xread_record(tls, "handshake record");

	xhdr = (void*)tls->inbuf;
	if (len < min_len
	 || xhdr->type != RECORD_TYPE_HANDSHAKE
	) {
		bad_record_die(tls, "handshake record", len);
	}
	dbg("got HANDSHAKE\n");
	return len;
}

static ALWAYS_INLINE void fill_handshake_record_hdr(void *buf, unsigned type, unsigned len)
{
	struct handshake_hdr {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
	} *h = buf;

	len -= 4;
	h->type = type;
	h->len24_hi  = len >> 16;
	h->len24_mid = len >> 8;
	h->len24_lo  = len & 0xff;
}

static void send_client_hello_and_alloc_hsd(tls_state_t *tls, const char *sni)
{
#define NUM_CIPHERS (0 \
	+ 4 * ENABLE_FEATURE_TLS_SHA1 \
	+ ALLOW_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 \
	+ ALLOW_ECDHE_RSA_WITH_AES_128_CBC_SHA256 \
	+ ALLOW_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 \
	+ ALLOW_ECDHE_RSA_WITH_AES_128_GCM_SHA256 \
	+ 2 * ENABLE_FEATURE_TLS_SHA1 \
	+ ALLOW_RSA_WITH_AES_128_CBC_SHA256 \
	+ ALLOW_RSA_WITH_AES_256_CBC_SHA256 \
	+ ALLOW_RSA_WITH_AES_128_GCM_SHA256 \
	+ ALLOW_RSA_NULL_SHA256 \
	)
	static const uint8_t ciphers[] = {
		0x00,2 * (1 + NUM_CIPHERS), //len16_be
		0x00,0xFF, //not a cipher - TLS_EMPTY_RENEGOTIATION_INFO_SCSV
		/* ^^^^^^ RFC 5746 Renegotiation Indication Extension - some servers will refuse to work with us otherwise */
#if ENABLE_FEATURE_TLS_SHA1
		0xC0,0x09, // 1 TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA - ok: wget https://is.gd/
		0xC0,0x0A, // 2 TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA - ok: wget https://is.gd/
		0xC0,0x13, // 3 TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA - ok: openssl s_server ... -cipher ECDHE-RSA-AES128-SHA
		0xC0,0x14, // 4 TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA - ok: openssl s_server ... -cipher ECDHE-RSA-AES256-SHA (might fail with older openssl)
	//	0xC0,0x18, //   TLS_ECDH_anon_WITH_AES_128_CBC_SHA
	//	0xC0,0x19, //   TLS_ECDH_anon_WITH_AES_256_CBC_SHA
#endif
#if ALLOW_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
		0xC0,0x23, // 5 TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 - ok: wget https://is.gd/
#endif
	//	0xC0,0x24, //   TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 - can't do SHA384 yet
#if ALLOW_ECDHE_RSA_WITH_AES_128_CBC_SHA256
		0xC0,0x27, // 6 TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 - ok: openssl s_server ... -cipher ECDHE-RSA-AES128-SHA256
#endif
	//	0xC0,0x28, //   TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384 - can't do SHA384 yet
#if ALLOW_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
		0xC0,0x2B, // 7 TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 - ok: wget https://is.gd/
#endif
	//	0xC0,0x2C, //   TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 - wget https://is.gd/: "TLS error from peer (alert code 20): bad MAC"
//TODO: GCM_SHA384 ciphers can be supported, only need sha384-based PRF?
#if ALLOW_ECDHE_RSA_WITH_AES_128_GCM_SHA256
		0xC0,0x2F, // 8 TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 - ok: openssl s_server ... -cipher ECDHE-RSA-AES128-GCM-SHA256
#endif
	//	0xC0,0x30, //   TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 - openssl s_server ... -cipher ECDHE-RSA-AES256-GCM-SHA384: "decryption failed or bad record mac"
	//possibly these too:
#if ENABLE_FEATURE_TLS_SHA1
	//	0xC0,0x35, //   TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA
	//	0xC0,0x36, //   TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA
#endif
	//	0xC0,0x37, //   TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
	//	0xC0,0x38, //   TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384 - can't do SHA384 yet
#if ENABLE_FEATURE_TLS_SHA1
		0x00,0x2F, // 9 TLS_RSA_WITH_AES_128_CBC_SHA - ok: openssl s_server ... -cipher AES128-SHA
		0x00,0x35, //10 TLS_RSA_WITH_AES_256_CBC_SHA - ok: openssl s_server ... -cipher AES256-SHA
#endif
#if ALLOW_RSA_WITH_AES_128_CBC_SHA256
		0x00,0x3C, //11 TLS_RSA_WITH_AES_128_CBC_SHA256 - ok: openssl s_server ... -cipher AES128-SHA256
#endif
#if ALLOW_RSA_WITH_AES_256_CBC_SHA256
		0x00,0x3D, //12 TLS_RSA_WITH_AES_256_CBC_SHA256 - ok: openssl s_server ... -cipher AES256-SHA256
#endif
#if ALLOW_RSA_WITH_AES_128_GCM_SHA256
		0x00,0x9C, //13 TLS_RSA_WITH_AES_128_GCM_SHA256 - ok: openssl s_server ... -cipher AES128-GCM-SHA256
#endif
	//	0x00,0x9D, //   TLS_RSA_WITH_AES_256_GCM_SHA384 - openssl s_server ... -cipher AES256-GCM-SHA384: "decryption failed or bad record mac"
#if ALLOW_RSA_NULL_SHA256
		0x00,0x3B, //   TLS_RSA_WITH_NULL_SHA256
#endif
		0x01,0x00, //not a cipher - comprtypes_len, comprtype
	};
	struct client_hello {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32];
		uint8_t session_id_len;
		/* uint8_t session_id[]; */
		uint8_t cipherid_len16_hi, cipherid_len16_lo;
		uint8_t cipherid[2 * (1 + NUM_CIPHERS)]; /* actually variable */
		uint8_t comprtypes_len;
		uint8_t comprtypes[1]; /* actually variable */
	};
	// https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml
	static const uint8_t extensions[] = {
		// is.gd responds with "handshake failure" to our hello if there's no supported_groups
		0x00,0x0a, //extension_type: "supported_groups"
			0x00,2 * (1 + ALLOW_CURVE_P256 + ALLOW_CURVE_X25519), //ext len
			0x00,2 * (0 + ALLOW_CURVE_P256 + ALLOW_CURVE_X25519), //list len
#if ALLOW_CURVE_P256
			0x00,0x17, //curve_secp256r1 (aka P256, aka prime256v1)
#endif
			//0x00,0x18, //curve_secp384r1
			//0x00,0x19, //curve_secp521r1
#if ALLOW_CURVE_X25519
			0x00,0x1d, //curve_x25519 (RFC 7748)
#endif
			//0x00,0x1e, //curve_x448 (RFC 7748)

		//0x00,0x0b,0x00,0x04,0x03,0x00,0x01,0x02, //extension_type: "ec_point_formats"
		//0x00,0x16,0x00,0x00, //extension_type: "encrpypt-then-mac"
		//0x00,0x17,0x00,0x00, //extension_type: "extended_master"
		//0x00,0x23,0x00,0x00, //extension_type: "session_ticket"

		// kojipkgs.fedoraproject.org responds with alert code 80 ("internal error")
		// to our hello without signature_algorithms.
		// It is satisfied with just 0x04,0x01.
		0x00,0x0d, //extension_type: "signature_algorithms" (RFC5246 section 7.4.1.4.1):
#define SIGALGS (3 + 3 * ENABLE_FEATURE_TLS_SHA1)
			0x00,2 * (1 + SIGALGS), //ext len
			0x00,2 * (0 + SIGALGS), //list len
			//Format: two bytes
			// byte 1: 0:none,1:md5,2:sha1,3:sha224,4:sha256,5:sha384,6:sha512
			// byte 2: 1:rsa,2:dsa,3:ecdsa
			// (note that TLS 1.3 changes this, see RFC8446 section 4.2.3)
#if ENABLE_FEATURE_TLS_SHA1
			0x02,0x01, //sha1 + rsa
			0x02,0x02, //sha1 + dsa
			0x02,0x03, //sha1 + ecdsa
#endif
			0x04,0x01, //sha256 + rsa - kojipkgs.fedoraproject.org wants this
			0x04,0x02, //sha256 + dsa
			0x04,0x03, //sha256 + ecdsa
// GNU Wget 1.18 to cdn.kernel.org sends these extensions:
// 0055
//   0005 0005 0100000000 - status_request
//   0000 0013 0011 00 000e 63646e 2e 6b65726e656c 2e 6f7267 - server_name
//   ff01 0001 00 - renegotiation_info
//   0023 0000 - session_ticket
//   000a 0008 0006001700180019 - supported_groups
//   000b 0002 0100 - ec_point_formats
//   000d 0016 0014 0401 0403 0501 0503 0601 0603 0301 0303 0201 0203 - signature_algorithms
// wolfssl library sends this option, RFC 7627 (closes a security weakness, some servers may require it. TODO?):
//   0017 0000 - extended master secret
	};
	struct client_hello *record;
	uint8_t *ptr;
	int len;
	int ext_len;
	int sni_len = sni ? strnlen(sni, 127 - 5) : 0;

	ext_len = 0;
	ext_len += sizeof(extensions);
	if (sni_len)
		ext_len += 9 + sni_len;

	/* +2 is for "len of all extensions" 2-byte field */
	len = sizeof(*record) + 2 + ext_len;
	record = tls_get_zeroed_outbuf(tls, len);

	fill_handshake_record_hdr(record, HANDSHAKE_CLIENT_HELLO, len);
	record->proto_maj = TLS_MAJ;	/* the "requested" version of the protocol, */
	record->proto_min = TLS_MIN;	/* can be higher than one in record headers */
	tls_get_random(record->rand32, sizeof(record->rand32));
	if (TLS_DEBUG_FIXED_SECRETS)
		memset(record->rand32, 0x11, sizeof(record->rand32));
	/* record->session_id_len = 0; - already is */

	BUILD_BUG_ON(sizeof(ciphers) != 2 * (1 + 1 + NUM_CIPHERS + 1));
	memcpy(&record->cipherid_len16_hi, ciphers, sizeof(ciphers));

	ptr = (void*)(record + 1);
	*ptr++ = ext_len >> 8;
	*ptr++ = ext_len;
	if (sni_len) {
		//ptr[0] = 0;             //
		//ptr[1] = 0;             //extension_type
		//ptr[2] = 0;         //
		ptr[3] = sni_len + 5; //list len
		//ptr[4] = 0;             //
		ptr[5] = sni_len + 3;     //len of 1st SNI
		//ptr[6] = 0;         //name type
		//ptr[7] = 0;             //
		ptr[8] = sni_len;         //name len
		ptr = mempcpy(&ptr[9], sni, sni_len);
	}
	memcpy(ptr, extensions, sizeof(extensions));

	tls->hsd = xzalloc(sizeof(*tls->hsd));
	/* HANDSHAKE HASH: ^^^ + len if need to save saved_client_hello */
	memcpy(tls->hsd->client_and_server_rand32, record->rand32, sizeof(record->rand32));
/* HANDSHAKE HASH:
	tls->hsd->saved_client_hello_size = len;
	memcpy(tls->hsd->saved_client_hello, record, len);
 */
	dbg(">> CLIENT_HELLO\n");
	/* Can hash immediately only if we know which MAC hash to use.
	 * So far we do know: it's sha256:
	 */
	sha256_begin(&tls->hsd->handshake_hash_ctx);
	xwrite_and_update_handshake_hash(tls, len);
	/* if this would become infeasible: save tls->hsd->saved_client_hello,
	 * use "xwrite_handshake_record(tls, len)" here,
	 * and hash saved_client_hello later.
	 */
}

static void get_server_hello(tls_state_t *tls)
{
	struct server_hello {
		struct record_hdr xhdr;
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32]; /* first 4 bytes are unix time in BE format */
		uint8_t session_id_len;
		uint8_t session_id[32];
		uint8_t cipherid_hi, cipherid_lo;
		uint8_t comprtype;
		/* extensions may follow, but only those which client offered in its Hello */
	};

	struct server_hello *hp;
	uint8_t *cipherid;
	uint8_t cipherid1;
	int len, len24;

	len = tls_xread_handshake_block(tls, 74 - 32);

	hp = (void*)tls->inbuf;
	// 74 bytes:
	// 02  000046 03|03   58|78|cf|c1 50|a5|49|ee|7e|29|48|71|fe|97|fa|e8|2d|19|87|72|90|84|9d|37|a3|f0|cb|6f|5f|e3|3c|2f |20  |d8|1a|78|96|52|d6|91|01|24|b3|d6|5b|b7|d0|6c|b3|e1|78|4e|3c|95|de|74|a0|ba|eb|a7|3a|ff|bd|a2|bf |00|9c |00|
	//SvHl len=70 maj.min unixtime^^^ 28randbytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^ slen sid32bytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cipSel comprSel
	if (hp->type != HANDSHAKE_SERVER_HELLO
	 || hp->len24_hi  != 0
	 || hp->len24_mid != 0
	 /* hp->len24_lo checked later */
	 || hp->proto_maj != TLS_MAJ
	 || hp->proto_min != TLS_MIN
	) {
		bad_record_die(tls, "'server hello'", len);
	}

	cipherid = &hp->cipherid_hi;
	len24 = hp->len24_lo;
	if (hp->session_id_len != 32) {
		if (hp->session_id_len != 0)
			bad_record_die(tls, "'server hello'", len);

		// session_id_len == 0: no session id
		// "The server
		// may return an empty session_id to indicate that the session will
		// not be cached and therefore cannot be resumed."
		cipherid -= 32;
		len24 += 32; /* what len would be if session id would be present */
	}

	if (len24 < 70)
		bad_record_die(tls, "'server hello'", len);
	dbg("<< SERVER_HELLO\n");

	memcpy(tls->hsd->client_and_server_rand32 + 32, hp->rand32, sizeof(hp->rand32));

	/* Set up encryption params based on selected cipher */
#if 0
		0xC0,0x09, // 1 TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA - ok: wget https://is.gd/
		0xC0,0x0A, // 2 TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA - ok: wget https://is.gd/
		0xC0,0x13, // 3 TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA - ok: openssl s_server ... -cipher ECDHE-RSA-AES128-SHA
		0xC0,0x14, // 4 TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA - ok: openssl s_server ... -cipher ECDHE-RSA-AES256-SHA (might fail with older openssl)
	//	0xC0,0x18, //   TLS_ECDH_anon_WITH_AES_128_CBC_SHA
	//	0xC0,0x19, //   TLS_ECDH_anon_WITH_AES_256_CBC_SHA
		0xC0,0x23, // 5 TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 - ok: wget https://is.gd/
	//	0xC0,0x24, //   TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 - can't do SHA384 yet
		0xC0,0x27, // 6 TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 - ok: openssl s_server ... -cipher ECDHE-RSA-AES128-SHA256
	//	0xC0,0x28, //   TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384 - can't do SHA384 yet
		0xC0,0x2B, // 7 TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 - ok: wget https://is.gd/
	//	0xC0,0x2C, //   TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 - wget https://is.gd/: "TLS error from peer (alert code 20): bad MAC"
		0xC0,0x2F, // 8 TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 - ok: openssl s_server ... -cipher ECDHE-RSA-AES128-GCM-SHA256
	//	0xC0,0x30, //   TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 - openssl s_server ... -cipher ECDHE-RSA-AES256-GCM-SHA384: "decryption failed or bad record mac"
	//possibly these too:
	//	0xC0,0x35, //   TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA
	//	0xC0,0x36, //   TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA
	//	0xC0,0x37, //   TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
	//	0xC0,0x38, //   TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384 - can't do SHA384 yet
		0x00,0x2F, // 9 TLS_RSA_WITH_AES_128_CBC_SHA - ok: openssl s_server ... -cipher AES128-SHA
		0x00,0x35, //10 TLS_RSA_WITH_AES_256_CBC_SHA - ok: openssl s_server ... -cipher AES256-SHA
		0x00,0x3C, //11 TLS_RSA_WITH_AES_128_CBC_SHA256 - ok: openssl s_server ... -cipher AES128-SHA256
		0x00,0x3D, //12 TLS_RSA_WITH_AES_256_CBC_SHA256 - ok: openssl s_server ... -cipher AES256-SHA256
		0x00,0x9C, //13 TLS_RSA_WITH_AES_128_GCM_SHA256 - ok: openssl s_server ... -cipher AES128-GCM-SHA256
	//	0x00,0x9D, //   TLS_RSA_WITH_AES_256_GCM_SHA384 - openssl s_server ... -cipher AES256-GCM-SHA384: "decryption failed or bad record mac"
		0x00,0x3B, //   TLS_RSA_WITH_NULL_SHA256
#endif
	cipherid1 = cipherid[1];
	tls->cipher_id = 0x100 * cipherid[0] + cipherid1;
	tls->key_size = AES256_KEYSIZE;
	tls->MAC_size = SHA256_OUTSIZE;
	/*tls->IV_size = 0; - already is */
	if (cipherid[0] == 0xC0) {
		/* All C0xx are ECDHE */
		tls->flags |= NEED_EC_KEY;
		if (cipherid1 & 1) {
			/* Odd numbered C0xx use AES128 (even ones use AES256) */
			tls->key_size = AES128_KEYSIZE;
		}
		if (ENABLE_FEATURE_TLS_SHA1 && cipherid1 <= 0x19) {
			tls->MAC_size = SHA1_OUTSIZE;
		} else
		if (cipherid1 >= 0x2B && cipherid1 <= 0x30) {
			/* C02B,2C,2F,30 are AES-GCM */
			tls->flags |= ENCRYPTION_AESGCM;
			tls->MAC_size = 0;
			tls->IV_size = 4;
		}
	} else {
		/* All 00xx are RSA */
		if ((ENABLE_FEATURE_TLS_SHA1 && cipherid1 == 0x2F)
		 || cipherid1 == 0x3C
		 || cipherid1 == 0x9C
		) {
			tls->key_size = AES128_KEYSIZE;
		}
		if (ENABLE_FEATURE_TLS_SHA1 && cipherid1 <= 0x35) {
			tls->MAC_size = SHA1_OUTSIZE;
		} else
		if (cipherid1 == 0x9C /*|| cipherid1 == 0x9D*/) {
			/* 009C,9D are AES-GCM */
			tls->flags |= ENCRYPTION_AESGCM;
			tls->MAC_size = 0;
			tls->IV_size = 4;
		}
	}
	dbg("server chose cipher %04x\n", tls->cipher_id);
	dbg("key_size:%u MAC_size:%u IV_size:%u\n", tls->key_size, tls->MAC_size, tls->IV_size);

	/* Handshake hash eventually destined to FINISHED record
	 * is sha256 regardless of cipher
	 * (at least for all ciphers defined by RFC5246).
	 * It's not sha1 for AES_128_CBC_SHA - only MAC is sha1, not this hash.
	 */
/* HANDSHAKE HASH:
	sha256_begin(&tls->hsd->handshake_hash_ctx);
	hash_handshake(tls, ">> client hello hash:%s",
		tls->hsd->saved_client_hello, tls->hsd->saved_client_hello_size
	);
	hash_handshake(tls, "<< server hello hash:%s",
		tls->inbuf + RECHDR_LEN, len
	);
 */
}

static void get_server_cert(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	uint8_t *certbuf;
	int len, len1;

	len = tls_xread_handshake_block(tls, 10);

	xhdr = (void*)tls->inbuf;
	certbuf = (void*)(xhdr + 1);
	if (certbuf[0] != HANDSHAKE_CERTIFICATE)
		bad_record_die(tls, "certificate", len);
	dbg("<< CERTIFICATE\n");
	// 4392 bytes:
	// 0b  00|11|24 00|11|21 00|05|b0 30|82|05|ac|30|82|04|94|a0|03|02|01|02|02|11|00|9f|85|bf|66|4b|0c|dd|af|ca|50|86|79|50|1b|2b|e4|30|0d...
	//Cert len=4388 ChainLen CertLen^ DER encoded X509 starts here. openssl x509 -in FILE -inform DER -noout -text
	len1 = get24be(certbuf + 1);
	if (len1 > len - 4) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 4);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 7);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;

	if (len)
		find_key_in_der_cert(tls, certbuf + 10, len);
}

/* On input, len is known to be >= 4.
 * The record is known to be SERVER_KEY_EXCHANGE.
 */
static void process_server_key(tls_state_t *tls, int len)
{
	struct record_hdr *xhdr;
	uint8_t *keybuf;
	int len1;
	uint32_t t32;

	xhdr = (void*)tls->inbuf;
	keybuf = (void*)(xhdr + 1);
//seen from is.gd: it selects curve_x25519:
//  0c 00006e //SERVER_KEY_EXCHANGE, len
//    03 //curve_type: named curve
//    001d //curve_x25519
//server-chosen EC point, and then signed_params
//      (RFC 8422: "A hash of the params, with the signature
//      appropriate to that hash applied.  The private key corresponding
//      to the certified public key in the server's Certificate message is
//      used for signing.")
//follow. Format unclear/guessed:
//    20 //eccPubKeyLen
//      25511923d73b70dd2f60e66ba2f3fda31a9c25170963c7a3a972e481dbb2835d //eccPubKey (32bytes)
//    0203 //hashSigAlg: 2:SHA1 (4:SHA256 5:SHA384 6:SHA512), 3:ECDSA (1:RSA)
//    0046 //len (16bit)
//      30 44 //SEQ, len
//        02 20 //INTEGER, len
//          2e18e7c2a9badd0a70cd3059a6ab114539b9f5163568911147386cd77ed7c412 //32bytes
//this item ^^^^^ is sometimes 33 bytes (with all container sizes also +1)
//        02 20 //INTEGER, len
//          64523d6216cb94c43c9b20e377d8c52c55be6703fd6730a155930c705eaf3af6 //32bytes
//same about this item ^^^^^

//seen from ftp.openbsd.org
//(which only accepts ECDHE-RSA-AESnnn-GCM-SHAnnn and ECDHE-RSA-CHACHA20-POLY1305 ciphers):
//  0c 000228 //SERVER_KEY_EXCHANGE, len
//    03 //curve_type: named curve
//    001d //curve_x25519
//    20 //eccPubKeyLen
//      eef7a15c43b71a4c7eaa48a39369399cc4332e569ec90a83274cc92596705c1a //eccPubKey
//    0401 //hashSigAlg: 4:SHA256, 1:RSA
//    0200 //len
//      //0x200 bytes follow

	/* Get and verify length */
	len1 = get24be(keybuf + 1);
	if (len1 > len - 4) tls_error_die(tls);
	len = len1;
	if (len < (1+2+1+32)) tls_error_die(tls);
	keybuf += 4;

#if BB_BIG_ENDIAN
# define _0x03001741 0x03001741
# define _0x03001d20 0x03001d20
#else
# define _0x03001741 0x41170003
# define _0x03001d20 0x201d0003
#endif
	move_from_unaligned32(t32, keybuf);
	keybuf += 4;
	switch (t32) {
	case _0x03001d20: //curve_x25519
		dbg("got x25519 eccPubKey\n");
		tls->flags |= GOT_EC_CURVE_X25519;
		memcpy(tls->hsd->ecc_pub_key32, keybuf, 32);
		break;
	case _0x03001741: //curve_secp256r1 (aka P256)
		dbg("got P256 eccPubKey\n");
		/* P256 point can be transmitted odd- or even-compressed
		 * (first byte is 3 or 2) or uncompressed (4).
		 */
		if (*keybuf++ != 4)
			bb_simple_error_msg_and_die("compressed EC points not supported");
		memcpy(tls->hsd->ecc_pub_key32, keybuf, 2 * 32);
		break;
	default:
		bb_error_msg_and_die("elliptic curve is not x25519 or P256: 0x%08x", t32);
	}

	tls->flags |= GOT_EC_KEY;
}

static void send_empty_client_cert(tls_state_t *tls)
{
	struct client_empty_cert {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t cert_chain_len24_hi, cert_chain_len24_mid, cert_chain_len24_lo;
	};
	struct client_empty_cert *record;

	record = tls_get_zeroed_outbuf(tls, sizeof(*record));
	//fill_handshake_record_hdr(record, HANDSHAKE_CERTIFICATE, sizeof(*record));
	//record->cert_chain_len24_hi = 0;
	//record->cert_chain_len24_mid = 0;
	//record->cert_chain_len24_lo = 0;
	// same as above:
	record->type = HANDSHAKE_CERTIFICATE;
	record->len24_lo = 3;

	dbg(">> CERTIFICATE\n");
	xwrite_and_update_handshake_hash(tls, sizeof(*record));
}

static void send_client_key_exchange(tls_state_t *tls)
{
	struct client_key_exchange {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t key[2 + 4 * 1024]; // size??
	};
//FIXME: better size estimate
	struct client_key_exchange *record = tls_get_zeroed_outbuf(tls, sizeof(*record));
	uint8_t premaster[RSA_PREMASTER_SIZE > EC_CURVE_KEYSIZE ? RSA_PREMASTER_SIZE : EC_CURVE_KEYSIZE];
	int premaster_size;
	int len;

	if (!(tls->flags & NEED_EC_KEY)) {
		/* RSA */
		if (!(tls->flags & GOT_CERT_RSA_KEY_ALG))
			bb_simple_error_msg_and_die("server cert is not RSA");

		tls_get_random(premaster, RSA_PREMASTER_SIZE);
		if (TLS_DEBUG_FIXED_SECRETS)
			memset(premaster, 0x44, RSA_PREMASTER_SIZE);
		// RFC 5246
		// "Note: The version number in the PreMasterSecret is the version
		// offered by the client in the ClientHello.client_version, not the
		// version negotiated for the connection."
		premaster[0] = TLS_MAJ;
		premaster[1] = TLS_MIN;
		dump_hex("premaster:%s\n", premaster, sizeof(premaster));
		len = psRsaEncryptPub(/*pool:*/ NULL,
			/* psRsaKey_t* */ &tls->hsd->server_rsa_pub_key,
			premaster, /*inlen:*/ RSA_PREMASTER_SIZE,
			record->key + 2, sizeof(record->key) - 2,
			data_param_ignored
		);
		/* keylen16 exists for RSA (in TLS, not in SSL), but not for some other key types */
		record->key[0] = len >> 8;
		record->key[1] = len & 0xff;
		len += 2;
		premaster_size = RSA_PREMASTER_SIZE;
	} else {
		/* ECDHE */
		if (!(tls->flags & GOT_EC_KEY))
			bb_simple_error_msg_and_die("server did not provide EC key");

		if (tls->flags & GOT_EC_CURVE_X25519) {
			/* ECDHE, curve x25519 */
			dbg("computing x25519_premaster\n");
			curve_x25519_compute_pubkey_and_premaster(
					record->key + 1, premaster,
					/*point:*/ tls->hsd->ecc_pub_key32
			);
			len = CURVE25519_KEYSIZE;
			//record->key[0] = len;
			//len++;
			//premaster_size = CURVE25519_KEYSIZE;
		} else {
			/* ECDHE, curve P256 */
			dbg("computing P256_premaster\n");
			curve_P256_compute_pubkey_and_premaster(
					record->key + 2, premaster,
					/*point:*/ tls->hsd->ecc_pub_key32
			);
			record->key[1] = 4; /* "uncompressed point" */
			len = 1 + P256_KEYSIZE * 2;
		}
		record->key[0] = len;
		len++;
		premaster_size = P256_KEYSIZE; // = CURVE25519_KEYSIZE = 32
	}

	record->type = HANDSHAKE_CLIENT_KEY_EXCHANGE;
	/* record->len24_hi = 0; - already is */
	record->len24_mid = len >> 8;
	record->len24_lo  = len & 0xff;
	len += 4;

	dbg(">> CLIENT_KEY_EXCHANGE\n");
	xwrite_and_update_handshake_hash(tls, len);

	// RFC 5246
	// For all key exchange methods, the same algorithm is used to convert
	// the pre_master_secret into the master_secret.  The pre_master_secret
	// should be deleted from memory once the master_secret has been
	// computed.
	//      master_secret = PRF(pre_master_secret, "master secret",
	//                          ClientHello.random + ServerHello.random)
	//                          [0..47];
	// The master secret is always exactly 48 bytes in length.  The length
	// of the premaster secret will vary depending on key exchange method.
	prf_hmac_sha256(/*tls,*/
		tls->hsd->master_secret, sizeof(tls->hsd->master_secret),
		premaster, premaster_size,
		"master secret",
		tls->hsd->client_and_server_rand32, sizeof(tls->hsd->client_and_server_rand32)
	);
	dump_hex("master secret:%s\n", tls->hsd->master_secret, sizeof(tls->hsd->master_secret));

	// RFC 5246
	// 6.3.  Key Calculation
	//
	// The Record Protocol requires an algorithm to generate keys required
	// by the current connection state (see Appendix A.6) from the security
	// parameters provided by the handshake protocol.
	//
	// The master secret is expanded into a sequence of secure bytes, which
	// is then split to a client write MAC key, a server write MAC key, a
	// client write encryption key, and a server write encryption key.  Each
	// of these is generated from the byte sequence in that order.  Unused
	// values are empty.  Some AEAD ciphers may additionally require a
	// client write IV and a server write IV (see Section 6.2.3.3).
	//
	// When keys and MAC keys are generated, the master secret is used as an
	// entropy source.
	//
	// To generate the key material, compute
	//
	//    key_block = PRF(SecurityParameters.master_secret,
	//                    "key expansion",
	//                    SecurityParameters.server_random +
	//                    SecurityParameters.client_random);
	//
	// until enough output has been generated.  Then, the key_block is
	// partitioned as follows:
	//
	//    client_write_MAC_key[SecurityParameters.mac_key_length]
	//    server_write_MAC_key[SecurityParameters.mac_key_length]
	//    client_write_key[SecurityParameters.enc_key_length]
	//    server_write_key[SecurityParameters.enc_key_length]
	//    client_write_IV[SecurityParameters.fixed_iv_length]
	//    server_write_IV[SecurityParameters.fixed_iv_length]
	{
		uint8_t tmp64[64];

		/* make "server_rand32 + client_rand32" */
		memcpy(&tmp64[0] , &tls->hsd->client_and_server_rand32[32], 32);
		memcpy(&tmp64[32], &tls->hsd->client_and_server_rand32[0] , 32);

		prf_hmac_sha256(/*tls,*/
			tls->client_write_MAC_key, 2 * (tls->MAC_size + tls->key_size + tls->IV_size),
			// also fills:
			// server_write_MAC_key[]
			// client_write_key[]
			// server_write_key[]
			// client_write_IV[]
			// server_write_IV[]
			tls->hsd->master_secret, sizeof(tls->hsd->master_secret),
			"key expansion",
			tmp64, 64
		);
		tls->client_write_key = tls->client_write_MAC_key + (2 * tls->MAC_size);
		tls->server_write_key = tls->client_write_key + tls->key_size;
		tls->client_write_IV = tls->server_write_key + tls->key_size;
		tls->server_write_IV = tls->client_write_IV + tls->IV_size;
		dump_hex("client_write_MAC_key:%s\n",
			tls->client_write_MAC_key, tls->MAC_size
		);
		dump_hex("client_write_key:%s\n",
			tls->client_write_key, tls->key_size
		);
		dump_hex("client_write_IV:%s\n",
			tls->client_write_IV, tls->IV_size
		);

		aes_setkey(&tls->aes_decrypt, tls->server_write_key, tls->key_size);
		aes_setkey(&tls->aes_encrypt, tls->client_write_key, tls->key_size);
		{
			uint8_t iv[AES_BLOCK_SIZE];
			memset(iv, 0, AES_BLOCK_SIZE);
			aes_encrypt_one_block(&tls->aes_encrypt, iv, tls->H);
		}
	}
}

static const uint8_t rec_CHANGE_CIPHER_SPEC[] ALIGN1 = {
	RECORD_TYPE_CHANGE_CIPHER_SPEC, TLS_MAJ, TLS_MIN, 00, 01,
	01
};

static void send_change_cipher_spec(tls_state_t *tls)
{
	dbg(">> CHANGE_CIPHER_SPEC\n");
	xwrite(tls->ofd, rec_CHANGE_CIPHER_SPEC, sizeof(rec_CHANGE_CIPHER_SPEC));
}

// 7.4.9.  Finished
// A Finished message is always sent immediately after a change
// cipher spec message to verify that the key exchange and
// authentication processes were successful.  It is essential that a
// change cipher spec message be received between the other handshake
// messages and the Finished message.
//...
// The Finished message is the first one protected with the just
// negotiated algorithms, keys, and secrets.  Recipients of Finished
// messages MUST verify that the contents are correct.  Once a side
// has sent its Finished message and received and validated the
// Finished message from its peer, it may begin to send and receive
// application data over the connection.
//...
// struct {
//     opaque verify_data[verify_data_length];
// } Finished;
//
// verify_data
//    PRF(master_secret, finished_label, Hash(handshake_messages))
//       [0..verify_data_length-1];
//
// finished_label
//    For Finished messages sent by the client, the string
//    "client finished".  For Finished messages sent by the server,
//    the string "server finished".
//
// Hash denotes a Hash of the handshake messages.  For the PRF
// defined in Section 5, the Hash MUST be the Hash used as the basis
// for the PRF.  Any cipher suite which defines a different PRF MUST
// also define the Hash to use in the Finished computation.
//
// In previous versions of TLS, the verify_data was always 12 octets
// long.  In the current version of TLS, it depends on the cipher
// suite.  Any cipher suite which does not explicitly specify
// verify_data_length has a verify_data_length equal to 12.  This
// includes all existing cipher suites.
static void send_client_finished(tls_state_t *tls)
{
	struct finished {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t prf_result[12];
	};
	struct finished *record = tls_get_outbuf(tls, sizeof(*record));
	uint8_t handshake_hash[TLS_MAX_MAC_SIZE];
	unsigned len;

	fill_handshake_record_hdr(record, HANDSHAKE_FINISHED, sizeof(*record));

	len = sha_end(&tls->hsd->handshake_hash_ctx, handshake_hash);

	prf_hmac_sha256(/*tls,*/
		record->prf_result, sizeof(record->prf_result),
		tls->hsd->master_secret, sizeof(tls->hsd->master_secret),
		"client finished",
		handshake_hash, len
	);
	dump_hex("from secret: %s\n", tls->hsd->master_secret, sizeof(tls->hsd->master_secret));
	dump_hex("from labelSeed: %s", "client finished", sizeof("client finished")-1);
	dump_hex("%s\n", handshake_hash, sizeof(handshake_hash));
	dump_hex("=> digest: %s\n", record->prf_result, sizeof(record->prf_result));

	dbg(">> FINISHED\n");
	xwrite_encrypted(tls, sizeof(*record), RECORD_TYPE_HANDSHAKE);
}

void FAST_FUNC tls_handshake(tls_state_t *tls, const char *sni)
{
	// Client              RFC 5246                Server
	// (*) - optional messages, not always sent
	//
	// ClientHello          ------->
	//                                        ServerHello
	//                                       Certificate*
	//                                 ServerKeyExchange*
	//                                CertificateRequest*
	//                      <-------      ServerHelloDone
	// Certificate*
	// ClientKeyExchange
	// CertificateVerify*
	// [ChangeCipherSpec]
	// Finished             ------->
	//                                 [ChangeCipherSpec]
	//                      <-------             Finished
	// Application Data     <------>     Application Data
	int len;
	int got_cert_req;

	send_client_hello_and_alloc_hsd(tls, sni);
	get_server_hello(tls);

	// RFC 5246
	// The server MUST send a Certificate message whenever the agreed-
	// upon key exchange method uses certificates for authentication
	// (this includes all key exchange methods defined in this document
	// except DH_anon).  This message will always immediately follow the
	// ServerHello message.
	//
	// IOW: in practice, Certificate *always* follows.
	// (for example, kernel.org does not even accept DH_anon cipher id)
	get_server_cert(tls);

	len = tls_xread_handshake_block(tls, 4);
	if (tls->inbuf[RECHDR_LEN] == HANDSHAKE_SERVER_KEY_EXCHANGE) {
		// 459 bytes:
		// 0c   00|01|c7 03|00|17|41|04|87|94|2e|2f|68|d0|c9|f4|97|a8|2d|ef|ed|67|ea|c6|f3|b3|56|47|5d|27|b6|bd|ee|70|25|30|5e|b0|8e|f6|21|5a...
		//SvKey len=455^
		// with TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA: 461 bytes:
		// 0c   00|01|c9 03|00|17|41|04|cd|9b|b4|29|1f|f6|b0|c2|84|82|7f|29|6a|47|4e|ec|87|0b|c1|9c|69|e1|f8|c6|d0|53|e9|27|90|a5|c8|02|15|75...
		//
		// RFC 8422 5.4. Server Key Exchange
		// This message is sent when using the ECDHE_ECDSA, ECDHE_RSA, and
		// ECDH_anon key exchange algorithms.
		// This message is used to convey the server's ephemeral ECDH public key
		// (and the corresponding elliptic curve domain parameters) to the
		// client.
		dbg("<< SERVER_KEY_EXCHANGE len:%u\n", len);
		dump_raw_in("<< %s\n", tls->inbuf, RECHDR_LEN + len);
		if (tls->flags & NEED_EC_KEY)
			process_server_key(tls, len);

		// read next handshake block
		len = tls_xread_handshake_block(tls, 4);
	}

	got_cert_req = (tls->inbuf[RECHDR_LEN] == HANDSHAKE_CERTIFICATE_REQUEST);
	if (got_cert_req) {
		dbg("<< CERTIFICATE_REQUEST\n");
		// RFC 5246: "If no suitable certificate is available,
		// the client MUST send a certificate message containing no
		// certificates.  That is, the certificate_list structure has a
		// length of zero. ...
		// Client certificates are sent using the Certificate structure
		// defined in Section 7.4.2."
		// (i.e. the same format as server certs)

		/*send_empty_client_cert(tls); - WRONG (breaks handshake hash calc) */
		/* need to hash _all_ server replies first, up to ServerHelloDone */
		len = tls_xread_handshake_block(tls, 4);
	}

	if (tls->inbuf[RECHDR_LEN] != HANDSHAKE_SERVER_HELLO_DONE) {
		bad_record_die(tls, "'server hello done'", len);
	}
	// 0e 000000 (len:0)
	dbg("<< SERVER_HELLO_DONE\n");

	if (got_cert_req)
		send_empty_client_cert(tls);

	send_client_key_exchange(tls);

	send_change_cipher_spec(tls);
	/* from now on we should send encrypted */
	/* tls->write_seq64_be = 0; - already is */
	tls->flags |= ENCRYPT_ON_WRITE;

	send_client_finished(tls);

	/* Get CHANGE_CIPHER_SPEC */
	len = tls_xread_record(tls, "switch to encrypted traffic");
	if (len != 1 || memcmp(tls->inbuf, rec_CHANGE_CIPHER_SPEC, 6) != 0)
		bad_record_die(tls, "switch to encrypted traffic", len);
	dbg("<< CHANGE_CIPHER_SPEC\n");

	if (ALLOW_RSA_NULL_SHA256
	 && tls->cipher_id == TLS_RSA_WITH_NULL_SHA256
	) {
		tls->min_encrypted_len_on_read = tls->MAC_size;
	} else
	if (!(tls->flags & ENCRYPTION_AESGCM)) {
		unsigned mac_blocks = (unsigned)(TLS_MAC_SIZE(tls) + AES_BLOCK_SIZE-1) / AES_BLOCK_SIZE;
		/* all incoming packets now should be encrypted and have
		 * at least IV + (MAC padded to blocksize):
		 */
		tls->min_encrypted_len_on_read = AES_BLOCK_SIZE + (mac_blocks * AES_BLOCK_SIZE);
	} else {
		tls->min_encrypted_len_on_read = 8 + AES_BLOCK_SIZE;
	}
	dbg("min_encrypted_len_on_read: %u\n", tls->min_encrypted_len_on_read);

	/* Get (encrypted) FINISHED from the server */
	len = tls_xread_record(tls, "'server finished'");
	if (len < 4 || tls->inbuf[RECHDR_LEN] != HANDSHAKE_FINISHED)
		bad_record_die(tls, "'server finished'", len);
	dbg("<< FINISHED\n");
	/* application data can be sent/received */

	/* free handshake data */
	psRsaKey_clear(&tls->hsd->server_rsa_pub_key);
//	if (PARANOIA)
//		memset(tls->hsd, 0, tls->hsd->hsd_size);
	free(tls->hsd);
	tls->hsd = NULL;
}

static void tls_xwrite(tls_state_t *tls, int len)
{
	dbg(">> DATA\n");
	xwrite_encrypted(tls, len, RECORD_TYPE_APPLICATION_DATA);
}

// To run a test server using openssl:
// openssl req -x509 -newkey rsa:$((4096/4*3)) -keyout key.pem -out server.pem -nodes -days 99999 -subj '/CN=localhost'
// openssl s_server -key key.pem -cert server.pem -debug -tls1_2
//
// Unencryped SHA256 example:
// openssl req -x509 -newkey rsa:$((4096/4*3)) -keyout key.pem -out server.pem -nodes -days 99999 -subj '/CN=localhost'
// openssl s_server -key key.pem -cert server.pem -debug -tls1_2 -cipher NULL
// openssl s_client -connect 127.0.0.1:4433 -debug -tls1_2 -cipher NULL-SHA256

void FAST_FUNC tls_run_copy_loop(tls_state_t *tls, unsigned flags)
{
	int inbuf_size;
	const int INBUF_STEP = 4 * 1024;
	struct pollfd pfds[2];

#if 0
// Debug aid for comparing P256 implementations.
// Enable this, set SP_DEBUG and FIXED_SECRET to 1,
// and add
//	tls_run_copy_loop(NULL, 0);
// e.g. at the very beginning of wget_main()
//
{
	uint8_t ecc_pub_key32[2 * 32];
	uint8_t pubkey2x32[2 * 32];
	uint8_t premaster32[32];

//Fixed input key:
//	memset(ecc_pub_key32, 0xee, sizeof(ecc_pub_key32));
//Fixed 000000000000000000000000000000000000ab000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
//	memset(ecc_pub_key32, 0x00, sizeof(ecc_pub_key32));
//	ecc_pub_key32[18] = 0xab;
//Random key:
//	tls_get_random(ecc_pub_key32, sizeof(ecc_pub_key32));
//Biased random (almost all zeros or almost all ones):
	srand(time(NULL) ^ getpid());
	if (rand() & 1)
		memset(ecc_pub_key32, 0x00, sizeof(ecc_pub_key32));
	else
		memset(ecc_pub_key32, 0xff, sizeof(ecc_pub_key32));
	ecc_pub_key32[rand() & 0x3f] = rand();

	xmove_fd(xopen("p256.OLD", O_WRONLY | O_CREAT | O_TRUNC), 2);
	curve_P256_compute_pubkey_and_premaster(
			pubkey2x32, premaster32,
			/*point:*/ ecc_pub_key32
	);
	xmove_fd(xopen("p256.NEW", O_WRONLY | O_CREAT | O_TRUNC), 2);
	curve_P256_compute_pubkey_and_premaster_NEW(
			pubkey2x32, premaster32,
			/*point:*/ ecc_pub_key32
	);
	exit(1);
}
#endif

	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN;
	pfds[1].fd = tls->ifd;
	pfds[1].events = POLLIN;

	inbuf_size = INBUF_STEP;
	for (;;) {
		int nread;

		if (safe_poll(pfds, 2, -1) < 0)
			bb_simple_perror_msg_and_die("poll");

		if (pfds[0].revents) {
			void *buf;

			dbg("STDIN HAS DATA\n");
			buf = tls_get_outbuf(tls, inbuf_size);
			nread = safe_read(STDIN_FILENO, buf, inbuf_size);
			if (nread < 1) {
				/* We'd want to do this: */
				/* Close outgoing half-connection so they get EOF,
				 * but leave incoming alone so we can see response
				 */
				//shutdown(tls->ofd, SHUT_WR);
				/* But TLS has no way to encode this,
				 * doubt it's ok to do it "raw"
				 */
				pfds[0].fd = -1;
				tls_free_outbuf(tls); /* mem usage optimization */
				if (flags & TLSLOOP_EXIT_ON_LOCAL_EOF)
					break;
			} else {
				if (nread == inbuf_size) {
					/* TLS has per record overhead, if input comes fast,
					 * read, encrypt and send bigger chunks
					 */
					inbuf_size += INBUF_STEP;
					if (inbuf_size > TLS_MAX_OUTBUF)
						inbuf_size = TLS_MAX_OUTBUF;
				}
				tls_xwrite(tls, nread);
			}
		}
		if (pfds[1].revents) {
			dbg("NETWORK HAS DATA\n");
 read_record:
			nread = tls_xread_record(tls, "encrypted data");
			if (nread < 1) {
				/* TLS protocol has no real concept of one-sided shutdowns:
				 * if we get "TLS EOF" from the peer, writes will fail too
				 */
				//pfds[1].fd = -1;
				//close(STDOUT_FILENO);
				//tls_free_inbuf(tls); /* mem usage optimization */
				//continue;
				break;
			}
			if (tls->inbuf[0] != RECORD_TYPE_APPLICATION_DATA)
				bad_record_die(tls, "encrypted data", nread);
			xwrite(STDOUT_FILENO, tls->inbuf + RECHDR_LEN, nread);
			/* We may already have a complete next record buffered,
			 * can process it without network reads (and possible blocking)
			 */
			if (tls_has_buffered_record(tls))
				goto read_record;
		}
	}
}
