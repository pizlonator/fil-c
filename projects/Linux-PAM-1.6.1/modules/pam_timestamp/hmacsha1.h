#ifndef pam_timestamp_hmacfile_h
#define pam_timestamp_hmacfile_h

#include <sys/types.h>
#include <security/pam_modules.h>

size_t hmac_sha1_size(void);
void hmac_sha1_generate(void **mac, size_t *mac_length,
			const void *key, size_t key_length,
			const void *text, size_t text_length);
void hmac_sha1_generate_file(pam_handle_t *pamh, void **mac, size_t *mac_length,
			     const char *keyfile, uid_t owner, gid_t group,
			     const void *text, size_t text_length);

#endif
