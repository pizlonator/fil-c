/* wget.c - Simple downloader to get the resource file from a HTTP server
 *
 * Copyright 2016 Lipi C.H. Lee <lipisoft@gmail.com>
 * Copyright 2021 Eric Molitor <eric@molitor.org>
 *
 * Relevant sources of information
 * -------------------------------
 * HTTP 1.1: https://www.rfc-editor.org/rfc/rfc7230
 * Chunked Encoding: https://www.rfc-editor.org/rfc/rfc7230#section-4.1
 * UTF-8 Encoded Header Values https://www.rfc-editor.org/rfc/rfc5987
 *
 * Test URLs
 * ---------
 * Chunked Encoding: https://jigsaw.w3.org/HTTP/ChunkedScript
 * Redirect 301: https://jigsaw.w3.org/HTTP/300/301.html
 * Redirect 302: https://jigsaw.w3.org/HTTP/300/302.html
 * TLS 1.0: https://tls-v1-0.badssl.com:1010/
 * TLS 1.1: https://tls-v1-1.badssl.com:1011/
 * TLS 1.2: https://tls-v1-2.badssl.com:1012/
 * TLS 1.3: https://tls13.1d.pw/
 * Transfer Encoding [gzip|deflate]: https://jigsaw.w3.org/HTTP/TE/bar.txt
 *
 *
 * TODO: Add support for configurable TLS versions
 * TODO: Add support for ftp
 * TODO: Add support for Transfer Encoding (gzip|deflate)
 * TODO: Add support for RFC5987

USE_WGET(NEWTOY(wget, "<1>1(max-redirect)#<0=20d(debug)O(output-document):p(post-data):", TOYFLAG_USR|TOYFLAG_BIN))

config WGET
  bool "wget"
  default y
  help
    usage: wget [OPTIONS]... [URL]
        --max-redirect          maximum redirections allowed
    -d, --debug                 print lots of debugging information
    -O, --output-document=FILE  specify output filename
    -p, --post-data=DATA        send data in body of POST request

    examples:
      wget http://www.example.com

config WGET_LIBTLS
  bool "Enable HTTPS support for wget via LibTLS"
  default n
  depends on WGET && !TOYBOX_LIBCRYPTO
  help
    Enable HTTPS support for wget by linking to LibTLS.
    Supports using libtls, libretls or libtls-bearssl.

    Use TOYBOX_LIBCRYPTO to enable HTTPS support via OpenSSL.
*/

#define FOR_wget
#include "toys.h"

#if CFG_WGET_LIBTLS
#define WGET_SSL 1
#include <tls.h>
#elif CFG_TOYBOX_LIBCRYPTO
#define WGET_SSL 1
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#else
#define WGET_SSL 0
#endif
#define HTTPS (WGET_SSL && TT.https)


GLOBALS(
  char *p, *O;
  long max_redirect;

  int sock, https;
  char *url;
#if CFG_WGET_LIBTLS
  struct tls *tls;
#elif CFG_TOYBOX_LIBCRYPTO
  struct ssl_ctx_st *ctx;
  struct ssl_st *ssl;
#endif
)

// get http info in URL
static void wget_info(char *url, char **host, char **port, char **path)
{
  char *ss = url;

  // Must start with case insensitive http:// or https://
  if (strncasecmp(url, "http", 4)) url = 0;
  else {
    url += 4;
    if ((TT.https = WGET_SSL && toupper(*url=='s'))) url++;
    if (!strstart(&url, "://")) url = 0;
  }
  if (!url) error_exit("unsupported protocol: %s", ss);
  if ((*path = strchr(*host = url, '/'))) *((*path)++) = 0;
  else *path = "";

  // Get port number and trim literal IPv6 addresses
  if (**host=='[' && (ss = strchr(++*host, ']'))) {
    *ss++ = 0;
    *port = (*ss==':') ? ++ss : 0;
  } else if ((*port = strchr(*host, ':'))) *((*port)++) = 0;
  if (!*port) *port = HTTPS ? "443" : "80";
}

static void wget_connect(char *host, char *port)
{
  if (!HTTPS)
    TT.sock = xconnectany(xgetaddrinfo(host, port, AF_UNSPEC, SOCK_STREAM, 0, 0));
  else {
#if CFG_WGET_LIBTLS
    struct tls_config *cfg = NULL;
    uint32_t protocols;
    if (!(TT.tls = tls_client()))
      error_exit("tls_client: %s", tls_error(TT.tls));
    if (!(cfg = tls_config_new()))
      error_exit("tls_config_new: %s", tls_config_error(cfg));
    if (tls_config_parse_protocols(&protocols, "tlsv1.2"))
      error_exit("tls_config_parse_protocols");
    if (tls_config_set_protocols(cfg, protocols))
      error_exit("tls_config_set_protocols: %s", tls_config_error(cfg));
    if (tls_configure(TT.tls, cfg))
      error_exit("tls_configure: %s", tls_error(TT.tls));
    tls_config_free(cfg);

    if (tls_connect(TT.tls, host, port))
      error_exit("tls_connect: %s", tls_error(TT.tls));
#elif CFG_TOYBOX_LIBCRYPTO
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    TT.ctx = SSL_CTX_new(TLS_client_method());
    if (!TT.ctx) error_exit("SSL_CTX_new");

    TT.sock = xconnectany(xgetaddrinfo(host, port, AF_UNSPEC, SOCK_STREAM, 0, 0));

    TT.ssl = SSL_new(TT.ctx);
    if (!TT.ssl)
      error_exit("SSL_new: %s", ERR_error_string(ERR_get_error(), NULL));

    if (!SSL_set_tlsext_host_name(TT.ssl, host))
      error_exit("SSL_set_tlsext_host_name: %s",
                 ERR_error_string(ERR_get_error(), NULL));

    SSL_set_fd(TT.ssl, TT.sock);
    if (SSL_connect(TT.ssl) == -1)
      error_exit("SSL_set_fd: %s", ERR_error_string(ERR_get_error(), NULL));

    if (FLAG(d)) printf("TLS: %s\n", SSL_get_cipher(TT.ssl));
#endif
  }
}

static size_t wget_read(void *buf, size_t len)
{
  if (!HTTPS) return xread(TT.sock, buf, len);
  else {
    char *err = 0;
    int ret;

#if CFG_WGET_LIBTLS
    if ((ret = tls_read(TT.tls, buf, len))<0) err = tls_error(TT.tls);
#elif CFG_TOYBOX_LIBCRYPTO
    if ((ret = SSL_read(TT.ssl, buf, len))<0)
      err = ERR_error_string(ERR_get_error(), 0);
#endif
    if (err) error_exit("https read: %s", err);

    return ret;
  }
}

static void wget_write(void *buf, size_t len)
{
  if (!HTTPS) xwrite(TT.sock, buf, len);
  else {
    char *err = 0;

#if CFG_WGET_LIBTLS
    if (len != tls_write(TT.tls, buf, len)) err = tls_error(TT.tls);
#elif CFG_TOYBOX_LIBCRYPTO
    if (len != SSL_write(TT.ssl, buf, len))
      err = ERR_error_string(ERR_get_error(), 0);
#endif
    if (err) error_exit("https write: %s", err);
  }
}

static void wget_close()
{
  if (TT.sock) {
      xclose(TT.sock);
      TT.sock = 0;
  }

#if CFG_WGET_LIBTLS
  if (TT.tls) {
    tls_close(TT.tls);
    tls_free(TT.tls);
    TT.tls = 0;
  }
#elif CFG_TOYBOX_LIBCRYPTO
  if (TT.ssl) {
    SSL_shutdown(TT.ssl);
    SSL_free(TT.ssl);
    TT.ssl = 0;
  }

  if (TT.ctx) {
    SSL_CTX_free(TT.ctx);
    TT.ctx = 0;
  }
#endif
}

static char *wget_find_header(char *header, char *val)
{
  if (!(header = strcasestr(header, val))) return 0;
  header += strlen(val);

  return xstrndup(header, strcspn(header, "\r\n"));
}

void wget_main(void)
{
  long status = 0;
  size_t len, c_len = 0;
  int fd = 0, ii;
  char *body, *index, *host, *port, *path = 0, *chunked, *ss;
  char agent[] = "toybox wget/" TOYBOX_VERSION;

  TT.url = escape_url(*toys.optargs, 0);

  // Ask server for URL, following redirects until success
  while (status != 200) {
    if (!TT.max_redirect--) error_exit("Too many redirects");

    // Connect and write request
    wget_info(TT.url, &host, &port, &path);
    if (TT.p) sprintf(toybuf, "Content-Length: %ld\r\n", (long)strlen(TT.p));
    ss = xmprintf("%s /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\n"
                  "Connection: close\r\n%s\r\n%s", FLAG(p) ? "POST" : "GET",
                  path, host, agent, TT.p ? toybuf : "", TT.p ? : "");
    if (FLAG(d)) printf("--- Request\n%s", ss);
    wget_connect(host, port);
    wget_write(ss, strlen(ss));
    free(ss);

    // Read HTTP response into toybuf (probably with some body at end)
    for (index = toybuf;
      (len = wget_read(index, sizeof(toybuf)-(index-toybuf)))>0; index += len);

    // Split response into header and body, and null terminate header.
    // (RFC7230 says header cannot contain NUL.)
    if (!(body = memmem(ss = toybuf, index-toybuf, "\r\n\r\n", 4)))
      error_exit("response header too large");
    *body = 0;
    body += 4;
    len = index-body;
    if (FLAG(d)) printf("--- Response\n%s\n\n", toybuf);

    status = strstart(&ss, "HTTP/1.1 ") ? strtol(ss, 0, 10) : 0;
    if ((status == 301) || (status == 302)) {
      if (!(ss = wget_find_header(toybuf, "Location: ")))
        error_exit("bad redirect");
      free(TT.url);
      TT.url = ss;
      wget_close();
    } else if (status != 200) error_exit("response %ld", status);
  }

  // Open output file
  if (TT.O && !strcmp(TT.O, "-")) fd = 1;
  else if (!TT.O) {
    ss = wget_find_header(toybuf, "Content-Disposition: attachment; filename=");
    if (ss) {
      unescape_url(ss, 1);
      for (ii = strlen(ss); ii; ii--) {
        if (ss[ii]=='/') memmove(ss, ss+ii, strlen(ss+ii));
        break;
      }
      if (!*ss) {
        free(ss);
        ss = 0;
      }
    }
    if (!ss) {
      path = 0;
      for (ii = 0, ss = *toys.optargs; *ss && *ss!='?' && *ss!='#'; ss++)
        if (*ss=='/' && ++ii>2) path = ss+1;
      ss = (path && ss>path) ? xstrndup(path, ss-path) : 0;
      // TODO: handle %20 style escapes
    }
    if (!ss) ss = "index.html";
    if (!access((TT.O = ss), F_OK)) error_exit("%s already exists", TT.O);
  }
  // TODO: don't allow header/basename to write to stdout
  if (!fd) fd = xcreate(TT.O, (O_WRONLY|O_CREAT|O_TRUNC), 0644);

  // If chunked we offset the first buffer by 2 character, meaning it is
  // pointing at half of the header boundary, aka '\r\n'. This simplifies
  // parsing of the first c_len length by allowing the do while loop to fall
  // through on the first iteration and parse the first c_len size.
  chunked = wget_find_header(toybuf, "transfer-encoding: chunked");
  if (chunked) memmove(toybuf, body-2, len += 2);
  else memmove(toybuf, body, len);

  // len is the size remaining in toybuf
  // c_len is the size of the remaining bytes in the current chunk
  do {
    if (chunked) {
      if (c_len > 0) { // We have an incomplete c_len to write
        if (len <= c_len) { // Buffer is less than the c_len so full write
          xwrite(fd, toybuf, len);
          c_len = c_len - len;
          len = 0;
        } else { // Buffer is larger than the c_len so partial write
          xwrite(fd, toybuf, c_len);
          len = len - c_len;
          memmove(toybuf, toybuf + c_len, len);
          c_len = 0;
        }
      }

      // If len is less than 2 we can't validate the chunk boundary so fall
      // through and go read more into toybuf.
      if (!c_len && (len > 2)) {
        char *c;
        if (strncmp(toybuf, "\r\n", 2) != 0) error_exit("chunk boundary");

        // If we can't find the end of the new chunk signature fall through and
        // read more into toybuf.
        c = memmem(toybuf + 2, len - 2, "\r\n",2);
        if (c) {
          c_len = strtol(toybuf + 2, NULL, 16);
          if (!c_len) break; // A c_len of zero means we are complete
          len = len - (c - toybuf) - 2;
          memmove(toybuf, c + 2, len);
        }
      }

      if (len == sizeof(toybuf)) error_exit("chunk overflow");
    } else {
      xwrite(fd, toybuf, len);
      len = 0;
    }
  } while ((len += wget_read(toybuf + len, sizeof(toybuf) - len)) > 0);

  wget_close();
  free(TT.url);
}
