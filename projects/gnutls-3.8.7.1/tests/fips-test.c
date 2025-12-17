#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include <stdlib.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>

/* This does check the FIPS140 support.
 */

void _gnutls_lib_simulate_error(void);

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "<%d>| %s", level, str);
}

static uint8_t key16[16];
static uint8_t iv16[16];
static uint8_t key_data[64];
static uint8_t iv_data[16];
static gnutls_fips140_context_t fips_context;

static const gnutls_datum_t data = { .data = (unsigned char *)"foo", 3 };

static const uint8_t rsa2342_sha1_sig_data[] = {
	0x9b, 0x3e, 0x15, 0x36, 0xec, 0x9d, 0x51, 0xd7, 0xa2, 0xb1, 0x3a, 0x15,
	0x1a, 0xfe, 0x4e, 0x12, 0x43, 0x3c, 0xa8, 0x58, 0x4c, 0x2a, 0x82, 0xc1,
	0x02, 0x3f, 0xc0, 0x6f, 0xa2, 0x23, 0xba, 0x58, 0x9f, 0xc0, 0xfc, 0x87,
	0x5e, 0xfd, 0x13, 0x32, 0xa6, 0xd9, 0x72, 0x63, 0x04, 0x68, 0xb9, 0x0f,
	0x46, 0x21, 0x3f, 0x7f, 0xe1, 0xa2, 0xb0, 0xfa, 0x66, 0x84, 0xd9, 0x64,
	0x87, 0x40, 0x31, 0x27, 0xec, 0xb3, 0xbb, 0x53, 0xb5, 0x8f, 0xf9, 0x3c,
	0x45, 0x1c, 0xcc, 0x30, 0xf5, 0xab, 0x9e, 0x1b, 0x86, 0x92, 0x6a, 0x58,
	0xeb, 0xa1, 0x87, 0x71, 0x40, 0xfb, 0x9d, 0x8f, 0x2c, 0x82, 0x32, 0xe1,
	0x7f, 0xfc, 0xe9, 0xd1, 0x76, 0xa3, 0x56, 0xdf, 0x38, 0xdb, 0xe2, 0x8a,
	0xd3, 0x7e, 0xb4, 0xe2, 0xc9, 0x6a, 0xb2, 0x02, 0xe8, 0xf6, 0x34, 0xde,
	0x51, 0x36, 0xd7, 0x3a, 0xba, 0x0f, 0x51, 0x3d, 0xb0, 0xe8, 0x8e, 0x58,
	0x72, 0x1c, 0x89, 0xac, 0x68, 0xa5, 0x03, 0xb1, 0xd6, 0x5d, 0x32, 0x2f,
	0x3c, 0x71, 0xcc, 0xc2, 0xd7, 0xf9, 0x51, 0xb1, 0xc8, 0x07, 0x07, 0x63,
	0xe7, 0xa9, 0x9b, 0x9f, 0xdb, 0xc5, 0xb5, 0x68, 0xfd, 0xed, 0x11, 0x0c,
	0xa7, 0xfa, 0x08, 0x59, 0xa8, 0x84, 0xcd, 0x36, 0x6b, 0xa5, 0xfe, 0xf9,
	0xd3, 0xe1, 0x36, 0xaf, 0x71, 0x47, 0x39, 0x1e, 0xb7, 0xbc, 0x06, 0x66,
	0xb8, 0xd7, 0x6d, 0x37, 0x6d, 0x52, 0x85, 0x34, 0x2b, 0x05, 0x62, 0x2e,
	0xbe, 0x6d, 0xa3, 0x76, 0xcd, 0xe0, 0xd6, 0x3e, 0x9d, 0xcf, 0x74, 0xf9,
	0xb4, 0x6b, 0xc0, 0x20, 0xe9, 0xd7, 0x19, 0x2d, 0xe6, 0x8a, 0xfd, 0xa2,
	0xa4, 0x4a, 0xea, 0x01, 0x91, 0xf5, 0xb5, 0x29, 0x7a, 0xda, 0x68, 0xc6,
	0x6c, 0xa0, 0x99, 0x5b, 0x79, 0x18, 0x96, 0xb1, 0xbe, 0x38, 0x74, 0x66,
	0x4b, 0x47, 0x46, 0x89, 0xea, 0x25, 0x2a, 0x9e, 0x3a, 0xdc, 0x49, 0x6b,
	0xba, 0xcb, 0xe4, 0x7a, 0x8f, 0x60, 0x35, 0xf3, 0x9f, 0x9d, 0xeb, 0x9d,
	0xfa, 0x0c, 0xaf, 0x6e, 0x47, 0x65, 0xaf, 0x17, 0x18, 0x56, 0x16, 0xe8,
	0x01, 0xd5, 0x55, 0xdf, 0xca, 0x41, 0x63, 0xd0, 0x48, 0x9b, 0x08, 0xdb,
	0xdd, 0x73, 0x4a, 0xa5,
};

static const gnutls_datum_t rsa2342_sha1_sig = {
	.data = (unsigned char *)rsa2342_sha1_sig_data,
	.size = sizeof(rsa2342_sha1_sig_data),
};

static const uint8_t ecc256_sha1_sig_data[] = {
	0x30, 0x45, 0x02, 0x21, 0x00, 0x9a, 0x28, 0xc9, 0xbf, 0xc8, 0x70, 0x4f,
	0x27, 0x2d, 0xe1, 0x66, 0xc4, 0xa5, 0xc6, 0xf2, 0xdc, 0x33, 0xb9, 0x41,
	0xdf, 0x78, 0x98, 0x8a, 0x22, 0x4d, 0x29, 0x37, 0xa0, 0x0f, 0x6f, 0xd4,
	0xed, 0x02, 0x20, 0x0b, 0x15, 0xca, 0x30, 0x09, 0x2d, 0x55, 0x44, 0xb4,
	0x1d, 0x3f, 0x48, 0x7a, 0xc3, 0xd1, 0x2a, 0xc1, 0x0e, 0x47, 0xfa, 0xe6,
	0xe9, 0x0f, 0x03, 0xe2, 0x01, 0x4e, 0xe4, 0x73, 0x37, 0xa7, 0x90,
};

static const gnutls_datum_t ecc256_sha1_sig = {
	.data = (unsigned char *)ecc256_sha1_sig_data,
	.size = sizeof(ecc256_sha1_sig_data),
};

static void import_keypair(gnutls_privkey_t *privkey, gnutls_pubkey_t *pubkey,
			   const char *filename)
{
	const char *srcdir;
	char path[256];
	gnutls_datum_t tmp;
	gnutls_x509_privkey_t xprivkey;
	int ret;

	ret = gnutls_x509_privkey_init(&xprivkey);
	if (ret < 0) {
		fail("gnutls_x509_privkey_init failed\n");
	}
	srcdir = getenv("srcdir");
	if (!srcdir) {
		srcdir = ".";
	}
	snprintf(path, sizeof(path), "%s/certs/%s", srcdir, filename);
	ret = gnutls_load_file(path, &tmp);
	if (ret < 0) {
		fail("gnutls_load_file failed\n");
	}
	ret = gnutls_x509_privkey_import(xprivkey, &tmp, GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		fail("gnutls_x509_privkey_import failed\n");
	}
	gnutls_free(tmp.data);

	ret = gnutls_privkey_init(privkey);
	if (ret < 0) {
		fail("gnutls_privkey_init failed\n");
	}
	ret = gnutls_privkey_import_x509(*privkey, xprivkey,
					 GNUTLS_PRIVKEY_IMPORT_COPY);
	if (ret < 0) {
		fail("gnutls_privkey_import_x509 failed\n");
	}
	gnutls_x509_privkey_deinit(xprivkey);

	ret = gnutls_pubkey_init(pubkey);
	if (ret < 0) {
		fail("gnutls_pubkey_init failed\n");
	}
	ret = gnutls_pubkey_import_privkey(*pubkey, *privkey,
					   GNUTLS_KEY_DIGITAL_SIGNATURE, 0);
	if (ret < 0) {
		fail("gnutls_pubkey_import_privkey failed\n");
	}
}

static void test_aead_cipher_approved(gnutls_cipher_algorithm_t cipher)
{
	int ret;
	unsigned key_size = gnutls_cipher_get_key_size(cipher);
	gnutls_aead_cipher_hd_t h;
	gnutls_datum_t key = { key_data, key_size };
	gnutls_memset(key_data, 0, key_size);

	FIPS_PUSH_CONTEXT();
	ret = gnutls_aead_cipher_init(&h, cipher, &key);
	if (ret < 0) {
		fail("gnutls_aead_cipher_init failed for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	gnutls_aead_cipher_deinit(h);
	FIPS_POP_CONTEXT(APPROVED);
}

static void test_cipher_approved(gnutls_cipher_algorithm_t cipher)
{
	int ret;
	unsigned key_size = gnutls_cipher_get_key_size(cipher);
	unsigned iv_size = gnutls_cipher_get_iv_size(cipher);
	gnutls_cipher_hd_t h;
	gnutls_datum_t key = { key_data, key_size };
	gnutls_datum_t iv = { iv_data, iv_size };
	gnutls_memset(key_data, 0, key_size);
	gnutls_memset(iv_data, 0, iv_size);

	FIPS_PUSH_CONTEXT();
	ret = gnutls_cipher_init(&h, cipher, &key, &iv);
	if (ret < 0) {
		fail("gnutls_cipher_init failed for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	gnutls_cipher_deinit(h);
	FIPS_POP_CONTEXT(APPROVED);
}

static void test_cipher_allowed(gnutls_cipher_algorithm_t cipher)
{
	int ret;
	unsigned key_size = gnutls_cipher_get_key_size(cipher);
	unsigned iv_size = gnutls_cipher_get_iv_size(cipher);
	gnutls_cipher_hd_t h;
	gnutls_datum_t key = { key_data, key_size };
	gnutls_datum_t iv = { iv_data, iv_size };
	gnutls_memset(key_data, 0, key_size);
	gnutls_memset(iv_data, 0, iv_size);

	FIPS_PUSH_CONTEXT();
	ret = gnutls_cipher_init(&h, cipher, &key, &iv);
	if (ret < 0) {
		fail("gnutls_cipher_init failed for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	gnutls_cipher_deinit(h);
	FIPS_POP_CONTEXT(NOT_APPROVED);
}

static void test_cipher_disallowed(gnutls_cipher_algorithm_t cipher)
{
	int ret;
	unsigned key_size = gnutls_cipher_get_key_size(cipher);
	unsigned iv_size = gnutls_cipher_get_iv_size(cipher);
	gnutls_cipher_hd_t h;
	gnutls_datum_t key = { key_data, key_size };
	gnutls_datum_t iv = { iv_data, iv_size };
	gnutls_memset(key_data, 0, key_size);
	gnutls_memset(iv_data, 0, iv_size);

	FIPS_PUSH_CONTEXT();
	ret = gnutls_cipher_init(&h, cipher, &key, &iv);
	if (ret != GNUTLS_E_UNWANTED_ALGORITHM) {
		if (ret == 0)
			gnutls_cipher_deinit(h);
		fail("gnutls_cipher_init should have failed with "
		     "GNUTLS_E_UNWANTED_ALGORITHM for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	FIPS_POP_CONTEXT(ERROR);
}

static void test_ccm_cipher(gnutls_cipher_algorithm_t cipher, size_t tag_length,
			    bool expect_encryption_fail,
			    gnutls_fips140_operation_state_t expected_state)
{
	int ret;
	unsigned key_size = gnutls_cipher_get_key_size(cipher);
	gnutls_aead_cipher_hd_t h;
	gnutls_datum_t key = { key_data, key_size };
	unsigned char buffer[256];
	size_t length;
	gnutls_memset(key_data, 0, key_size);

	FIPS_PUSH_CONTEXT();
	ret = gnutls_aead_cipher_init(&h, cipher, &key);
	if (ret < 0) {
		fail("gnutls_aead_cipher_init failed for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	FIPS_POP_CONTEXT(APPROVED);

	fips_push_context(fips_context);
	memset(buffer, 0, sizeof(buffer));
	length = sizeof(buffer);
	ret = gnutls_aead_cipher_encrypt(h, iv_data,
					 gnutls_cipher_get_iv_size(cipher),
					 NULL, 0, tag_length, buffer,
					 length - tag_length, buffer, &length);
	if (expect_encryption_fail) {
		if (ret != GNUTLS_E_INVALID_REQUEST) {
			fail("gnutls_aead_cipher_encrypt(%s) returned %d "
			     "while %d is expected\n",
			     gnutls_cipher_get_name(cipher), ret,
			     GNUTLS_E_INVALID_REQUEST);
		}
	} else if (ret < 0) {
		fail("gnutls_aead_cipher_encrypt failed for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	fips_pop_context(fips_context, expected_state);

	fips_push_context(fips_context);
	length = sizeof(buffer);
	ret = gnutls_aead_cipher_decrypt(h, iv_data,
					 gnutls_cipher_get_iv_size(cipher),
					 NULL, 0, tag_length, buffer, length,
					 buffer, &length);
	if (expect_encryption_fail) {
		if (ret != GNUTLS_E_INVALID_REQUEST) {
			fail("gnutls_aead_cipher_decrypt(%s) returned %d "
			     "while %d is expected\n",
			     gnutls_cipher_get_name(cipher), ret,
			     GNUTLS_E_INVALID_REQUEST);
		}
	} else if (ret < 0) {
		fail("gnutls_aead_cipher_decrypt failed for %s\n",
		     gnutls_cipher_get_name(cipher));
	}
	fips_pop_context(fips_context, expected_state);

	gnutls_aead_cipher_deinit(h);
}

static inline void test_ciphers(void)
{
	size_t i;

	test_cipher_approved(GNUTLS_CIPHER_AES_128_CBC);
	test_cipher_approved(GNUTLS_CIPHER_AES_192_CBC);
	test_cipher_approved(GNUTLS_CIPHER_AES_256_CBC);

	/* Check for all allowed Tlen */
	for (i = 4; i <= 16; i += 2) {
		test_ccm_cipher(GNUTLS_CIPHER_AES_128_CCM, i, false,
				GNUTLS_FIPS140_OP_APPROVED);
		test_ccm_cipher(GNUTLS_CIPHER_AES_256_CCM, i, false,
				GNUTLS_FIPS140_OP_APPROVED);
	}
	test_ccm_cipher(GNUTLS_CIPHER_AES_128_CCM, 3, true,
			GNUTLS_FIPS140_OP_ERROR);
	test_ccm_cipher(GNUTLS_CIPHER_AES_256_CCM, 3, true,
			GNUTLS_FIPS140_OP_ERROR);
	test_ccm_cipher(GNUTLS_CIPHER_AES_128_CCM, 5, true,
			GNUTLS_FIPS140_OP_ERROR);
	test_ccm_cipher(GNUTLS_CIPHER_AES_256_CCM, 5, true,
			GNUTLS_FIPS140_OP_ERROR);

	test_aead_cipher_approved(GNUTLS_CIPHER_AES_128_CCM_8);
	test_aead_cipher_approved(GNUTLS_CIPHER_AES_256_CCM_8);
	test_cipher_approved(GNUTLS_CIPHER_AES_128_CFB8);
	test_cipher_approved(GNUTLS_CIPHER_AES_192_CFB8);
	test_cipher_approved(GNUTLS_CIPHER_AES_256_CFB8);
	test_cipher_allowed(GNUTLS_CIPHER_AES_128_GCM);
	test_cipher_allowed(GNUTLS_CIPHER_AES_192_GCM);
	test_cipher_allowed(GNUTLS_CIPHER_AES_256_GCM);
	test_cipher_disallowed(GNUTLS_CIPHER_ARCFOUR_128);
	test_cipher_disallowed(GNUTLS_CIPHER_ESTREAM_SALSA20_256);
	test_cipher_disallowed(GNUTLS_CIPHER_SALSA20_256);
	test_cipher_disallowed(GNUTLS_CIPHER_CHACHA20_32);
	test_cipher_disallowed(GNUTLS_CIPHER_CHACHA20_64);
	test_cipher_disallowed(GNUTLS_CIPHER_CAMELLIA_192_CBC);
	test_cipher_disallowed(GNUTLS_CIPHER_CAMELLIA_128_CBC);
	test_cipher_disallowed(GNUTLS_CIPHER_CHACHA20_POLY1305);
	test_cipher_disallowed(GNUTLS_CIPHER_CAMELLIA_128_GCM);
	test_cipher_disallowed(GNUTLS_CIPHER_CAMELLIA_256_GCM);
	test_cipher_disallowed(GNUTLS_CIPHER_GOST28147_CPA_CFB);
	test_cipher_disallowed(GNUTLS_CIPHER_GOST28147_CPB_CFB);
	test_cipher_disallowed(GNUTLS_CIPHER_GOST28147_CPC_CFB);
	test_cipher_disallowed(GNUTLS_CIPHER_AES_128_SIV);
	test_cipher_disallowed(GNUTLS_CIPHER_AES_256_SIV);
	test_cipher_disallowed(GNUTLS_CIPHER_GOST28147_TC26Z_CNT);
	test_cipher_disallowed(GNUTLS_CIPHER_MAGMA_CTR_ACPKM);
	test_cipher_disallowed(GNUTLS_CIPHER_KUZNYECHIK_CTR_ACPKM);
	test_cipher_disallowed(GNUTLS_CIPHER_3DES_CBC);
	test_cipher_disallowed(GNUTLS_CIPHER_DES_CBC);
	test_cipher_disallowed(GNUTLS_CIPHER_ARCFOUR_40);
	test_cipher_disallowed(GNUTLS_CIPHER_RC2_40_CBC);
}

void doit(void)
{
	int ret;
	gnutls_fips140_operation_state_t fips_state;
	unsigned int mode;
	gnutls_cipher_hd_t ch;
	gnutls_hmac_hd_t mh;
	gnutls_session_t session;
	gnutls_pubkey_t pubkey;
	gnutls_x509_privkey_t xprivkey;
	gnutls_privkey_t privkey;
	gnutls_datum_t key = { key16, sizeof(key16) };
	gnutls_datum_t iv = { iv16, sizeof(iv16) };
	gnutls_datum_t signature;
	unsigned int bits;
	uint8_t hmac[64];
	uint8_t hash[64];
	gnutls_datum_t hashed_data;
	uint8_t pbkdf2[64];
	gnutls_datum_t temp_key = { NULL, 0 };

	fprintf(stderr,
		"Please note that if in FIPS140 mode, you need to assure the library's integrity prior to running this test\n");

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	mode = gnutls_fips140_mode_enabled();
	if (mode == 0) {
		success("We are not in FIPS140 mode\n");
		exit(77);
	}

	ret = global_init();
	if (ret < 0) {
		fail("Cannot initialize library\n");
	}

	ret = gnutls_fips140_context_init(&fips_context);
	if (ret < 0) {
		fail("Cannot initialize FIPS context\n");
	}
	fips_state = gnutls_fips140_get_operation_state(fips_context);
	if (fips_state != GNUTLS_FIPS140_OP_INITIAL) {
		fail("operation state is not initial\n");
	}
	ret = gnutls_fips140_pop_context();
	if (ret != GNUTLS_E_INVALID_REQUEST) {
		fail("gnutls_fips140_pop_context succeeded while not pushed\n");
	}

	/* Try crypto.h functionality */
	test_ciphers();

	FIPS_PUSH_CONTEXT();
	ret = gnutls_cipher_init(&ch, GNUTLS_CIPHER_AES_128_CBC, &key, &iv);
	if (ret < 0) {
		fail("gnutls_cipher_init failed\n");
	}
	gnutls_cipher_deinit(ch);
	FIPS_POP_CONTEXT(APPROVED);

	FIPS_PUSH_CONTEXT();
	ret = gnutls_cipher_init(&ch, GNUTLS_CIPHER_ARCFOUR_128, &key, &iv);
	if (ret != GNUTLS_E_UNWANTED_ALGORITHM) {
		fail("gnutls_cipher_init succeeded for arcfour\n");
	}
	FIPS_POP_CONTEXT(ERROR);

	ret = gnutls_hmac_init(&mh, GNUTLS_MAC_SHA1, key.data, key.size);
	if (ret < 0) {
		fail("gnutls_hmac_init failed\n");
	}
	gnutls_hmac_deinit(mh, NULL);

	ret = gnutls_hmac_init(&mh, GNUTLS_MAC_MD5, key.data, key.size);
	if (ret != GNUTLS_E_UNWANTED_ALGORITHM) {
		fail("gnutls_hmac_init succeeded for md5\n");
	}

	/* HMAC with key equal to or longer than 112 bits: approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_hmac_init(&mh, GNUTLS_MAC_SHA256, key.data, key.size);
	if (ret < 0) {
		fail("gnutls_hmac_init failed\n");
	}
	gnutls_hmac_deinit(mh, NULL);
	FIPS_POP_CONTEXT(APPROVED);

	/* HMAC with key shorter than 112 bits: not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_hmac_init(&mh, GNUTLS_MAC_SHA256, key.data, 13);
	if (ret < 0) {
		fail("gnutls_hmac_init failed\n");
	}
	gnutls_hmac_deinit(mh, NULL);
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* HMAC with key equal to or longer than 112 bits: approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_hmac_fast(GNUTLS_MAC_SHA256, key.data, key.size, data.data,
			       data.size, hmac);
	if (ret < 0) {
		fail("gnutls_hmac_fast failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* HMAC with key shorter than 112 bits: not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_hmac_fast(GNUTLS_MAC_SHA256, key.data, 13, data.data,
			       data.size, hmac);
	if (ret < 0) {
		fail("gnutls_hmac_fast failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* PBKDF2 with key equal to or longer than 112 bits: approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pbkdf2(GNUTLS_MAC_SHA256, &key, &iv, 1000, &pbkdf2,
			    sizeof(pbkdf2));
	if (ret < 0) {
		fail("gnutls_pbkdf2 failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* PBKDF2 with key shorter than 112 bits: not approved */
	FIPS_PUSH_CONTEXT();
	key.size = 13;
	ret = gnutls_pbkdf2(GNUTLS_MAC_SHA256, &key, &iv, 1000, &pbkdf2,
			    sizeof(pbkdf2));
	if (ret < 0) {
		fail("gnutls_pbkdf2 failed\n");
	}
	key.size = sizeof(key16);
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* PBKDF2 with iteration count lower than 1000: not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pbkdf2(GNUTLS_MAC_SHA256, &key, &iv, 999, &pbkdf2,
			    sizeof(pbkdf2));
	if (ret < 0) {
		fail("gnutls_pbkdf2 failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* PBKDF2 with salt shorter than 16 bytes: not approved */
	FIPS_PUSH_CONTEXT();
	iv.size = 13;
	ret = gnutls_pbkdf2(GNUTLS_MAC_SHA256, &key, &iv, 1000, &pbkdf2,
			    sizeof(pbkdf2));
	if (ret < 0) {
		fail("gnutls_pbkdf2 failed\n");
	}
	iv.size = sizeof(iv16);
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* PBKDF2 with output shorter than 112 bits: not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pbkdf2(GNUTLS_MAC_SHA256, &key, &iv, 1000, &pbkdf2, 13);
	if (ret < 0) {
		fail("gnutls_pbkdf2 failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);

	ret = gnutls_rnd(GNUTLS_RND_NONCE, key16, sizeof(key16));
	if (ret < 0) {
		fail("gnutls_rnd failed\n");
	}

	/* Symmetric key generation equal to or longer than 112 bits: approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_key_generate(&temp_key, 14);
	if (ret < 0) {
		fail("gnutls_key_generate failed\n");
	}
	gnutls_free(temp_key.data);
	FIPS_POP_CONTEXT(APPROVED);

	/* Symmetric key generation shorter than 112 bits: not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_key_generate(&temp_key, 13);
	if (ret < 0) {
		fail("gnutls_key_generate failed\n");
	}
	gnutls_free(temp_key.data);
	FIPS_POP_CONTEXT(NOT_APPROVED);

	ret = gnutls_pubkey_init(&pubkey);
	if (ret < 0) {
		fail("gnutls_pubkey_init failed\n");
	}
	gnutls_pubkey_deinit(pubkey);

	ret = gnutls_privkey_init(&privkey);
	if (ret < 0) {
		fail("gnutls_privkey_init failed\n");
	}
	gnutls_privkey_deinit(privkey);

	ret = gnutls_init(&session, 0);
	if (ret < 0) {
		fail("gnutls_init failed\n");
	}
	gnutls_deinit(session);

	/* Generate 2048-bit RSA key */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_x509_privkey_init(&xprivkey);
	if (ret < 0) {
		fail("gnutls_privkey_init failed\n");
	}
	bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_RSA,
					   GNUTLS_SEC_PARAM_MEDIUM);
	ret = gnutls_x509_privkey_generate(xprivkey, GNUTLS_PK_RSA, bits, 0);
	if (ret < 0) {
		fail("gnutls_x509_privkey_generate failed (%d) for %u-bit key\n",
		     ret, bits);
	}
	gnutls_x509_privkey_deinit(xprivkey);
	FIPS_POP_CONTEXT(APPROVED);

	/* Generate 512-bit RSA key */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_x509_privkey_init(&xprivkey);
	if (ret < 0) {
		fail("gnutls_privkey_init failed\n");
	}
	ret = gnutls_x509_privkey_generate(xprivkey, GNUTLS_PK_RSA, 512, 0);
	if (ret != GNUTLS_E_PK_GENERATION_ERROR) {
		fail("gnutls_x509_privkey_generate succeeded (%d) for 512-bit key\n",
		     ret);
	}
	gnutls_x509_privkey_deinit(xprivkey);
	FIPS_POP_CONTEXT(ERROR);

	/* Import 2432-bit RSA key; not a security function */
	FIPS_PUSH_CONTEXT();
	import_keypair(&privkey, &pubkey, "rsa-2432.pem");
	FIPS_POP_CONTEXT(INITIAL);

	/* Create a signature with 2432-bit RSA and SHA256; approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data(privkey, GNUTLS_DIG_SHA256, 0, &data,
				       &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* Verify a signature with 2432-bit RSA and SHA256; approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pubkey_verify_data2(pubkey, GNUTLS_SIGN_RSA_SHA256, 0,
					 &data, &signature);
	if (ret < 0) {
		fail("gnutls_pubkey_verify_data2 failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);
	gnutls_free(signature.data);

	/* Create a signature with 2432-bit RSA and SHA-1; not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data(privkey, GNUTLS_DIG_SHA1, 0, &data,
				       &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* Verify a signature created with 2432-bit RSA and SHA-1; approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pubkey_verify_data2(pubkey, GNUTLS_SIGN_RSA_SHA1,
					 GNUTLS_VERIFY_ALLOW_SIGN_WITH_SHA1,
					 &data, &rsa2342_sha1_sig);
	if (ret < 0) {
		fail("gnutls_pubkey_verify_data2 failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);
	gnutls_free(signature.data);
	gnutls_pubkey_deinit(pubkey);
	gnutls_privkey_deinit(privkey);

	/* Import 512-bit RSA key; not a security function */
	FIPS_PUSH_CONTEXT();
	import_keypair(&privkey, &pubkey, "rsa-512.pem");
	FIPS_POP_CONTEXT(INITIAL);

	/* Create a signature with 512-bit RSA and SHA256; not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data(privkey, GNUTLS_DIG_SHA256, 0, &data,
				       &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* Verify a signature with 512-bit RSA and SHA256; not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pubkey_verify_data2(pubkey, GNUTLS_SIGN_RSA_SHA256, 0,
					 &data, &signature);
	if (ret < 0) {
		fail("gnutls_pubkey_verify_data2 failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(signature.data);
	gnutls_pubkey_deinit(pubkey);
	gnutls_privkey_deinit(privkey);

	/* Import ECDSA key; not a security function */
	FIPS_PUSH_CONTEXT();
	import_keypair(&privkey, &pubkey, "ecc256.pem");
	FIPS_POP_CONTEXT(INITIAL);

	/* Create a signature with ECDSA and SHA256; approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data2(privkey, GNUTLS_SIGN_ECDSA_SHA256, 0,
					&data, &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data2 failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* Verify a signature with ECDSA and SHA256; approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pubkey_verify_data2(pubkey, GNUTLS_SIGN_ECDSA_SHA256, 0,
					 &data, &signature);
	if (ret < 0) {
		fail("gnutls_pubkey_verify_data2 failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);
	gnutls_free(signature.data);

	/* Create a signature with ECDSA and SHA256 (old API); approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data(privkey, GNUTLS_DIG_SHA256, 0, &data,
				       &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* Create a SHA256 hashed data for 2-pass signature API; not a
	 * crypto operation */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_hash_fast(GNUTLS_DIG_SHA256, data.data, data.size, hash);
	if (ret < 0) {
		fail("gnutls_hash_fast failed\n");
	}
	hashed_data.data = hash;
	hashed_data.size = 32;
	FIPS_POP_CONTEXT(INITIAL);

	/* Create a signature with ECDSA and SHA256 (2-pass API); not-approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_hash2(privkey, GNUTLS_SIGN_ECDSA_SHA256, 0,
					&hashed_data, &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_hash2 failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(signature.data);

	/* Create a signature with ECDSA and SHA256 (2-pass old API); not-approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_hash(privkey, GNUTLS_DIG_SHA256, 0,
				       &hashed_data, &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_hash failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(signature.data);

	/* Create a signature with ECDSA and SHA-1; not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data2(privkey, GNUTLS_SIGN_ECDSA_SHA1, 0,
					&data, &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data2 failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);

	/* Verify a signature created with ECDSA and SHA-1; approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_pubkey_verify_data2(pubkey, GNUTLS_SIGN_ECDSA_SHA1,
					 GNUTLS_VERIFY_ALLOW_SIGN_WITH_SHA1,
					 &data, &ecc256_sha1_sig);
	if (ret < 0) {
		fail("gnutls_pubkey_verify_data2 failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);
	gnutls_free(signature.data);

	/* Create a signature with ECDSA and SHA-1 (old API); not approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_data(privkey, GNUTLS_DIG_SHA1, 0, &data,
				       &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_data failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(signature.data);

	/* Create a SHA1 hashed data for 2-pass signature API; not a
	 * crypto operation */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_hash_fast(GNUTLS_DIG_SHA1, data.data, data.size, hash);
	if (ret < 0) {
		fail("gnutls_hash_fast failed\n");
	}
	hashed_data.data = hash;
	hashed_data.size = 20;
	FIPS_POP_CONTEXT(INITIAL);

	/* Create a signature with ECDSA and SHA1 (2-pass API); not-approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_hash2(privkey, GNUTLS_SIGN_ECDSA_SHA1, 0,
					&hashed_data, &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_hash2 failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(signature.data);

	/* Create a signature with ECDSA and SHA1 (2-pass old API); not-approved */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_privkey_sign_hash(privkey, GNUTLS_DIG_SHA1, 0,
				       &hashed_data, &signature);
	if (ret < 0) {
		fail("gnutls_privkey_sign_hash failed\n");
	}
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(signature.data);

	gnutls_pubkey_deinit(pubkey);
	gnutls_privkey_deinit(privkey);

	/* Test RND functions */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_rnd(GNUTLS_RND_RANDOM, key16, sizeof(key16));
	if (ret < 0) {
		fail("gnutls_rnd failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* run self-tests manually */
	FIPS_PUSH_CONTEXT();
	ret = gnutls_rnd(GNUTLS_RND_RANDOM, key16, sizeof(key16));
	ret = gnutls_fips140_run_self_tests();
	if (ret < 0) {
		fail("gnutls_fips140_run_self_tests failed\n");
	}
	FIPS_POP_CONTEXT(APPROVED);

	/* Test when FIPS140 is set to error state */
	_gnutls_lib_simulate_error();

	/* Try crypto.h functionality */
	ret = gnutls_cipher_init(&ch, GNUTLS_CIPHER_AES_128_CBC, &key, &iv);
	if (ret >= 0) {
		fail("gnutls_cipher_init succeeded when in FIPS140 error state\n");
	}

	ret = gnutls_hmac_init(&mh, GNUTLS_MAC_SHA1, key.data, key.size);
	if (ret >= 0) {
		fail("gnutls_hmac_init succeeded when in FIPS140 error state\n");
	}

	ret = gnutls_rnd(GNUTLS_RND_NONCE, key16, sizeof(key16));
	if (ret >= 0) {
		fail("gnutls_rnd succeeded when in FIPS140 error state\n");
	}

	ret = gnutls_pubkey_init(&pubkey);
	if (ret >= 0) {
		fail("gnutls_pubkey_init succeeded when in FIPS140 error state\n");
	}

	ret = gnutls_privkey_init(&privkey);
	if (ret >= 0) {
		fail("gnutls_privkey_init succeeded when in FIPS140 error state\n");
	}

	ret = gnutls_x509_privkey_init(&xprivkey);
	if (ret >= 0) {
		fail("gnutls_x509_privkey_init succeeded when in FIPS140 error state\n");
	}

	ret = gnutls_init(&session, 0);
	if (ret >= 0) {
		fail("gnutls_init succeeded when in FIPS140 error state\n");
	}

	gnutls_fips140_context_deinit(fips_context);

	gnutls_global_deinit();
	return;
}
