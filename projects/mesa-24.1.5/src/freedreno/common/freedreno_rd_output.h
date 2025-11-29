/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#ifndef __FREEDRENO_RD_OUTPUT_H__
#define __FREEDRENO_RD_OUTPUT_H__

#include <stdbool.h>
#include <stdint.h>
#include <zlib.h>

#include "redump.h"

#ifdef __cplusplus
extern "C" {
#endif

enum fd_rd_dump_flags {
   FD_RD_DUMP_ENABLE = 1 << 0,
   FD_RD_DUMP_COMBINE = 1 << 1,
   FD_RD_DUMP_FULL = 1 << 2,
   FD_RD_DUMP_TRIGGER = 1 << 3,
};

struct fd_rd_dump_env {
   uint32_t flags;
};

extern struct fd_rd_dump_env fd_rd_dump_env;

#define FD_RD_DUMP(name) unlikely(fd_rd_dump_env.flags & FD_RD_DUMP_##name)

void
fd_rd_dump_env_init(void);

struct fd_rd_output {
   char *name;
   bool combine;
   gzFile file;

   int trigger_fd;
   uint32_t trigger_count;
};

void
fd_rd_output_init(struct fd_rd_output *output, char* output_name);

void
fd_rd_output_fini(struct fd_rd_output *output);

bool
fd_rd_output_begin(struct fd_rd_output *output, uint32_t submit_idx);

void
fd_rd_output_write_section(struct fd_rd_output *output, enum rd_sect_type type,
                           const void *buffer, int size);

void
fd_rd_output_end(struct fd_rd_output *output);

#ifdef __cplusplus
}
#endif

#endif /* __FREEDRENO_RD_OUTPUT_H__ */
