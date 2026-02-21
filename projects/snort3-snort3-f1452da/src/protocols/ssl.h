//--------------------------------------------------------------------------
// Copyright (C) 2015-2025 Cisco and/or its affiliates. All rights reserved.
// Copyright (C) 2007-2013 Sourcefire, Inc.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// ssl.h author Adam Keeton

#ifndef SSL_H
#define SSL_H

#include <string>

#include "main/snort_types.h"

#define SSL_NO_FLAG             0x00000000

/* SSL record type flags */
#define SSL_CHANGE_CIPHER_FLAG  0x00000001
#define SSL_ALERT_FLAG          0x00000002
#define SSL_POSSIBLE_HS_FLAG    0x00000004 /* For handshakes in TLSv3 that are encrypted */
#define SSL_CLIENT_HELLO_FLAG   0x00000008
#define SSL_SERVER_HELLO_FLAG   0x00000010
#define SSL_CERTIFICATE_FLAG    0x00000020
#define SSL_SERVER_KEYX_FLAG    0x00000040
#define SSL_CLIENT_KEYX_FLAG    0x00000080
#define SSL_CIPHER_SPEC_FLAG    0x00000100
#define SSL_SFINISHED_FLAG      0x00000200
#define SSL_SAPP_FLAG           0x00000400
#define SSL_CAPP_FLAG           0x00000800
#define SSL_HS_SDONE_FLAG       0x00001000
#define SSL_HEARTBEAT_SEEN      0x00002000

/* Misc state flag */
#define SSL_POSSIBLY_ENC_FLAG   0x00004000

/* Version flags */
#define SSL_VER_SSLV2_FLAG      0x00008000
#define SSL_VER_SSLV3_FLAG      0x00010000
#define SSL_VER_TLS10_FLAG      0x00020000
#define SSL_VER_TLS11_FLAG      0x00040000
#define SSL_VER_TLS12_FLAG      0x00080000

/* Convert 3-byte lengths in TLS headers to integers. */
#define ntoh3(msb_ptr) \
    ((uint32_t)((uint32_t)(((const uint8_t*)(msb_ptr))[0] << 16) \
    + (uint32_t)(((const uint8_t*)(msb_ptr))[1] << 8) \
    + (uint32_t)(((const uint8_t*)(msb_ptr))[2])))

#define SSL_CERTS_LEN_SIZE 3

#define SSL_VERFLAGS \
    (SSL_VER_SSLV2_FLAG | SSL_VER_SSLV3_FLAG | \
    SSL_VER_TLS10_FLAG | SSL_VER_TLS11_FLAG | \
    SSL_VER_TLS12_FLAG)

#define SSL_V3_SERVER_HELLO(x) \
    (((x) & SSL_CUR_SERVER_HELLO_FLAG) \
    && ((x) & SSL_VERFLAGS) && (((x) & SSL_VERFLAGS) != SSL_VER_SSLV2_FLAG))

/* For rule state matching. These are only set when presently valid,
 * and do not stay set across packets. */
#define SSL_CUR_CLIENT_HELLO_FLAG   0x00100000
#define SSL_CUR_SERVER_HELLO_FLAG   0x00200000
#define SSL_CUR_SERVER_KEYX_FLAG    0x00400000
#define SSL_CUR_CLIENT_KEYX_FLAG    0x00800000
#define SSL_ENCRYPTED_FLAG          0x01000000 /* Provided for external use */
#define SSL_UNKNOWN_FLAG            0x02000000 /* Set when we decoded mostly garbage */

#define SSL_STATEFLAGS \
    (SSL_CUR_CLIENT_HELLO_FLAG | SSL_CUR_SERVER_HELLO_FLAG | \
    SSL_CUR_SERVER_KEYX_FLAG | SSL_CUR_CLIENT_KEYX_FLAG | \
    SSL_UNKNOWN_FLAG)

#define SSL_TLS_METADATA_FINISH_PACKET (SSL_SERVER_KEYX_FLAG | SSL_CLIENT_KEYX_FLAG | SSL_CHANGE_CIPHER_FLAG)

// Flag set when a client uses SSLv3/TLS backward compatibility and sends a
// SSLv2 Hello specifying an SSLv3/TLS version.
#define SSL_V3_BACK_COMPAT_V2   0x04000000

/* Error flags */
#define SSL_BOGUS_HS_DIR_FLAG   0x08000000 /* Record type disagrees with direction */
#define SSL_TRAILING_GARB_FLAG  0x10000000
#define SSL_BAD_TYPE_FLAG       0x20000000
#define SSL_BAD_VER_FLAG        0x40000000
#define SSL_TRUNCATED_FLAG      0x80000000
#define SSL_ARG_ERROR_FLAG      0x00000000 /* Note: overloaded with SSL_NO_FLAG */

/* The following flags are not presently of interest:
* #define SSL_CERT_URL_FLAG       (RFC 3546)
* #define SSL_CERT_STATUS_FLAG    (RFC 3546)
* #define SSL_CFINISHED_FLAG      This is contained in encrypted data
* #define SSL_HS_FINISHED_FLAG    Ignored for our purposes
*/

/* Flags for additional info */
#define SSL_ALERT_LVL_FATAL_FLAG    0x00000001
#define SSL_TLS_METADATA_PUBLISHED  0x00000002

/* The constants used below are from RFC 2246 */

/* SSLv3 & TLS Record types */
#define SSL_CHANGE_CIPHER_REC 20
#define SSL_ALERT_REC 21
#define SSL_HANDSHAKE_REC 22
#define SSL_APPLICATION_REC 23
#define SSL_HEARTBEAT_REC 24

/* SSLv3 heartbeat types */
#define SSL_HEARTBEAT_REQUEST 1
#define SSL_HEARTBEAT_RESPONSE 2

/* SSLv3 & TLS handshake types */
#define SSL_HS_HELLO_REQ 0
#define SSL_HS_CHELLO    1
#define SSL_HS_SHELLO    2
#define SSL_HS_CERT      11
#define SSL_HS_SKEYX     12
#define SSL_HS_CERT_REQ  13
#define SSL_HS_SHELLO_DONE 14
#define SSL_HS_CERT_VERIFY 15
#define SSL_HS_CKEYX     16
#define SSL_HS_FINISHED  20
#define SSL_CERT_URL     21
#define SSL_CERT_STATUS  22

/* SSLv2 handshake types */
#define SSL_V2_CHELLO  1
#define SSL_V2_CKEY    2
#define SSL_V2_SHELLO  4

#pragma pack(1)

struct SSL_record_t
{
    uint8_t type;
    uint8_t major;
    uint8_t minor;
    uint16_t length;
};

#define SSL_REC_PAYLOAD_OFFSET (sizeof(uint8_t) * 5)

#define SSL_ALERT_LEVEL_WARNING 1
#define SSL_ALERT_LEVEL_FATAL   2

struct SSL_alert_t
{
    uint8_t level;
    uint8_t description;
};

struct SSL_heartbeat
{
    uint8_t type;
    uint16_t length;
};

struct SSL_handshake_t
{
    uint8_t type;
    uint8_t length[3];
};

struct SSL_random_t
{
    uint32_t gmt_unix_time;
    uint8_t random_bytes[28];
};

struct SSL_handshake_hello_t
{
    uint8_t type;
    uint8_t length[3];
    uint8_t major;
    uint8_t minor;
    SSL_random_t random;
    uint8_t session_id_length;
};

// http://www.mozilla.org/projects/security/pki/nss/ssl/draft02.html
struct SSLv2_record_t
{
    uint16_t length;
    uint8_t type;
};

struct SSLv2_chello_t
{
    uint16_t length;
    uint8_t type;
    uint8_t major;
    uint8_t minor;
};

struct SSLv2_shello_t
{
    uint16_t length;
    uint8_t type;
    uint8_t ssnid;
    uint8_t certtype;
    uint8_t major;
    uint8_t minor;
};

struct SSLv2_client_master_key_t
{
    uint16_t length;
    uint8_t type;
    uint8_t cipher_spec[3];
};

#pragma pack()

struct SO_PUBLIC SSLV3ClientHelloData
{
    ~SSLV3ClientHelloData();
    void clear();
    char* host_name = nullptr;
};

struct SO_PUBLIC SSLV3ServerCertData
{
    ~SSLV3ServerCertData();
    void clear();
    /* While collecting certificates: */
    unsigned certs_len = 0;         // (Total) length of certificate(s).
    uint8_t* certs_data = nullptr;  // Certificate(s) data (each proceeded by length (3 bytes)).
    /* Data collected from certificates afterwards: */
    char* common_name = nullptr;
    int common_name_strlen = 0;
    char* org_unit = nullptr;
    int org_unit_strlen = 0;
    char* issuer_info = nullptr;
    int issuer_info_strlen = 0;
    char* subject_info = nullptr;
    int subject_info_strlen = 0;
};

#pragma pack(1)

struct SO_PUBLIC TLSConnectionParams
{
    int32_t curve = -1;
    int32_t cipher = -1;
    int32_t selected_tls_version = -1;
};

#pragma pack()

struct SO_PUBLIC TLSConnectionData
{
    void process(const SSLV3ServerCertData& cert_data);
    void process(const SSLV3ClientHelloData& client_hello_data);
    
    std::string server_name_identifier;
    std::string subject_info;
    std::string issuer_info;
    TLSConnectionParams tls_params;
};

enum class SSLV3RecordType : uint8_t
{
    CLIENT_HELLO = 1,
    SERVER_HELLO = 2,
    CERTIFICATE = 11,
    SERVER_KEY_XCHG = 12,
    SERVER_CERT_REQ = 13,
    SERVER_HELLO_DONE = 14,
    CERTIFICATE_STATUS = 22
};

#pragma pack(1)

/* Usually referred to as a Certificate Handshake. */
struct ServiceSSLV3CertsRecord
{
    uint8_t type;
    uint8_t length_msb;
    uint16_t length;
    uint8_t certs_len[3];  // 3-byte length, network byte order.
    /* Certificate(s) follow.
     * For each:
     *  - Length: 3 bytes
     *  - Data  : "Length" bytes */
};

/* Usually referred to as a TLS Handshake. */
struct ServiceSSLV3Record
{
    SSLV3RecordType type;
    uint8_t length_msb;
    uint16_t length;
    uint16_t version;
    struct
    {
        uint32_t time;
        uint8_t data[28];
    } random;
};

struct ServiceSSLV3ExtensionServerName
{
    uint16_t type;
    uint16_t length;
    uint16_t list_length;
    uint8_t string_length_msb;
    uint16_t string_length;
    /* String follows. */
};

struct ServiceSSLV3ExtensionSupportedVersion
{
    uint16_t type;
    uint16_t length;
    uint16_t supported_version;
};

/* Extension types. */
#define SSL_EXT_SERVER_NAME 0
#define SSL_EXT_SUPPORTED_VERSION 43

#define SSL_V2_MIN_LEN 5

#pragma pack()

#define SSL_HS_PAYLOAD_OFFSET (sizeof(uint8_t) * 4) /* Type and length fields */

#define SSL_BAD_HS(x) ((x) & SSL_BOGUS_HS_DIR_FLAG)

#define SSL_IS_HANDSHAKE(x) \
    ((x) & (SSL_CLIENT_HELLO_FLAG | SSL_SERVER_HELLO_FLAG | \
    SSL_CERTIFICATE_FLAG | SSL_SERVER_KEYX_FLAG | \
    SSL_CLIENT_KEYX_FLAG | SSL_CIPHER_SPEC_FLAG))

#define SSL_IS_CHELLO(x) ((x) & SSL_CLIENT_HELLO_FLAG)
#define SSL_IS_SHELLO(x) ((x) & SSL_SERVER_HELLO_FLAG)
#define SSL_IS_CKEYX(x) ((x) & SSL_CLIENT_KEYX_FLAG)
#define SSL_IS_CHANGE_CIPHER(x) ((x) & SSL_CHANGE_CIPHER_FLAG)
#define SSL_IS_APP(x) (((x) & SSL_SAPP_FLAG) || ((x) & SSL_CAPP_FLAG))
#define SSL_IS_ALERT(x) ((x) & SSL_ALERT_FLAG)
#define SSL_CLEAR_TEMPORARY_FLAGS(x) (x) &= ~SSL_STATEFLAGS

/* Verifies that the error flags haven't been triggered */
#define SSL_IS_CLEAN(x) \
    !((x) & (SSL_BOGUS_HS_DIR_FLAG | SSL_TRUNCATED_FLAG | \
    SSL_BAD_VER_FLAG | SSL_BAD_TYPE_FLAG | \
    SSL_TRAILING_GARB_FLAG | SSL_UNKNOWN_FLAG))

#define SSL_HEARTBLEED_REQUEST  0x01
#define SSL_HEARTBLEED_RESPONSE 0x02
#define SSL_HEARTBLEED_UNKNOWN  0x04

namespace snort
{

enum ParseHelloResult : uint8_t
{
    SUCCESS = 0,
    FRAGMENTED_PACKET = 1,
    FAILURE = 2
};

SO_PUBLIC uint32_t SSL_decode(
    const uint8_t* pkt, int size, uint32_t pktflags, uint32_t prevflags,
    uint8_t* alert_flags, uint16_t* partial_rec_len, int hblen, uint32_t* info_flags = nullptr,
    SSLV3ClientHelloData* data = nullptr, SSLV3ServerCertData* server_cert_data = nullptr, TLSConnectionParams* tls_connection_params = nullptr);

    ParseHelloResult parse_client_hello_data(const uint8_t* pkt, uint16_t size, SSLV3ClientHelloData*);
    ParseHelloResult parse_server_hello_data(const uint8_t* pkt, uint16_t size, TLSConnectionParams*);
    bool parse_server_certificates(SSLV3ServerCertData* server_cert_data);
    bool parse_server_key_exchange(const uint8_t* pkt, uint16_t size, TLSConnectionParams*);

SO_PUBLIC bool IsTlsClientHello(const uint8_t* ptr, const uint8_t* end);
SO_PUBLIC bool IsTlsServerHello(const uint8_t* ptr, const uint8_t* end);
SO_PUBLIC bool IsSSL(const uint8_t* ptr, int len, int pkt_flags);
}
#endif

