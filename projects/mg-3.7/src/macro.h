/*	$OpenBSD: macro.h,v 1.6 2005/06/14 18:14:40 kjell Exp $	*/

/* This file is in the public domain. */

/* definitions for keyboard macros */

#define MAXMACRO 256		/* maximum functs in a macro */

extern int inmacro;
extern int macrodef;
extern int macrocount;

union macrodef {
	PF	m_funct;
	int	m_count;	/* for count-prefix	 */
};

extern union macrodef macro[MAXMACRO];

extern struct line	*maclhead;
extern struct line	*maclcur;
