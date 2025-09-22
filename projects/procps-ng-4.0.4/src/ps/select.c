/*
 * select.c - ps process selection
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2020 Craig Small <csmall@dropbear.xyz
 * Copyright © 1998-2002 Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

//#define process_group_leader(p) (rSv(ID_PID, s_int, p) == rSv(ID_TGID, s_int, p))
//#define some_other_user(p)      (rSv(ID_EUID, u_int, p) != cached_euid)
#define has_our_euid(p)         (rSv(ID_EUID, u_int, p) == cached_euid)
#define on_our_tty(p)           (rSv(TTY, s_int, p) == cached_tty)
#define running(p)              (rSv(STATE, s_ch, p) == 'R' || rSv(STATE, s_ch, p) == 'D')
#define session_leader(p)       (rSv(ID_SESSION, s_int, p) == rSv(ID_TGID, s_int, p))
#define without_a_tty(p)        (!rSv(TTY, s_int, p))

static unsigned long select_bits = 0;

/***** prepare select_bits for use */
const char *select_bits_setup(void){
  int switch_val = 0;
  /* don't want a 'g' screwing up simple_select */
  if(!simple_select && !prefer_bsd_defaults){
    select_bits = 0xaa00; /* the STANDARD selection */
    return NULL;
  }
  /* For every BSD but SunOS, the 'g' option is a NOP. (enabled by default) */
  if( !(personality & PER_NO_DEFAULT_g) && !(simple_select&(SS_U_a|SS_U_d)) )
    switch_val = simple_select|SS_B_g;
  else
    switch_val = simple_select;
  switch(switch_val){
  /* UNIX options */
  case SS_U_a | SS_U_d:           select_bits = 0x3f3f; break; /* 3333 or 3f3f */
  case SS_U_a:                    select_bits = 0x0303; break; /* 0303 or 0f0f */
  case SS_U_d:                    select_bits = 0x3333; break;
  /* SunOS 4 only (others have 'g' enabled all the time) */
  case 0:                         select_bits = 0x0202; break;
  case                   SS_B_a:  select_bits = 0x0303; break;
  case          SS_B_x         :  select_bits = 0x2222; break;
  case          SS_B_x | SS_B_a:  select_bits = 0x3333; break;
  /* General BSD options */
  case SS_B_g                  :  select_bits = 0x0a0a; break;
  case SS_B_g |          SS_B_a:  select_bits = 0x0f0f; break;
  case SS_B_g | SS_B_x         :  select_bits = 0xaaaa; break;
  case SS_B_g | SS_B_x | SS_B_a:  /* convert to -e instead of using 0xffff */
    all_processes = 1;
    simple_select = 0;
    break;
  default:
    return _("process selection options conflict");
    break;
  }
  return NULL;
}

/***** selected by simple option? */
static int table_accept(proc_t *buf){
  unsigned proc_index;
  proc_index = (has_our_euid(buf)    <<0)
             | (session_leader(buf)  <<1)
             | (without_a_tty(buf)   <<2)
             | (on_our_tty(buf)      <<3);
  return (select_bits & (1<<proc_index));
}

/***** selected by some kind of list? */
static int proc_was_listed(proc_t *buf){
  selection_node *sn = selection_list;
  int i;
  if(!sn) return 0;
  while(sn){
    switch(sn->typecode){
    default:
      catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));

#define return_if_match(foo,bar) \
        i=sn->n; while(i--) \
        if((unsigned)foo == (unsigned)(*(sn->u+i)).bar) \
        return 1

    break; case SEL_RUID: return_if_match(rSv(ID_RUID, u_int, buf),uid);
    break; case SEL_EUID: return_if_match(rSv(ID_EUID, u_int, buf),uid);
    break; case SEL_SUID: return_if_match(rSv(ID_SUID, u_int, buf),uid);
    break; case SEL_FUID: return_if_match(rSv(ID_FUID, u_int, buf),uid);

    break; case SEL_RGID: return_if_match(rSv(ID_RGID, u_int, buf),gid);
    break; case SEL_EGID: return_if_match(rSv(ID_EGID, u_int, buf),gid);
    break; case SEL_SGID: return_if_match(rSv(ID_SGID, u_int, buf),gid);
    break; case SEL_FGID: return_if_match(rSv(ID_FGID, u_int, buf),gid);

    break; case SEL_PGRP: return_if_match(rSv(ID_PGRP, s_int, buf),pid);
    break; case SEL_PID : return_if_match(rSv(ID_TGID, s_int, buf),pid);
    break; case SEL_PID_QUICK : return_if_match(rSv(ID_TGID, s_int, buf),pid);
    break; case SEL_PPID: return_if_match(rSv(ID_PPID, s_int, buf),ppid);
    break; case SEL_TTY : return_if_match(rSv(TTY, s_int, buf),tty);
    break; case SEL_SESS: return_if_match(rSv(ID_SESSION, s_int, buf),pid);

    break;
    case SEL_COMM:
        i=sn->n;
        while(i--) {
            /* special case, comm is 16 characters but match is longer */
            if (strlen(rSv(CMD, str, buf)) == 15 && strlen((*(sn->u+i)).cmd) >= 15)
                if(!strncmp( rSv(CMD, str, buf), (*(sn->u+i)).cmd, 15 )) return 1;
            if(!strncmp( rSv(CMD, str, buf), (*(sn->u+i)).cmd, 63 )) return 1;
        }


#undef return_if_match

    }
    sn = sn->next;
  }
  return 0;
}


/***** This must satisfy Unix98 and as much BSD as possible */
int want_this_proc(proc_t *buf){
  int accepted_proc = 1; /* assume success */
  /* elsewhere, convert T to list, U sets x implicitly */

  /* handle -e -A */
  if(all_processes) goto finish;

  /* use table for -a a d g x */
  if((simple_select || !selection_list))
    if(table_accept(buf)) goto finish;

  /* search lists */
  if(proc_was_listed(buf)) goto finish;

  /* fail, fall through to loose ends */
  accepted_proc = 0;

  /* do r N */
finish:
  if(running_only && !running(buf)) accepted_proc = 0;
  if(negate_selection) return !accepted_proc;
  return accepted_proc;
}
