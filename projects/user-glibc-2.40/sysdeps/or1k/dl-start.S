/* Machine-dependent ELF startup code.  OpenRISC version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <sysdep.h>

/* Initial entry point code for the dynamic linker.
   The function _dl_start is the real entry point;
   it's return value is the user program's entry point.  */
ENTRY (_start)
	/* Count arguments in r11 */
	l.ori	r3, r1, 0
	l.movhi	r11, 0
1:
	l.addi	r3, r3, 4
	l.lwz	r12, 0(r3)
	l.sfnei	r12, 0
	l.addi	r11, r11, 1
	l.bf	1b
	 l.nop
	l.addi	r11, r11, -1
	/* store argument counter to stack.  */
	l.sw	0(r1), r11

	/* Load the PIC register.  */
	l.jal	0x8
	 l.movhi r16, gotpchi(_GLOBAL_OFFSET_TABLE_-4)
	l.ori	r16, r16, gotpclo(_GLOBAL_OFFSET_TABLE_+0)
	l.add	r16, r16, r9

	l.ori	r3, r1, 0

	l.jal	_dl_start
	 l.nop
	/* Save user entry in a call saved reg.  */
	l.ori	r22, r11, 0
	/* Fall through to _dl_start_user.  */

_dl_start_user:
	/* Set up for _dl_init.  */

	/* Load _rtld_local (a.k.a _dl_loaded).  */
	l.lwz	r12, got(_rtld_local)(r16)
	l.lwz	r3, 0(r12)

	/* Load argc */
	l.lwz	r18, got(_dl_argc)(r16)
	l.lwz	r4, 0(r18)

	/* Load argv */
	l.lwz	r20, got(_dl_argv)(r16)
	l.lwz	r5, 0(r20)

	/* Load envp = &argv[argc + 1].  */
	l.slli	r6, r4, 2
	l.addi	r6, r6, 4
	l.add	r6, r6, r5

	l.jal	plt(_dl_init)
	 l.nop

	/* Now set up for user entry.
	   The already defined ABI loads argc and argv from the stack.

	   argc = 0(r1)
	   argv = r1 + 4
	*/

	/* Load SP as argv - 4.  */
	l.lwz	r3, 0(r20)
	l.addi	r1, r3, -4

	/* Save argc.  */
	l.lwz	r3, 0(r18)
	l.sw	0(r1), r3

	/* Pass _dl_fini function address to _start.
	   Next start.S will then pass this as rtld_fini to __libc_start_main.  */
	l.lwz	r3, got(_dl_fini)(r16)

	l.jr	r22
	 l.nop

END (_start)
