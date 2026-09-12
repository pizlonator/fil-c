#ifndef _ARPA_INET_H
/* Note: _ARPA_INET_H is defined by inet/arpa/inet.h below.  */
#include <inet/arpa/inet.h>

#ifndef _ISOMAC
/* Declare functions with security checks.
   This needs to be included unconditionally as these definition are needed even
   when fortification is disabled in inet/arpa/inet.h.  */
#include <bits/inet-fortified-decl.h>

/* Variant of inet_aton which rejects trailing garbage.  */
extern int __inet_aton_exact (const char *__cp, struct in_addr *__inp);
libc_hidden_proto (__inet_aton_exact)

extern __typeof (inet_ntop) __inet_ntop;
libc_hidden_proto (__inet_ntop)

libc_hidden_proto (inet_pton)
extern __typeof (inet_pton) __inet_pton;
libc_hidden_proto (__inet_pton)

extern __typeof (inet_makeaddr) __inet_makeaddr;
libc_hidden_proto (__inet_makeaddr)
libc_hidden_proto (inet_netof)
extern __typeof (inet_network) __inet_network;
libc_hidden_proto (__inet_network)
#endif
#endif
