/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * (pre-GNU) HISTORY
 *
 * Revision 2.4  91/05/14  17:53:15  mrt
 * 	Correcting copyright
 *
 * Revision 2.3  91/02/14  14:17:43  mrt
 * 	Added new Mach copyright
 * 	[91/02/13  12:44:15  mrt]
 *
 * Revision 2.2  90/08/06  17:24:22  rpd
 * 	Created.
 *
 */

#include <libc-pointer-arith.h>
#if 1
#include <mach.h>
#else
/* This is what CMU did, but that fails to declare some used functions.  */
#include <mach/port.h>
#include <mach/message.h>
#include <mach_init.h>
#endif

static void mach_msg_destroy_port(mach_port_t, mach_msg_type_name_t);
static void mach_msg_destroy_memory(vm_offset_t, vm_size_t);

/*
 *	Routine:	mach_msg_destroy
 *	Purpose:
 *		Deallocates all port rights and out-of-line memory
 *		found in a received message.
 */

void
__mach_msg_destroy (mach_msg_header_t *msg)
{
    mach_msg_bits_t mbits = msg->msgh_bits;

    /*
     *	The msgh_local_port field doesn't hold a port right.
     *	The receive operation consumes the destination port right.
     */

    mach_msg_destroy_port(msg->msgh_remote_port, MACH_MSGH_BITS_REMOTE(mbits));

    if (mbits & MACH_MSGH_BITS_COMPLEX) {
	vm_offset_t saddr;
	vm_offset_t eaddr;

	saddr = (vm_offset_t) (msg + 1);
	eaddr = (vm_offset_t) msg + msg->msgh_size;

	while (saddr < eaddr) {
	    mach_msg_type_long_t *type;
	    mach_msg_type_name_t name;
	    mach_msg_type_size_t size;
	    mach_msg_type_number_t number;
	    boolean_t is_inline;
	    vm_size_t length;
	    vm_offset_t addr;

	    type = (mach_msg_type_long_t *) saddr;
	    is_inline = type->msgtl_header.msgt_inline;
	    if (type->msgtl_header.msgt_longform) {
		    name = type->msgtl_name;
		    size = type->msgtl_size;
		    number = type->msgtl_number;
		    saddr += sizeof(mach_msg_type_long_t);
	    } else {
		    name = type->msgtl_header.msgt_name;
		    size = type->msgtl_header.msgt_size;
		    number = type->msgtl_header.msgt_number;
		    saddr += sizeof(mach_msg_type_t);
	    }

	    /* Calculate length of data in bytes... */
	    length = ((number * size) + 7) >> 3;
	    /* ... and round up using uintptr_t alignment */
	    length = ALIGN_UP (length, __alignof__ (uintptr_t));

	    addr = is_inline ? saddr : * (vm_offset_t *) saddr;

	    if (MACH_MSG_TYPE_PORT_ANY(name)) {
		mach_msg_type_number_t i;

		if (is_inline) {
		    mach_port_name_inlined_t *inlined_ports = (mach_port_name_inlined_t *)addr;
		    for (i = 0; i < number; i++)
			mach_msg_destroy_port(inlined_ports[i].name, name);
		} else {
		    mach_port_t *ports = (mach_port_t *) addr;
		    for (i = 0; i < number; i++)
			mach_msg_destroy_port(ports[i], name);
		}
	    }

	    if (is_inline) {
		saddr += length;
	    } else {
		mach_msg_destroy_memory(addr, length);
		saddr += sizeof(vm_offset_t);
	    }
	}
    }
}

weak_alias (__mach_msg_destroy, mach_msg_destroy)
libc_hidden_def (__mach_msg_destroy)

static void
mach_msg_destroy_port (mach_port_t port, mach_msg_type_name_t type)
{
    if (MACH_PORT_VALID(port)) switch (type) {
      case MACH_MSG_TYPE_MOVE_SEND:
      case MACH_MSG_TYPE_MOVE_SEND_ONCE:
	/* destroy the send/send-once right */
	(void) __mach_port_deallocate(mach_task_self(), port);
	break;

      case MACH_MSG_TYPE_MOVE_RECEIVE:
	/* destroy the receive right */
	(void) __mach_port_mod_refs(mach_task_self(), port,
				    MACH_PORT_RIGHT_RECEIVE, -1);
	break;

      case MACH_MSG_TYPE_MAKE_SEND:
	/* create a send right and then destroy it */
	(void) __mach_port_insert_right(mach_task_self(), port,
					port, MACH_MSG_TYPE_MAKE_SEND);
	(void) __mach_port_deallocate(mach_task_self(), port);
	break;

      case MACH_MSG_TYPE_MAKE_SEND_ONCE:
	/* create a send-once right and then destroy it */
	(void) __mach_port_extract_right(mach_task_self(), port,
					 MACH_MSG_TYPE_MAKE_SEND_ONCE,
					 &port, &type);
	(void) __mach_port_deallocate(mach_task_self(), port);
	break;
    }
}

static void
mach_msg_destroy_memory (vm_offset_t addr, vm_size_t size)
{
    if (size > 0)
	(void) __vm_deallocate(__mach_task_self(), addr, size);
}
