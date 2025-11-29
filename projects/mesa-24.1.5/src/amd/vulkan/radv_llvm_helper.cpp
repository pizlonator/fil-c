/*
 * Copyright © 2018 Red Hat.
 *
 * SPDX-License-Identifier: MIT
 */
#include "radv_llvm_helper.h"
#include "ac_llvm_util.h"

#include <list>
class radv_llvm_per_thread_info {
public:
   radv_llvm_per_thread_info(enum radeon_family arg_family, enum ac_target_machine_options arg_tm_options,
                             unsigned arg_wave_size)
       : family(arg_family), tm_options(arg_tm_options), wave_size(arg_wave_size), passes(NULL)
   {
   }

   ~radv_llvm_per_thread_info()
   {
      ac_destroy_llvm_compiler(&llvm_info);
   }

   bool init(void)
   {
      if (!ac_init_llvm_compiler(&llvm_info, family, tm_options))
         return false;

      passes = ac_create_llvm_passes(llvm_info.tm);
      if (!passes)
         return false;

      return true;
   }

   bool compile_to_memory_buffer(LLVMModuleRef module, char **pelf_buffer, size_t *pelf_size)
   {
      return ac_compile_module_to_elf(passes, module, pelf_buffer, pelf_size);
   }

   bool is_same(enum radeon_family arg_family, enum ac_target_machine_options arg_tm_options, unsigned arg_wave_size)
   {
      if (arg_family == family && arg_tm_options == tm_options && arg_wave_size == wave_size)
         return true;
      return false;
   }
   struct ac_llvm_compiler llvm_info;

private:
   enum radeon_family family;
   enum ac_target_machine_options tm_options;
   unsigned wave_size;
   struct ac_compiler_passes *passes;
};

/* we have to store a linked list per thread due to the possibility of multiple gpus being required */
static thread_local std::list<radv_llvm_per_thread_info> radv_llvm_per_thread_list;

bool
radv_compile_to_elf(struct ac_llvm_compiler *info, LLVMModuleRef module, char **pelf_buffer, size_t *pelf_size)
{
   radv_llvm_per_thread_info *thread_info = nullptr;

   for (auto &I : radv_llvm_per_thread_list) {
      if (I.llvm_info.tm == info->tm) {
         thread_info = &I;
         break;
      }
   }

   if (!thread_info) {
      struct ac_compiler_passes *passes = ac_create_llvm_passes(info->tm);
      bool ret = ac_compile_module_to_elf(passes, module, pelf_buffer, pelf_size);
      ac_destroy_llvm_passes(passes);
      return ret;
   }

   return thread_info->compile_to_memory_buffer(module, pelf_buffer, pelf_size);
}

bool
radv_init_llvm_compiler(struct ac_llvm_compiler *info, enum radeon_family family,
                        enum ac_target_machine_options tm_options, unsigned wave_size)
{
   for (auto &I : radv_llvm_per_thread_list) {
      if (I.is_same(family, tm_options, wave_size)) {
         *info = I.llvm_info;
         return true;
      }
   }

   radv_llvm_per_thread_list.emplace_back(family, tm_options, wave_size);
   radv_llvm_per_thread_info &tinfo = radv_llvm_per_thread_list.back();

   if (!tinfo.init()) {
      radv_llvm_per_thread_list.pop_back();
      return false;
   }

   *info = tinfo.llvm_info;
   return true;
}
