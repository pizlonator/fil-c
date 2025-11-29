/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "freedreno_rd_output.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c11/threads.h"
#include "util/log.h"
#include "util/u_atomic.h"
#include "util/u_debug.h"

#ifdef ANDROID
static const char *fd_rd_output_base_path = "/data/local/tmp";
#else
static const char *fd_rd_output_base_path = "/tmp";
#endif

static const struct debug_control fd_rd_dump_options[] = {
   { "enable", FD_RD_DUMP_ENABLE },
   { "combine", FD_RD_DUMP_COMBINE },
   { "full", FD_RD_DUMP_FULL },
   { "trigger", FD_RD_DUMP_TRIGGER },
   { NULL, 0 }
};

struct fd_rd_dump_env fd_rd_dump_env;

static void
fd_rd_dump_env_init_once(void)
{
   fd_rd_dump_env.flags = parse_debug_string(os_get_option("FD_RD_DUMP"),
                                             fd_rd_dump_options);

   /* If any of the more-detailed FD_RD_DUMP flags is enabled, the general
    * FD_RD_DUMP_ENABLE flag should also implicitly be set.
    */
   if (fd_rd_dump_env.flags & ~FD_RD_DUMP_ENABLE)
      fd_rd_dump_env.flags |= FD_RD_DUMP_ENABLE;
}

void
fd_rd_dump_env_init(void)
{
   static once_flag once = ONCE_FLAG_INIT;
   call_once(&once, fd_rd_dump_env_init_once);
}

static void
fd_rd_output_sanitize_name(char *name)
{
   /* The name string is null-terminated after being constructed via asprintf.
    * Sanitize it by reducing to an underscore anything that's not a hyphen,
    * underscore, dot or alphanumeric character.
    */
   for (char *s = name; *s; ++s) {
      if (isalnum(*s) || *s == '-' || *s == '_' || *s == '.')
         continue;
      *s = '_';
   }
}

void
fd_rd_output_init(struct fd_rd_output *output, char* output_name)
{
   const char *test_name = os_get_option("FD_RD_DUMP_TESTNAME");
   ASSERTED int name_len;
   if (test_name)
      name_len = asprintf(&output->name, "%s_%s", test_name, output_name);
   else
      name_len = asprintf(&output->name, "%s", output_name);
   assert(name_len != -1);
   fd_rd_output_sanitize_name(output->name);

   output->combine = false;
   output->file = NULL;
   output->trigger_fd = -1;
   output->trigger_count = 0;

   if (FD_RD_DUMP(COMBINE)) {
      output->combine = true;

      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s/%s_combined.rd",
               fd_rd_output_base_path, output->name);
      output->file = gzopen(file_path, "w");
   }

   if (FD_RD_DUMP(TRIGGER)) {
      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s/%s_trigger",
               fd_rd_output_base_path, output->name);
      output->trigger_fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
   }
}

void
fd_rd_output_fini(struct fd_rd_output *output)
{
   if (output->name != NULL)
      free(output->name);

   if (output->file != NULL) {
      assert(output->combine);
      gzclose(output->file);
   }

   if (output->trigger_fd >= 0) {
      close(output->trigger_fd);

      /* Remove the trigger file. The filename is reconstructed here
       * instead of having to spend memory to store it in the struct.
       */
      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s/%s_trigger",
               fd_rd_output_base_path, output->name);
      unlink(file_path);
   }
}

static void
fd_rd_output_update_trigger_count(struct fd_rd_output *output)
{
   assert(FD_RD_DUMP(TRIGGER));

   /* Retrieve the trigger file size, only attempt to update the trigger
    * value if anything was actually written to that file.
    */
   struct stat stat;
   if (fstat(output->trigger_fd, &stat) != 0) {
      mesa_loge("[fd_rd_output] failed to acccess the %s trigger file",
                output->name);
      return;
   }

   if (stat.st_size == 0)
      return;

   char trigger_data[32];
   int ret = read(output->trigger_fd, trigger_data, sizeof(trigger_data));
   if (ret < 0) {
      mesa_loge("[fd_rd_output] failed to read from the %s trigger file",
                output->name);
      return;
   }
   int num_read = MIN2(ret, sizeof(trigger_data) - 1);

   /* After reading from it, the trigger file should be reset, which means
    * moving the file offset to the start of the file as well as truncating
    * it to zero bytes.
    */
   if (lseek(output->trigger_fd, 0, SEEK_SET) < 0) {
      mesa_loge("[fd_rd_output] failed to reset the %s trigger file position",
                output->name);
      return;
   }

   if (ftruncate(output->trigger_fd, 0) < 0) {
      mesa_loge("[fd_rd_output] failed to truncate the %s trigger file",
                output->name);
      return;
   }

   /* Try to decode the count value through strtol. -1 translates to UINT_MAX
    * and keeps generating dumps until disabled. Any positive value will
    * allow generating dumps for that many submits. Any other value will
    * disable any further generation of RD dumps.
    */
   trigger_data[num_read] = '\0';
   int32_t value = strtol(trigger_data, NULL, 0);

   if (value == -1) {
      output->trigger_count = UINT_MAX;
      mesa_logi("[fd_rd_output] %s trigger enabling RD dumps until disabled",
                output->name);
   } else if (value > 0) {
      output->trigger_count = (uint32_t) value;
      mesa_logi("[fd_rd_output] %s trigger enabling RD dumps for next %u submissions",
                output->name, output->trigger_count);
   } else {
      output->trigger_count = 0;
      mesa_logi("[fd_rd_output] %s trigger disabling RD dumps", output->name);
   }
}

bool
fd_rd_output_begin(struct fd_rd_output *output, uint32_t submit_idx)
{
   assert(output->combine ^ (output->file == NULL));

   if (FD_RD_DUMP(TRIGGER)) {
      fd_rd_output_update_trigger_count(output);

      if (output->trigger_count == 0)
         return false;
      /* UINT_MAX corresponds to generating dumps until disabled. */
      if (output->trigger_count != UINT_MAX)
          --output->trigger_count;
   }

   if (output->combine)
      return true;

   char file_path[PATH_MAX];
   snprintf(file_path, sizeof(file_path), "%s/%s_%.5d.rd",
            fd_rd_output_base_path, output->name, submit_idx);
   output->file = gzopen(file_path, "w");
   return true;
}

static void
fd_rd_output_write(struct fd_rd_output *output, const void *buffer, int size)
{
   const uint8_t *pos = (uint8_t *) buffer;
   while (size > 0) {
      int ret = gzwrite(output->file, pos, size);
      if (ret < 0) {
         mesa_loge("[fd_rd_output] failed to write to compressed output: %s",
                   gzerror(output->file, NULL));
         return;
      }
      pos += ret;
      size -= ret;
   }
}

void
fd_rd_output_write_section(struct fd_rd_output *output, enum rd_sect_type type,
                           const void *buffer, int size)
{
   fd_rd_output_write(output, &type, 4);
   fd_rd_output_write(output, &size, 4);
   fd_rd_output_write(output, buffer, size);
}

void
fd_rd_output_end(struct fd_rd_output *output)
{
   assert(output->file != NULL);

   /* When combining output, flush the gzip stream on each submit. This should
    * store all the data before any problem during the submit itself occurs.
    */
   if (output->combine) {
      gzflush(output->file, Z_FINISH);
      return;
   }

   gzclose(output->file);
   output->file = NULL;
}
