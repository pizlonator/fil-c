// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// #define USE_FLAGS
// #define DUMP_SUPER
// #define SAVE_FS_IMAGE

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <fuzzer/FuzzedDataProvider.h>

#include "ext2fs/ext2fs.h"
extern "C" {
#include "e2p/e2p.h"
#include "support/print_fs_flags.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

  const char *progname = "ext2fs_read_bitmap_fuzzer";
  add_error_table(&et_ext2_error_table);

  enum FuzzerType {
    ext2fsReadBlockBitmap,
    ext2fsReadInodeBitmap,
    kMaxValue = ext2fsReadInodeBitmap
  };

  FuzzedDataProvider stream(data, size);
  const FuzzerType f = stream.ConsumeEnum<FuzzerType>();
  int flags = stream.ConsumeIntegral<int>();
#ifndef USE_FLAGS
  flags = 0;
#endif

  static const char* fname = "/tmp/ext2_test_file";

  // Write our data to a temp file.
#ifdef SAVE_FS_IMAGE
  int fd = open(fname, O_CREAT|O_TRUNC|O_RDWR, 0644);
#else
  int fd = syscall(SYS_memfd_create, fname, 0);
#endif
  std::vector<char> buffer = stream.ConsumeRemainingBytes<char>();
  write(fd, buffer.data(), buffer.size());

  std::string fspath("/proc/self/fd/" + std::to_string(fd));

  ext2_filsys fs;
#ifdef USE_FLAGS
  printf("Flags: 0x%08x ", flags);
  print_fs_flags(stdout, flags);
  flags &= ~EXT2_FLAG_NOFREE_ON_ERROR;
#endif
  errcode_t retval = ext2fs_open(
      fspath.c_str(),
      flags | EXT2_FLAG_IGNORE_CSUM_ERRORS, 0, 0,
      unix_io_manager,
      &fs);

  if (retval) {
    com_err(progname, retval, "while trying to open file system");
  } else {
#ifdef DUMP_SUPER
    list_super2(fs->super, stdout);
#endif
    switch (f) {
      case ext2fsReadBlockBitmap: {
        retval = ext2fs_read_block_bitmap(fs);
        if (retval)
          com_err(progname, retval, "while trying to read block bitmap");
        break;
      }
      case ext2fsReadInodeBitmap: {
        retval = ext2fs_read_inode_bitmap(fs);
        if (retval)
          com_err(progname, retval, "while trying to read inode bitmap");
        break;
      }
      default: {
        assert(false);
      }
    }
    retval = ext2fs_close(fs);
    if (retval)
      com_err(progname, retval, "while trying to close file system");
  }
  close(fd);

  return 0;
}
