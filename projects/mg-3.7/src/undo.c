/* $OpenBSD: undo.c,v 1.57 2015/12/11 20:21:23 mmcc Exp $ */
/*
 * This file is in the public domain
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "kbd.h"

#define MAX_FREE_RECORDS	32

/*
 * Local variables
 */
static struct undoq		 undo_free;
static int			 undo_free_num;
static int			 boundary_flag = TRUE;
static int			 undo_enable_flag = TRUE;

/*
 * Local functions
 */
static int find_dot(struct line *, int);
static int find_lo(int, struct line **, int *, int *);
static struct undo_rec *new_undo_record(void);
static int drop_oldest_undo_record(void);

/*
 * find_dot, find_lo()
 *
 * Find an absolute dot in the buffer from a line/offset pair, and vice-versa.
 *
 * Since lines can be deleted while they are referenced by undo record, we
 * need to have an absolute dot to have something reliable.
 */
static int
find_dot(struct line *lp, int off)
{
	int	 count = 0;
	struct line	*p;

	for (p = curbp->b_headp; p != lp; p = lforw(p)) {
		if (count != 0) {
			if (p == curbp->b_headp) {
				dobeep();
				ewprintf("Error: Undo stuff called with a"
				    "nonexistent line");
				return (FALSE);
			}
		}
		count += llength(p) + 1;
	}
	count += off;

	return (count);
}

static int
find_lo(int pos, struct line **olp, int *offset, int *lnum)
{
	struct line *p;
	int lineno;

	p = curbp->b_headp;
	lineno = 0;
	while (pos > llength(p)) {
		pos -= llength(p) + 1;
		if ((p = lforw(p)) == curbp->b_headp) {
			*olp = NULL;
			*offset = 0;
			return (FALSE);
		}
		lineno++;
	}
	*olp = p;
	*offset = pos;
	*lnum = lineno;

	return (TRUE);
}

static struct undo_rec *
new_undo_record(void)
{
	struct undo_rec *rec;

	rec = TAILQ_FIRST(&undo_free);
	if (rec != NULL) {
		/* Remove it from the free-list */
		TAILQ_REMOVE(&undo_free, rec, next);
		undo_free_num--;
	} else {
		if ((rec = malloc(sizeof(*rec))) == NULL)
			panic("Out of memory in undo code (record)");
	}
	memset(rec, 0, sizeof(struct undo_rec));

	return (rec);
}

void
free_undo_record(struct undo_rec *rec)
{
	static int initialised = 0;

	/*
	 * On the first run, do initialisation of the free list.
	 */
	if (initialised == 0) {
		TAILQ_INIT(&undo_free);
		initialised = 1;
	}
	free(rec->content);
	rec->content = NULL;
	if (undo_free_num >= MAX_FREE_RECORDS) {
		free(rec);
		return;
	}
	undo_free_num++;

	TAILQ_INSERT_HEAD(&undo_free, rec, next);
}

/*
 * Drop the oldest undo record in our list. Return 1 if we could remove it,
 * 0 if the undo list was empty.
 */
static int
drop_oldest_undo_record(void)
{
	struct undo_rec *rec;

	rec = TAILQ_LAST(&curbp->b_undo, undoq);
	if (rec != NULL) {
		undo_free_num--;
		TAILQ_REMOVE(&curbp->b_undo, rec, next);
		free_undo_record(rec);
		return (1);
	}
	return (0);
}

static int
lastrectype(void)
{
	struct undo_rec *rec;

	if ((rec = TAILQ_FIRST(&curbp->b_undo)) != NULL)
		return (rec->type);
	return (0);
}

/*
 * Returns TRUE if undo is enabled, FALSE otherwise.
 */
int
undo_enabled(void)
{
	return (undo_enable_flag);
}

/*
 * undo_enable: toggle undo_enable.
 * Returns the previous value of the flag.
 */
int
undo_enable(int f, int n)
{
	int pon = undo_enable_flag;

	if (f & (FFARG | FFRAND))
		undo_enable_flag = n > 0;
	else
		undo_enable_flag = !undo_enable_flag;

	if (!(f & FFRAND))
		ewprintf("Undo %sabled", undo_enable_flag ? "en" : "dis");

	return (pon);
}

/*
 * If undo is enabled, then:
 *  Toggle undo boundary recording.
 *  If called with an argument, (n > 0) => enable. Otherwise disable.
 * In either case, add an undo boundary
 * If undo is disabled, this function has no effect.
 */
int
undo_boundary_enable(int f, int n)
{
	int bon = boundary_flag;

	if (!undo_enable_flag)
		return (FALSE);

	undo_add_boundary(FFRAND, 1);

	if (f & (FFARG | FFRAND))
		boundary_flag = n > 0;
	else
		boundary_flag = !boundary_flag;

	if (!(f & FFRAND))
		ewprintf("Undo boundaries %sabled",
		    boundary_flag ? "en" : "dis");

	return (bon);
}

/*
 * Record an undo boundary, unless boundary_flag == FALSE.
 * Does nothing if previous undo entry is already a boundary or 'modified' flag.
 */
int
undo_add_boundary(int f, int n)
{
	struct undo_rec *rec;
	int last;

	if (boundary_flag == FALSE)
		return (FALSE);

	last = lastrectype();
	if (last == BOUNDARY || last == MODIFIED)
		return (TRUE);

	rec = new_undo_record();
	rec->type = BOUNDARY;

	TAILQ_INSERT_HEAD(&curbp->b_undo, rec, next);

	return (TRUE);
}

/*
 * Record an undo "modified" boundary
 */
void
undo_add_modified(void)
{
	struct undo_rec *rec, *trec;

	TAILQ_FOREACH_SAFE(rec, &curbp->b_undo, next, trec)
		if (rec->type == MODIFIED) {
			TAILQ_REMOVE(&curbp->b_undo, rec, next);
			free_undo_record(rec);
		}

	rec = new_undo_record();
	rec->type = MODIFIED;

	TAILQ_INSERT_HEAD(&curbp->b_undo, rec, next);

	return;
}

int
undo_add_insert(struct line *lp, int offset, int size)
{
	struct region	reg;
	struct	undo_rec *rec;
	int	pos;

	if (!undo_enable_flag)
		return (TRUE);

	memset(&reg, 0, sizeof(reg));
	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;

	pos = find_dot(lp, offset);

	/*
	 * We try to reuse the last undo record to `compress' things.
	 */
	rec = TAILQ_FIRST(&curbp->b_undo);
	if (rec != NULL && rec->type == INSERT) {
		if (rec->pos + rec->region.r_size == pos) {
			rec->region.r_size += reg.r_size;
			return (TRUE);
		}
	}

	/*
	 * We couldn't reuse the last undo record, so prepare a new one.
	 */
	rec = new_undo_record();
	rec->pos = pos;
	rec->type = INSERT;
	memmove(&rec->region, &reg, sizeof(struct region));
	rec->content = NULL;

	undo_add_boundary(FFRAND, 1);

	TAILQ_INSERT_HEAD(&curbp->b_undo, rec, next);

	return (TRUE);
}

/*
 * This of course must be done _before_ the actual deletion is done.
 */
int
undo_add_delete(struct line *lp, int offset, int size, int isreg)
{
	struct region	reg;
	struct	undo_rec *rec;
	int	pos;

	if (!undo_enable_flag)
		return (TRUE);

	memset(&reg, 0, sizeof(reg));
	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;

	pos = find_dot(lp, offset);

	if (offset == llength(lp))	/* if it's a newline... */
		undo_add_boundary(FFRAND, 1);
	else if ((rec = TAILQ_FIRST(&curbp->b_undo)) != NULL) {
		/*
		 * Separate this command from the previous one if we're not
		 * just before the previous record...
		 */
		if (!isreg && rec->type == DELETE) {
			if (rec->pos - rec->region.r_size != pos)
				undo_add_boundary(FFRAND, 1);
		}
	}
	rec = new_undo_record();
	rec->pos = pos;
	if (isreg)
		rec->type = DELREG;
	else
		rec->type = DELETE;
	memmove(&rec->region, &reg, sizeof(struct region));
	do {
		rec->content = malloc(reg.r_size + 1);
	} while ((rec->content == NULL) && drop_oldest_undo_record());

	if (rec->content == NULL)
		panic("Out of memory");

	region_get_data(&reg, rec->content, reg.r_size);

	if (isreg || lastrectype() != DELETE)
		undo_add_boundary(FFRAND, 1);

	TAILQ_INSERT_HEAD(&curbp->b_undo, rec, next);

	return (TRUE);
}

/*
 * This of course must be called before the change takes place.
 */
int
undo_add_change(struct line *lp, int offset, int size)
{
	if (!undo_enable_flag)
		return (TRUE);
	undo_add_boundary(FFRAND, 1);
	boundary_flag = FALSE;
	undo_add_delete(lp, offset, size, 0);
	undo_add_insert(lp, offset, size);
	boundary_flag = TRUE;
	undo_add_boundary(FFRAND, 1);

	return (TRUE);
}

/*
 * Show the undo records for the current buffer in a new buffer.
 */
int
undo_dump(int f, int n)
{
	struct	 undo_rec *rec;
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 buf[4096], tmp[1024];
	int	 num;

	/*
	 * Prepare the buffer for insertion.
	 */
	if ((bp = bfind("*undo*", TRUE)) == NULL)
		return (FALSE);
	bp->b_flag |= BFREADONLY | BFIGNDIRTY;
	(void)bclear(bp);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == bp) {
			wp->w_dotp = bp->b_headp;
			wp->w_doto = 0;
		}
	}

	num = 0;
	TAILQ_FOREACH(rec, &curbp->b_undo, next) {
		num++;
		snprintf(buf, sizeof(buf),
		    "%d:\t %s at %d ", num,
		    (rec->type == DELETE) ? "DELETE":
		    (rec->type == DELREG) ? "DELREGION":
		    (rec->type == INSERT) ? "INSERT":
		    (rec->type == BOUNDARY) ? "----" :
		    (rec->type == MODIFIED) ? "MODIFIED": "UNKNOWN",
		    rec->pos);

		if (rec->content) {
			(void)strlcat(buf, "\"", sizeof(buf));
			snprintf(tmp, sizeof(tmp), "%.*s", rec->region.r_size,
			    rec->content);
			(void)strlcat(buf, tmp, sizeof(buf));
			(void)strlcat(buf, "\"", sizeof(buf));
		}
		snprintf(tmp, sizeof(tmp), " [%d]", rec->region.r_size);
		if (strlcat(buf, tmp, sizeof(buf)) >= sizeof(buf)) {
			dobeep();
			ewprintf("Undo record too large. Aborted.");
			return (FALSE);
		}
		addlinef(bp, "%s", buf);
	}
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == bp) {
			wp->w_dotline = num+1;
			wp->w_rflag |= WFFULL;
		}
	}
	return (TRUE);
}

/*
 * After the user did action1, then action2, then action3:
 *
 *	[action3] <--- Undoptr
 *	[action2]
 *	[action1]
 *	 ------
 *	 [undo]
 *
 * After undo:
 *
 *	[undo of action3]
 *	[action2] <--- Undoptr
 *	[action1]
 *	 ------
 *	 [undo]
 *
 * After another undo:
 *
 *
 *	[undo of action2]
 *	[undo of action3]
 *	[action1]  <--- Undoptr
 *	 ------
 *	 [undo]
 *
 * Note that the "undo of actionX" have no special meaning. Only when
 * we undo a deletion, the insertion will be recorded just as if it
 * was typed on the keyboard. Resulting in the inverse operation being
 * saved in the list.
 *
 * If undoptr reaches the bottom of the list, or if we moved between
 * two undo actions, we make it point back at the topmost record. This is
 * how we handle redoing.
 */
int
undo(int f, int n)
{
	struct undo_rec	*ptr, *nptr;
	int		 done, rval;
	struct line	*lp;
	int		 offset, save;
	static int	 nulled = FALSE;
	int		 lineno;

	if (n < 0)
		return (FALSE);

	ptr = curbp->b_undoptr;

	/* first invocation, make ptr point back to the top of the list */
	if ((ptr == NULL && nulled == TRUE) ||  rptcount == 0) {
		ptr = TAILQ_FIRST(&curbp->b_undo);
		nulled = TRUE;
	}

	rval = TRUE;
	while (n--) {
		/* if we have a spurious boundary, free it and move on.... */
		while (ptr && ptr->type == BOUNDARY) {
			nptr = TAILQ_NEXT(ptr, next);
			TAILQ_REMOVE(&curbp->b_undo, ptr, next);
			free_undo_record(ptr);
			ptr = nptr;
		}
		/*
		 * Ptr is NULL, but on the next run, it will point to the
		 * top again, redoing all stuff done in the buffer since
		 * its creation.
		 */
		if (ptr == NULL) {
			dobeep();
			ewprintf("No further undo information");
			rval = FALSE;
			nulled = TRUE;
			break;
		}
		nulled = FALSE;

		/*
		 * Loop while we don't get a boundary specifying we've
		 * finished the current action...
		 */

		undo_add_boundary(FFRAND, 1);

		save = boundary_flag;
		boundary_flag = FALSE;

		done = 0;
		do {
			/*
			 * Move to where this has to apply
			 *
			 * Boundaries (and the modified flag)  are put as
			 * position 0 (to save lookup time in find_dot)
			 * so we must not move there...
			 */
			if (ptr->type != BOUNDARY && ptr->type != MODIFIED) {
				if (find_lo(ptr->pos, &lp,
				    &offset, &lineno) == FALSE) {
					dobeep();
					ewprintf("Internal error in Undo!");
					rval = FALSE;
					break;
				}
				curwp->w_dotp = lp;
				curwp->w_doto = offset;
				curwp->w_markline = curwp->w_dotline;
				curwp->w_dotline = lineno;
			}

			/*
			 * Do operation^-1
			 */
			switch (ptr->type) {
			case INSERT:
				ldelete(ptr->region.r_size, KNONE);
				break;
			case DELETE:
				lp = curwp->w_dotp;
				offset = curwp->w_doto;
				region_put_data(ptr->content,
				    ptr->region.r_size);
				curwp->w_dotp = lp;
				curwp->w_doto = offset;
				break;
			case DELREG:
				region_put_data(ptr->content,
				    ptr->region.r_size);
				break;
			case BOUNDARY:
				done = 1;
				break;
			case MODIFIED:
				curbp->b_flag &= ~BFCHG;
				break;
			default:
				break;
			}

			/* And move to next record */
			ptr = TAILQ_NEXT(ptr, next);
		} while (ptr != NULL && !done);

		boundary_flag = save;
		undo_add_boundary(FFRAND, 1);

		ewprintf("Undo!");
	}

	curbp->b_undoptr = ptr;

	return (rval);
}
