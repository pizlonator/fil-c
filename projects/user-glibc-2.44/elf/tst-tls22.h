/* Test TLS with varied alignment and multiple modules and threads: header.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef TST_TLS22_H
#define TST_TLS22_H

#include <stdint.h>

extern __thread int exe_tdata1 __attribute__ ((tls_model ("initial-exec")));
extern __thread int exe_tdata2 __attribute__ ((tls_model ("initial-exec")));
extern __thread int exe_tdata3 __attribute__ ((tls_model ("initial-exec")));
extern __thread int exe_tbss1 __attribute__ ((tls_model ("initial-exec")));
extern __thread int exe_tbss2 __attribute__ ((tls_model ("initial-exec")));
extern __thread int exe_tbss3 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod1_tdata1 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod1_tdata2 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod1_tdata3 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod1_tbss1 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod1_tbss2 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod1_tbss3 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod2_tdata1 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod2_tdata2 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod2_tdata3 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod2_tbss1 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod2_tbss2 __attribute__ ((tls_model ("initial-exec")));
extern __thread int mod2_tbss3 __attribute__ ((tls_model ("initial-exec")));

/* Structure to store the addresses of one set of TLS objects in one
   thread, as seen by one module in the program.  */
struct obj_addrs
{
  uintptr_t addr_tdata1, addr_tdata2, addr_tdata3;
  uintptr_t addr_tbss1, addr_tbss2, addr_tbss3;
};

/* Structure to store the values of one set of TLS objects in one
   thread.  */
struct obj_values
{
  uintptr_t val_tdata1, val_tdata2, val_tdata3;
  uintptr_t val_tbss1, val_tbss2, val_tbss3;
};

/* Structure to store all the data about TLS objects in one
   thread.  */
struct one_thread_data
{
  struct obj_addrs exe_self, exe_from_mod1, exe_from_mod2;
  struct obj_addrs mod1_self, mod1_from_exe;
  struct obj_addrs mod2_self, mod2_from_exe, mod2_from_mod1;
  struct obj_values exe_before, mod1_before, mod2_before;
  struct obj_values exe_after, mod1_after, mod2_after;
};

/* Store the addresses of variables prefixed by PFX in the structure
   pointed to by DST.  */
#define STORE_ADDRS(DST, PFX)				\
  do							\
    {							\
      (DST)->addr_tdata1 = (uintptr_t) &PFX ## _tdata1;	\
      (DST)->addr_tdata2 = (uintptr_t) &PFX ## _tdata2;	\
      (DST)->addr_tdata3 = (uintptr_t) &PFX ## _tdata3;	\
      (DST)->addr_tbss1 = (uintptr_t) &PFX ## _tbss1;	\
      (DST)->addr_tbss2 = (uintptr_t) &PFX ## _tbss2;	\
      (DST)->addr_tbss3 = (uintptr_t) &PFX ## _tbss3;	\
    }							\
  while (0)

/* Store the values of variables prefixed by PFX in the structure
   pointed to by DST.  */
#define STORE_VALUES(DST, PFX)			\
  do						\
    {						\
      (DST)->val_tdata1 = PFX ## _tdata1;	\
      (DST)->val_tdata2 = PFX ## _tdata2;	\
      (DST)->val_tdata3 = PFX ## _tdata3;	\
      (DST)->val_tbss1 = PFX ## _tbss1;		\
      (DST)->val_tbss2 = PFX ## _tbss2;		\
      (DST)->val_tbss3 = PFX ## _tbss3;		\
    }						\
  while (0)

/* Overwrite the values of variables prefixed by PFX with values
   starting with VAL.  */
#define OVERWRITE_VALUES(PFX, VAL)		\
  do						\
    {						\
      PFX ## _tdata1 = (VAL);			\
      PFX ## _tdata2 = (VAL) + 1;		\
      PFX ## _tdata3 = (VAL) + 2;		\
      PFX ## _tbss1 = (VAL) + 3;		\
      PFX ## _tbss2 = (VAL) + 4;		\
      PFX ## _tbss3 = (VAL) + 5;		\
    }						\
  while (0)

void test_mod1 (struct one_thread_data *data, int base_val);
void test_mod2 (struct one_thread_data *data, int base_val);

#endif /* TST_TLS22_H */
