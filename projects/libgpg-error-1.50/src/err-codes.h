/* Output of mkstrtable.awk.  DO NOT EDIT.  */

/* err-codes.h - List of error codes and their description.
   Copyright (C) 2003, 2004 g10 Code GmbH

   This file is part of libgpg-error.

   libgpg-error is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   libgpg-error is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with libgpg-error; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */


/* The purpose of this complex string table is to produce
   optimal code with a minimum of relocations.  */

static const char msgstr[] = 
  gettext_noop ("Success") "\0"
  gettext_noop ("General error") "\0"
  gettext_noop ("Unknown packet") "\0"
  gettext_noop ("Unknown version in packet") "\0"
  gettext_noop ("Invalid public key algorithm") "\0"
  gettext_noop ("Invalid digest algorithm") "\0"
  gettext_noop ("Bad public key") "\0"
  gettext_noop ("Bad secret key") "\0"
  gettext_noop ("Bad signature") "\0"
  gettext_noop ("No public key") "\0"
  gettext_noop ("Checksum error") "\0"
  gettext_noop ("Bad passphrase") "\0"
  gettext_noop ("Invalid cipher algorithm") "\0"
  gettext_noop ("Cannot open keyring") "\0"
  gettext_noop ("Invalid packet") "\0"
  gettext_noop ("Invalid armor") "\0"
  gettext_noop ("No user ID") "\0"
  gettext_noop ("No secret key") "\0"
  gettext_noop ("Wrong secret key used") "\0"
  gettext_noop ("Bad session key") "\0"
  gettext_noop ("Unknown compression algorithm") "\0"
  gettext_noop ("Number is not prime") "\0"
  gettext_noop ("Invalid encoding method") "\0"
  gettext_noop ("Invalid encryption scheme") "\0"
  gettext_noop ("Invalid signature scheme") "\0"
  gettext_noop ("Invalid attribute") "\0"
  gettext_noop ("No value") "\0"
  gettext_noop ("Not found") "\0"
  gettext_noop ("Value not found") "\0"
  gettext_noop ("Syntax error") "\0"
  gettext_noop ("Bad MPI value") "\0"
  gettext_noop ("Invalid passphrase") "\0"
  gettext_noop ("Invalid signature class") "\0"
  gettext_noop ("Resources exhausted") "\0"
  gettext_noop ("Invalid keyring") "\0"
  gettext_noop ("Trust DB error") "\0"
  gettext_noop ("Bad certificate") "\0"
  gettext_noop ("Invalid user ID") "\0"
  gettext_noop ("Unexpected error") "\0"
  gettext_noop ("Time conflict") "\0"
  gettext_noop ("Keyserver error") "\0"
  gettext_noop ("Wrong public key algorithm") "\0"
  gettext_noop ("Tribute to D. A.") "\0"
  gettext_noop ("Weak encryption key") "\0"
  gettext_noop ("Invalid key length") "\0"
  gettext_noop ("Invalid argument") "\0"
  gettext_noop ("Syntax error in URI") "\0"
  gettext_noop ("Invalid URI") "\0"
  gettext_noop ("Network error") "\0"
  gettext_noop ("Unknown host") "\0"
  gettext_noop ("Selftest failed") "\0"
  gettext_noop ("Data not encrypted") "\0"
  gettext_noop ("Data not processed") "\0"
  gettext_noop ("Unusable public key") "\0"
  gettext_noop ("Unusable secret key") "\0"
  gettext_noop ("Invalid value") "\0"
  gettext_noop ("Bad certificate chain") "\0"
  gettext_noop ("Missing certificate") "\0"
  gettext_noop ("No data") "\0"
  gettext_noop ("Bug") "\0"
  gettext_noop ("Not supported") "\0"
  gettext_noop ("Invalid operation code") "\0"
  gettext_noop ("Timeout") "\0"
  gettext_noop ("Internal error") "\0"
  gettext_noop ("EOF (gcrypt)") "\0"
  gettext_noop ("Invalid object") "\0"
  gettext_noop ("Provided object is too short") "\0"
  gettext_noop ("Provided object is too large") "\0"
  gettext_noop ("Missing item in object") "\0"
  gettext_noop ("Not implemented") "\0"
  gettext_noop ("Conflicting use") "\0"
  gettext_noop ("Invalid cipher mode") "\0"
  gettext_noop ("Invalid flag") "\0"
  gettext_noop ("Invalid handle") "\0"
  gettext_noop ("Result truncated") "\0"
  gettext_noop ("Incomplete line") "\0"
  gettext_noop ("Invalid response") "\0"
  gettext_noop ("No agent running") "\0"
  gettext_noop ("Agent error") "\0"
  gettext_noop ("Invalid data") "\0"
  gettext_noop ("Unspecific Assuan server fault") "\0"
  gettext_noop ("General Assuan error") "\0"
  gettext_noop ("Invalid session key") "\0"
  gettext_noop ("Invalid S-expression") "\0"
  gettext_noop ("Unsupported algorithm") "\0"
  gettext_noop ("No pinentry") "\0"
  gettext_noop ("pinentry error") "\0"
  gettext_noop ("Bad PIN") "\0"
  gettext_noop ("Invalid name") "\0"
  gettext_noop ("Bad data") "\0"
  gettext_noop ("Invalid parameter") "\0"
  gettext_noop ("Wrong card") "\0"
  gettext_noop ("No dirmngr") "\0"
  gettext_noop ("dirmngr error") "\0"
  gettext_noop ("Certificate revoked") "\0"
  gettext_noop ("No CRL known") "\0"
  gettext_noop ("CRL too old") "\0"
  gettext_noop ("Line too long") "\0"
  gettext_noop ("Not trusted") "\0"
  gettext_noop ("Operation cancelled") "\0"
  gettext_noop ("Bad CA certificate") "\0"
  gettext_noop ("Certificate expired") "\0"
  gettext_noop ("Certificate too young") "\0"
  gettext_noop ("Unsupported certificate") "\0"
  gettext_noop ("Unknown S-expression") "\0"
  gettext_noop ("Unsupported protection") "\0"
  gettext_noop ("Corrupted protection") "\0"
  gettext_noop ("Ambiguous name") "\0"
  gettext_noop ("Card error") "\0"
  gettext_noop ("Card reset required") "\0"
  gettext_noop ("Card removed") "\0"
  gettext_noop ("Invalid card") "\0"
  gettext_noop ("Card not present") "\0"
  gettext_noop ("No PKCS15 application") "\0"
  gettext_noop ("Not confirmed") "\0"
  gettext_noop ("Configuration error") "\0"
  gettext_noop ("No policy match") "\0"
  gettext_noop ("Invalid index") "\0"
  gettext_noop ("Invalid ID") "\0"
  gettext_noop ("No SmartCard daemon") "\0"
  gettext_noop ("SmartCard daemon error") "\0"
  gettext_noop ("Unsupported protocol") "\0"
  gettext_noop ("Bad PIN method") "\0"
  gettext_noop ("Card not initialized") "\0"
  gettext_noop ("Unsupported operation") "\0"
  gettext_noop ("Wrong key usage") "\0"
  gettext_noop ("Nothing found") "\0"
  gettext_noop ("Wrong blob type") "\0"
  gettext_noop ("Missing value") "\0"
  gettext_noop ("Hardware problem") "\0"
  gettext_noop ("PIN blocked") "\0"
  gettext_noop ("Conditions of use not satisfied") "\0"
  gettext_noop ("PINs are not synced") "\0"
  gettext_noop ("Invalid CRL") "\0"
  gettext_noop ("BER error") "\0"
  gettext_noop ("Invalid BER") "\0"
  gettext_noop ("Element not found") "\0"
  gettext_noop ("Identifier not found") "\0"
  gettext_noop ("Invalid tag") "\0"
  gettext_noop ("Invalid length") "\0"
  gettext_noop ("Invalid key info") "\0"
  gettext_noop ("Unexpected tag") "\0"
  gettext_noop ("Not DER encoded") "\0"
  gettext_noop ("No CMS object") "\0"
  gettext_noop ("Invalid CMS object") "\0"
  gettext_noop ("Unknown CMS object") "\0"
  gettext_noop ("Unsupported CMS object") "\0"
  gettext_noop ("Unsupported encoding") "\0"
  gettext_noop ("Unsupported CMS version") "\0"
  gettext_noop ("Unknown algorithm") "\0"
  gettext_noop ("Invalid crypto engine") "\0"
  gettext_noop ("Public key not trusted") "\0"
  gettext_noop ("Decryption failed") "\0"
  gettext_noop ("Key expired") "\0"
  gettext_noop ("Signature expired") "\0"
  gettext_noop ("Encoding problem") "\0"
  gettext_noop ("Invalid state") "\0"
  gettext_noop ("Duplicated value") "\0"
  gettext_noop ("Missing action") "\0"
  gettext_noop ("ASN.1 module not found") "\0"
  gettext_noop ("Invalid OID string") "\0"
  gettext_noop ("Invalid time") "\0"
  gettext_noop ("Invalid CRL object") "\0"
  gettext_noop ("Unsupported CRL version") "\0"
  gettext_noop ("Invalid certificate object") "\0"
  gettext_noop ("Unknown name") "\0"
  gettext_noop ("A locale function failed") "\0"
  gettext_noop ("Not locked") "\0"
  gettext_noop ("Protocol violation") "\0"
  gettext_noop ("Invalid MAC") "\0"
  gettext_noop ("Invalid request") "\0"
  gettext_noop ("Unknown extension") "\0"
  gettext_noop ("Unknown critical extension") "\0"
  gettext_noop ("Locked") "\0"
  gettext_noop ("Unknown option") "\0"
  gettext_noop ("Unknown command") "\0"
  gettext_noop ("Not operational") "\0"
  gettext_noop ("No passphrase given") "\0"
  gettext_noop ("No PIN given") "\0"
  gettext_noop ("Not enabled") "\0"
  gettext_noop ("No crypto engine") "\0"
  gettext_noop ("Missing key") "\0"
  gettext_noop ("Too many objects") "\0"
  gettext_noop ("Limit reached") "\0"
  gettext_noop ("Not initialized") "\0"
  gettext_noop ("Missing issuer certificate") "\0"
  gettext_noop ("No keyserver available") "\0"
  gettext_noop ("Invalid elliptic curve") "\0"
  gettext_noop ("Unknown elliptic curve") "\0"
  gettext_noop ("Duplicated key") "\0"
  gettext_noop ("Ambiguous result") "\0"
  gettext_noop ("No crypto context") "\0"
  gettext_noop ("Wrong crypto context") "\0"
  gettext_noop ("Bad crypto context") "\0"
  gettext_noop ("Conflict in the crypto context") "\0"
  gettext_noop ("Broken public key") "\0"
  gettext_noop ("Broken secret key") "\0"
  gettext_noop ("Invalid MAC algorithm") "\0"
  gettext_noop ("Operation fully cancelled") "\0"
  gettext_noop ("Operation not yet finished") "\0"
  gettext_noop ("Buffer too short") "\0"
  gettext_noop ("Invalid length specifier in S-expression") "\0"
  gettext_noop ("String too long in S-expression") "\0"
  gettext_noop ("Unmatched parentheses in S-expression") "\0"
  gettext_noop ("S-expression not canonical") "\0"
  gettext_noop ("Bad character in S-expression") "\0"
  gettext_noop ("Bad quotation in S-expression") "\0"
  gettext_noop ("Zero prefix in S-expression") "\0"
  gettext_noop ("Nested display hints in S-expression") "\0"
  gettext_noop ("Unmatched display hints") "\0"
  gettext_noop ("Unexpected reserved punctuation in S-expression") "\0"
  gettext_noop ("Bad hexadecimal character in S-expression") "\0"
  gettext_noop ("Odd hexadecimal numbers in S-expression") "\0"
  gettext_noop ("Bad octal character in S-expression") "\0"
  gettext_noop ("All subkeys are expired or revoked") "\0"
  gettext_noop ("Database is corrupted") "\0"
  gettext_noop ("Server indicated a failure") "\0"
  gettext_noop ("No name") "\0"
  gettext_noop ("No key") "\0"
  gettext_noop ("Legacy key") "\0"
  gettext_noop ("Request too short") "\0"
  gettext_noop ("Request too long") "\0"
  gettext_noop ("Object is in termination state") "\0"
  gettext_noop ("No certificate chain") "\0"
  gettext_noop ("Certificate is too large") "\0"
  gettext_noop ("Invalid record") "\0"
  gettext_noop ("The MAC does not verify") "\0"
  gettext_noop ("Unexpected message") "\0"
  gettext_noop ("Compression or decompression failed") "\0"
  gettext_noop ("A counter would wrap") "\0"
  gettext_noop ("Fatal alert message received") "\0"
  gettext_noop ("No cipher algorithm") "\0"
  gettext_noop ("Missing client certificate") "\0"
  gettext_noop ("Close notification received") "\0"
  gettext_noop ("Ticket expired") "\0"
  gettext_noop ("Bad ticket") "\0"
  gettext_noop ("Unknown identity") "\0"
  gettext_noop ("Bad certificate message in handshake") "\0"
  gettext_noop ("Bad certificate request message in handshake") "\0"
  gettext_noop ("Bad certificate verify message in handshake") "\0"
  gettext_noop ("Bad change cipher message in handshake") "\0"
  gettext_noop ("Bad client hello message in handshake") "\0"
  gettext_noop ("Bad server hello message in handshake") "\0"
  gettext_noop ("Bad server hello done message in handshake") "\0"
  gettext_noop ("Bad finished message in handshake") "\0"
  gettext_noop ("Bad server key exchange message in handshake") "\0"
  gettext_noop ("Bad client key exchange message in handshake") "\0"
  gettext_noop ("Bogus string") "\0"
  gettext_noop ("Forbidden") "\0"
  gettext_noop ("Key disabled") "\0"
  gettext_noop ("Not possible with a card based key") "\0"
  gettext_noop ("Invalid lock object") "\0"
  gettext_noop ("True") "\0"
  gettext_noop ("False") "\0"
  gettext_noop ("General IPC error") "\0"
  gettext_noop ("IPC accept call failed") "\0"
  gettext_noop ("IPC connect call failed") "\0"
  gettext_noop ("Invalid IPC response") "\0"
  gettext_noop ("Invalid value passed to IPC") "\0"
  gettext_noop ("Incomplete line passed to IPC") "\0"
  gettext_noop ("Line passed to IPC too long") "\0"
  gettext_noop ("Nested IPC commands") "\0"
  gettext_noop ("No data callback in IPC") "\0"
  gettext_noop ("No inquire callback in IPC") "\0"
  gettext_noop ("Not an IPC server") "\0"
  gettext_noop ("Not an IPC client") "\0"
  gettext_noop ("Problem starting IPC server") "\0"
  gettext_noop ("IPC read error") "\0"
  gettext_noop ("IPC write error") "\0"
  gettext_noop ("Too much data for IPC layer") "\0"
  gettext_noop ("Unexpected IPC command") "\0"
  gettext_noop ("Unknown IPC command") "\0"
  gettext_noop ("IPC syntax error") "\0"
  gettext_noop ("IPC call has been cancelled") "\0"
  gettext_noop ("No input source for IPC") "\0"
  gettext_noop ("No output source for IPC") "\0"
  gettext_noop ("IPC parameter error") "\0"
  gettext_noop ("Unknown IPC inquire") "\0"
  gettext_noop ("Crypto engine too old") "\0"
  gettext_noop ("Screen or window too small") "\0"
  gettext_noop ("Screen or window too large") "\0"
  gettext_noop ("Required environment variable not set") "\0"
  gettext_noop ("User ID already exists") "\0"
  gettext_noop ("Name already exists") "\0"
  gettext_noop ("Duplicated name") "\0"
  gettext_noop ("Object is too young") "\0"
  gettext_noop ("Object is too old") "\0"
  gettext_noop ("Unknown flag") "\0"
  gettext_noop ("Invalid execution order") "\0"
  gettext_noop ("Already fetched") "\0"
  gettext_noop ("Try again later") "\0"
  gettext_noop ("Wrong name") "\0"
  gettext_noop ("Not authenticated") "\0"
  gettext_noop ("Bad authentication") "\0"
  gettext_noop ("No Keybox daemon running") "\0"
  gettext_noop ("Keybox daemon error") "\0"
  gettext_noop ("Service is not running") "\0"
  gettext_noop ("Service error") "\0"
  gettext_noop ("Bad PUK") "\0"
  gettext_noop ("No reset code") "\0"
  gettext_noop ("Bad reset code") "\0"
  gettext_noop ("System bug detected") "\0"
  gettext_noop ("Unknown DNS error") "\0"
  gettext_noop ("Invalid DNS section") "\0"
  gettext_noop ("Invalid textual address form") "\0"
  gettext_noop ("Missing DNS query packet") "\0"
  gettext_noop ("Missing DNS answer packet") "\0"
  gettext_noop ("Connection closed in DNS") "\0"
  gettext_noop ("Verification failed in DNS") "\0"
  gettext_noop ("DNS Timeout") "\0"
  gettext_noop ("General LDAP error") "\0"
  gettext_noop ("General LDAP attribute error") "\0"
  gettext_noop ("General LDAP name error") "\0"
  gettext_noop ("General LDAP security error") "\0"
  gettext_noop ("General LDAP service error") "\0"
  gettext_noop ("General LDAP update error") "\0"
  gettext_noop ("Experimental LDAP error code") "\0"
  gettext_noop ("Private LDAP error code") "\0"
  gettext_noop ("Other general LDAP error") "\0"
  gettext_noop ("LDAP connecting failed (X)") "\0"
  gettext_noop ("LDAP referral limit exceeded") "\0"
  gettext_noop ("LDAP client loop") "\0"
  gettext_noop ("No LDAP results returned") "\0"
  gettext_noop ("LDAP control not found") "\0"
  gettext_noop ("Not supported by LDAP") "\0"
  gettext_noop ("LDAP connect error") "\0"
  gettext_noop ("Out of memory in LDAP") "\0"
  gettext_noop ("Bad parameter to an LDAP routine") "\0"
  gettext_noop ("User cancelled LDAP operation") "\0"
  gettext_noop ("Bad LDAP search filter") "\0"
  gettext_noop ("Unknown LDAP authentication method") "\0"
  gettext_noop ("Timeout in LDAP") "\0"
  gettext_noop ("LDAP decoding error") "\0"
  gettext_noop ("LDAP encoding error") "\0"
  gettext_noop ("LDAP local error") "\0"
  gettext_noop ("Cannot contact LDAP server") "\0"
  gettext_noop ("LDAP success") "\0"
  gettext_noop ("LDAP operations error") "\0"
  gettext_noop ("LDAP protocol error") "\0"
  gettext_noop ("Time limit exceeded in LDAP") "\0"
  gettext_noop ("Size limit exceeded in LDAP") "\0"
  gettext_noop ("LDAP compare false") "\0"
  gettext_noop ("LDAP compare true") "\0"
  gettext_noop ("LDAP authentication method not supported") "\0"
  gettext_noop ("Strong(er) LDAP authentication required") "\0"
  gettext_noop ("Partial LDAP results+referral received") "\0"
  gettext_noop ("LDAP referral") "\0"
  gettext_noop ("Administrative LDAP limit exceeded") "\0"
  gettext_noop ("Critical LDAP extension is unavailable") "\0"
  gettext_noop ("Confidentiality required by LDAP") "\0"
  gettext_noop ("LDAP SASL bind in progress") "\0"
  gettext_noop ("No such LDAP attribute") "\0"
  gettext_noop ("Undefined LDAP attribute type") "\0"
  gettext_noop ("Inappropriate matching in LDAP") "\0"
  gettext_noop ("Constraint violation in LDAP") "\0"
  gettext_noop ("LDAP type or value exists") "\0"
  gettext_noop ("Invalid syntax in LDAP") "\0"
  gettext_noop ("No such LDAP object") "\0"
  gettext_noop ("LDAP alias problem") "\0"
  gettext_noop ("Invalid DN syntax in LDAP") "\0"
  gettext_noop ("LDAP entry is a leaf") "\0"
  gettext_noop ("LDAP alias dereferencing problem") "\0"
  gettext_noop ("LDAP proxy authorization failure (X)") "\0"
  gettext_noop ("Inappropriate LDAP authentication") "\0"
  gettext_noop ("Invalid LDAP credentials") "\0"
  gettext_noop ("Insufficient access for LDAP") "\0"
  gettext_noop ("LDAP server is busy") "\0"
  gettext_noop ("LDAP server is unavailable") "\0"
  gettext_noop ("LDAP server is unwilling to perform") "\0"
  gettext_noop ("Loop detected by LDAP") "\0"
  gettext_noop ("LDAP naming violation") "\0"
  gettext_noop ("LDAP object class violation") "\0"
  gettext_noop ("LDAP operation not allowed on non-leaf") "\0"
  gettext_noop ("LDAP operation not allowed on RDN") "\0"
  gettext_noop ("Already exists (LDAP)") "\0"
  gettext_noop ("Cannot modify LDAP object class") "\0"
  gettext_noop ("LDAP results too large") "\0"
  gettext_noop ("LDAP operation affects multiple DSAs") "\0"
  gettext_noop ("Virtual LDAP list view error") "\0"
  gettext_noop ("Other LDAP error") "\0"
  gettext_noop ("Resources exhausted in LCUP") "\0"
  gettext_noop ("Security violation in LCUP") "\0"
  gettext_noop ("Invalid data in LCUP") "\0"
  gettext_noop ("Unsupported scheme in LCUP") "\0"
  gettext_noop ("Reload required in LCUP") "\0"
  gettext_noop ("LDAP cancelled") "\0"
  gettext_noop ("No LDAP operation to cancel") "\0"
  gettext_noop ("Too late to cancel LDAP") "\0"
  gettext_noop ("Cannot cancel LDAP") "\0"
  gettext_noop ("LDAP assertion failed") "\0"
  gettext_noop ("Proxied authorization denied by LDAP") "\0"
  gettext_noop ("User defined error code 1") "\0"
  gettext_noop ("User defined error code 2") "\0"
  gettext_noop ("User defined error code 3") "\0"
  gettext_noop ("User defined error code 4") "\0"
  gettext_noop ("User defined error code 5") "\0"
  gettext_noop ("User defined error code 6") "\0"
  gettext_noop ("User defined error code 7") "\0"
  gettext_noop ("User defined error code 8") "\0"
  gettext_noop ("User defined error code 9") "\0"
  gettext_noop ("User defined error code 10") "\0"
  gettext_noop ("User defined error code 11") "\0"
  gettext_noop ("User defined error code 12") "\0"
  gettext_noop ("User defined error code 13") "\0"
  gettext_noop ("User defined error code 14") "\0"
  gettext_noop ("User defined error code 15") "\0"
  gettext_noop ("User defined error code 16") "\0"
  gettext_noop ("SQL success") "\0"
  gettext_noop ("SQL error") "\0"
  gettext_noop ("Internal logic error in SQL library") "\0"
  gettext_noop ("Access permission denied (SQL)") "\0"
  gettext_noop ("SQL abort was requested") "\0"
  gettext_noop ("SQL database file is locked") "\0"
  gettext_noop ("An SQL table in the database is locked") "\0"
  gettext_noop ("SQL library ran out of core") "\0"
  gettext_noop ("Attempt to write a readonly SQL database") "\0"
  gettext_noop ("SQL operation terminated by interrupt") "\0"
  gettext_noop ("I/O error during SQL operation") "\0"
  gettext_noop ("SQL database disk image is malformed") "\0"
  gettext_noop ("Unknown opcode in SQL file control") "\0"
  gettext_noop ("Insertion failed because SQL database is full") "\0"
  gettext_noop ("Unable to open the SQL database file") "\0"
  gettext_noop ("SQL database lock protocol error") "\0"
  gettext_noop ("(internal SQL code: empty)") "\0"
  gettext_noop ("SQL database schema changed") "\0"
  gettext_noop ("String or blob exceeds size limit (SQL)") "\0"
  gettext_noop ("SQL abort due to constraint violation") "\0"
  gettext_noop ("Data type mismatch (SQL)") "\0"
  gettext_noop ("SQL library used incorrectly") "\0"
  gettext_noop ("SQL library uses unsupported OS features") "\0"
  gettext_noop ("Authorization denied (SQL)") "\0"
  gettext_noop ("(unused SQL code: format)") "\0"
  gettext_noop ("SQL bind parameter out of range") "\0"
  gettext_noop ("File opened that is not an SQL database file") "\0"
  gettext_noop ("Notifications from SQL logger") "\0"
  gettext_noop ("Warnings from SQL logger") "\0"
  gettext_noop ("SQL has another row ready") "\0"
  gettext_noop ("SQL has finished executing") "\0"
  gettext_noop ("System error w/o errno") "\0"
  gettext_noop ("Unknown system error") "\0"
  gettext_noop ("End of file") "\0"
  gettext_noop ("Unknown error code");

static const int msgidx[] =
  {
    0,
    8,
    22,
    37,
    63,
    92,
    117,
    132,
    147,
    161,
    175,
    190,
    205,
    230,
    250,
    265,
    279,
    290,
    304,
    326,
    342,
    372,
    392,
    416,
    442,
    467,
    485,
    494,
    504,
    520,
    533,
    547,
    566,
    590,
    610,
    626,
    641,
    657,
    673,
    690,
    704,
    720,
    747,
    764,
    784,
    803,
    820,
    840,
    852,
    866,
    879,
    895,
    914,
    933,
    953,
    973,
    987,
    1009,
    1029,
    1037,
    1041,
    1055,
    1078,
    1086,
    1101,
    1114,
    1129,
    1158,
    1187,
    1210,
    1226,
    1242,
    1262,
    1275,
    1290,
    1307,
    1323,
    1340,
    1357,
    1369,
    1382,
    1413,
    1434,
    1454,
    1475,
    1497,
    1509,
    1524,
    1532,
    1545,
    1554,
    1572,
    1583,
    1594,
    1608,
    1628,
    1641,
    1653,
    1667,
    1679,
    1699,
    1718,
    1738,
    1760,
    1784,
    1805,
    1828,
    1849,
    1864,
    1875,
    1895,
    1908,
    1921,
    1938,
    1960,
    1974,
    1994,
    2010,
    2024,
    2035,
    2055,
    2078,
    2099,
    2114,
    2135,
    2157,
    2173,
    2187,
    2203,
    2217,
    2234,
    2246,
    2278,
    2298,
    2310,
    2320,
    2332,
    2350,
    2371,
    2383,
    2398,
    2415,
    2430,
    2446,
    2460,
    2479,
    2498,
    2521,
    2542,
    2566,
    2584,
    2606,
    2629,
    2647,
    2659,
    2677,
    2694,
    2708,
    2725,
    2740,
    2763,
    2782,
    2795,
    2814,
    2838,
    2865,
    2878,
    2903,
    2914,
    2933,
    2945,
    2961,
    2979,
    3006,
    3013,
    3028,
    3044,
    3060,
    3080,
    3093,
    3105,
    3122,
    3134,
    3151,
    3165,
    3181,
    3208,
    3231,
    3254,
    3277,
    3292,
    3309,
    3327,
    3348,
    3367,
    3398,
    3416,
    3434,
    3456,
    3482,
    3509,
    3526,
    3567,
    3599,
    3637,
    3664,
    3694,
    3724,
    3752,
    3789,
    3813,
    3861,
    3903,
    3943,
    3979,
    4014,
    4036,
    4063,
    4071,
    4078,
    4089,
    4107,
    4124,
    4155,
    4176,
    4201,
    4216,
    4240,
    4259,
    4295,
    4316,
    4345,
    4365,
    4392,
    4420,
    4435,
    4446,
    4463,
    4500,
    4545,
    4589,
    4628,
    4666,
    4704,
    4747,
    4781,
    4826,
    4871,
    4884,
    4894,
    4907,
    4942,
    4962,
    4967,
    4973,
    4991,
    5014,
    5038,
    5059,
    5087,
    5117,
    5145,
    5165,
    5189,
    5216,
    5234,
    5252,
    5280,
    5295,
    5311,
    5339,
    5362,
    5382,
    5399,
    5427,
    5451,
    5476,
    5496,
    5516,
    5538,
    5565,
    5592,
    5630,
    5653,
    5673,
    5689,
    5709,
    5727,
    5740,
    5764,
    5780,
    5796,
    5807,
    5825,
    5844,
    5869,
    5889,
    5912,
    5926,
    5934,
    5948,
    5963,
    5983,
    6001,
    6021,
    6050,
    6075,
    6101,
    6126,
    6153,
    6165,
    6184,
    6213,
    6237,
    6265,
    6292,
    6318,
    6347,
    6371,
    6396,
    6423,
    6452,
    6469,
    6494,
    6517,
    6539,
    6558,
    6580,
    6613,
    6643,
    6666,
    6701,
    6717,
    6737,
    6757,
    6774,
    6801,
    6814,
    6836,
    6856,
    6884,
    6912,
    6931,
    6949,
    6990,
    7030,
    7069,
    7083,
    7118,
    7157,
    7190,
    7217,
    7240,
    7270,
    7301,
    7330,
    7356,
    7379,
    7399,
    7418,
    7444,
    7465,
    7498,
    7535,
    7569,
    7594,
    7623,
    7643,
    7670,
    7706,
    7728,
    7750,
    7778,
    7817,
    7851,
    7873,
    7905,
    7928,
    7965,
    7994,
    8011,
    8039,
    8066,
    8087,
    8114,
    8138,
    8153,
    8181,
    8205,
    8224,
    8246,
    8283,
    8309,
    8335,
    8361,
    8387,
    8413,
    8439,
    8465,
    8491,
    8517,
    8544,
    8571,
    8598,
    8625,
    8652,
    8679,
    8706,
    8718,
    8728,
    8764,
    8795,
    8819,
    8847,
    8886,
    8914,
    8955,
    8993,
    9024,
    9061,
    9096,
    9142,
    9179,
    9212,
    9239,
    9267,
    9307,
    9345,
    9370,
    9399,
    9440,
    9467,
    9493,
    9525,
    9570,
    9600,
    9625,
    9651,
    9678,
    9701,
    9722,
    9734
  };

static GPG_ERR_INLINE int
msgidxof (int code)
{
  return (0 ? 0
  : ((code >= 0) && (code <= 213)) ? (code - 0)
  : ((code >= 217) && (code <= 271)) ? (code - 3)
  : ((code >= 273) && (code <= 281)) ? (code - 4)
  : ((code >= 300) && (code <= 322)) ? (code - 22)
  : ((code >= 666) && (code <= 666)) ? (code - 365)
  : ((code >= 711) && (code <= 718)) ? (code - 409)
  : ((code >= 721) && (code <= 729)) ? (code - 411)
  : ((code >= 750) && (code <= 752)) ? (code - 431)
  : ((code >= 754) && (code <= 782)) ? (code - 432)
  : ((code >= 784) && (code <= 789)) ? (code - 433)
  : ((code >= 800) && (code <= 804)) ? (code - 443)
  : ((code >= 815) && (code <= 822)) ? (code - 453)
  : ((code >= 832) && (code <= 839)) ? (code - 462)
  : ((code >= 844) && (code <= 844)) ? (code - 466)
  : ((code >= 848) && (code <= 848)) ? (code - 469)
  : ((code >= 881) && (code <= 891)) ? (code - 501)
  : ((code >= 1024) && (code <= 1039)) ? (code - 633)
  : ((code >= 1500) && (code <= 1528)) ? (code - 1093)
  : ((code >= 1600) && (code <= 1601)) ? (code - 1164)
  : ((code >= 16381) && (code <= 16383)) ? (code - 15943)
  : 16384 - 15943);
}
