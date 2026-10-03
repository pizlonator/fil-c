#ifndef COSMOPOLITAN_LIBC_CALLS_STRUCT_F_OWNER_EX_H_
#define COSMOPOLITAN_LIBC_CALLS_STRUCT_F_OWNER_EX_H_
#include "libc/calls/weirdtypes.h"
COSMOPOLITAN_C_START_

/* This is part of the Fil-C yolocosmo support: struct f_owner_ex (the
   F_SETOWN_EX / F_GETOWN_EX argument) with the Linux kernel ABI. */

#define F_OWNER_TID   0
#define F_OWNER_PID   1
#define F_OWNER_PGRP  2

struct f_owner_ex {
  int type;
  pid_t pid;
};

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_LIBC_CALLS_STRUCT_F_OWNER_EX_H_ */
