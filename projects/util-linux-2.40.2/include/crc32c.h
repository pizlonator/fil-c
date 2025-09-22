/*
 * No copyright is claimed. This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UL_CRC32C_H
#define UL_CRC32C_H

#include <sys/types.h>
#include <stdint.h>

extern uint32_t crc32c(uint32_t crc, const void *buf, size_t size);
extern uint32_t ul_crc32c_exclude_offset(uint32_t crc, const unsigned char *buf,
					 size_t size, size_t exclude_off,
					 size_t exclude_len);


#endif /* UL_CRC32C_H */
