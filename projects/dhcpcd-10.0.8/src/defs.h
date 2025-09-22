/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DEFS_H
#define DEFS_H

#define PACKAGE			"dhcpcd"
#define VERSION			"10.0.8"

#ifndef PRIVSEP_USER
# define PRIVSEP_USER		"_" PACKAGE
#endif

#ifndef CONFIG
# define CONFIG			SYSCONFDIR "/" PACKAGE ".conf"
#endif
#ifndef SCRIPT
# define SCRIPT			LIBEXECDIR "/" PACKAGE "-run-hooks"
#endif
#ifndef DEVDIR
# define DEVDIR			LIBDIR "/" PACKAGE "/dev"
#endif
#ifndef DUID
# define DUID			DBDIR "/duid"
#endif
#ifndef SECRET
# define SECRET			DBDIR "/secret"
#endif
#ifndef LEASEFILE
# define LEASEFILE		DBDIR "/%s%s.lease"
#endif
#ifndef LEASEFILE6
# define LEASEFILE6		LEASEFILE "6"
#endif
#ifndef PIDFILE
# define PIDFILE		RUNDIR "/%s%s%spid"
#endif
#ifndef CONTROLSOCKET
# define CONTROLSOCKET		RUNDIR "/%s%s%s%ssock"
#endif
#ifndef RDM_MONOFILE
# define RDM_MONOFILE		DBDIR "/rdm_monotonic"
#endif

#ifndef NO_SIGNALS
#  define USE_SIGNALS
#endif
#ifndef USE_SIGNALS
#  ifndef THERE_IS_NO_FORK
#    define THERE_IS_NO_FORK
#  endif
#endif

#endif
