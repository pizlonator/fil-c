#include <stdbool.h>
#include <stdio.h>

#ifndef GNUTLS_SERV_OPTIONS_H_
#define GNUTLS_SERV_OPTIONS_H_ 1

struct gnutls_serv_list
{
  const char **args;
  unsigned int count;
};

struct gnutls_serv_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool sni_hostname;
    bool sni_hostname_fatal;
    bool alpn;
    bool alpn_fatal;
    bool noticket;
    bool earlydata;
    bool maxearlydata;
    bool nocookie;
    bool generate;
    bool quiet;
    bool nodb;
    bool http;
    bool echo;
    bool crlf;
    bool udp;
    bool mtu;
    bool srtp_profiles;
    bool disable_client_cert;
    bool require_client_cert;
    bool verify_client_cert;
    bool compress_cert;
    bool heartbeat;
    bool x509fmtder;
    bool priority;
    bool dhparams;
    bool x509cafile;
    bool x509crlfile;
    bool pgpkeyfile;
    bool x509keyfile;
    bool x509certfile;
    bool rawpkkeyfile;
    bool rawpkfile;
    bool srppasswd;
    bool srppasswdconf;
    bool pskpasswd;
    bool pskhint;
    bool ocsp_response;
    bool ignore_ocsp_response_errors;
    bool port;
    bool list;
    bool provider;
    bool keymatexport;
    bool keymatexportsize;
    bool recordsize;
    bool httpdata;
    bool timeout;
    bool attime;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *sni_hostname;
    const char *maxearlydata;
    const char *mtu;
    const char *srtp_profiles;
    const char *priority;
    const char *dhparams;
    const char *x509cafile;
    const char *x509crlfile;
    const char *pgpkeyfile;
    const char *srppasswd;
    const char *srppasswdconf;
    const char *pskpasswd;
    const char *pskhint;
    const char *port;
    const char *provider;
    const char *keymatexport;
    const char *keymatexportsize;
    const char *recordsize;
    const char *httpdata;
    const char *timeout;
    const char *attime;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int maxearlydata;
    int mtu;
    int port;
    int keymatexportsize;
    int recordsize;
    int timeout;
  } value;

  /* Option arguments parsed as list */
  struct
  {
    struct gnutls_serv_list alpn;
    struct gnutls_serv_list compress_cert;
    struct gnutls_serv_list x509keyfile;
    struct gnutls_serv_list x509certfile;
    struct gnutls_serv_list rawpkkeyfile;
    struct gnutls_serv_list rawpkfile;
    struct gnutls_serv_list ocsp_response;
  } list;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool sni_hostname;
    bool sni_hostname_fatal;
    bool alpn;
    bool alpn_fatal;
    bool noticket;
    bool earlydata;
    bool maxearlydata;
    bool nocookie;
    bool generate;
    bool quiet;
    bool nodb;
    bool http;
    bool echo;
    bool crlf;
    bool udp;
    bool mtu;
    bool srtp_profiles;
    bool disable_client_cert;
    bool require_client_cert;
    bool verify_client_cert;
    bool compress_cert;
    bool heartbeat;
    bool x509fmtder;
    bool priority;
    bool dhparams;
    bool x509cafile;
    bool x509crlfile;
    bool pgpkeyfile;
    bool x509keyfile;
    bool x509certfile;
    bool rawpkkeyfile;
    bool rawpkfile;
    bool srppasswd;
    bool srppasswdconf;
    bool pskpasswd;
    bool pskhint;
    bool ocsp_response;
    bool ignore_ocsp_response_errors;
    bool port;
    bool list;
    bool provider;
    bool keymatexport;
    bool keymatexportsize;
    bool recordsize;
    bool httpdata;
    bool timeout;
    bool attime;
    bool version;
    bool help;
    bool more_help;
  } enabled;
};

#define HAVE_OPT(name) HAVE_OPT_ ## name
#define OPT_ARG(name) OPT_ARG_ ## name
#define ENABLED_OPT(name) ENABLED_OPT_ ## name
#define OPTS_COUNT(name) OPTS_COUNT_ ## name
#define OPTS_ARRAY(name) OPTS_ARRAY_ ## name
#define USAGE(status) usage (stdout, (status))

#define HAVE_OPT_DEBUG gnutls_serv_options.present.debug
#define HAVE_OPT_SNI_HOSTNAME gnutls_serv_options.present.sni_hostname
#define HAVE_OPT_SNI_HOSTNAME_FATAL gnutls_serv_options.present.sni_hostname_fatal
#define HAVE_OPT_ALPN gnutls_serv_options.present.alpn
#define HAVE_OPT_ALPN_FATAL gnutls_serv_options.present.alpn_fatal
#define HAVE_OPT_NOTICKET gnutls_serv_options.present.noticket
#define HAVE_OPT_EARLYDATA gnutls_serv_options.present.earlydata
#define HAVE_OPT_MAXEARLYDATA gnutls_serv_options.present.maxearlydata
#define HAVE_OPT_NOCOOKIE gnutls_serv_options.present.nocookie
#define HAVE_OPT_GENERATE gnutls_serv_options.present.generate
#define HAVE_OPT_QUIET gnutls_serv_options.present.quiet
#define HAVE_OPT_NODB gnutls_serv_options.present.nodb
#define HAVE_OPT_HTTP gnutls_serv_options.present.http
#define HAVE_OPT_ECHO gnutls_serv_options.present.echo
#define HAVE_OPT_CRLF gnutls_serv_options.present.crlf
#define HAVE_OPT_UDP gnutls_serv_options.present.udp
#define HAVE_OPT_MTU gnutls_serv_options.present.mtu
#define HAVE_OPT_SRTP_PROFILES gnutls_serv_options.present.srtp_profiles
#define HAVE_OPT_DISABLE_CLIENT_CERT gnutls_serv_options.present.disable_client_cert
#define HAVE_OPT_REQUIRE_CLIENT_CERT gnutls_serv_options.present.require_client_cert
#define HAVE_OPT_VERIFY_CLIENT_CERT gnutls_serv_options.present.verify_client_cert
#define HAVE_OPT_COMPRESS_CERT gnutls_serv_options.present.compress_cert
#define HAVE_OPT_HEARTBEAT gnutls_serv_options.present.heartbeat
#define HAVE_OPT_X509FMTDER gnutls_serv_options.present.x509fmtder
#define HAVE_OPT_PRIORITY gnutls_serv_options.present.priority
#define HAVE_OPT_DHPARAMS gnutls_serv_options.present.dhparams
#define HAVE_OPT_X509CAFILE gnutls_serv_options.present.x509cafile
#define HAVE_OPT_X509CRLFILE gnutls_serv_options.present.x509crlfile
#define HAVE_OPT_PGPKEYFILE gnutls_serv_options.present.pgpkeyfile
#define HAVE_OPT_X509KEYFILE gnutls_serv_options.present.x509keyfile
#define HAVE_OPT_X509CERTFILE gnutls_serv_options.present.x509certfile
#define HAVE_OPT_RAWPKKEYFILE gnutls_serv_options.present.rawpkkeyfile
#define HAVE_OPT_RAWPKFILE gnutls_serv_options.present.rawpkfile
#define HAVE_OPT_SRPPASSWD gnutls_serv_options.present.srppasswd
#define HAVE_OPT_SRPPASSWDCONF gnutls_serv_options.present.srppasswdconf
#define HAVE_OPT_PSKPASSWD gnutls_serv_options.present.pskpasswd
#define HAVE_OPT_PSKHINT gnutls_serv_options.present.pskhint
#define HAVE_OPT_OCSP_RESPONSE gnutls_serv_options.present.ocsp_response
#define HAVE_OPT_IGNORE_OCSP_RESPONSE_ERRORS gnutls_serv_options.present.ignore_ocsp_response_errors
#define HAVE_OPT_PORT gnutls_serv_options.present.port
#define HAVE_OPT_LIST gnutls_serv_options.present.list
#define HAVE_OPT_PROVIDER gnutls_serv_options.present.provider
#define HAVE_OPT_KEYMATEXPORT gnutls_serv_options.present.keymatexport
#define HAVE_OPT_KEYMATEXPORTSIZE gnutls_serv_options.present.keymatexportsize
#define HAVE_OPT_RECORDSIZE gnutls_serv_options.present.recordsize
#define HAVE_OPT_HTTPDATA gnutls_serv_options.present.httpdata
#define HAVE_OPT_TIMEOUT gnutls_serv_options.present.timeout
#define HAVE_OPT_ATTIME gnutls_serv_options.present.attime
#define HAVE_OPT_VERSION gnutls_serv_options.present.version
#define HAVE_OPT_HELP gnutls_serv_options.present.help
#define HAVE_OPT_MORE_HELP gnutls_serv_options.present.more_help

#define OPT_ARG_DEBUG gnutls_serv_options.arg.debug
#define OPT_ARG_SNI_HOSTNAME gnutls_serv_options.arg.sni_hostname
#define OPT_ARG_SNI_HOSTNAME_FATAL gnutls_serv_options.arg.sni_hostname_fatal
#define OPT_ARG_ALPN gnutls_serv_options.arg.alpn
#define OPT_ARG_ALPN_FATAL gnutls_serv_options.arg.alpn_fatal
#define OPT_ARG_NOTICKET gnutls_serv_options.arg.noticket
#define OPT_ARG_EARLYDATA gnutls_serv_options.arg.earlydata
#define OPT_ARG_MAXEARLYDATA gnutls_serv_options.arg.maxearlydata
#define OPT_ARG_NOCOOKIE gnutls_serv_options.arg.nocookie
#define OPT_ARG_GENERATE gnutls_serv_options.arg.generate
#define OPT_ARG_QUIET gnutls_serv_options.arg.quiet
#define OPT_ARG_NODB gnutls_serv_options.arg.nodb
#define OPT_ARG_HTTP gnutls_serv_options.arg.http
#define OPT_ARG_ECHO gnutls_serv_options.arg.echo
#define OPT_ARG_CRLF gnutls_serv_options.arg.crlf
#define OPT_ARG_UDP gnutls_serv_options.arg.udp
#define OPT_ARG_MTU gnutls_serv_options.arg.mtu
#define OPT_ARG_SRTP_PROFILES gnutls_serv_options.arg.srtp_profiles
#define OPT_ARG_DISABLE_CLIENT_CERT gnutls_serv_options.arg.disable_client_cert
#define OPT_ARG_REQUIRE_CLIENT_CERT gnutls_serv_options.arg.require_client_cert
#define OPT_ARG_VERIFY_CLIENT_CERT gnutls_serv_options.arg.verify_client_cert
#define OPT_ARG_COMPRESS_CERT gnutls_serv_options.arg.compress_cert
#define OPT_ARG_HEARTBEAT gnutls_serv_options.arg.heartbeat
#define OPT_ARG_X509FMTDER gnutls_serv_options.arg.x509fmtder
#define OPT_ARG_PRIORITY gnutls_serv_options.arg.priority
#define OPT_ARG_DHPARAMS gnutls_serv_options.arg.dhparams
#define OPT_ARG_X509CAFILE gnutls_serv_options.arg.x509cafile
#define OPT_ARG_X509CRLFILE gnutls_serv_options.arg.x509crlfile
#define OPT_ARG_PGPKEYFILE gnutls_serv_options.arg.pgpkeyfile
#define OPT_ARG_X509KEYFILE gnutls_serv_options.arg.x509keyfile
#define OPT_ARG_X509CERTFILE gnutls_serv_options.arg.x509certfile
#define OPT_ARG_RAWPKKEYFILE gnutls_serv_options.arg.rawpkkeyfile
#define OPT_ARG_RAWPKFILE gnutls_serv_options.arg.rawpkfile
#define OPT_ARG_SRPPASSWD gnutls_serv_options.arg.srppasswd
#define OPT_ARG_SRPPASSWDCONF gnutls_serv_options.arg.srppasswdconf
#define OPT_ARG_PSKPASSWD gnutls_serv_options.arg.pskpasswd
#define OPT_ARG_PSKHINT gnutls_serv_options.arg.pskhint
#define OPT_ARG_OCSP_RESPONSE gnutls_serv_options.arg.ocsp_response
#define OPT_ARG_IGNORE_OCSP_RESPONSE_ERRORS gnutls_serv_options.arg.ignore_ocsp_response_errors
#define OPT_ARG_PORT gnutls_serv_options.arg.port
#define OPT_ARG_LIST gnutls_serv_options.arg.list
#define OPT_ARG_PROVIDER gnutls_serv_options.arg.provider
#define OPT_ARG_KEYMATEXPORT gnutls_serv_options.arg.keymatexport
#define OPT_ARG_KEYMATEXPORTSIZE gnutls_serv_options.arg.keymatexportsize
#define OPT_ARG_RECORDSIZE gnutls_serv_options.arg.recordsize
#define OPT_ARG_HTTPDATA gnutls_serv_options.arg.httpdata
#define OPT_ARG_TIMEOUT gnutls_serv_options.arg.timeout
#define OPT_ARG_ATTIME gnutls_serv_options.arg.attime
#define OPT_ARG_VERSION gnutls_serv_options.arg.version
#define OPT_ARG_HELP gnutls_serv_options.arg.help
#define OPT_ARG_MORE_HELP gnutls_serv_options.arg.more_help

#define OPT_VALUE_DEBUG gnutls_serv_options.value.debug
#define OPT_VALUE_MAXEARLYDATA gnutls_serv_options.value.maxearlydata
#define OPT_VALUE_MTU gnutls_serv_options.value.mtu
#define OPT_VALUE_PORT gnutls_serv_options.value.port
#define OPT_VALUE_KEYMATEXPORTSIZE gnutls_serv_options.value.keymatexportsize
#define OPT_VALUE_RECORDSIZE gnutls_serv_options.value.recordsize
#define OPT_VALUE_TIMEOUT gnutls_serv_options.value.timeout

#define ENABLED_OPT_DEBUG gnutls_serv_options.enabled.debug
#define ENABLED_OPT_SNI_HOSTNAME gnutls_serv_options.enabled.sni_hostname
#define ENABLED_OPT_SNI_HOSTNAME_FATAL gnutls_serv_options.enabled.sni_hostname_fatal
#define ENABLED_OPT_ALPN gnutls_serv_options.enabled.alpn
#define ENABLED_OPT_ALPN_FATAL gnutls_serv_options.enabled.alpn_fatal
#define ENABLED_OPT_NOTICKET gnutls_serv_options.enabled.noticket
#define ENABLED_OPT_EARLYDATA gnutls_serv_options.enabled.earlydata
#define ENABLED_OPT_MAXEARLYDATA gnutls_serv_options.enabled.maxearlydata
#define ENABLED_OPT_NOCOOKIE gnutls_serv_options.enabled.nocookie
#define ENABLED_OPT_GENERATE gnutls_serv_options.enabled.generate
#define ENABLED_OPT_QUIET gnutls_serv_options.enabled.quiet
#define ENABLED_OPT_NODB gnutls_serv_options.enabled.nodb
#define ENABLED_OPT_HTTP gnutls_serv_options.enabled.http
#define ENABLED_OPT_ECHO gnutls_serv_options.enabled.echo
#define ENABLED_OPT_CRLF gnutls_serv_options.enabled.crlf
#define ENABLED_OPT_UDP gnutls_serv_options.enabled.udp
#define ENABLED_OPT_MTU gnutls_serv_options.enabled.mtu
#define ENABLED_OPT_SRTP_PROFILES gnutls_serv_options.enabled.srtp_profiles
#define ENABLED_OPT_DISABLE_CLIENT_CERT gnutls_serv_options.enabled.disable_client_cert
#define ENABLED_OPT_REQUIRE_CLIENT_CERT gnutls_serv_options.enabled.require_client_cert
#define ENABLED_OPT_VERIFY_CLIENT_CERT gnutls_serv_options.enabled.verify_client_cert
#define ENABLED_OPT_COMPRESS_CERT gnutls_serv_options.enabled.compress_cert
#define ENABLED_OPT_HEARTBEAT gnutls_serv_options.enabled.heartbeat
#define ENABLED_OPT_X509FMTDER gnutls_serv_options.enabled.x509fmtder
#define ENABLED_OPT_PRIORITY gnutls_serv_options.enabled.priority
#define ENABLED_OPT_DHPARAMS gnutls_serv_options.enabled.dhparams
#define ENABLED_OPT_X509CAFILE gnutls_serv_options.enabled.x509cafile
#define ENABLED_OPT_X509CRLFILE gnutls_serv_options.enabled.x509crlfile
#define ENABLED_OPT_PGPKEYFILE gnutls_serv_options.enabled.pgpkeyfile
#define ENABLED_OPT_X509KEYFILE gnutls_serv_options.enabled.x509keyfile
#define ENABLED_OPT_X509CERTFILE gnutls_serv_options.enabled.x509certfile
#define ENABLED_OPT_RAWPKKEYFILE gnutls_serv_options.enabled.rawpkkeyfile
#define ENABLED_OPT_RAWPKFILE gnutls_serv_options.enabled.rawpkfile
#define ENABLED_OPT_SRPPASSWD gnutls_serv_options.enabled.srppasswd
#define ENABLED_OPT_SRPPASSWDCONF gnutls_serv_options.enabled.srppasswdconf
#define ENABLED_OPT_PSKPASSWD gnutls_serv_options.enabled.pskpasswd
#define ENABLED_OPT_PSKHINT gnutls_serv_options.enabled.pskhint
#define ENABLED_OPT_OCSP_RESPONSE gnutls_serv_options.enabled.ocsp_response
#define ENABLED_OPT_IGNORE_OCSP_RESPONSE_ERRORS gnutls_serv_options.enabled.ignore_ocsp_response_errors
#define ENABLED_OPT_PORT gnutls_serv_options.enabled.port
#define ENABLED_OPT_LIST gnutls_serv_options.enabled.list
#define ENABLED_OPT_PROVIDER gnutls_serv_options.enabled.provider
#define ENABLED_OPT_KEYMATEXPORT gnutls_serv_options.enabled.keymatexport
#define ENABLED_OPT_KEYMATEXPORTSIZE gnutls_serv_options.enabled.keymatexportsize
#define ENABLED_OPT_RECORDSIZE gnutls_serv_options.enabled.recordsize
#define ENABLED_OPT_HTTPDATA gnutls_serv_options.enabled.httpdata
#define ENABLED_OPT_TIMEOUT gnutls_serv_options.enabled.timeout
#define ENABLED_OPT_ATTIME gnutls_serv_options.enabled.attime
#define ENABLED_OPT_VERSION gnutls_serv_options.enabled.version
#define ENABLED_OPT_HELP gnutls_serv_options.enabled.help
#define ENABLED_OPT_MORE_HELP gnutls_serv_options.enabled.more_help

#define OPTS_COUNT_ALPN gnutls_serv_options.list.alpn.count
#define OPTS_COUNT_COMPRESS_CERT gnutls_serv_options.list.compress_cert.count
#define OPTS_COUNT_X509KEYFILE gnutls_serv_options.list.x509keyfile.count
#define OPTS_COUNT_X509CERTFILE gnutls_serv_options.list.x509certfile.count
#define OPTS_COUNT_RAWPKKEYFILE gnutls_serv_options.list.rawpkkeyfile.count
#define OPTS_COUNT_RAWPKFILE gnutls_serv_options.list.rawpkfile.count
#define OPTS_COUNT_OCSP_RESPONSE gnutls_serv_options.list.ocsp_response.count

#define OPTS_ARRAY_ALPN gnutls_serv_options.list.alpn.args
#define OPTS_ARRAY_COMPRESS_CERT gnutls_serv_options.list.compress_cert.args
#define OPTS_ARRAY_X509KEYFILE gnutls_serv_options.list.x509keyfile.args
#define OPTS_ARRAY_X509CERTFILE gnutls_serv_options.list.x509certfile.args
#define OPTS_ARRAY_RAWPKKEYFILE gnutls_serv_options.list.rawpkkeyfile.args
#define OPTS_ARRAY_RAWPKFILE gnutls_serv_options.list.rawpkfile.args
#define OPTS_ARRAY_OCSP_RESPONSE gnutls_serv_options.list.ocsp_response.args


extern struct gnutls_serv_options gnutls_serv_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* GNUTLS_SERV_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
