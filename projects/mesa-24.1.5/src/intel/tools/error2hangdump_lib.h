/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline void
_fail(const char *prefix, const char *format, ...)
{
   va_list args;

   va_start(args, format);
   if (prefix)
      fprintf(stderr, "%s: ", prefix);
   vfprintf(stderr, format, args);
   va_end(args);

   abort();
}

#define _fail_if(cond, prefix, ...) do { \
   if (cond) \
      _fail(prefix, __VA_ARGS__); \
} while (0)

#define fail_if(cond, ...) _fail_if(cond, NULL, __VA_ARGS__)

#define fail(...) fail_if(true, __VA_ARGS__)

void write_header(FILE *f);
void write_buffer(FILE *f, uint64_t offset, const void *data, uint64_t size, const char *name);
void write_hw_image_buffer(FILE *f, const void *data, uint64_t size);
void write_exec(FILE *f, uint64_t offset);
