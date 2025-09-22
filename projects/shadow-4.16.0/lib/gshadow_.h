/*
 * SPDX-FileCopyrightText: 1988 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 *	$Id$
 */

#ifndef	_H_GSHADOW
#define	_H_GSHADOW

/*
 * Shadow group security file structure
 */

struct sgrp {
	char *sg_name;		/* group name */
	char *sg_passwd;	/* group password */
	char **sg_adm;		/* group administrator list */
	char **sg_mem;		/* group membership list */
};

/*
 * Shadow group security file functions.
 */

#include <stdio.h>		/* for FILE */

#if __STDC__
/*@observer@*//*@null@*/struct sgrp *getsgent (void);
/*@observer@*//*@null@*/struct sgrp *getsgnam (const char *);
/*@observer@*//*@null@*/struct sgrp *sgetsgent (const char *);
/*@observer@*//*@null@*/struct sgrp *fgetsgent (/*@null@*/FILE *);
void setsgent (void);
void endsgent (void);
int putsgent (const struct sgrp *, FILE *);
#else
/*@observer@*//*@null@*/struct sgrp *getsgent ();
/*@observer@*//*@null@*/struct sgrp *getsgnam ();
/*@observer@*//*@null@*/struct sgrp *sgetsgent ();
/*@observer@*//*@null@*/struct sgrp *fgetsgent ();
void setsgent ();
void endsgent ();
int putsgent ();
#endif

#define	GSHADOW	"/etc/gshadow"
#endif				/* ifndef _H_GSHADOW */
