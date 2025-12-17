#include <stdbool.h>
#include <stdio.h>

#ifndef GNUTLS_CLI_OPTIONS_H_
#define GNUTLS_CLI_OPTIONS_H_ 1

struct gnutls_cli_list
{
  const char **args;
  unsigned int count;
};

struct gnutls_cli_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool verbose;
    bool tofu;
    bool strict_tofu;
    bool dane;
    bool local_dns;
    bool ca_verification;
    bool ocsp;
    bool resume;
    bool earlydata;
    bool rehandshake;
    bool sni_hostname;
    bool verify_hostname;
    bool starttls;
    bool starttls_proto;
    bool starttls_name;
    bool udp;
    bool mtu;
    bool crlf;
    bool fastopen;
    bool x509fmtder;
    bool print_cert;
    bool save_cert;
    bool save_ocsp;
    bool save_ocsp_multi;
    bool save_server_trace;
    bool save_client_trace;
    bool dh_bits;
    bool priority;
    bool x509cafile;
    bool x509crlfile;
    bool x509keyfile;
    bool x509certfile;
    bool rawpkkeyfile;
    bool rawpkfile;
    bool srpusername;
    bool srppasswd;
    bool pskusername;
    bool pskkey;
    bool port;
    bool insecure;
    bool verify_allow_broken;
    bool ranges;
    bool benchmark_ciphers;
    bool benchmark_tls_kx;
    bool benchmark_tls_ciphers;
    bool list;
    bool priority_list;
    bool noticket;
    bool srtp_profiles;
    bool alpn;
    bool compress_cert;
    bool heartbeat;
    bool recordsize;
    bool disable_sni;
    bool disable_extensions;
    bool single_key_share;
    bool post_handshake_auth;
    bool inline_commands;
    bool inline_commands_prefix;
    bool provider;
    bool fips140_mode;
    bool list_config;
    bool logfile;
    bool keymatexport;
    bool keymatexportsize;
    bool waitresumption;
    bool ca_auto_retrieve;
    bool attime;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *earlydata;
    const char *sni_hostname;
    const char *verify_hostname;
    const char *starttls_proto;
    const char *starttls_name;
    const char *mtu;
    const char *save_cert;
    const char *save_ocsp;
    const char *save_ocsp_multi;
    const char *save_server_trace;
    const char *save_client_trace;
    const char *dh_bits;
    const char *priority;
    const char *x509cafile;
    const char *x509crlfile;
    const char *x509keyfile;
    const char *x509certfile;
    const char *rawpkkeyfile;
    const char *rawpkfile;
    const char *srpusername;
    const char *srppasswd;
    const char *pskusername;
    const char *pskkey;
    const char *port;
    const char *srtp_profiles;
    const char *recordsize;
    const char *inline_commands_prefix;
    const char *provider;
    const char *logfile;
    const char *keymatexport;
    const char *keymatexportsize;
    const char *attime;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int mtu;
    int dh_bits;
    int recordsize;
    int keymatexportsize;
  } value;

  /* Option arguments parsed as list */
  struct
  {
    struct gnutls_cli_list alpn;
    struct gnutls_cli_list compress_cert;
  } list;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool verbose;
    bool tofu;
    bool strict_tofu;
    bool dane;
    bool local_dns;
    bool ca_verification;
    bool ocsp;
    bool resume;
    bool earlydata;
    bool rehandshake;
    bool sni_hostname;
    bool verify_hostname;
    bool starttls;
    bool starttls_proto;
    bool starttls_name;
    bool udp;
    bool mtu;
    bool crlf;
    bool fastopen;
    bool x509fmtder;
    bool print_cert;
    bool save_cert;
    bool save_ocsp;
    bool save_ocsp_multi;
    bool save_server_trace;
    bool save_client_trace;
    bool dh_bits;
    bool priority;
    bool x509cafile;
    bool x509crlfile;
    bool x509keyfile;
    bool x509certfile;
    bool rawpkkeyfile;
    bool rawpkfile;
    bool srpusername;
    bool srppasswd;
    bool pskusername;
    bool pskkey;
    bool port;
    bool insecure;
    bool verify_allow_broken;
    bool ranges;
    bool benchmark_ciphers;
    bool benchmark_tls_kx;
    bool benchmark_tls_ciphers;
    bool list;
    bool priority_list;
    bool noticket;
    bool srtp_profiles;
    bool alpn;
    bool compress_cert;
    bool heartbeat;
    bool recordsize;
    bool disable_sni;
    bool disable_extensions;
    bool single_key_share;
    bool post_handshake_auth;
    bool inline_commands;
    bool inline_commands_prefix;
    bool provider;
    bool fips140_mode;
    bool list_config;
    bool logfile;
    bool keymatexport;
    bool keymatexportsize;
    bool waitresumption;
    bool ca_auto_retrieve;
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

#define HAVE_OPT_DEBUG gnutls_cli_options.present.debug
#define HAVE_OPT_VERBOSE gnutls_cli_options.present.verbose
#define HAVE_OPT_TOFU gnutls_cli_options.present.tofu
#define HAVE_OPT_STRICT_TOFU gnutls_cli_options.present.strict_tofu
#define HAVE_OPT_DANE gnutls_cli_options.present.dane
#define HAVE_OPT_LOCAL_DNS gnutls_cli_options.present.local_dns
#define HAVE_OPT_CA_VERIFICATION gnutls_cli_options.present.ca_verification
#define HAVE_OPT_OCSP gnutls_cli_options.present.ocsp
#define HAVE_OPT_RESUME gnutls_cli_options.present.resume
#define HAVE_OPT_EARLYDATA gnutls_cli_options.present.earlydata
#define HAVE_OPT_REHANDSHAKE gnutls_cli_options.present.rehandshake
#define HAVE_OPT_SNI_HOSTNAME gnutls_cli_options.present.sni_hostname
#define HAVE_OPT_VERIFY_HOSTNAME gnutls_cli_options.present.verify_hostname
#define HAVE_OPT_STARTTLS gnutls_cli_options.present.starttls
#define HAVE_OPT_STARTTLS_PROTO gnutls_cli_options.present.starttls_proto
#define HAVE_OPT_STARTTLS_NAME gnutls_cli_options.present.starttls_name
#define HAVE_OPT_UDP gnutls_cli_options.present.udp
#define HAVE_OPT_MTU gnutls_cli_options.present.mtu
#define HAVE_OPT_CRLF gnutls_cli_options.present.crlf
#define HAVE_OPT_FASTOPEN gnutls_cli_options.present.fastopen
#define HAVE_OPT_X509FMTDER gnutls_cli_options.present.x509fmtder
#define HAVE_OPT_PRINT_CERT gnutls_cli_options.present.print_cert
#define HAVE_OPT_SAVE_CERT gnutls_cli_options.present.save_cert
#define HAVE_OPT_SAVE_OCSP gnutls_cli_options.present.save_ocsp
#define HAVE_OPT_SAVE_OCSP_MULTI gnutls_cli_options.present.save_ocsp_multi
#define HAVE_OPT_SAVE_SERVER_TRACE gnutls_cli_options.present.save_server_trace
#define HAVE_OPT_SAVE_CLIENT_TRACE gnutls_cli_options.present.save_client_trace
#define HAVE_OPT_DH_BITS gnutls_cli_options.present.dh_bits
#define HAVE_OPT_PRIORITY gnutls_cli_options.present.priority
#define HAVE_OPT_X509CAFILE gnutls_cli_options.present.x509cafile
#define HAVE_OPT_X509CRLFILE gnutls_cli_options.present.x509crlfile
#define HAVE_OPT_X509KEYFILE gnutls_cli_options.present.x509keyfile
#define HAVE_OPT_X509CERTFILE gnutls_cli_options.present.x509certfile
#define HAVE_OPT_RAWPKKEYFILE gnutls_cli_options.present.rawpkkeyfile
#define HAVE_OPT_RAWPKFILE gnutls_cli_options.present.rawpkfile
#define HAVE_OPT_SRPUSERNAME gnutls_cli_options.present.srpusername
#define HAVE_OPT_SRPPASSWD gnutls_cli_options.present.srppasswd
#define HAVE_OPT_PSKUSERNAME gnutls_cli_options.present.pskusername
#define HAVE_OPT_PSKKEY gnutls_cli_options.present.pskkey
#define HAVE_OPT_PORT gnutls_cli_options.present.port
#define HAVE_OPT_INSECURE gnutls_cli_options.present.insecure
#define HAVE_OPT_VERIFY_ALLOW_BROKEN gnutls_cli_options.present.verify_allow_broken
#define HAVE_OPT_RANGES gnutls_cli_options.present.ranges
#define HAVE_OPT_BENCHMARK_CIPHERS gnutls_cli_options.present.benchmark_ciphers
#define HAVE_OPT_BENCHMARK_TLS_KX gnutls_cli_options.present.benchmark_tls_kx
#define HAVE_OPT_BENCHMARK_TLS_CIPHERS gnutls_cli_options.present.benchmark_tls_ciphers
#define HAVE_OPT_LIST gnutls_cli_options.present.list
#define HAVE_OPT_PRIORITY_LIST gnutls_cli_options.present.priority_list
#define HAVE_OPT_NOTICKET gnutls_cli_options.present.noticket
#define HAVE_OPT_SRTP_PROFILES gnutls_cli_options.present.srtp_profiles
#define HAVE_OPT_ALPN gnutls_cli_options.present.alpn
#define HAVE_OPT_COMPRESS_CERT gnutls_cli_options.present.compress_cert
#define HAVE_OPT_HEARTBEAT gnutls_cli_options.present.heartbeat
#define HAVE_OPT_RECORDSIZE gnutls_cli_options.present.recordsize
#define HAVE_OPT_DISABLE_SNI gnutls_cli_options.present.disable_sni
#define HAVE_OPT_DISABLE_EXTENSIONS gnutls_cli_options.present.disable_extensions
#define HAVE_OPT_SINGLE_KEY_SHARE gnutls_cli_options.present.single_key_share
#define HAVE_OPT_POST_HANDSHAKE_AUTH gnutls_cli_options.present.post_handshake_auth
#define HAVE_OPT_INLINE_COMMANDS gnutls_cli_options.present.inline_commands
#define HAVE_OPT_INLINE_COMMANDS_PREFIX gnutls_cli_options.present.inline_commands_prefix
#define HAVE_OPT_PROVIDER gnutls_cli_options.present.provider
#define HAVE_OPT_FIPS140_MODE gnutls_cli_options.present.fips140_mode
#define HAVE_OPT_LIST_CONFIG gnutls_cli_options.present.list_config
#define HAVE_OPT_LOGFILE gnutls_cli_options.present.logfile
#define HAVE_OPT_KEYMATEXPORT gnutls_cli_options.present.keymatexport
#define HAVE_OPT_KEYMATEXPORTSIZE gnutls_cli_options.present.keymatexportsize
#define HAVE_OPT_WAITRESUMPTION gnutls_cli_options.present.waitresumption
#define HAVE_OPT_CA_AUTO_RETRIEVE gnutls_cli_options.present.ca_auto_retrieve
#define HAVE_OPT_ATTIME gnutls_cli_options.present.attime
#define HAVE_OPT_VERSION gnutls_cli_options.present.version
#define HAVE_OPT_HELP gnutls_cli_options.present.help
#define HAVE_OPT_MORE_HELP gnutls_cli_options.present.more_help

#define OPT_ARG_DEBUG gnutls_cli_options.arg.debug
#define OPT_ARG_VERBOSE gnutls_cli_options.arg.verbose
#define OPT_ARG_TOFU gnutls_cli_options.arg.tofu
#define OPT_ARG_STRICT_TOFU gnutls_cli_options.arg.strict_tofu
#define OPT_ARG_DANE gnutls_cli_options.arg.dane
#define OPT_ARG_LOCAL_DNS gnutls_cli_options.arg.local_dns
#define OPT_ARG_CA_VERIFICATION gnutls_cli_options.arg.ca_verification
#define OPT_ARG_OCSP gnutls_cli_options.arg.ocsp
#define OPT_ARG_RESUME gnutls_cli_options.arg.resume
#define OPT_ARG_EARLYDATA gnutls_cli_options.arg.earlydata
#define OPT_ARG_REHANDSHAKE gnutls_cli_options.arg.rehandshake
#define OPT_ARG_SNI_HOSTNAME gnutls_cli_options.arg.sni_hostname
#define OPT_ARG_VERIFY_HOSTNAME gnutls_cli_options.arg.verify_hostname
#define OPT_ARG_STARTTLS gnutls_cli_options.arg.starttls
#define OPT_ARG_STARTTLS_PROTO gnutls_cli_options.arg.starttls_proto
#define OPT_ARG_STARTTLS_NAME gnutls_cli_options.arg.starttls_name
#define OPT_ARG_UDP gnutls_cli_options.arg.udp
#define OPT_ARG_MTU gnutls_cli_options.arg.mtu
#define OPT_ARG_CRLF gnutls_cli_options.arg.crlf
#define OPT_ARG_FASTOPEN gnutls_cli_options.arg.fastopen
#define OPT_ARG_X509FMTDER gnutls_cli_options.arg.x509fmtder
#define OPT_ARG_PRINT_CERT gnutls_cli_options.arg.print_cert
#define OPT_ARG_SAVE_CERT gnutls_cli_options.arg.save_cert
#define OPT_ARG_SAVE_OCSP gnutls_cli_options.arg.save_ocsp
#define OPT_ARG_SAVE_OCSP_MULTI gnutls_cli_options.arg.save_ocsp_multi
#define OPT_ARG_SAVE_SERVER_TRACE gnutls_cli_options.arg.save_server_trace
#define OPT_ARG_SAVE_CLIENT_TRACE gnutls_cli_options.arg.save_client_trace
#define OPT_ARG_DH_BITS gnutls_cli_options.arg.dh_bits
#define OPT_ARG_PRIORITY gnutls_cli_options.arg.priority
#define OPT_ARG_X509CAFILE gnutls_cli_options.arg.x509cafile
#define OPT_ARG_X509CRLFILE gnutls_cli_options.arg.x509crlfile
#define OPT_ARG_X509KEYFILE gnutls_cli_options.arg.x509keyfile
#define OPT_ARG_X509CERTFILE gnutls_cli_options.arg.x509certfile
#define OPT_ARG_RAWPKKEYFILE gnutls_cli_options.arg.rawpkkeyfile
#define OPT_ARG_RAWPKFILE gnutls_cli_options.arg.rawpkfile
#define OPT_ARG_SRPUSERNAME gnutls_cli_options.arg.srpusername
#define OPT_ARG_SRPPASSWD gnutls_cli_options.arg.srppasswd
#define OPT_ARG_PSKUSERNAME gnutls_cli_options.arg.pskusername
#define OPT_ARG_PSKKEY gnutls_cli_options.arg.pskkey
#define OPT_ARG_PORT gnutls_cli_options.arg.port
#define OPT_ARG_INSECURE gnutls_cli_options.arg.insecure
#define OPT_ARG_VERIFY_ALLOW_BROKEN gnutls_cli_options.arg.verify_allow_broken
#define OPT_ARG_RANGES gnutls_cli_options.arg.ranges
#define OPT_ARG_BENCHMARK_CIPHERS gnutls_cli_options.arg.benchmark_ciphers
#define OPT_ARG_BENCHMARK_TLS_KX gnutls_cli_options.arg.benchmark_tls_kx
#define OPT_ARG_BENCHMARK_TLS_CIPHERS gnutls_cli_options.arg.benchmark_tls_ciphers
#define OPT_ARG_LIST gnutls_cli_options.arg.list
#define OPT_ARG_PRIORITY_LIST gnutls_cli_options.arg.priority_list
#define OPT_ARG_NOTICKET gnutls_cli_options.arg.noticket
#define OPT_ARG_SRTP_PROFILES gnutls_cli_options.arg.srtp_profiles
#define OPT_ARG_ALPN gnutls_cli_options.arg.alpn
#define OPT_ARG_COMPRESS_CERT gnutls_cli_options.arg.compress_cert
#define OPT_ARG_HEARTBEAT gnutls_cli_options.arg.heartbeat
#define OPT_ARG_RECORDSIZE gnutls_cli_options.arg.recordsize
#define OPT_ARG_DISABLE_SNI gnutls_cli_options.arg.disable_sni
#define OPT_ARG_DISABLE_EXTENSIONS gnutls_cli_options.arg.disable_extensions
#define OPT_ARG_SINGLE_KEY_SHARE gnutls_cli_options.arg.single_key_share
#define OPT_ARG_POST_HANDSHAKE_AUTH gnutls_cli_options.arg.post_handshake_auth
#define OPT_ARG_INLINE_COMMANDS gnutls_cli_options.arg.inline_commands
#define OPT_ARG_INLINE_COMMANDS_PREFIX gnutls_cli_options.arg.inline_commands_prefix
#define OPT_ARG_PROVIDER gnutls_cli_options.arg.provider
#define OPT_ARG_FIPS140_MODE gnutls_cli_options.arg.fips140_mode
#define OPT_ARG_LIST_CONFIG gnutls_cli_options.arg.list_config
#define OPT_ARG_LOGFILE gnutls_cli_options.arg.logfile
#define OPT_ARG_KEYMATEXPORT gnutls_cli_options.arg.keymatexport
#define OPT_ARG_KEYMATEXPORTSIZE gnutls_cli_options.arg.keymatexportsize
#define OPT_ARG_WAITRESUMPTION gnutls_cli_options.arg.waitresumption
#define OPT_ARG_CA_AUTO_RETRIEVE gnutls_cli_options.arg.ca_auto_retrieve
#define OPT_ARG_ATTIME gnutls_cli_options.arg.attime
#define OPT_ARG_VERSION gnutls_cli_options.arg.version
#define OPT_ARG_HELP gnutls_cli_options.arg.help
#define OPT_ARG_MORE_HELP gnutls_cli_options.arg.more_help

#define OPT_VALUE_DEBUG gnutls_cli_options.value.debug
#define OPT_VALUE_MTU gnutls_cli_options.value.mtu
#define OPT_VALUE_DH_BITS gnutls_cli_options.value.dh_bits
#define OPT_VALUE_RECORDSIZE gnutls_cli_options.value.recordsize
#define OPT_VALUE_KEYMATEXPORTSIZE gnutls_cli_options.value.keymatexportsize

#define ENABLED_OPT_DEBUG gnutls_cli_options.enabled.debug
#define ENABLED_OPT_VERBOSE gnutls_cli_options.enabled.verbose
#define ENABLED_OPT_TOFU gnutls_cli_options.enabled.tofu
#define ENABLED_OPT_STRICT_TOFU gnutls_cli_options.enabled.strict_tofu
#define ENABLED_OPT_DANE gnutls_cli_options.enabled.dane
#define ENABLED_OPT_LOCAL_DNS gnutls_cli_options.enabled.local_dns
#define ENABLED_OPT_CA_VERIFICATION gnutls_cli_options.enabled.ca_verification
#define ENABLED_OPT_OCSP gnutls_cli_options.enabled.ocsp
#define ENABLED_OPT_RESUME gnutls_cli_options.enabled.resume
#define ENABLED_OPT_EARLYDATA gnutls_cli_options.enabled.earlydata
#define ENABLED_OPT_REHANDSHAKE gnutls_cli_options.enabled.rehandshake
#define ENABLED_OPT_SNI_HOSTNAME gnutls_cli_options.enabled.sni_hostname
#define ENABLED_OPT_VERIFY_HOSTNAME gnutls_cli_options.enabled.verify_hostname
#define ENABLED_OPT_STARTTLS gnutls_cli_options.enabled.starttls
#define ENABLED_OPT_STARTTLS_PROTO gnutls_cli_options.enabled.starttls_proto
#define ENABLED_OPT_STARTTLS_NAME gnutls_cli_options.enabled.starttls_name
#define ENABLED_OPT_UDP gnutls_cli_options.enabled.udp
#define ENABLED_OPT_MTU gnutls_cli_options.enabled.mtu
#define ENABLED_OPT_CRLF gnutls_cli_options.enabled.crlf
#define ENABLED_OPT_FASTOPEN gnutls_cli_options.enabled.fastopen
#define ENABLED_OPT_X509FMTDER gnutls_cli_options.enabled.x509fmtder
#define ENABLED_OPT_PRINT_CERT gnutls_cli_options.enabled.print_cert
#define ENABLED_OPT_SAVE_CERT gnutls_cli_options.enabled.save_cert
#define ENABLED_OPT_SAVE_OCSP gnutls_cli_options.enabled.save_ocsp
#define ENABLED_OPT_SAVE_OCSP_MULTI gnutls_cli_options.enabled.save_ocsp_multi
#define ENABLED_OPT_SAVE_SERVER_TRACE gnutls_cli_options.enabled.save_server_trace
#define ENABLED_OPT_SAVE_CLIENT_TRACE gnutls_cli_options.enabled.save_client_trace
#define ENABLED_OPT_DH_BITS gnutls_cli_options.enabled.dh_bits
#define ENABLED_OPT_PRIORITY gnutls_cli_options.enabled.priority
#define ENABLED_OPT_X509CAFILE gnutls_cli_options.enabled.x509cafile
#define ENABLED_OPT_X509CRLFILE gnutls_cli_options.enabled.x509crlfile
#define ENABLED_OPT_X509KEYFILE gnutls_cli_options.enabled.x509keyfile
#define ENABLED_OPT_X509CERTFILE gnutls_cli_options.enabled.x509certfile
#define ENABLED_OPT_RAWPKKEYFILE gnutls_cli_options.enabled.rawpkkeyfile
#define ENABLED_OPT_RAWPKFILE gnutls_cli_options.enabled.rawpkfile
#define ENABLED_OPT_SRPUSERNAME gnutls_cli_options.enabled.srpusername
#define ENABLED_OPT_SRPPASSWD gnutls_cli_options.enabled.srppasswd
#define ENABLED_OPT_PSKUSERNAME gnutls_cli_options.enabled.pskusername
#define ENABLED_OPT_PSKKEY gnutls_cli_options.enabled.pskkey
#define ENABLED_OPT_PORT gnutls_cli_options.enabled.port
#define ENABLED_OPT_INSECURE gnutls_cli_options.enabled.insecure
#define ENABLED_OPT_VERIFY_ALLOW_BROKEN gnutls_cli_options.enabled.verify_allow_broken
#define ENABLED_OPT_RANGES gnutls_cli_options.enabled.ranges
#define ENABLED_OPT_BENCHMARK_CIPHERS gnutls_cli_options.enabled.benchmark_ciphers
#define ENABLED_OPT_BENCHMARK_TLS_KX gnutls_cli_options.enabled.benchmark_tls_kx
#define ENABLED_OPT_BENCHMARK_TLS_CIPHERS gnutls_cli_options.enabled.benchmark_tls_ciphers
#define ENABLED_OPT_LIST gnutls_cli_options.enabled.list
#define ENABLED_OPT_PRIORITY_LIST gnutls_cli_options.enabled.priority_list
#define ENABLED_OPT_NOTICKET gnutls_cli_options.enabled.noticket
#define ENABLED_OPT_SRTP_PROFILES gnutls_cli_options.enabled.srtp_profiles
#define ENABLED_OPT_ALPN gnutls_cli_options.enabled.alpn
#define ENABLED_OPT_COMPRESS_CERT gnutls_cli_options.enabled.compress_cert
#define ENABLED_OPT_HEARTBEAT gnutls_cli_options.enabled.heartbeat
#define ENABLED_OPT_RECORDSIZE gnutls_cli_options.enabled.recordsize
#define ENABLED_OPT_DISABLE_SNI gnutls_cli_options.enabled.disable_sni
#define ENABLED_OPT_DISABLE_EXTENSIONS gnutls_cli_options.enabled.disable_extensions
#define ENABLED_OPT_SINGLE_KEY_SHARE gnutls_cli_options.enabled.single_key_share
#define ENABLED_OPT_POST_HANDSHAKE_AUTH gnutls_cli_options.enabled.post_handshake_auth
#define ENABLED_OPT_INLINE_COMMANDS gnutls_cli_options.enabled.inline_commands
#define ENABLED_OPT_INLINE_COMMANDS_PREFIX gnutls_cli_options.enabled.inline_commands_prefix
#define ENABLED_OPT_PROVIDER gnutls_cli_options.enabled.provider
#define ENABLED_OPT_FIPS140_MODE gnutls_cli_options.enabled.fips140_mode
#define ENABLED_OPT_LIST_CONFIG gnutls_cli_options.enabled.list_config
#define ENABLED_OPT_LOGFILE gnutls_cli_options.enabled.logfile
#define ENABLED_OPT_KEYMATEXPORT gnutls_cli_options.enabled.keymatexport
#define ENABLED_OPT_KEYMATEXPORTSIZE gnutls_cli_options.enabled.keymatexportsize
#define ENABLED_OPT_WAITRESUMPTION gnutls_cli_options.enabled.waitresumption
#define ENABLED_OPT_CA_AUTO_RETRIEVE gnutls_cli_options.enabled.ca_auto_retrieve
#define ENABLED_OPT_ATTIME gnutls_cli_options.enabled.attime
#define ENABLED_OPT_VERSION gnutls_cli_options.enabled.version
#define ENABLED_OPT_HELP gnutls_cli_options.enabled.help
#define ENABLED_OPT_MORE_HELP gnutls_cli_options.enabled.more_help

#define OPTS_COUNT_ALPN gnutls_cli_options.list.alpn.count
#define OPTS_COUNT_COMPRESS_CERT gnutls_cli_options.list.compress_cert.count

#define OPTS_ARRAY_ALPN gnutls_cli_options.list.alpn.args
#define OPTS_ARRAY_COMPRESS_CERT gnutls_cli_options.list.compress_cert.args


extern struct gnutls_cli_options gnutls_cli_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* GNUTLS_CLI_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
