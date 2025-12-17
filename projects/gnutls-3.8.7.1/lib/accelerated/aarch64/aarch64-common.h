/*
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef GNUTLS_LIB_ACCELERATED_AARCH64_AARCH64_COMMON_H
#define GNUTLS_LIB_ACCELERATED_AARCH64_AARCH64_COMMON_H

#if !__ASSEMBLER__
#define NN_HASH(name, update_func, digest_func, NAME) \
	{ #name,                                      \
	  sizeof(struct name##_ctx),                  \
	  NAME##_DIGEST_SIZE,                         \
	  NAME##_DATA_SIZE,                           \
	  (nettle_hash_init_func *)name##_init,       \
	  (nettle_hash_update_func *)update_func,     \
	  (nettle_hash_digest_func *)digest_func }

void register_aarch64_crypto(void);
#endif

#define ARMV7_NEON (1 << 0)
#define ARMV7_TICK (1 << 1)
#define ARMV8_AES (1 << 2)
#define ARMV8_SHA1 (1 << 3)
#define ARMV8_SHA256 (1 << 4)
#define ARMV8_PMULL (1 << 5)
#define ARMV8_SHA512 (1 << 6)

/*
 * Support macros for
 *   - Armv8.3-A Pointer Authentication and
 *   - Armv8.5-A Branch Target Identification
 * Further documentation can be found at:
 *  - https://developer.arm.com/documentation/101028/0012/5--Feature-test-macros?lang=en
 *
 * Note that the hint instrunctions are used which means that on older assemblers they will assemble
 * and that they are in the NOP space on older architectures and are NOP'd.
 */

/* BTI Support */
#if defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT == 1
#define GNU_PROPERTY_AARCH64_BTI (1 << 0) /* Has Branch Target Identification */
#define AARCH64_VALID_CALL_TARGET hint #34 /* BTI 'c' */
#else
#define GNU_PROPERTY_AARCH64_BTI 0 /* No Branch Target Identification */
#define AARCH64_VALID_CALL_TARGET
#endif

/* PAC Support has 2 modes if enabled, using the A key or the B key */
#if defined(__ARM_FEATURE_PAC_DEFAULT)

/* PAC Support enabled, define the property value for adding to the NOTES section */
#define GNU_PROPERTY_AARCH64_POINTER_AUTH (1 << 1)

/* Use the A Key */
#if __ARM_FEATURE_PAC_DEFAULT & 1 == 1 /* MNEMONIC */
#define AARCH64_SIGN_LINK_REGISTER hint #25 /* PACIASP */
#define AARCH64_VALIDATE_LINK_REGISTER hint #29 /* AUTIASP */
/* Use the B Key */
#elif __ARM_FEATURE_PAC_DEFAULT & 2 == 2 /* MNEMONIC */
#define AARCH64_SIGN_LINK_REGISTER hint #27 /* PACIBSP */
#define AARCH64_VALIDATE_LINK_REGISTER hint #31 /* AUTIBSP */
#else
/* huh? We should have a key to use */
#error "Expected __ARM_FEATURE_PAC_DEFAULT to have either bit 1 or bit 0 set"
#endif /* __ARM_FEATURE_PAC_DEFAULT */
#else
/* No PAC Support turn down the bit in the GNU Notes section */
#define GNU_PROPERTY_AARCH64_POINTER_AUTH 0

#if GNU_PROPERTY_AARCH64_BTI != 0
/*
     * If BTI is enabled we need to define certain macros back to the BTI macros as they
     * as they mark valid jump locations.
     */
#define AARCH64_SIGN_LINK_REGISTER AARCH64_VALID_CALL_TARGET
#else
#define AARCH64_SIGN_LINK_REGISTER
#endif /* GNU_PROPERTY_AARCH64_BTI */
#define AARCH64_VALIDATE_LINK_REGISTER
#endif /* __ARM_FEATURE_PAC_DEFAULT */

/*
 * The GNU notes section declares if PAC and/or BTI are enabled. For BTI is important
 * as the first ELF loaded that does not support BTI disables the support. For PAC it
 * is a nice to have to know if the executable has support without needing to peer into
 * it's instructions.
 */
#if defined(__ASSEMBLER__)
#if GNU_PROPERTY_AARCH64_POINTER_AUTH != 0 || GNU_PROPERTY_AARCH64_BTI != 0
/* clang-format off */
.pushsection .note.gnu.property, "a";
.balign 8;
.long 4;
.long 0x10;
.long 0x5;
.asciz "GNU";
.long 0xc0000000; /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
.long 4;
.long(GNU_PROPERTY_AARCH64_POINTER_AUTH | GNU_PROPERTY_AARCH64_BTI);
.long 0;
.popsection;
/* clang-format on */
#endif
#endif

#endif /* GNUTLS_LIB_ACCELERATED_AARCH64_AARCH64_COMMON_H */
