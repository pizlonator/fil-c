#pragma once

/* Windows is always little endian. */
#define htonl	__builtin_bswap32
#define ntohl	__builtin_bswap32
