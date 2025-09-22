
#include "strutils.h"
#include "fdiskP.h"

/**
 * SECTION: ask
 * @title: Ask
 * @short_description: interface for dialog driven partitioning, warning and info messages
 *
 */

static void fdisk_ask_menu_reset_items(struct fdisk_ask *ask);


/**
 * fdisk_set_ask:
 * @cxt: context
 * @ask_cb: callback
 * @data: callback data
 *
 * Set callback for dialog driven partitioning and library warnings/errors.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_set_ask(struct fdisk_context *cxt,
		int (*ask_cb)(struct fdisk_context *, struct fdisk_ask *, void *),
		void *data)
{
	assert(cxt);

	cxt->ask_cb = ask_cb;
	cxt->ask_data = data;
	return 0;
}

struct fdisk_ask *fdisk_new_ask(void)
{
	struct fdisk_ask *ask = calloc(1, sizeof(struct fdisk_ask));

	if (!ask)
		return NULL;

	DBG(ASK, ul_debugobj(ask, "alloc"));
	ask->refcount = 1;
	return ask;
}

void fdisk_reset_ask(struct fdisk_ask *ask)
{
	int refcount;

	assert(ask);
	free(ask->query);

	DBG(ASK, ul_debugobj(ask, "reset"));
	refcount = ask->refcount;

	if (fdisk_is_ask(ask, MENU))
		fdisk_ask_menu_reset_items(ask);

	memset(ask, 0, sizeof(*ask));
	ask->refcount = refcount;
}

/**
 * fdisk_ref_ask:
 * @ask: ask instance
 *
 * Increments reference counter.
 */
void fdisk_ref_ask(struct fdisk_ask *ask)
{
	if (ask)
		ask->refcount++;
}


/**
 * fdisk_unref_ask:
 * @ask: ask instance
 *
 * Decrements reference counter, on zero the @ask is automatically
 * deallocated.
 */
void fdisk_unref_ask(struct fdisk_ask *ask)
{
	if (!ask)
		return;
	ask->refcount--;

	if (ask->refcount <= 0) {
		fdisk_reset_ask(ask);
		DBG(ASK, ul_debugobj(ask, "free"));
		free(ask);
	}
}

/**
 * fdisk_ask_get_query:
 * @ask: ask instance
 *
 * Returns: pointer to dialog string.
 */
const char *fdisk_ask_get_query(struct fdisk_ask *ask)
{
	assert(ask);
	return ask->query;
}

int fdisk_ask_set_query(struct fdisk_ask *ask, const char *str)
{
	assert(ask);
	return strdup_to_struct_member(ask, query, str);
}

/**
 * fdisk_ask_get_type:
 * @ask: ask instance
 *
 * Returns: FDISK_ASKTYPE_*
 */
int fdisk_ask_get_type(struct fdisk_ask *ask)
{
	assert(ask);
	return ask->type;
}

int fdisk_ask_set_type(struct fdisk_ask *ask, int type)
{
	assert(ask);
	ask->type = type;
	return 0;
}

int fdisk_do_ask(struct fdisk_context *cxt, struct fdisk_ask *ask)
{
	int rc;

	assert(ask);
	assert(cxt);

	DBG(ASK, ul_debugobj(ask, "do_ask for '%s'",
				ask->query ? ask->query :
				ask->type == FDISK_ASKTYPE_INFO  ? "info" :
				ask->type == FDISK_ASKTYPE_WARNX ? "warnx" :
				ask->type == FDISK_ASKTYPE_WARN  ? "warn" :
				"?nothing?"));

	if (!fdisk_has_dialogs(cxt) &&
	    !(ask->type == FDISK_ASKTYPE_INFO ||
	      ask->type == FDISK_ASKTYPE_WARNX ||
	      ask->type == FDISK_ASKTYPE_WARN)) {
		DBG(ASK, ul_debugobj(ask, "dialogs disabled"));
		return -EINVAL;
	}

	if (!cxt->ask_cb) {
		DBG(ASK, ul_debugobj(ask, "no ask callback specified!"));
		return -EINVAL;
	}

	rc = cxt->ask_cb(cxt, ask, cxt->ask_data);

	DBG(ASK, ul_debugobj(ask, "do_ask done [rc=%d]", rc));
	return rc;
}

#define is_number_ask(a)  (fdisk_is_ask(a, NUMBER) || fdisk_is_ask(a, OFFSET))

/**
 * fdisk_ask_number_get_range:
 * @ask: ask instance
 *
 * Returns: string with range (e.g. "1,3,5-10")
 */
const char *fdisk_ask_number_get_range(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.range;
}

int fdisk_ask_number_set_range(struct fdisk_ask *ask, const char *range)
{
	assert(ask);
	assert(is_number_ask(ask));
	ask->data.num.range = range;
	return 0;
}

/**
 * fdisk_ask_number_get_default:
 * @ask: ask instance
 *
 * Returns: default number
 *
 */
uint64_t fdisk_ask_number_get_default(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.dfl;
}

int fdisk_ask_number_set_default(struct fdisk_ask *ask, uint64_t dflt)
{
	assert(ask);
	ask->data.num.dfl = dflt;
	return 0;
}

/**
 * fdisk_ask_number_get_low:
 * @ask: ask instance
 *
 * Returns: minimal possible number when ask for numbers in range
 */
uint64_t fdisk_ask_number_get_low(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.low;
}

int fdisk_ask_number_set_low(struct fdisk_ask *ask, uint64_t low)
{
	assert(ask);
	ask->data.num.low = low;
	return 0;
}

/**
 * fdisk_ask_number_get_high:
 * @ask: ask instance
 *
 * Returns: maximal possible number when ask for numbers in range
 */
uint64_t fdisk_ask_number_get_high(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.hig;
}

int fdisk_ask_number_set_high(struct fdisk_ask *ask, uint64_t high)
{
	assert(ask);
	ask->data.num.hig = high;
	return 0;
}

/**
 * fdisk_ask_number_get_result:
 * @ask: ask instance
 *
 * Returns: result
 */
uint64_t fdisk_ask_number_get_result(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.result;
}

/**
 * fdisk_ask_number_set_result:
 * @ask: ask instance
 * @result: dialog result
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_ask_number_set_result(struct fdisk_ask *ask, uint64_t result)
{
	assert(ask);
	ask->data.num.result = result;
	return 0;
}

/**
 * fdisk_ask_number_get_base:
 * @ask: ask instance
 *
 * Returns: base when user specify number in relative notation (+size)
 */
uint64_t fdisk_ask_number_get_base(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.base;
}

int fdisk_ask_number_set_base(struct fdisk_ask *ask, uint64_t base)
{
	assert(ask);
	ask->data.num.base = base;
	return 0;
}

/**
 * fdisk_ask_number_get_unit:
 * @ask: ask instance
 *
 * Returns: number of bytes per the unit
 */
uint64_t fdisk_ask_number_get_unit(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.unit;
}

int fdisk_ask_number_set_unit(struct fdisk_ask *ask, uint64_t unit)
{
	assert(ask);
	ask->data.num.unit = unit;
	return 0;
}

int fdisk_ask_number_is_relative(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.relative;
}

/**
 * fdisk_ask_number_is_wrap_negative:
 * @ask: ask instance
 *
 * The wrap-negative flag can be used to accept negative number from user. In this
 * case the dialog result is calculated as "high - num" (-N from high limit).
 *
 * Returns: 1 or 0.
 *
 * Since: 2.33
 */
int fdisk_ask_number_is_wrap_negative(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.wrap_negative;
}

/**
 * fdisk_ask_number_set_relative
 * @ask: ask instance
 * @relative: 0 or 1
 *
 * Inform libfdisk that user can specify the number in relative notation rather than
 * by explicit number. This is useful for some optimization (e.g.
 * align end of partition, etc.)
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_ask_number_set_relative(struct fdisk_ask *ask, int relative)
{
	assert(ask);
	ask->data.num.relative = relative ? 1 : 0;
	return 0;
}

/**
 * fdisk_ask_number_inchars:
 * @ask: ask instance
 *
 * For example for BSD is normal to address partition by chars rather than by
 * number (first partition is 'a').
 *
 * Returns: 1 if number should be presented as chars
 *
 */
int fdisk_ask_number_inchars(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_number_ask(ask));
	return ask->data.num.inchars;
}

int fdisk_ask_number_set_wrap_negative(struct fdisk_ask *ask, int wrap_negative)
{
	assert(ask);
	ask->data.num.wrap_negative = wrap_negative ? 1 : 0;
	return 0;
}

/*
 * Generates string with list ranges (e.g. 1,2,5-8) for the 'cur'
 */
#define tochar(num)	((int) ('a' + num - 1))
static char *mk_string_list(char *ptr, size_t *len, size_t *begin,
			    size_t *run, ssize_t cur, int inchar)
{
	int rlen;

	if (cur != -1) {
		if (!*begin) {			/* begin of the list */
			*begin = cur + 1;
			return ptr;
		}

		if (*begin + *run == (size_t)cur) {	/* no gap, continue */
			(*run)++;
			return ptr;
		}
	} else if (!*begin) {
		*ptr = '\0';
		return ptr;		/* end of empty list */
	}

					/* add to the list */
	if (!*run)
		rlen = inchar ? snprintf(ptr, *len, "%c,", tochar(*begin)) :
				snprintf(ptr, *len, "%zu,", *begin);
	else if (*run == 1)
		rlen = inchar ?
			snprintf(ptr, *len, "%c,%c,", tochar(*begin), tochar(*begin + 1)) :
			snprintf(ptr, *len, "%zu,%zu,", *begin, *begin + 1);
	else
		rlen = inchar ?
			snprintf(ptr, *len, "%c-%c,", tochar(*begin), tochar(*begin + *run)) :
			snprintf(ptr, *len, "%zu-%zu,", *begin, *begin + *run);

	if (rlen < 0 || (size_t) rlen >= *len)
		return NULL;

	ptr += rlen;
	*len -= rlen;

	if (cur == -1 && *begin) {
		/* end of the list */
		*(ptr - 1) = '\0';	/* remove tailing ',' from the list */
		return ptr;
	}

	*begin = cur + 1;
	*run = 0;

	return ptr;
}

/**
 * fdisk_ask_partnum:
 * @cxt: context
 * @partnum: returns partition number
 * @wantnew: 0|1
 *
 * High-level API to ask for used or unused partition number.
 *
 * Returns: 0 on success, < 0 on error, 1 if no free/used partition
 */
int fdisk_ask_partnum(struct fdisk_context *cxt, size_t *partnum, int wantnew)
{
	int rc = 0, inchar = 0;
	char range[BUFSIZ], *ptr = range;
	size_t i, len = sizeof(range), begin = 0, run = 0;
	struct fdisk_ask *ask = NULL;
	__typeof__(ask->data.num) *num;

	assert(cxt);
	assert(cxt->label);
	assert(partnum);

	if (cxt->label && cxt->label->flags & FDISK_LABEL_FL_INCHARS_PARTNO)
		inchar = 1;

	DBG(ASK, ul_debug("%s: asking for %s partition number "
			  "(max: %zu, inchar: %s)",
			cxt->label ? cxt->label->name : "???",
			wantnew ? "new" : "used",
			cxt->label ? cxt->label->nparts_max : 0,
			inchar ? "yes" : "not"));

	ask = fdisk_new_ask();
	if (!ask)
		return -ENOMEM;

	fdisk_ask_set_type(ask, FDISK_ASKTYPE_NUMBER);
	num = &ask->data.num;

	ask->data.num.inchars = inchar ? 1 : 0;

	for (i = 0; i < cxt->label->nparts_max; i++) {
		int used = fdisk_is_partition_used(cxt, i);

		if (wantnew && !used) {
			ptr = mk_string_list(ptr, &len, &begin, &run, i, inchar);
			if (!ptr) {
				rc = -EINVAL;
				break;
			}
			if (!num->low)
				num->dfl = num->low = i + 1;
			num->hig = i + 1;
		} else if (!wantnew && used) {
			ptr = mk_string_list(ptr, &len, &begin, &run, i, inchar);
			if (!num->low)
				num->low = i + 1;
			num->dfl = num->hig = i + 1;
		}
	}

	DBG(ASK, ul_debugobj(ask, "ask limits: low: %"PRIu64", high: %"PRIu64", default: %"PRIu64"",
				num->low, num->hig, num->dfl));

	if (!rc && !wantnew && num->low == num->hig) {
		if (num->low > 0) {
			/* only one existing partition, don't ask, return the number */
			fdisk_ask_number_set_result(ask, num->low);
			fdisk_info(cxt, _("Selected partition %ju"), num->low);

		} else if (num->low == 0) {
			fdisk_warnx(cxt, _("No partition is defined yet!"));
			rc = 1;
		}
		goto dont_ask;
	}
	if (!rc && wantnew && num->low == num->hig) {
		if (num->low > 0) {
			/* only one free partition, don't ask, return the number */
			fdisk_ask_number_set_result(ask, num->low);
			fdisk_info(cxt, _("Selected partition %ju"), num->low);
		}
		if (num->low == 0) {
			fdisk_warnx(cxt, _("No free partition available!"));
			rc = 1;
		}
		goto dont_ask;
	}
	if (!rc) {
		mk_string_list(ptr, &len, &begin, &run, -1, inchar);	/* terminate the list */
		rc = fdisk_ask_number_set_range(ask, range);
	}
	if (!rc)
		rc = fdisk_ask_set_query(ask, _("Partition number"));
	if (!rc)
		rc = fdisk_do_ask(cxt, ask);

dont_ask:
	if (!rc) {
		*partnum = fdisk_ask_number_get_result(ask);
		if (*partnum)
			*partnum -= 1;
	}
	DBG(ASK, ul_debugobj(ask, "result: %"PRIu64" [rc=%d]\n", fdisk_ask_number_get_result(ask), rc));
	fdisk_unref_ask(ask);
	return rc;
}

/**
 * fdisk_ask_number:
 * @cxt: context
 * @low: minimal possible number
 * @dflt: default suggestion
 * @high: maximal possible number
 * @query: question string
 * @result: returns result
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_ask_number(struct fdisk_context *cxt,
		     uintmax_t low,
		     uintmax_t dflt,
		     uintmax_t high,
		     const char *query,
		     uintmax_t *result)
{
	struct fdisk_ask *ask;
	int rc;

	assert(cxt);

	ask = fdisk_new_ask();
	if (!ask)
		return -ENOMEM;

	rc = fdisk_ask_set_type(ask, FDISK_ASKTYPE_NUMBER);
	if (!rc)
		fdisk_ask_number_set_low(ask, low);
	if (!rc)
		fdisk_ask_number_set_default(ask, dflt);
	if (!rc)
		fdisk_ask_number_set_high(ask, high);
	if (!rc)
		fdisk_ask_set_query(ask, query);
	if (!rc)
		rc = fdisk_do_ask(cxt, ask);
	if (!rc)
		*result = fdisk_ask_number_get_result(ask);

	DBG(ASK, ul_debugobj(ask, "result: %ju [rc=%d]\n", *result, rc));
	fdisk_unref_ask(ask);
	return rc;
}

/**
 * fdisk_ask_string_get_result:
 * @ask: ask instance
 *
 * Returns: pointer to dialog result
 */
char *fdisk_ask_string_get_result(struct fdisk_ask *ask)
{
	assert(ask);
	assert(fdisk_is_ask(ask, STRING));
	return ask->data.str.result;
}

/**
 * fdisk_ask_string_set_result:
 * @ask: ask instance
 * @result: pointer to allocated buffer with string
 *
 * You don't have to care about the @result deallocation, libfdisk is going to
 * deallocate the result when destroy @ask instance.
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_ask_string_set_result(struct fdisk_ask *ask, char *result)
{
	assert(ask);
	ask->data.str.result = result;
	return 0;
}

/**
 * fdisk_ask_string:
 * @cxt: context:
 * @query: question string
 * @result: returns allocated buffer
 *
 * High-level API to ask for strings. Don't forget to deallocate the @result.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_ask_string(struct fdisk_context *cxt,
		     const char *query,
		     char **result)
{
	struct fdisk_ask *ask;
	int rc;

	assert(cxt);

	ask = fdisk_new_ask();
	if (!ask)
		return -ENOMEM;

	rc = fdisk_ask_set_type(ask, FDISK_ASKTYPE_STRING);
	if (!rc)
		fdisk_ask_set_query(ask, query);
	if (!rc)
		rc = fdisk_do_ask(cxt, ask);
	if (!rc)
		*result = fdisk_ask_string_get_result(ask);

	DBG(ASK, ul_debugobj(ask, "result: %s [rc=%d]\n", *result, rc));
	fdisk_unref_ask(ask);
	return rc;
}

/**
 * fdisk_ask_yesno:
 * @cxt: context
 * @query: question string
 * @result: returns 0 (no) or 1 (yes)
 *
 * High-level API to ask Yes/No questions
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_ask_yesno(struct fdisk_context *cxt,
		     const char *query,
		     int *result)
{
	struct fdisk_ask *ask;
	int rc;

	assert(cxt);

	ask = fdisk_new_ask();
	if (!ask)
		return -ENOMEM;

	rc = fdisk_ask_set_type(ask, FDISK_ASKTYPE_YESNO);
	if (!rc)
		fdisk_ask_set_query(ask, query);
	if (!rc)
		rc = fdisk_do_ask(cxt, ask);
	if (!rc)
		*result = fdisk_ask_yesno_get_result(ask) == 1 ? 1 : 0;

	DBG(ASK, ul_debugobj(ask, "result: %d [rc=%d]\n", *result, rc));
	fdisk_unref_ask(ask);
	return rc;
}

/**
 * fdisk_ask_yesno_get_result:
 * @ask: ask instance
 *
 * Returns: 0 or 1
 */
int fdisk_ask_yesno_get_result(struct fdisk_ask *ask)
{
	assert(ask);
	assert(fdisk_is_ask(ask, YESNO));
	return ask->data.yesno.result;
}

/**
 * fdisk_ask_yesno_set_result:
 * @ask: ask instance
 * @result: 1 or 0
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_ask_yesno_set_result(struct fdisk_ask *ask, int result)
{
	assert(ask);
	ask->data.yesno.result = result;
	return 0;
}

/*
 * menu
 */
int fdisk_ask_menu_set_default(struct fdisk_ask *ask, int dfl)
{
	assert(ask);
	assert(fdisk_is_ask(ask, MENU));
	ask->data.menu.dfl = dfl;
	return 0;
}

/**
 * fdisk_ask_menu_get_default:
 * @ask: ask instance
 *
 * Returns: default menu item key
 */
int fdisk_ask_menu_get_default(struct fdisk_ask *ask)
{
	assert(ask);
	assert(fdisk_is_ask(ask, MENU));
	return ask->data.menu.dfl;
}

/**
 * fdisk_ask_menu_set_result:
 * @ask: ask instance
 * @key: result
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_ask_menu_set_result(struct fdisk_ask *ask, int key)
{
	assert(ask);
	assert(fdisk_is_ask(ask, MENU));
	ask->data.menu.result = key;
	DBG(ASK, ul_debugobj(ask, "menu result: %c\n", key));
	return 0;

}

/**
 * fdisk_ask_menu_get_result:
 * @ask: ask instance
 * @key: returns selected menu item key
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_ask_menu_get_result(struct fdisk_ask *ask, int *key)
{
	assert(ask);
	assert(fdisk_is_ask(ask, MENU));
	if (key)
		*key =  ask->data.menu.result;
	return 0;
}

/**
 * fdisk_ask_menu_get_item:
 * @ask: ask menu instance
 * @idx: wanted menu item index
 * @key: returns key of the menu item
 * @name: returns name of the menu item
 * @desc: returns description of the menu item
 *
 * Returns: 0 on success, <0 on error, >0 if idx out-of-range
 */
int fdisk_ask_menu_get_item(struct fdisk_ask *ask, size_t idx, int *key,
			    const char **name, const char **desc)
{
	size_t i;
	struct ask_menuitem *mi;

	assert(ask);
	assert(fdisk_is_ask(ask, MENU));

	for (i = 0, mi = ask->data.menu.first; mi; mi = mi->next, i++) {
		if (i == idx)
			break;
	}

	if (!mi)
		return 1;	/* no more items */
	if (key)
		*key = mi->key;
	if (name)
		*name = mi->name;
	if (desc)
		*desc = mi->desc;
	return 0;
}

static void fdisk_ask_menu_reset_items(struct fdisk_ask *ask)
{
	struct ask_menuitem *mi;

	assert(ask);
	assert(fdisk_is_ask(ask, MENU));

	for (mi = ask->data.menu.first; mi; ) {
		struct ask_menuitem *next = mi->next;
		free(mi);
		mi = next;
	}
}

/**
 * fdisk_ask_menu_get_nitems:
 * @ask: ask instance
 *
 * Returns: number of menu items
 */
size_t fdisk_ask_menu_get_nitems(struct fdisk_ask *ask)
{
	struct ask_menuitem *mi;
	size_t n;

	assert(ask);
	assert(fdisk_is_ask(ask, MENU));

	for (n = 0, mi = ask->data.menu.first; mi; mi = mi->next, n++);

	return n;
}

int fdisk_ask_menu_add_item(struct fdisk_ask *ask, int key,
			const char *name, const char *desc)
{
	struct ask_menuitem *mi;

	assert(ask);
	assert(fdisk_is_ask(ask, MENU));

	mi = calloc(1, sizeof(*mi));
	if (!mi)
		return -ENOMEM;
	mi->key = key;
	mi->name = name;
	mi->desc = desc;

	if (!ask->data.menu.first)
		ask->data.menu.first = mi;
	else {
	        struct ask_menuitem *last = ask->data.menu.first;

		while (last->next)
			last = last->next;
		last->next = mi;
	}

	DBG(ASK, ul_debugobj(ask, "new menu item: %c, \"%s\" (%s)\n", mi->key, mi->name, mi->desc));
	return 0;
}


/*
 * print-like
 */

#define is_print_ask(a) (fdisk_is_ask(a, WARN) || fdisk_is_ask(a, WARNX) || fdisk_is_ask(a, INFO))

/**
 * fdisk_ask_print_get_errno:
 * @ask: ask instance
 *
 * Returns: error number for warning/error messages
 */
int fdisk_ask_print_get_errno(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_print_ask(ask));
	return ask->data.print.errnum;
}

int fdisk_ask_print_set_errno(struct fdisk_ask *ask, int errnum)
{
	assert(ask);
	ask->data.print.errnum = errnum;
	return 0;
}

/**
 * fdisk_ask_print_get_mesg:
 * @ask: ask instance
 *
 * Returns: pointer to message
 */
const char *fdisk_ask_print_get_mesg(struct fdisk_ask *ask)
{
	assert(ask);
	assert(is_print_ask(ask));
	return ask->data.print.mesg;
}

/* does not reallocate the message! */
int fdisk_ask_print_set_mesg(struct fdisk_ask *ask, const char *mesg)
{
	assert(ask);
	ask->data.print.mesg = mesg;
	return 0;
}

static int do_vprint(struct fdisk_context *cxt, int errnum, int type,
		     const char *fmt, va_list va)
{
	struct fdisk_ask *ask;
	int rc;
	char *mesg;

	assert(cxt);

	if (vasprintf(&mesg, fmt, va) < 0)
		return -ENOMEM;

	ask = fdisk_new_ask();
	if (!ask) {
		free(mesg);
		return -ENOMEM;
	}

	fdisk_ask_set_type(ask, type);
	fdisk_ask_print_set_mesg(ask, mesg);
	if (errnum >= 0)
		fdisk_ask_print_set_errno(ask, errnum);
	rc = fdisk_do_ask(cxt, ask);

	fdisk_unref_ask(ask);
	free(mesg);
	return rc;
}

/**
 * fdisk_info:
 * @cxt: context
 * @fmt: printf-like formatted string
 * @...: variable parameters
 *
 * High-level API to print info messages,
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_info(struct fdisk_context *cxt, const char *fmt, ...)
{
	int rc;
	va_list ap;

	assert(cxt);
	va_start(ap, fmt);
	rc = do_vprint(cxt, -1, FDISK_ASKTYPE_INFO, fmt, ap);
	va_end(ap);
	return rc;
}

/**
 * fdisk_info:
 * @cxt: context
 * @fmt: printf-like formatted string
 * @...: variable parameters
 *
 * High-level API to print warning message (errno expected)
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_warn(struct fdisk_context *cxt, const char *fmt, ...)
{
	int rc;
	va_list ap;

	assert(cxt);
	va_start(ap, fmt);
	rc = do_vprint(cxt, errno, FDISK_ASKTYPE_WARN, fmt, ap);
	va_end(ap);
	return rc;
}

/**
 * fdisk_warnx:
 * @cxt: context
 * @fmt: printf-like formatted string
 * @...: variable options
 *
 * High-level API to print warning message
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_warnx(struct fdisk_context *cxt, const char *fmt, ...)
{
	int rc;
	va_list ap;

	assert(cxt);
	va_start(ap, fmt);
	rc = do_vprint(cxt, -1, FDISK_ASKTYPE_WARNX, fmt, ap);
	va_end(ap);
	return rc;
}

int fdisk_info_new_partition(
			struct fdisk_context *cxt,
			int num, fdisk_sector_t start, fdisk_sector_t stop,
			struct fdisk_parttype *t)
{
	int rc;
	char *str = size_to_human_string(SIZE_SUFFIX_3LETTER | SIZE_SUFFIX_SPACE,
				     (uint64_t)(stop - start + 1) * cxt->sector_size);

	rc = fdisk_info(cxt,
			_("Created a new partition %d of type '%s' and of size %s."),
			num, t ? t->name : _("Unknown"), str);
	free(str);
	return rc;
}

#ifdef TEST_PROGRAM
static int test_ranges(struct fdisk_test *ts __attribute__((unused)),
		       int argc __attribute__((unused)),
		       char *argv[] __attribute__((unused)))
{
	/*                1  -  3,       6,    8, 9,   11    13 */
	size_t nums[] = { 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1 };
	size_t numx[] = { 0, 0, 0 };
	char range[BUFSIZ], *ptr = range;
	size_t i, len = sizeof(range), begin = 0, run = 0;

	for (i = 0; i < ARRAY_SIZE(nums); i++) {
		if (!nums[i])
			continue;
		ptr = mk_string_list(ptr, &len, &begin, &run, i, 0);
	}
	mk_string_list(ptr, &len, &begin, &run, -1, 0);
	printf("list: '%s'\n", range);

	ptr = range;
	len = sizeof(range), begin = 0, run = 0;
	for (i = 0; i < ARRAY_SIZE(numx); i++) {
		if (!numx[i])
			continue;
		ptr = mk_string_list(ptr, &len, &begin, &run, i, 0);
	}
	mk_string_list(ptr, &len, &begin, &run, -1, 0);
	printf("empty list: '%s'\n", range);

	return 0;
}

int main(int argc, char *argv[])
{
	struct fdisk_test tss[] = {
	{ "--ranges",  test_ranges,    "generates ranges" },
	{ NULL }
	};

	return fdisk_run_test(tss, argc, argv);
}

#endif
