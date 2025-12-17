#ifndef GNUTLS_LIB_ACCELERATED_X86_AES_X86_H
#define GNUTLS_LIB_ACCELERATED_X86_AES_X86_H

#include "gnutls_int.h"

void register_x86_crypto(void);

#define ALIGN16(x) \
	((void *)(((ptrdiff_t)(x) + (ptrdiff_t)0x0f) & ~((ptrdiff_t)0x0f)))

#define AES_KEY_ALIGN_SIZE 4
#define AES_MAXNR 14
typedef struct {
	/* We add few more integers to allow alignment 
	 * on a 16-byte boundary.
	 */
	uint32_t rd_key[4 * (AES_MAXNR + 1) + AES_KEY_ALIGN_SIZE];
	uint32_t rounds;
} AES_KEY;

#define CHECK_AES_KEYSIZE(s)               \
	if (s != 16 && s != 24 && s != 32) \
	return GNUTLS_E_INVALID_REQUEST

#include <intprops.h>
#define AES_GCM_ENCRYPT_MAX_BYTES ((1ULL << 36) - 32)
static inline int record_aes_gcm_encrypt_size(size_t *counter, size_t size)
{
	size_t sum;

	if (!INT_ADD_OK(*counter, size, &sum) ||
	    sum > AES_GCM_ENCRYPT_MAX_BYTES) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}
	*counter = sum;

	return 0;
}

void aesni_ecb_encrypt(const unsigned char *in, unsigned char *out, size_t len,
		       const AES_KEY *key, int enc);

void aesni_cbc_encrypt(const unsigned char *in, unsigned char *out, size_t len,
		       const AES_KEY *key, unsigned char *ivec, const int enc);
int aesni_set_decrypt_key(const unsigned char *userKey, const int bits,
			  AES_KEY *key);
int aesni_set_encrypt_key(const unsigned char *userKey, const int bits,
			  AES_KEY *key);

void aesni_ctr32_encrypt_blocks(const unsigned char *in, unsigned char *out,
				size_t blocks, const void *key,
				const unsigned char *ivec);

size_t aesni_gcm_encrypt(const void *inp, void *out, size_t len,
			 const AES_KEY *key, const unsigned char iv[16],
			 uint64_t *Xi);

size_t aesni_gcm_decrypt(const void *inp, void *out, size_t len,
			 const AES_KEY *key, const unsigned char iv[16],
			 uint64_t *Xi);

void aesni_xts_encrypt(const unsigned char *in, unsigned char *out, size_t len,
		       const AES_KEY *key, const AES_KEY *key2,
		       const unsigned char iv[16]);

void aesni_xts_decrypt(const unsigned char *in, unsigned char *out, size_t len,
		       const AES_KEY *key, const AES_KEY *key2,
		       const unsigned char iv[16]);

int vpaes_set_encrypt_key(const unsigned char *userKey, int bits, AES_KEY *key);
int vpaes_set_decrypt_key(const unsigned char *userKey, int bits, AES_KEY *key);
void vpaes_cbc_encrypt(const unsigned char *in, unsigned char *out,
		       size_t length, const AES_KEY *key, unsigned char *ivec,
		       int enc);
void vpaes_encrypt(const unsigned char *in, unsigned char *out,
		   const AES_KEY *key);
void vpaes_decrypt(const unsigned char *in, unsigned char *out,
		   const AES_KEY *key);

extern const gnutls_crypto_cipher_st _gnutls_aes_gcm_pclmul;
extern const gnutls_crypto_cipher_st _gnutls_aes_gcm_pclmul_avx;
extern const gnutls_crypto_cipher_st _gnutls_aes_gcm_x86_aesni;
extern const gnutls_crypto_cipher_st _gnutls_aes_ccm_x86_aesni;
extern const gnutls_crypto_cipher_st _gnutls_aes_xts_x86_aesni;
extern const gnutls_crypto_cipher_st _gnutls_aes_gcm_x86_ssse3;

extern const gnutls_crypto_cipher_st _gnutls_aes_ssse3;

extern const gnutls_crypto_cipher_st _gnutls_aesni_x86;

#endif /* GNUTLS_LIB_ACCELERATED_X86_AES_X86_H */
