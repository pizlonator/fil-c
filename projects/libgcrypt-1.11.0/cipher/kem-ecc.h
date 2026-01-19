/* kem-ecc.h - Key Encapsulation Mechanism with ECC
 * Copyright (C) 2024 g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

gpg_err_code_t _gcry_ecc_raw_keypair (int algo,
                                      void *pubkey, size_t pubkey_len,
                                      void *seckey, size_t seckey_len);
gpg_err_code_t _gcry_ecc_raw_encap (int algo,
                                    const void *pubkey, size_t pubkey_len,
                                    void *ciphertext, size_t ciphertext_len,
                                    void *shared, size_t shared_len);
gpg_err_code_t _gcry_ecc_raw_decap (int algo,
                                    const void *seckey, size_t seckey_len,
                                    const void *ciphertext,
                                    size_t ciphertext_len,
                                    void *shared, size_t shared_len);

gpg_err_code_t _gcry_ecc_dhkem_encap (int algo, const void *pubkey,
                                      void *ciphertext,
                                      void *shared);
gpg_err_code_t _gcry_ecc_dhkem_decap (int algo, const void *seckey,
                                      const void *ciphertext,
                                      void *shared, const void *optional);
