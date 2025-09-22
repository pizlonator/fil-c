
#include <ctype.h>

#include "fdiskP.h"
#include "strutils.h"

/**
 * SECTION: parttype
 * @title: Partition types
 * @short_description: abstraction to partition types
 *
 * There are two basic types of parttypes, string based (e.g. GPT)
 * and code/hex based (e.g. MBR).
 */

/**
 * fdisk_new_parttype:
 *
 * It's recommended to use fdisk_label_get_parttype_from_code() or
 * fdisk_label_get_parttype_from_string() for well known types rather
 * than allocate a new instance.
 *
 * Returns: new instance.
 */
struct fdisk_parttype *fdisk_new_parttype(void)
{
	struct fdisk_parttype *t = calloc(1, sizeof(*t));

	if (!t)
		return NULL;

	t->refcount = 1;
	t->flags = FDISK_PARTTYPE_ALLOCATED;
	DBG(PARTTYPE, ul_debugobj(t, "alloc"));
	return t;
}

/**
 * fdisk_ref_parttype:
 * @t: partition type
 *
 * Increments reference counter for allocated types
 */
void fdisk_ref_parttype(struct fdisk_parttype *t)
{
	if (fdisk_parttype_is_allocated(t))
		t->refcount++;
}

/**
 * fdisk_unref_parttype
 * @t: partition pointer
 *
 * Decrements reference counter, on zero the @t is automatically
 * deallocated.
 */
void fdisk_unref_parttype(struct fdisk_parttype *t)
{
	if (!fdisk_parttype_is_allocated(t))
		return;

	t->refcount--;
	if (t->refcount <= 0) {
		DBG(PARTTYPE, ul_debugobj(t, "free"));
		free(t->typestr);
		free(t->name);
		free(t);
	}
}

/**
 * fdisk_parttype_set_name:
 * @t: partition type
 * @str: type name
 *
 * Sets type name to allocated partition type, for static types
 * it returns -EINVAL.
 *
 * Return: 0 on success, <0 on error
 */
int fdisk_parttype_set_name(struct fdisk_parttype *t, const char *str)
{
	if (!t || !fdisk_parttype_is_allocated(t))
		return -EINVAL;
	return strdup_to_struct_member(t, name, str);
}

/**
 * fdisk_parttype_set_typestr:
 * @t: partition type
 * @str: type identifier (e.g. GUID for GPT)
 *
 * Sets type string to allocated partition type, for static types
 * it returns -EINVAL. Don't use this function for MBR, see
 * fdisk_parttype_set_code().
 *
 * Return: 0 on success, <0 on error
 */
int fdisk_parttype_set_typestr(struct fdisk_parttype *t, const char *str)
{
	if (!t || !fdisk_parttype_is_allocated(t))
		return -EINVAL;
	return strdup_to_struct_member(t, typestr, str);
}

/**
 * fdisk_parttype_set_code:
 * @t: partition type
 * @code: type identifier (e.g. MBR type codes)
 *
 * Sets type code to allocated partition type, for static types it returns
 * -EINVAL. Don't use this function for GPT, see fdisk_parttype_set_typestr().
 *
 * Return: 0 on success, <0 on error
 */
int fdisk_parttype_set_code(struct fdisk_parttype *t, int code)
{
	if (!t || !fdisk_parttype_is_allocated(t))
		return -EINVAL;
	t->code = code;
	return 0;
}

/**
 * fdisk_label_get_nparttypes:
 * @lb: label
 *
 * Returns: number of types supported by label.
 */
size_t fdisk_label_get_nparttypes(const struct fdisk_label *lb)
{
	if (!lb)
		return 0;
	return lb->nparttypes;
}

/**
 * fdisk_label_get_parttype:
 * @lb: label
 * @n: number
 *
 * Returns: return parttype
 */
struct fdisk_parttype *fdisk_label_get_parttype(const struct fdisk_label *lb, size_t n)
{
	if (!lb || n >= lb->nparttypes)
		return NULL;
	return (struct fdisk_parttype *)&lb->parttypes[n];
}

/**
 * fdisk_label_get_parttype_shortcut:
 * @lb: label
 * @n: number
 * @typestr: returns type as string
 * @shortcut: returns type shortcut string
 * @alias: returns type alias string
 *
 * Returns: return 0 on success, <0 on error, 2 for deprecated alias, 1 for @n out of range
 *
 * Since: 2.36
 */
int fdisk_label_get_parttype_shortcut(const struct fdisk_label *lb, size_t n,
		const char **typestr, const char **shortcut, const char **alias)
{
	const struct fdisk_shortcut *sc;

	if (!lb)
		return -EINVAL;
	if (n >= lb->nparttype_cuts)
		return 1;

	sc = &lb->parttype_cuts[n];
	if (typestr)
		*typestr = sc->data;
	if (shortcut)
		*shortcut = sc->shortcut;
	if (alias)
		*alias = sc->alias;

	return sc->deprecated == 1 ? 2 : 0;

}


/**
 * fdisk_label_has_code_parttypes:
 * @lb: label
 *
 * Returns: 1 if the label uses code as partition type
 *          identifiers (e.g. MBR) or 0.
 */
int fdisk_label_has_code_parttypes(const struct fdisk_label *lb)
{
	assert(lb);

	if (lb->parttypes && lb->parttypes[0].typestr)
		return 0;
	return 1;
}

/**
 * fdisk_label_has_parttypes_shortcuts
 * @lb: label
 *
 * Returns: 1 if the label support shortuts/aliases for partition types or 0.
 *
 * Since: 2.36
 */
int fdisk_label_has_parttypes_shortcuts(const struct fdisk_label *lb)
{
	assert(lb);
	return lb->nparttype_cuts ? 1 : 0;
}


/**
 * fdisk_label_get_parttype_from_code:
 * @lb: label
 * @code: code to search for
 *
 * Search for partition type in label-specific table. The result
 * is pointer to static array of label types.
 *
 * Returns: partition type or NULL upon failure or invalid @code.
 */
struct fdisk_parttype *fdisk_label_get_parttype_from_code(
				const struct fdisk_label *lb,
				unsigned int code)
{
	size_t i;

	assert(lb);

	if (!lb->nparttypes)
		return NULL;

	for (i = 0; i < lb->nparttypes; i++)
		if (lb->parttypes[i].code == code)
			return (struct fdisk_parttype *)&lb->parttypes[i];
	return NULL;
}

/**
 * fdisk_label_get_parttype_from_string:
 * @lb: label
 * @str: string to search for
 *
 * Search for partition type in label-specific table. The result
 * is pointer to static array of label types.
 *
 * Returns: partition type or NULL upon failure or invalid @str.
 */
struct fdisk_parttype *fdisk_label_get_parttype_from_string(
				const struct fdisk_label *lb,
				const char *str)
{
	size_t i;

	assert(lb);

	if (!lb->nparttypes)
		return NULL;

	for (i = 0; i < lb->nparttypes; i++)
		if (lb->parttypes[i].typestr
		    && strcasecmp(lb->parttypes[i].typestr, str) == 0)
			return (struct fdisk_parttype *)&lb->parttypes[i];

	return NULL;
}

/**
 * fdisk_new_unknown_parttype:
 * @code: type as number
 * @typestr: type as string

 * Allocates new 'unknown' partition type. Use fdisk_unref_parttype() to
 * deallocate.
 *
 * Returns: newly allocated partition type, or NULL upon failure.
 */
struct fdisk_parttype *fdisk_new_unknown_parttype(unsigned int code,
						  const char *typestr)
{
	struct fdisk_parttype *t = fdisk_new_parttype();

	if (!t)
		return NULL;

	fdisk_parttype_set_name(t, _("unknown"));
	fdisk_parttype_set_code(t, code);
	fdisk_parttype_set_typestr(t, typestr);
	t->flags |= FDISK_PARTTYPE_UNKNOWN;

	return t;
}

/**
 * fdisk_copy_parttype:
 * @type: type to copy
 *
 * Use fdisk_unref_parttype() to deallocate.
 *
 * Returns: newly allocated partition type, or NULL upon failure.
 */
struct fdisk_parttype *fdisk_copy_parttype(const struct fdisk_parttype *type)
{
	struct fdisk_parttype *t = fdisk_new_parttype();

	if (!t)
		return NULL;

	fdisk_parttype_set_name(t, type->name);
	fdisk_parttype_set_code(t, type->code);
	fdisk_parttype_set_typestr(t, type->typestr);

	return t;
}

static struct fdisk_parttype *parttype_from_data(
				const struct fdisk_label *lb,
				const char *str,
				unsigned int *xcode,
				int use_seqnum)
{
	const struct fdisk_parttype *types;
	struct fdisk_parttype *ret = NULL;
	char *end = NULL;

	assert(lb);
	assert(str);

	if (xcode)
		*xcode = 0;
	if (!lb->nparttypes)
		return NULL;

	DBG(LABEL, ul_debugobj(lb, " parsing '%s' data", str));
	types = lb->parttypes;

	if (types[0].typestr == NULL) {
		unsigned int code;

		DBG(LABEL, ul_debugobj(lb, " +hex"));

		errno = 0;
		code = strtol(str, &end, 16);

		if (errno || *end != '\0') {
			DBG(LABEL, ul_debugobj(lb, "  failed: %m"));
			return NULL;
		}
		if (xcode)
			*xcode = code;
		ret = fdisk_label_get_parttype_from_code(lb, code);
	} else {
		DBG(LABEL, ul_debugobj(lb, " +string"));

		/* maybe specified by type string (e.g. UUID) */
		ret = fdisk_label_get_parttype_from_string(lb, str);

		if (!ret) {
			/* maybe specified by order number */
			int i;

			errno = 0;
			i = strtol(str, &end, 0);

			if (use_seqnum && errno == 0
			    && *end == '\0' && i > 0
			    && i - 1 < (int) lb->nparttypes)
				ret = (struct fdisk_parttype *)&types[i - 1];
		}
	}

	if (ret)
		DBG(PARTTYPE, ul_debugobj(ret, " result '%s'", ret->name));
	return ret;
}

static struct fdisk_parttype *parttype_from_shortcut(
				const struct fdisk_label *lb,
				const char *str, int deprecated)
{
	size_t i;

	DBG(LABEL, ul_debugobj(lb, " parsing '%s' shortcut", str));

	for (i = 0; i < lb->nparttype_cuts; i++) {
		const struct fdisk_shortcut *sc = &lb->parttype_cuts[i];

		if (sc->deprecated && !deprecated)
			continue;
		if (sc->shortcut && strcmp(sc->shortcut, str) == 0)
			return parttype_from_data(lb, sc->data, NULL, 0);
	}
	return NULL;
}

static struct fdisk_parttype *parttype_from_alias(
				const struct fdisk_label *lb,
				const char *str, int deprecated)
{
	size_t i;

	DBG(LABEL, ul_debugobj(lb, " parsing '%s' alias", str));

	for (i = 0; i < lb->nparttype_cuts; i++) {
		const struct fdisk_shortcut *sc = &lb->parttype_cuts[i];

		if (sc->deprecated && !deprecated)
			continue;
		if (sc->alias && strcmp(sc->alias, str) == 0)
			return parttype_from_data(lb, sc->data, NULL, 0);
	}
	return NULL;
}

static struct fdisk_parttype *parttype_from_name(
				const struct fdisk_label *lb,
				const char *str)
{
	size_t i;

	DBG(LABEL, ul_debugobj(lb, " parsing '%s' name", str));

	for (i = 0; i < lb->nparttypes; i++) {
		const char *name = lb->parttypes[i].name;

		if (name && *name && ul_stralnumcmp(name, str) == 0)
			return (struct fdisk_parttype *)&lb->parttypes[i];
	}

	return NULL;
}

/**
 * fdisk_label_advparse_parttype:
 * @lb: label
 * @str: string to parse from
 * @flags: FDISK_PARTTYPE_PARSE_*
 *
 * This function is advanced partition types parser. It parses partition type
 * from @str according to the label. The function returns a pointer to static
 * table of the partition types, or newly allocated partition type for unknown
 * types (see fdisk_parttype_is_unknown(). It's safe to call fdisk_unref_parttype()
 * for all results.
 *
 * The @str may be type data (hex code or UUID), alias or shortcut. For GPT
 * also sequence number of the type in the list of the supported types.
 *
 * Returns: pointer to type or NULL on error.
 */
struct fdisk_parttype *fdisk_label_advparse_parttype(
				const struct fdisk_label *lb,
				const char *str,
				int flags)
{
	struct fdisk_parttype *res = NULL;
	unsigned int code = 0;

	if (!lb || !lb->nparttypes)
		return NULL;

	DBG(LABEL, ul_debugobj(lb, "parsing '%s' (%s) type", str, lb->name));

	if ((flags & FDISK_PARTTYPE_PARSE_DATA)
	    && !(flags & FDISK_PARTTYPE_PARSE_DATALAST))
		res = parttype_from_data(lb, str, &code,
				flags & FDISK_PARTTYPE_PARSE_SEQNUM);

	if (!res && (flags & FDISK_PARTTYPE_PARSE_ALIAS))
		res = parttype_from_alias(lb, str,
				flags & FDISK_PARTTYPE_PARSE_DEPRECATED);

	if (!res && (flags & FDISK_PARTTYPE_PARSE_SHORTCUT))
		res = parttype_from_shortcut(lb, str,
				flags & FDISK_PARTTYPE_PARSE_DEPRECATED);

	if (!res && (flags & FDISK_PARTTYPE_PARSE_NAME))
		res = parttype_from_name(lb, str);

	if (!res && (flags & FDISK_PARTTYPE_PARSE_DATA)
	    && (flags & FDISK_PARTTYPE_PARSE_DATALAST))
		res = parttype_from_data(lb, str, &code,
				flags & FDISK_PARTTYPE_PARSE_SEQNUM);

	if (!res && !(flags & FDISK_PARTTYPE_PARSE_NOUNKNOWN)) {
		if (lb->parttypes[0].typestr)
			res = fdisk_new_unknown_parttype(0, str);
		else
			res = fdisk_new_unknown_parttype(code, NULL);
	}

	if (res)
		DBG(PARTTYPE, ul_debugobj(res, "returns parsed '%s' [%s] partition type",
				res->name, res->typestr ? : ""));
	return res;
}

/**
 * fdisk_label_parse_parttype:
 * @lb: label
 * @str: string to parse from (type name, UUID, etc.)
 *
 * Parses partition type from @str according to the label. The function returns
 * a pointer to static table of the partition types, or newly allocated
 * partition type for unknown types (see fdisk_parttype_is_unknown(). It's
 * safe to call fdisk_unref_parttype() for all results.
 *
 * Note that for GPT it accepts sequence number of UUID.
 *
 * Returns: pointer to type or NULL on error.
 */
struct fdisk_parttype *fdisk_label_parse_parttype(
				const struct fdisk_label *lb,
				const char *str)
{
	return fdisk_label_advparse_parttype(lb, str, FDISK_PARTTYPE_PARSE_DATA);
}

/**
 * fdisk_parttype_get_string:
 * @t: type
 *
 * Returns: partition type string (e.g. GUID for GPT)
 */
const char *fdisk_parttype_get_string(const struct fdisk_parttype *t)
{
	assert(t);
	return t->typestr && *t->typestr ? t->typestr : NULL;
}

/**
 * fdisk_parttype_get_code:
 * @t: type
 *
 * Returns: partition type code (e.g. for MBR)
 */
unsigned int fdisk_parttype_get_code(const struct fdisk_parttype *t)
{
	assert(t);
	return t->code;
}

/**
 * fdisk_parttype_get_name:
 * @t: type
 *
 * Returns: partition type human readable name
 */
const char *fdisk_parttype_get_name(const struct fdisk_parttype *t)
{
	assert(t);
	return t->name;
}

/**
 * fdisk_parttype_is_unknown:
 * @t: type
 *
 * Checks for example result from fdisk_label_parse_parttype().
 *
 * Returns: 1 is type is "unknown" or 0.
 */
int fdisk_parttype_is_unknown(const struct fdisk_parttype *t)
{
	return t && (t->flags & FDISK_PARTTYPE_UNKNOWN) ? 1 : 0;
}
