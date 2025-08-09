## SPDX-License-Identifier: 0BSD
## Author: Lasse Collin

## Note: There is no check for COND_CHECK_CRC32 because
## currently crc32 is always enabled.

EXTRA_DIST += \
	check/crc32_tablegen.c \
	check/crc64_tablegen.c

liblzma_la_SOURCES += \
	check/check.c \
	check/check.h \
	check/crc_common.h \
	check/crc_x86_clmul.h \
	check/crc32_arm64.h

if COND_SMALL
liblzma_la_SOURCES += check/crc32_small.c
else
liblzma_la_SOURCES += \
	check/crc32_table.c \
	check/crc32_table_le.h \
	check/crc32_table_be.h
if COND_ASM_X86
liblzma_la_SOURCES += check/crc32_x86.S
else
liblzma_la_SOURCES += check/crc32_fast.c
endif
endif

if COND_CHECK_CRC64
if COND_SMALL
liblzma_la_SOURCES += check/crc64_small.c
else
liblzma_la_SOURCES += \
	check/crc64_table.c \
	check/crc64_table_le.h \
	check/crc64_table_be.h
if COND_ASM_X86
liblzma_la_SOURCES += check/crc64_x86.S
else
liblzma_la_SOURCES += check/crc64_fast.c
endif
endif
endif

if COND_CHECK_SHA256
if COND_INTERNAL_SHA256
liblzma_la_SOURCES += check/sha256.c
endif
endif
