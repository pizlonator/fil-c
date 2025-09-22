/*
 * Copyright(c) 2020 Free Software Foundation, Inc.
 *
 * This file is part of libtasn1.
 *
 * Libtasn1 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Libtasn1 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libtasn1.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This fuzzer is testing asn1_array2tree()'s robustness with arbitrary
 * input data.
 */

#include <config.h>
#include <assert.h>

#include "libtasn1.h"
#include "fuzzer.h"

int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
  int ret;
  unsigned int ret_len;
  const unsigned char *str;

  ret =
    asn1_decode_simple_der (ASN1_ETYPE_BIT_STRING, data, size, &str,
			    &ret_len);
  if (ret == ASN1_SUCCESS)
    {
      assert (ret_len <= size);
    }

  ret =
    asn1_decode_simple_der (ASN1_ETYPE_OCTET_STRING, data, size, &str,
			    &ret_len);
  if (ret == ASN1_SUCCESS)
    {
      assert (ret_len <= size);
    }

  return 0;
}
