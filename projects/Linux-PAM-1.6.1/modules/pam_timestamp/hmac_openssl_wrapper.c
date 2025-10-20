/* Wrapper for hmac openssl implementation.
 *
 * Copyright (c) 2021 Red Hat, Inc.
 * Written by Iker Pedrosa <ipedrosa@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#ifdef WITH_OPENSSL

#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <openssl/rand.h>

#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#include "hmac_openssl_wrapper.h"
#include "pam_inline.h"

#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif

#define LOGIN_DEFS          "/etc/login.defs"
#define CRYPTO_KEY          "HMAC_CRYPTO_ALGO"
#define DEFAULT_ALGORITHM   "SHA512"
#define MAX_HMAC_LENGTH     512
#define MAX_KEY_LENGTH      EVP_MAX_KEY_LENGTH

static char *
get_crypto_algorithm(pam_handle_t *pamh, int debug){
    char *config_value = NULL;

    config_value = pam_modutil_search_key(pamh, LOGIN_DEFS, CRYPTO_KEY);

    if (config_value == NULL) {
        config_value = strdup(DEFAULT_ALGORITHM);
        if (debug) {
            pam_syslog(pamh, LOG_DEBUG,
                   "Key [%s] not found, falling back to default algorithm [%s]\n",
                   CRYPTO_KEY, DEFAULT_ALGORITHM);
        }
    }

    return config_value;
}

static int
PAM_NONNULL((1, 2))
generate_key(pam_handle_t *pamh, char **key, size_t key_size)
{
    int fd = 0;
    ssize_t bytes_read = 0;
    char *tmp = *key = NULL;

    tmp = calloc(1, key_size);
    if (!tmp) {
        pam_syslog(pamh, LOG_CRIT, "Not enough memory");
        return PAM_AUTH_ERR;
    }

    /* Try to get random data from OpenSSL first */
    if (RAND_priv_bytes((unsigned char *)tmp, key_size) == 1) {
        *key = tmp;
        return PAM_SUCCESS;
    }

#ifdef HAVE_GETRANDOM
    /* Fallback to getrandom(2) if available */
    if (getrandom(tmp, key_size, 0) == (ssize_t)key_size) {
        *key = tmp;
        return PAM_SUCCESS;
    }
#endif

    /* Fallback to /dev/urandom */
    fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        pam_syslog(pamh, LOG_ERR, "Cannot open /dev/urandom: %m");
        pam_overwrite_n(tmp, key_size);
        free(tmp);
        return PAM_AUTH_ERR;
    }

    bytes_read = pam_modutil_read(fd, tmp, key_size);
    close(fd);

    if (bytes_read < 0 || (size_t)bytes_read < key_size) {
        pam_syslog(pamh, LOG_ERR, "Short read on random device");
        pam_overwrite_n(tmp, key_size);
        free(tmp);
        return PAM_AUTH_ERR;
    }

    *key = tmp;

    return PAM_SUCCESS;
}

static int
PAM_NONNULL((1, 3, 4))
read_file(pam_handle_t *pamh, int fd, char **text, size_t *text_length)
{
    struct stat st;
    ssize_t bytes_read = 0;
    char *tmp = NULL;

    if (fstat(fd, &st) == -1) {
        pam_syslog(pamh, LOG_ERR, "Unable to stat file: %m");
        close(fd);
        return PAM_AUTH_ERR;
    }

    if (st.st_size == 0) {
        pam_syslog(pamh, LOG_ERR, "Key file size cannot be 0");
        close(fd);
        return PAM_AUTH_ERR;
    }

    if ((uintmax_t)st.st_size > (uintmax_t)INT_MAX) {
        pam_syslog(pamh, LOG_ERR, "Key file is too large");
        close(fd);
        return PAM_AUTH_ERR;
    }

    tmp = calloc(1, st.st_size);
    if (!tmp) {
        pam_syslog(pamh, LOG_CRIT, "Not enough memory");
        close(fd);
        return PAM_AUTH_ERR;
    }

    bytes_read = pam_modutil_read(fd, tmp, st.st_size);
    close(fd);

    if (bytes_read < st.st_size) {
        pam_syslog(pamh, LOG_ERR, "Short read on key file");
        pam_overwrite_n(tmp, st.st_size);
        free(tmp);
        return PAM_AUTH_ERR;
    }

    *text = tmp;
    *text_length = st.st_size;

    return PAM_SUCCESS;
}

static int
PAM_NONNULL((1, 2, 3))
write_file(pam_handle_t *pamh, const char *file_name, char *text,
           size_t text_length, uid_t owner, gid_t group)
{
    int fd = 0;
    ssize_t bytes_written = 0;

    fd = open(file_name,
              O_WRONLY | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR);
    if (fd == -1) {
        pam_syslog(pamh, LOG_ERR, "Unable to open [%s]: %m", file_name);
        pam_overwrite_n(text, text_length);
        free(text);
        return PAM_AUTH_ERR;
    }

    if (fchown(fd, owner, group) == -1) {
        pam_syslog(pamh, LOG_ERR, "Unable to change ownership [%s]: %m", file_name);
        pam_overwrite_n(text, text_length);
        free(text);
        close(fd);
        return PAM_AUTH_ERR;
    }

    bytes_written = pam_modutil_write(fd, text, text_length);
    close(fd);

    if (bytes_written < 0 || (size_t)bytes_written < text_length) {
        pam_syslog(pamh, LOG_ERR, "Short write on %s", file_name);
        pam_overwrite_n(text, text_length);
        free(text);
        return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;
}

static int
PAM_NONNULL((1, 2, 3))
key_management(pam_handle_t *pamh, const char *file_name, char **text,
                size_t text_length, uid_t owner, gid_t group)
{
    int fd = 0;

    fd = open(file_name, O_RDONLY | O_NOFOLLOW);
    if (fd == -1) {
        if (errno == ENOENT) {
            if (generate_key(pamh, text, text_length)) {
                pam_syslog(pamh, LOG_ERR, "Unable to generate key");
                return PAM_AUTH_ERR;
            }

            if (write_file(pamh, file_name, *text, text_length, owner, group)) {
                pam_syslog(pamh, LOG_ERR, "Unable to write key");
                return PAM_AUTH_ERR;
            }
        } else {
            pam_syslog(pamh, LOG_ERR, "Unable to open %s: %m", file_name);
            return PAM_AUTH_ERR;
        }
    } else {
        if (read_file(pamh, fd, text, &text_length)) {
            pam_syslog(pamh, LOG_ERR, "Error reading key file %s\n", file_name);
            return PAM_AUTH_ERR;
        }
    }

    return PAM_SUCCESS;
}

static int
hmac_management(pam_handle_t *pamh, int debug, void **out, size_t *out_length,
                char *key, size_t key_length,
                const void *text, size_t text_length)
{
    int ret = PAM_AUTH_ERR;
    EVP_MAC *evp_mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    unsigned char *hmac_message = NULL;
    size_t hmac_length;
    char *algo = NULL;
    OSSL_PARAM subalg_param[] = { OSSL_PARAM_END, OSSL_PARAM_END };

    algo = get_crypto_algorithm(pamh, debug);

    subalg_param[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                       algo,
                                                       0);

    evp_mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (evp_mac == NULL) {
        pam_syslog(pamh, LOG_ERR, "Unable to create hmac implementation");
        goto done;
    }

    ctx = EVP_MAC_CTX_new(evp_mac);
    if (ctx == NULL) {
        pam_syslog(pamh, LOG_ERR, "Unable to create hmac context");
        goto done;
    }

    ret = EVP_MAC_init(ctx, (const unsigned char *)key, key_length, subalg_param);
    if (ret == 0) {
        pam_syslog(pamh, LOG_ERR, "Unable to initialize hmac context");
        goto done;
    }

    ret = EVP_MAC_update(ctx, (const unsigned char *)text, text_length);
    if (ret == 0) {
        pam_syslog(pamh, LOG_ERR, "Unable to update hmac context");
        goto done;
    }

    hmac_message = malloc(sizeof(unsigned char) * MAX_HMAC_LENGTH);
    if (!hmac_message) {
        pam_syslog(pamh, LOG_CRIT, "Not enough memory");
        goto done;
    }

    ret = EVP_MAC_final(ctx, hmac_message, &hmac_length, MAX_HMAC_LENGTH);
    if (ret == 0) {
        pam_syslog(pamh, LOG_ERR, "Unable to calculate hmac message");
        goto done;
    }

    *out_length = hmac_length;
    *out = malloc(*out_length);
    if (*out == NULL) {
        pam_syslog(pamh, LOG_CRIT, "Not enough memory");
        goto done;
    }

    memcpy(*out, hmac_message, *out_length);
    ret = PAM_SUCCESS;

done:
    free(hmac_message);
    if (key != NULL) {
        pam_overwrite_n(key, key_length);
        free(key);
    }
    if (ctx != NULL) {
        EVP_MAC_CTX_free(ctx);
    }
    if (evp_mac != NULL) {
        EVP_MAC_free(evp_mac);
    }
    free(algo);

    return ret;
}

int
hmac_size(pam_handle_t *pamh, int debug, size_t *hmac_length)
{
    int ret = PAM_AUTH_ERR;
    EVP_MAC *evp_mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    const unsigned char key[] = "ThisIsJustAKey";
    size_t key_length = MAX_KEY_LENGTH;
    char *algo = NULL;
    OSSL_PARAM subalg_param[] = { OSSL_PARAM_END, OSSL_PARAM_END };

    algo = get_crypto_algorithm(pamh, debug);

    subalg_param[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                       algo,
                                                       0);

    evp_mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (evp_mac == NULL) {
        pam_syslog(pamh, LOG_ERR, "Unable to create hmac implementation");
        goto done;
    }

    ctx = EVP_MAC_CTX_new(evp_mac);
    if (ctx == NULL) {
        pam_syslog(pamh, LOG_ERR, "Unable to create hmac context");
        goto done;
    }

    ret = EVP_MAC_init(ctx, key, key_length, subalg_param);
    if (ret == 0) {
        pam_syslog(pamh, LOG_ERR, "Unable to initialize hmac context");
        goto done;
    }

    *hmac_length = EVP_MAC_CTX_get_mac_size(ctx);
    ret = PAM_SUCCESS;

done:
    if (ctx != NULL) {
        EVP_MAC_CTX_free(ctx);
    }
    if (evp_mac != NULL) {
        EVP_MAC_free(evp_mac);
    }
    free(algo);

    return ret;
}

int
hmac_generate(pam_handle_t *pamh, int debug, void **mac, size_t *mac_length,
              const char *key_file, uid_t owner, gid_t group,
              const void *text, size_t text_length)
{
    char *key = NULL;
    size_t key_length = MAX_KEY_LENGTH;

    if (key_management(pamh, key_file, &key, key_length, owner, group)) {
        return PAM_AUTH_ERR;
    }

    if (hmac_management(pamh, debug, mac, mac_length, key, key_length,
                        text, text_length)) {
        return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;
}

#endif /* WITH_OPENSSL */
