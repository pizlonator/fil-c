/*	$NetBSD: queue.h,v 1.65 2013/12/25 17:19:34 christos Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 */

#ifndef COMPAT_QUEUE_H
#define COMPAT_QUEUE_H

/*
 * Tail queue definitions.
 */
#ifndef TAILQ_END
#define	TAILQ_END(head)			(NULL)
#endif

#ifndef TAILQ_HEAD
#define	_TAILQ_HEAD(name, type, qual)					\
struct name {								\
	qual type *tqh_first;		/* first element */		\
	qual type *qual *tqh_last;	/* addr of last next element */	\
}
#define TAILQ_HEAD(name, type)	_TAILQ_HEAD(name, struct type,)

#define	TAILQ_HEAD_INITIALIZER(head)					\
	{ TAILQ_END(head), &(head).tqh_first }

#define	_TAILQ_ENTRY(type, qual)					\
struct {								\
	qual type *tqe_next;		/* next element */		\
	qual type *qual *tqe_prev;	/* address of previous next element */\
}
#define TAILQ_ENTRY(type)	_TAILQ_ENTRY(struct type,)
#endif /* !TAILQ_HEAD */

/*
 * Tail queue access methods.
 */
#ifndef TAILQ_FIRST
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#define	TAILQ_LAST(head, headname) \
	(*(((struct headname *)((head)->tqh_last))->tqh_last))
#define	TAILQ_PREV(elm, headname, field) \
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#define	TAILQ_EMPTY(head)		(TAILQ_FIRST(head) == TAILQ_END(head))
#endif /* !TAILQ_FIRST */

#ifndef TAILQ_FOREACH
#define	TAILQ_FOREACH(var, head, field)					\
	for ((var) = ((head)->tqh_first);				\
	    (var) != TAILQ_END(head);					\
	    (var) = ((var)->field.tqe_next))

#define	TAILQ_FOREACH_REVERSE(var, head, headname, field)		\
	for ((var) = (*(((struct headname *)((head)->tqh_last))->tqh_last));\
	    (var) != TAILQ_END(head);					\
	    (var) = (*(((struct headname *)((var)->field.tqe_prev))->tqh_last)))
#endif /* !TAILQ_FOREACH */

#ifndef TAILQ_INIT
#define	TAILQ_INIT(head) do {						\
	(head)->tqh_first = TAILQ_END(head);				\
	(head)->tqh_last = &(head)->tqh_first;				\
} while (/*CONSTCOND*/0)

#define	TAILQ_INSERT_HEAD(head, elm, field) do {			\
	if (((elm)->field.tqe_next = (head)->tqh_first) != TAILQ_END(head))\
		(head)->tqh_first->field.tqe_prev =			\
		    &(elm)->field.tqe_next;				\
	else								\
		(head)->tqh_last = &(elm)->field.tqe_next;		\
	(head)->tqh_first = (elm);					\
	(elm)->field.tqe_prev = &(head)->tqh_first;			\
} while (/*CONSTCOND*/0)

#define	TAILQ_INSERT_TAIL(head, elm, field) do {			\
	(elm)->field.tqe_next = TAILQ_END(head);			\
	(elm)->field.tqe_prev = (head)->tqh_last;			\
	*(head)->tqh_last = (elm);					\
	(head)->tqh_last = &(elm)->field.tqe_next;			\
} while (/*CONSTCOND*/0)

#define	TAILQ_INSERT_AFTER(head, listelm, elm, field) do {		\
	if (((elm)->field.tqe_next = (listelm)->field.tqe_next) !=	\
	    TAILQ_END(head))						\
		(elm)->field.tqe_next->field.tqe_prev =			\
		    &(elm)->field.tqe_next;				\
	else								\
		(head)->tqh_last = &(elm)->field.tqe_next;		\
	(listelm)->field.tqe_next = (elm);				\
	(elm)->field.tqe_prev = &(listelm)->field.tqe_next;		\
} while (/*CONSTCOND*/0)

#define	TAILQ_INSERT_BEFORE(listelm, elm, field) do {			\
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev;		\
	(elm)->field.tqe_next = (listelm);				\
	*(listelm)->field.tqe_prev = (elm);				\
	(listelm)->field.tqe_prev = &(elm)->field.tqe_next;		\
} while (/*CONSTCOND*/0)

#define	TAILQ_REMOVE(head, elm, field) do {				\
	if (((elm)->field.tqe_next) != TAILQ_END(head))			\
		(elm)->field.tqe_next->field.tqe_prev =			\
		    (elm)->field.tqe_prev;				\
	else								\
		(head)->tqh_last = (elm)->field.tqe_prev;		\
	*(elm)->field.tqe_prev = (elm)->field.tqe_next;			\
} while (/*CONSTCOND*/0)
#endif /* !TAILQ_INIT */

#ifndef TAILQ_REPLACE
#define TAILQ_REPLACE(head, elm, elm2, field) do {                      \
        if (((elm2)->field.tqe_next = (elm)->field.tqe_next) !=		\
	    TAILQ_END(head))						\
                (elm2)->field.tqe_next->field.tqe_prev =                \
                    &(elm2)->field.tqe_next;                            \
        else                                                            \
                (head)->tqh_last = &(elm2)->field.tqe_next;             \
        (elm2)->field.tqe_prev = (elm)->field.tqe_prev;                 \
        *(elm2)->field.tqe_prev = (elm2);                               \
} while (/*CONSTCOND*/0)
#endif /* !TAILQ_REPLACE */

#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, next)			\
	for ((var) = TAILQ_FIRST(head);					\
	    (var) != TAILQ_END(head) &&					\
	    ((next) = TAILQ_NEXT(var, field), 1); (var) = (next))

#define	TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, prev)	\
	for ((var) = TAILQ_LAST((head), headname);			\
	    (var) != TAILQ_END(head) &&					\
	    ((prev) = TAILQ_PREV((var), headname, field), 1); (var) = (prev))
#endif /* !TAILQ_FOREACH_SAFE */

#ifndef TAILQ_CONCAT
#define	TAILQ_CONCAT(head1, head2, field) do {				\
	if (!TAILQ_EMPTY(head2)) {					\
		*(head1)->tqh_last = (head2)->tqh_first;		\
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last;	\
		(head1)->tqh_last = (head2)->tqh_last;			\
		TAILQ_INIT((head2));					\
	}								\
} while (/*CONSTCOND*/0)
#endif /* !TAILQ_CONCAT */

#endif	/* !COMAPT_QUEUE_H */
