/*
 * This file was automatically generated from zstd.h,
 * which is covered by the following license:
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
FUNC(size_t, ZSTD_compress, (void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel), (dst, dstCapacity, src, srcSize, compressionLevel))
FUNC(size_t, ZSTD_decompress, (void *dst, size_t dstCapacity, const void *src, size_t compressedSize), (dst, dstCapacity, src, compressedSize))
FUNC(size_t, ZSTD_compressBound, (size_t srcSize), (srcSize))
FUNC(unsigned int, ZSTD_isError, (size_t code), (code))
