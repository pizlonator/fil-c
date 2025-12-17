/*
 * This file was automatically generated from encode.h,
 * which is covered by the following license:
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Distributed under MIT license.
 * See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
 */
FUNC(size_t, BrotliEncoderMaxCompressedSize, (size_t input_size), (input_size))
FUNC(int, BrotliEncoderCompress, (int quality, int lgwin, BrotliEncoderMode mode, size_t input_size, const uint8_t input_buffer[], size_t *encoded_size, uint8_t encoded_buffer[]), (quality, lgwin, mode, input_size, input_buffer, encoded_size, encoded_buffer))
