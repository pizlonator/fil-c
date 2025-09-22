/* M68K specific core note handling.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <elf.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>

#define BACKEND	m68k_
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
    { .offset = 0, .regno = 1, .count = 14, .bits = 32 }, /* d1-d7, a0-a6 */
    { .offset = 14 * 4, .regno = 0, .count = 1, .bits = 32 }, /* d0 */
    { .offset = 15 * 4, .regno = 15, .count = 1, .bits = 32 }, /* a7 */
    { .offset = 18 * 4, .regno = 24, .count = 1, .bits = 32 } /* pc */
  };
#define PRSTATUS_REGS_SIZE	(20 * 4)

#define ULONG			uint32_t
#define PID_T			int32_t
#define	UID_T			uint16_t
#define	GID_T			uint16_t
#define ALIGN_INT		2
#define ALIGN_ULONG		2
#define ALIGN_PID_T		2
#define ALIGN_UID_T		2
#define ALIGN_GID_T		2
#define ALIGN_PRSTATUS		2
#define TYPE_ULONG		ELF_T_WORD
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_HALF
#define TYPE_GID_T		ELF_T_HALF

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 16, .count = 8, .bits = 96 }, /* fp0-fp7 */
  };
#define FPREGSET_SIZE	(27 * 4)

#include "linux-core-note.c"
