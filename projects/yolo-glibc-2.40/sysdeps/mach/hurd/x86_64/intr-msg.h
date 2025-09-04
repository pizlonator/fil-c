/* Machine-dependent details of interruptible RPC messaging.  x86_64 version.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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


/* Note that we must mark OPTION and TIMEOUT as outputs of this operation,
   to indicate that the signal thread might mutate them as part
   of sending us to a signal handler.  */

#define INTR_MSG_TRAP(msg, option, send_size, rcv_size, rcv_name, timeout, notify, cancel_p, intr_port_p) \
({									      \
  error_t err;								      \
  register uintptr_t rcv_size_r10 asm ("r10") = rcv_size;		      \
  register uintptr_t rcv_name_r8 asm ("r8") = rcv_name;			      \
  register uintptr_t timeout_r9 asm ("r9") = timeout;			      \
  asm volatile (							      \
       ".globl _hurd_intr_rpc_msg_about_to\n"				      \
       ".globl _hurd_intr_rpc_msg_setup_done\n"				      \
       ".globl _hurd_intr_rpc_msg_in_trap\n"				      \
       /* Clear rax before we do the check for cancel below.  This is to
          detect rax being set to non-zero (actually MACH_SEND_INTERRUPTED)
          from the outside (namely, _hurdsig_abort_rpcs), which signals us
          to skip the trap we were about to enter.  */			      \
       "				xor %[err], %[err]\n"		      \
       "_hurd_intr_rpc_msg_about_to:"					      \
       /* We need to make a last check of cancel, in case we got interrupted
          right before _hurd_intr_rpc_msg_about_to.  */			      \
       "				cmpl $0, %[cancel]\n"		      \
       "				jz _hurd_intr_rpc_msg_do\n"	      \
       /* We got interrupted, note so and return EINTR.  */		      \
       "				movl $0, %[intr_port]\n"	      \
       "				movl %[eintr], %[err]\n"	      \
       "				jmp _hurd_intr_rpc_msg_sp_restored\n" \
       "_hurd_intr_rpc_msg_do:"						      \
       /* Ok, prepare the mach_msg_trap arguments.  We pass the first 6 args
          in registers, and push the last one and a fake return address.  */  \
       "				pushq %[notify]\n"		      \
       "				pushq $0\n"			      \
       "_hurd_intr_rpc_msg_setup_done:"					      \
       /* From here on, it is safe to make us jump over the syscall.  Now
          check if we have been told to skip the syscall while running
          the above.  */						      \
       "				test %[err], %[err]\n"		      \
       "				jnz _hurd_intr_rpc_msg_in_trap\n"     \
       /* Do the actual syscall.  */					      \
       "				movq $-25, %%rax\n"		      \
       "_hurd_intr_rpc_msg_do_trap:	syscall # status in %[err]\n"	      \
       "_hurd_intr_rpc_msg_in_trap:"					      \
       /* Clean the arguments.  */					      \
       "				addq $16, %%rsp\n"		      \
       "_hurd_intr_rpc_msg_sp_restored:"				      \
       : [err] "=&a" (err), "+S" (option), [intr_port] "=m" (*intr_port_p),   \
         "+r" (timeout_r9)						      \
       : "D" (msg), "d" (send_size), "r" (rcv_size_r10), "r" (rcv_name_r8),   \
         [notify] "rm" ((uintptr_t) notify), [cancel] "m" (*cancel_p),	      \
         [eintr] "i" (EINTR)						      \
       : "rcx", "r11");							      \
  timeout = timeout_r9;							      \
  err;									      \
})

#include "hurdfault.h"

/* This cannot be an inline function because it calls setjmp.  */
#define SYSCALL_EXAMINE(state, callno)					      \
({									      \
  struct { unsigned char c[2]; } *p = (void *) ((state)->rip - 2);	      \
  int result;								      \
  if (_hurdsig_catch_memory_fault (p))					      \
    return 0;								      \
  if (result = p->c[0] == 0xf && p->c[1] == 0x5)			      \
    /* The PC appears to be just after a `syscall' instruction.		      \
       This is a system call in progress; %rax holds the call number.  */     \
    *(callno) = (state)->rax;						      \
  _hurdsig_end_catch_fault ();						      \
  result;								      \
})


/* This cannot be an inline function because it calls setjmp.  */
#define MSG_EXAMINE(state, msgid, rcvname, send_name, opt, tmout)	      \
({									      \
  int ret = 0;								      \
  const struct machine_thread_state *s = (state);			      \
  const mach_msg_header_t *msg = (const void *) s->rdi;			      \
  *(rcvname) = s->r8;							      \
  *(opt) = s->rsi;							      \
  *(tmout) = s->r9;							      \
  if (msg == 0)								      \
    {									      \
      *(send_name) = MACH_PORT_NULL;					      \
      *(msgid) = 0;							      \
    }									      \
  else									      \
    {									      \
      ret = _hurdsig_catch_memory_fault (msg) ? -1 : 0;			      \
      if (ret == 0)							      \
        {								      \
          *(send_name) = msg->msgh_remote_port;				      \
          *(msgid) = msg->msgh_id;					      \
          _hurdsig_end_catch_fault ();					      \
	}								      \
    }									      \
  ret;									      \
})
