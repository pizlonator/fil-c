/*
 * This file is public domain software, i.e. not copyrighted.
 *
 * Warranty Exclusion
 * ------------------
 * You agree that this software is a non-commercially developed program
 * that may contain "bugs" (as that term is used in the industry) and
 * that it may not function as intended. The software is licensed
 * "as is". NSA makes no, and hereby expressly disclaims all, warranties,
 * express, implied, statutory, or otherwise with respect to the software,
 * including noninfringement and the implied warranties of merchantability
 * and fitness for a particular purpose.
 *
 * Limitation of Liability
 *-----------------------
 * In no event will NSA be liable for any damages, including loss of data,
 * lost profits, cost of cover, or other special, incidental, consequential,
 * direct or indirect damages arising from the software or the use thereof,
 * however caused and on any theory of liability. This limitation will apply
 * even if NSA has been advised of the possibility of such damage. You
 * acknowledge that this is a reasonable allocation of risk.
 *
 * Original author: James Carter
 */

#include <sepol/policydb/ebitmap.h>

#include "cil_internal.h"
#include "cil_find.h"
#include "cil_flavor.h"
#include "cil_list.h"
#include "cil_strpool.h"
#include "cil_log.h"
#include "cil_symtab.h"
#include "cil_build_ast.h"
#include "cil_copy_ast.h"
#include "cil_deny.h"

#define CIL_DENY_ATTR_PREFIX "deny_rule_attr"

/*
 * A deny rule is like a neverallow rule, except that permissions are
 * removed rather than an error reported.
 *
 * (allow S1 T1 P1)
 * (deny  S2 T2 P2)
 *
 * First, write the allow rule with all of the permissions not in the deny rule
 * P3 = P1 and not P2
 * (allow S1 T1 P3)
 *
 * Obviously, the rule is only written if P3 is not an empty list. This goes
 * for the rest of the rules as well--they are only written if the source and
 * target exist.
 *
 * The remaining rules will only involve the common permissions
 * P4 = P1 and P2
 *
 * Next, write the allow rule for any types in S1 that are not in S2
 * S3 = S1 and not S2
 * (allow S3 T1 P4)
 *
 * Finally, write any allow rules needed to cover the types in T1 that are
 * not in T2. Since, T1 and T2 might be "self", "notself", or "other", this
 * requires more complicated handling. Any rule with "self" will not match
 * a rule with either "notself" or "other".
 *
 * if (T1 is self and T2 is self) or (T1 is notself and T2 is notself) then
 *   Nothing more needs to be done.
 *
 * The rest of the rules will depend on the intersection of S1 and S2
 * which cannot be the empty set since the allow and deny rules match.
 * S4 = S1 and S2
 *
 * if T1 is notself or T1 is other or T2 is notself or T2 is other then
 *   if T1 is notself then
 *     if T2 is other then
 *       T = ALL and not S2
 *       (allow S4 T P4)
 *     else [T2 is not self, notself, or other]
 *       S5 = S4 and not T2
 *       S6 = S4 and T2
 *       TA = ALL and not T2
 *       TB = TA and not S4
 *       (allow S6 TA P4)
 *       (allow S5 TB P4)
 *       if cardinality(S5) > 1 then
 *         (allow S5 other P4)
 *   else if T1 is other then
 *     (allow S3 S4 P4)
 *     if T2 is notself then
 *       [Nothing else is needed]
 *     else if T2 is other then
 *       (allow S4 S3 P4)
 *     else [T2 is not self, notself, or other]
 *       S5 = S4 and not T2
 *       S6 = S4 and T2
 *       TC = S1 and not T2
 *       TD = S3 and not T2
 *       (allow S6 TC P4)
 *       (allow S5 TD P4)
 *       if cardinality(S5) > 1 then
 *         (allow S5 other P4)
 *   else [T1 is not self, notself, or other]
 *     S8 = S4 and T1
 *     (allow S8 self P4)
 *     if T2 is notself then
 *       [Nothing else is needed]
 *     else [T2 is other]
 *       T = T1 and not S2
 *       (allow S4 T P4)
 * else [Neither T1 nor T2 are notself or other]
 *   if T1 is self and T2 is not self then
 *     S5 = S4 and not T2
 *     (allow S5 self P4)
 *   else if T1 is not self and T2 is self then
 *     S7 = S4 and not T1
 *     S8 = S4 and T1
 *     T8 = T1 and not S4
 *     (allow S7 T1 P4)
 *     (allow S8 T8 P4)
 *     if cardinality(S8) > 1 then
 *       (allow S8 other P4)
 *   else [Neither T1 nor T2 is self]
 *     T3 = T1 and not T2
 *     (allow S4 T3 P4)
 */

static int cil_perm_match(const struct cil_perm *p1, const struct cil_list *pl2)
{
	struct cil_list_item *curr;

	cil_list_for_each(curr, pl2) {
		struct cil_perm *p = curr->data;
		if (p == p1) {
			return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

static int cil_class_perm_match(const struct cil_class *c1, const struct cil_perm *p1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	cil_list_for_each(curr, cpl2) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				if (cp->class == c1) {
					if (cil_perm_match(p1, cp->perms)) {
						return CIL_TRUE;
					}
				}
			} else { /* MAP */
				struct cil_list_item *p;
				cil_list_for_each(p, cp->perms) {
					struct cil_perm *cmp = p->data;
					if (cil_class_perm_match(c1, p1, cmp->classperms)) {
						return CIL_TRUE;
					}
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			if (cil_class_perm_match(c1, p1, cp->classperms)) {
				return CIL_TRUE;
			}
		}
	}
	return CIL_FALSE;
}

static int cil_classperms_match_any(const struct cil_classperms *cp1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	cil_list_for_each(curr, cp1->perms) {
		struct cil_perm *perm = curr->data;
		if (cil_class_perm_match(cp1->class, perm, cpl2)) {
			return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

int cil_classperms_list_match_any(const struct cil_list *cpl1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	if (!cpl1 || !cpl2) {
		return (!cpl1 && !cpl2) ? CIL_TRUE : CIL_FALSE;
	}

	cil_list_for_each(curr, cpl1) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				if (cil_classperms_match_any(cp, cpl2)) {
					return CIL_TRUE;
				}
			} else { /* MAP */
				struct cil_list_item *p;
				cil_list_for_each(p, cp->perms) {
					struct cil_perm *cmp = p->data;
					if (cil_classperms_list_match_any(cmp->classperms, cpl2)) {
						return CIL_TRUE;
					}
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			if (cil_classperms_list_match_any(cp->classperms, cpl2)) {
				return CIL_TRUE;
			}
		}
	}
	return CIL_FALSE;
}

static int cil_classperms_match_all(const struct cil_classperms *cp1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	cil_list_for_each(curr, cp1->perms) {
		struct cil_perm *perm = curr->data;
		if (!cil_class_perm_match(cp1->class, perm, cpl2)) {
			return CIL_FALSE;
		}
	}
	return CIL_TRUE;
}

int cil_classperms_list_match_all(const struct cil_list *cpl1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	if (!cpl1 || !cpl2) {
		return (!cpl1 && !cpl2) ? CIL_TRUE : CIL_FALSE;
	}

	cil_list_for_each(curr, cpl1) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				if (!cil_classperms_match_all(cp, cpl2)) {
					return CIL_FALSE;
				}
			} else { /* MAP */
				struct cil_list_item *p;
				cil_list_for_each(p, cp->perms) {
					struct cil_perm *cmp = p->data;
					if (!cil_classperms_list_match_all(cmp->classperms, cpl2)) {
						return CIL_FALSE;
					}
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			if (!cil_classperms_list_match_all(cp->classperms, cpl2)) {
				return CIL_FALSE;
			}
		}
	}
	return CIL_TRUE;
}

static void cil_classperms_copy(struct cil_classperms **new, const struct cil_classperms *old)
{
	cil_classperms_init(new);
	(*new)->class_str = old->class_str;
	(*new)->class = old->class;
	cil_copy_list(old->perm_strs, &(*new)->perm_strs);
	cil_copy_list(old->perms, &(*new)->perms);
}

static void cil_classperms_set_copy(struct cil_classperms_set **new, const struct cil_classperms_set *old)
{
	cil_classperms_set_init(new);
	(*new)->set_str = old->set_str;
	(*new)->set = old->set;
}

void cil_classperms_list_copy(struct cil_list **new, const struct cil_list *old)
{
	struct cil_list_item *curr;

	if (!new) {
		return;
	}

	if (!old) {
		*new = NULL;
		return;
	}

	cil_list_init(new, CIL_LIST);

	cil_list_for_each(curr, old) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *new_cp;
			cil_classperms_copy(&new_cp, curr->data);
			cil_list_append(*new, CIL_CLASSPERMS, new_cp);
		} else { /* SET */
			struct cil_classperms_set *new_cps;
			cil_classperms_set_copy(&new_cps, curr->data);
			cil_list_append(*new, CIL_CLASSPERMS_SET, new_cps);
		}
	}

	if (cil_list_is_empty(*new)) {
		cil_list_destroy(new, CIL_FALSE);
	}
}

/* Append cp1 and cpl2 to result */
static void cil_classperms_and(struct cil_list **result, const struct cil_classperms *cp1, const struct cil_list *cpl2)
{
	struct cil_classperms *new_cp = NULL;
	struct cil_list_item *curr;

	if (cil_classperms_match_all(cp1, cpl2)) {
		cil_classperms_copy(&new_cp, cp1);
		cil_list_append(*result, CIL_CLASSPERMS, new_cp);
		return;
	}

	cil_list_for_each(curr, cp1->perms) {
		struct cil_perm *perm = curr->data;
		if (cil_class_perm_match(cp1->class, perm, cpl2)) {
			if (new_cp == NULL) {
				cil_classperms_init(&new_cp);
				new_cp->class_str = cp1->class_str;
				new_cp->class = cp1->class;
				cil_list_init(&new_cp->perm_strs, CIL_PERM);
				cil_list_init(&new_cp->perms, CIL_PERM);
				cil_list_append(*result, CIL_CLASSPERMS, new_cp);
			}
			cil_list_append(new_cp->perm_strs, CIL_STRING, perm->datum.fqn);
			cil_list_append(new_cp->perms, CIL_DATUM, perm);
		}
	}
}

/* Append cp1 and cpl2 to result */
static void cil_classperms_map_and(struct cil_list **result, const struct cil_classperms *cp1, const struct cil_list *cpl2)
{
	struct cil_classperms *new_cp = NULL;
	struct cil_list_item *p;

	cil_list_for_each(p, cp1->perms) {
		struct cil_perm *map_perm = p->data;
		if (cil_classperms_list_match_all(map_perm->classperms, cpl2)) {
			if (new_cp == NULL) {
				cil_classperms_init(&new_cp);
				new_cp->class_str = cp1->class_str;
				new_cp->class = cp1->class;
				cil_list_init(&new_cp->perm_strs, CIL_PERM);
				cil_list_init(&new_cp->perms, CIL_PERM);
				cil_list_append(*result, CIL_CLASSPERMS, new_cp);
			}
			cil_list_append(new_cp->perm_strs, CIL_STRING, map_perm->datum.fqn);
			cil_list_append(new_cp->perms, CIL_DATUM, map_perm);
		} else {
			struct cil_list *new_cpl = NULL;
			cil_classperms_list_and(&new_cpl, map_perm->classperms, cpl2);
			if (new_cpl) {
				struct cil_list_item *i;
				cil_list_for_each(i, new_cpl) {
					cil_list_append(*result, i->flavor, i->data);
				}
				cil_list_destroy(&new_cpl, CIL_FALSE);
			}
		}
	}
}

/* Append cps1 and cpl2 to result */
static void cil_classperms_set_and(struct cil_list **result, const struct cil_classperms_set *cps1, const struct cil_list *cpl2)
{
	struct cil_classpermission *cp = cps1->set;

	if (cil_classperms_list_match_all(cp->classperms, cpl2)) {
		struct cil_classperms_set *new_cps;
		cil_classperms_set_copy(&new_cps, cps1);
		cil_list_append(*result, CIL_CLASSPERMS_SET, new_cps);
	} else {
		struct cil_list *new_cpl;
		cil_classperms_list_and(&new_cpl, cp->classperms, cpl2);
		if (new_cpl) {
			struct cil_list_item *i;
			cil_list_for_each(i, new_cpl) {
				cil_list_append(*result, i->flavor, i->data);
			}
			cil_list_destroy(&new_cpl, CIL_FALSE);
		}
	}
}

/* result = cpl1 and cpl2 */
void cil_classperms_list_and(struct cil_list **result, const struct cil_list *cpl1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	if (!result) {
		return;
	}

	if (!cpl1 || !cpl2) {
		*result = NULL;
		return;
	}

	if (cil_classperms_list_match_all(cpl1, cpl2)) {
		cil_classperms_list_copy(result, cpl1);
		return;
	}

	cil_list_init(result, CIL_LIST);

	cil_list_for_each(curr, cpl1) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				cil_classperms_and(result, cp, cpl2);
			} else { /* MAP */
				cil_classperms_map_and(result, cp, cpl2);
			}
		} else { /* SET */
			struct cil_classperms_set *cps = curr->data;
			cil_classperms_set_and(result, cps, cpl2);
		}
	}

	if (cil_list_is_empty(*result)) {
		cil_list_destroy(result, CIL_FALSE);
	}
}

/* Append cp1 and not cpl2 to result */
static void cil_classperms_andnot(struct cil_list **result, const struct cil_classperms *cp1, const struct cil_list *cpl2)
{
	struct cil_classperms *new_cp = NULL;
	struct cil_list_item *curr;

	if (!cil_classperms_match_any(cp1, cpl2)) {
		cil_classperms_copy(&new_cp, cp1);
		cil_list_append(*result, CIL_CLASSPERMS, new_cp);
		return;
	}

	cil_list_for_each(curr, cp1->perms) {
		struct cil_perm *perm = curr->data;
		if (!cil_class_perm_match(cp1->class, perm, cpl2)) {
			if (new_cp == NULL) {
				cil_classperms_init(&new_cp);
				new_cp->class_str = cp1->class_str;
				new_cp->class = cp1->class;
				cil_list_init(&new_cp->perm_strs, CIL_PERM);
				cil_list_init(&new_cp->perms, CIL_PERM);
				cil_list_append(*result, CIL_CLASSPERMS, new_cp);
			}
			cil_list_append(new_cp->perm_strs, CIL_STRING, perm->datum.fqn);
			cil_list_append(new_cp->perms, CIL_DATUM, perm);
		}
	}
}

/* Append cp1 and not cpl2 to result */
static void cil_classperms_map_andnot(struct cil_list **result, const struct cil_classperms *cp1, const struct cil_list *cpl2)
{
	struct cil_classperms *new_cp = NULL;
	struct cil_list_item *p;

	cil_list_for_each(p, cp1->perms) {
		struct cil_perm *map_perm = p->data;
		if (!cil_classperms_list_match_any(map_perm->classperms, cpl2)) {
			if (new_cp == NULL) {
				cil_classperms_init(&new_cp);
				new_cp->class_str = cp1->class_str;
				new_cp->class = cp1->class;
				cil_list_init(&new_cp->perm_strs, CIL_PERM);
				cil_list_init(&new_cp->perms, CIL_PERM);
				cil_list_append(*result, CIL_CLASSPERMS, new_cp);
			}
			cil_list_append(new_cp->perm_strs, CIL_STRING, map_perm->datum.fqn);
			cil_list_append(new_cp->perms, CIL_DATUM, map_perm);
		} else {
			struct cil_list *new_cpl = NULL;
			cil_classperms_list_andnot(&new_cpl, map_perm->classperms, cpl2);
			if (new_cpl) {
				struct cil_list_item *i;
				cil_list_for_each(i, new_cpl) {
					cil_list_append(*result, i->flavor, i->data);
				}
				cil_list_destroy(&new_cpl, CIL_FALSE);
			}
		}
	}
}

/* Append cps1 and not cpl2 to result */
static void cil_classperms_set_andnot(struct cil_list **result, const struct cil_classperms_set *cps1, const struct cil_list *cpl2)
{
	struct cil_classpermission *cp = cps1->set;

	if (!cil_classperms_list_match_any(cp->classperms, cpl2)) {
		struct cil_classperms_set *new_cps;
		cil_classperms_set_copy(&new_cps, cps1);
		cil_list_append(*result, CIL_CLASSPERMS_SET, new_cps);
	} else {
		struct cil_list *new_cpl;
		cil_classperms_list_andnot(&new_cpl, cp->classperms, cpl2);
		if (new_cpl) {
			struct cil_list_item *i;
			cil_list_for_each(i, new_cpl) {
				cil_list_append(*result, i->flavor, i->data);
			}
			cil_list_destroy(&new_cpl, CIL_FALSE);
		}
	}
}

/* result = cpl1 and not cpl2 */
void cil_classperms_list_andnot(struct cil_list **result, const struct cil_list *cpl1, const struct cil_list *cpl2)
{
	struct cil_list_item *curr;

	if (!result) {
		return;
	}

	if (!cpl1) {
		*result = NULL;
		return;
	}

	if (!cpl2 || !cil_classperms_list_match_any(cpl1, cpl2)) {
		cil_classperms_list_copy(result, cpl1);
		return;
	}

	cil_list_init(result, CIL_LIST);

	cil_list_for_each(curr, cpl1) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				cil_classperms_andnot(result, cp, cpl2);
			} else { /* MAP */
				cil_classperms_map_andnot(result, cp, cpl2);
			}
		} else { /* SET */
			struct cil_classperms_set *cps = curr->data;
			cil_classperms_set_andnot(result, cps, cpl2);
		}
	}

	if (cil_list_is_empty(*result)) {
		cil_list_destroy(result, CIL_FALSE);
	}
}

static int cil_datum_cardinality(const struct cil_symtab_datum *d)
{
	if (!d) {
		return 0;
	}
	if (FLAVOR(d) != CIL_TYPEATTRIBUTE) {
		return 1;
	} else {
		struct cil_typeattribute *a = (struct cil_typeattribute *)d;
		return ebitmap_cardinality(a->types);
	}
}

/* result = ALL and not d2 */
static int cil_datum_not(ebitmap_t *result, const struct cil_symtab_datum *d, int max)
{
	int rc = SEPOL_OK;

	if (FLAVOR(d) != CIL_TYPEATTRIBUTE) {
		struct cil_type *t = (struct cil_type *)d;
		ebitmap_t e;

		ebitmap_init(&e);
		rc = ebitmap_set_bit(&e, t->value, 1);
		if (rc != SEPOL_OK) {
			ebitmap_destroy(&e);
			goto exit;
		}

		ebitmap_init(result);
		rc = ebitmap_not(result, &e, max);
		if (rc != SEPOL_OK) {
			ebitmap_destroy(&e);
			ebitmap_destroy(result);
			goto exit;
		}
		ebitmap_destroy(&e);
	} else {
		struct cil_typeattribute *a = (struct cil_typeattribute *)d;

		ebitmap_init(result);
		rc = ebitmap_not(result, a->types, max);
		if (rc != SEPOL_OK) {
			ebitmap_destroy(result);
			goto exit;
		}
	}
exit:
	return rc;
}

/* result = d1 and d2 */
static int cil_datums_and(ebitmap_t *result, const struct cil_symtab_datum *d1, const struct cil_symtab_datum *d2)
{
	int rc = SEPOL_OK;
	enum cil_flavor f1 = FLAVOR(d1);
	enum cil_flavor f2 = FLAVOR(d2);

	if (f1 != CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
		struct cil_type *t1 = (struct cil_type *)d1;
		struct cil_type *t2 = (struct cil_type *)d2;
		ebitmap_init(result);
		if (t1->value == t2->value) {
			rc = ebitmap_set_bit(result, t1->value, 1);
			if (rc != SEPOL_OK) {
				ebitmap_destroy(result);
				goto exit;
			}
		}
	} else if (f1 == CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *a1 = (struct cil_typeattribute *)d1;
		struct cil_type *t2 = (struct cil_type *)d2;
		ebitmap_init(result);
		if (ebitmap_get_bit(a1->types, t2->value)) {
			rc = ebitmap_set_bit(result, t2->value, 1);
			if (rc != SEPOL_OK) {
				ebitmap_destroy(result);
				goto exit;
			}
		}
	} else if (f1 != CIL_TYPEATTRIBUTE && f2 == CIL_TYPEATTRIBUTE) {
		struct cil_type *t1 = (struct cil_type *)d1;
		struct cil_typeattribute *a2 = (struct cil_typeattribute *)d2;
		ebitmap_init(result);
		if (ebitmap_get_bit(a2->types, t1->value)) {
			rc = ebitmap_set_bit(result, t1->value, 1);
			if (rc != SEPOL_OK) {
				ebitmap_destroy(result);
				goto exit;
			}
		}
	} else {
		/* Both are attributes */
		struct cil_typeattribute *a1 = (struct cil_typeattribute *)d1;
		struct cil_typeattribute *a2 = (struct cil_typeattribute *)d2;
		rc = ebitmap_and(result, a1->types, a2->types);
		if (rc != SEPOL_OK) {
			ebitmap_destroy(result);
			goto exit;
		}
	}
exit:
	return rc;
}

/* result = d1 and not d2 */
static int cil_datums_andnot(ebitmap_t *result, const struct cil_symtab_datum *d1, const struct cil_symtab_datum *d2)
{
	int rc = SEPOL_OK;
	enum cil_flavor f1 = FLAVOR(d1);
	enum cil_flavor f2 = FLAVOR(d2);

	if (f1 != CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
		struct cil_type *t1 = (struct cil_type *)d1;
		struct cil_type *t2 = (struct cil_type *)d2;
		ebitmap_init(result);
		if (t1->value != t2->value) {
			rc = ebitmap_set_bit(result, t1->value, 1);
			if (rc != SEPOL_OK) {
				ebitmap_destroy(result);
				goto exit;
			}
		}
	} else if (f1 == CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *a1 = (struct cil_typeattribute *)d1;
		struct cil_type *t2 = (struct cil_type *)d2;
		rc = ebitmap_cpy(result, a1->types);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		rc = ebitmap_set_bit(result, t2->value, 0);
		if (rc != SEPOL_OK) {
			ebitmap_destroy(result);
			goto exit;
		}
	} else if (f1 != CIL_TYPEATTRIBUTE && f2 == CIL_TYPEATTRIBUTE) {
		struct cil_type *t1 = (struct cil_type *)d1;
		struct cil_typeattribute *a2 = (struct cil_typeattribute *)d2;
		ebitmap_init(result);
		if (!ebitmap_get_bit(a2->types, t1->value)) {
			rc = ebitmap_set_bit(result, t1->value, 1);
			if (rc != SEPOL_OK) {
				ebitmap_destroy(result);
				goto exit;
			}
		}
	} else {
		/* Both are attributes */
		struct cil_typeattribute *a1 = (struct cil_typeattribute *)d1;
		struct cil_typeattribute *a2 = (struct cil_typeattribute *)d2;
		rc = ebitmap_andnot(result, a1->types, a2->types, a1->types->highbit);
		if (rc != SEPOL_OK) {
			ebitmap_destroy(result);
			goto exit;
		}
	}
exit:
	return rc;
}

static size_t num_digits(unsigned n)
{
	size_t num = 1;
	while (n >= 10) {
		n /= 10;
		num++;
	}
	return num;
}

static char *cil_create_new_attribute_name(unsigned num)
{
	char *s1 = NULL;
	char *s2 = NULL;
	size_t len_num = num_digits(num);
	size_t len = strlen(CIL_DENY_ATTR_PREFIX) + 1 + len_num + 1;
	int rc;

	if (len >= CIL_MAX_NAME_LENGTH) {
		cil_log(CIL_ERR, "Name length greater than max name length of %d",
				CIL_MAX_NAME_LENGTH);
		goto exit;
	}

	s1 = cil_malloc(len);
	rc = snprintf(s1, len, "%s_%u", CIL_DENY_ATTR_PREFIX, num);
	if (rc < 0 || (size_t)rc >= len) {
		cil_log(CIL_ERR, "Error creating new attribute name");
		free(s1);
		goto exit;
	}

	s2 = cil_strpool_add(s1);
	free(s1);

exit:
	return s2;
}

static struct cil_list *cil_create_and_expr_list(enum cil_flavor f1, void *v1, enum cil_flavor f2, void *v2)
{
	struct cil_list *expr;

	cil_list_init(&expr, CIL_TYPE);
	cil_list_append(expr, CIL_OP, (void *)CIL_AND);
	cil_list_append(expr, f1, v1);
	cil_list_append(expr, f2, v2);

	return expr;
}

static struct cil_list *cil_create_andnot_expr_list(enum cil_flavor f1, void *v1, enum cil_flavor f2, void *v2)
{
	struct cil_list *expr, *sub_expr;

	cil_list_init(&expr, CIL_TYPE);
	cil_list_append(expr, CIL_OP, (void *)CIL_AND);
	cil_list_append(expr, f1, v1);
	cil_list_init(&sub_expr, CIL_TYPE);
	cil_list_append(sub_expr, CIL_OP, (void *)CIL_NOT);
	cil_list_append(sub_expr, f2, v2);
	cil_list_append(expr, CIL_LIST, sub_expr);

	return expr;
}

static struct cil_tree_node *cil_create_and_insert_node(struct cil_tree_node *prev, enum cil_flavor flavor, void *data)
{
	struct cil_tree_node *new;

	cil_tree_node_init(&new);
	new->parent = prev->parent;
	new->line = prev->line;
	new->hll_offset = prev->hll_offset;
	new->flavor = flavor;
	new->data = data;
	new->next = prev->next;
	prev->next = new;

	return new;
}

static int cil_create_and_insert_attribute_and_set(struct cil_db *db, struct cil_tree_node *prev, struct cil_list *str_expr, struct cil_list *datum_expr, ebitmap_t *types, struct cil_symtab_datum **d)
{
	struct cil_tree_node *attr_node = NULL;
	char *name;
	struct cil_typeattribute *attr = NULL;
	struct cil_tree_node *attrset_node = NULL;
	struct cil_typeattributeset *attrset = NULL;
	symtab_t *symtab = NULL;
	int rc = SEPOL_ERR;

	name = cil_create_new_attribute_name(db->num_types_and_attrs);
	if (!name) {
		goto exit;
	}

	cil_typeattributeset_init(&attrset);
	attrset->attr_str = name;
	attrset->str_expr = str_expr;
	attrset->datum_expr = datum_expr;

	cil_typeattribute_init(&attr);
	cil_list_init(&attr->expr_list, CIL_TYPE);
	cil_list_append(attr->expr_list, CIL_LIST, datum_expr);
	attr->types = types;
	attr->used = CIL_ATTR_AVRULE;
	attr->keep = (ebitmap_cardinality(types) < db->attrs_expand_size) ? CIL_FALSE : CIL_TRUE;

	attr_node = cil_create_and_insert_node(prev, CIL_TYPEATTRIBUTE, attr);
	attrset_node = cil_create_and_insert_node(attr_node, CIL_TYPEATTRIBUTESET, attrset);

	rc = cil_get_symtab(prev->parent, &symtab, CIL_SYM_TYPES);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	rc = cil_symtab_insert(symtab, name, &attr->datum, attr_node);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	db->num_types_and_attrs++;

	*d = &attr->datum;

	return SEPOL_OK;

exit:
	if (attr_node) {
		cil_destroy_typeattribute(attr_node->data); // This will not destroy datum_expr
		free(attr_node);
	}
	if (attrset_node) {
		prev->next = attrset_node->next;
		free(attrset_node);
	}
	return rc;
}

struct attr_symtab_map_data {
	struct cil_symtab_datum *d;
	ebitmap_t *types;
};

static int cil_check_attribute_in_symtab(__attribute__((unused))hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct attr_symtab_map_data *data = args;

	if (FLAVOR(d) == CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *attr = (struct cil_typeattribute *)d;
		if (ebitmap_cmp(data->types, attr->types)) {
			data->d = d;
		}
	}
	return SEPOL_OK;
}

static struct cil_symtab_datum *cil_check_for_previously_defined_attribute(struct cil_db *db, ebitmap_t *types, struct cil_symtab_datum *d)
{
	symtab_t *local_symtab, *root_symtab;
	struct attr_symtab_map_data data;
	int rc;

	data.d = NULL;
	data.types = types;

	local_symtab = d->symtab;
	root_symtab = &((struct cil_root *)db->ast->root->data)->symtab[CIL_SYM_TYPES];

	if (local_symtab != root_symtab) {
		rc = cil_symtab_map(local_symtab, cil_check_attribute_in_symtab, &data);
		if (rc != SEPOL_OK) {
			return NULL;
		}
	}

	if (!data.d) {
		rc = cil_symtab_map(root_symtab, cil_check_attribute_in_symtab, &data);
		if (rc != SEPOL_OK) {
			return NULL;
		}
	}

	return data.d;
}

static int cil_create_attribute_all_and_not_d(struct cil_db *db, struct cil_symtab_datum *d, struct cil_symtab_datum **d3)
{
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
	ebitmap_t *types;
	int rc;

	*d3 = NULL;

	if (!d) {
		return SEPOL_ERR;
	}

	str_expr = cil_create_andnot_expr_list(CIL_OP, (void *)CIL_ALL, CIL_STRING, d->fqn);
	datum_expr = cil_create_andnot_expr_list(CIL_OP, (void *)CIL_ALL, CIL_DATUM, d);

	types = cil_malloc(sizeof(*types));
	rc = cil_datum_not(types, d, db->num_types);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	if (ebitmap_is_empty(types)) {
		rc = SEPOL_OK;
		goto exit;
	}

	if (ebitmap_cardinality(types) == 1) {
		unsigned i = ebitmap_highest_set_bit(types);
		*d3 = DATUM(db->val_to_type[i]);
		ebitmap_destroy(types);
		rc = SEPOL_OK;
		goto exit;
	}

	*d3 = cil_check_for_previously_defined_attribute(db, types, d);
	if (*d3) {
		ebitmap_destroy(types);
		rc = SEPOL_OK;
		goto exit;
	}

	rc = cil_create_and_insert_attribute_and_set(db, NODE(d), str_expr, datum_expr, types, d3);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_list_destroy(&str_expr, CIL_FALSE);
	cil_list_destroy(&datum_expr, CIL_FALSE);
	free(types);
	return rc;
}

static int cil_create_attribute_d1_and_not_d2(struct cil_db *db, struct cil_symtab_datum *d1, struct cil_symtab_datum *d2, struct cil_symtab_datum **d3)
{
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
	ebitmap_t *types;
	int rc;

	if (!d2) {
		*d3 = d1;
		return SEPOL_OK;
	}

	*d3 = NULL;

	if (!d1 || d1 == d2) {
		return SEPOL_OK;
	}

	str_expr = cil_create_andnot_expr_list(CIL_STRING, d1->fqn, CIL_STRING, d2->fqn);
	datum_expr = cil_create_andnot_expr_list(CIL_DATUM, d1, CIL_DATUM, d2);

	types = cil_malloc(sizeof(*types));
	rc = cil_datums_andnot(types, d1, d2);
	if (rc != SEPOL_OK) {
		goto exit;
	}
	if (ebitmap_is_empty(types)) {
		rc = SEPOL_OK;
		goto exit;
	}

	if (ebitmap_cardinality(types) == 1) {
		unsigned i = ebitmap_highest_set_bit(types);
		*d3 = DATUM(db->val_to_type[i]);
		ebitmap_destroy(types);
		rc = SEPOL_OK;
		goto exit;
	}

	*d3 = cil_check_for_previously_defined_attribute(db, types, d1);
	if (*d3) {
		ebitmap_destroy(types);
		rc = SEPOL_OK;
		goto exit;
	}

	rc = cil_create_and_insert_attribute_and_set(db, NODE(d1), str_expr, datum_expr, types, d3);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_list_destroy(&str_expr, CIL_FALSE);
	cil_list_destroy(&datum_expr, CIL_FALSE);
	free(types);
	return rc;
}

static int cil_create_attribute_d1_and_d2(struct cil_db *db, struct cil_symtab_datum *d1, struct cil_symtab_datum *d2, struct cil_symtab_datum **d3)
{
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
	ebitmap_t *types;
	int rc;

	if (d1 == d2) {
		*d3 = d1;
		return SEPOL_OK;
	}

	*d3 = NULL;

	if (!d1 || !d2) {
		return SEPOL_OK;
	}

	str_expr = cil_create_and_expr_list(CIL_STRING, d1->fqn, CIL_STRING, d2->fqn);
	datum_expr = cil_create_and_expr_list(CIL_DATUM, d1, CIL_DATUM, d2);

	types = cil_malloc(sizeof(*types));
	rc = cil_datums_and(types, d1, d2);
	if (rc != SEPOL_OK) {
		goto exit;
	}
	if (ebitmap_is_empty(types)) {
		rc = SEPOL_OK;
		goto exit;
	}

	if (ebitmap_cardinality(types) == 1) {
		unsigned i = ebitmap_highest_set_bit(types);
		*d3 = DATUM(db->val_to_type[i]);
		ebitmap_destroy(types);
		rc = SEPOL_OK;
		goto exit;
	}

	*d3 = cil_check_for_previously_defined_attribute(db, types, d1);
	if (*d3) {
		ebitmap_destroy(types);
		rc = SEPOL_OK;
		goto exit;
	}

	rc = cil_create_and_insert_attribute_and_set(db, NODE(d1), str_expr, datum_expr, types, d3);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_list_destroy(&str_expr, CIL_FALSE);
	cil_list_destroy(&datum_expr, CIL_FALSE);
	free(types);
	return rc;
}

static struct cil_avrule *cil_create_avrule(struct cil_symtab_datum *src, struct cil_symtab_datum *tgt, struct cil_list *classperms)
{
	struct cil_avrule *new;

	cil_avrule_init(&new);
	new->is_extended = CIL_FALSE;
	new->rule_kind = CIL_AVRULE_ALLOWED;
	new->src_str = src->name;
	new->src = src;
	new->tgt_str = tgt->name;
	new->tgt = tgt;
	new->perms.classperms = classperms;

	return new;
}

static struct cil_tree_node *cil_create_and_add_avrule(struct cil_tree_node *curr, struct cil_symtab_datum *src, struct cil_symtab_datum *tgt, struct cil_list *classperms)
{
	struct cil_avrule *new_avrule;
	struct cil_list *new_cp_list;

	if (!src || !tgt) {
		return curr;
	}

	cil_classperms_list_copy(&new_cp_list, classperms);
	new_avrule = cil_create_avrule(src, tgt, new_cp_list);
	return cil_create_and_insert_node(curr, CIL_AVRULE, new_avrule);
}

static int cil_remove_permissions_from_special_rule(struct cil_db *db, struct cil_tree_node *curr, struct cil_symtab_datum *s1, struct cil_symtab_datum *t1, struct cil_symtab_datum *s2, struct cil_symtab_datum *t2, struct cil_list *p4, struct cil_symtab_datum *s3, struct cil_symtab_datum *s4)
{
	int rc;

	if (t1 == DATUM(db->notselftype)) {
		if (t2 == DATUM(db->othertype)) {
			struct cil_symtab_datum *t;
			rc = cil_create_attribute_all_and_not_d(db, s2, &t);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			curr = cil_create_and_add_avrule(curr, s4, t, p4);
		} else {
			struct cil_symtab_datum *s5, *s6, *ta, *tb;
			rc = cil_create_attribute_d1_and_not_d2(db, s4, t2, &s5);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			rc = cil_create_attribute_d1_and_d2(db, s4, t2, &s6);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			rc = cil_create_attribute_all_and_not_d(db, t2, &ta);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			rc = cil_create_attribute_d1_and_not_d2(db, ta, s4, &tb);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			curr = cil_create_and_add_avrule(curr, s6, ta, p4);
			curr = cil_create_and_add_avrule(curr, s5, tb, p4);
			if (cil_datum_cardinality(s5) > 1) {
				curr = cil_create_and_add_avrule(curr, s5, DATUM(db->othertype), p4);
			}
		}
	} else if (t1 == DATUM(db->othertype)) {
		curr = cil_create_and_add_avrule(curr, s3, s4, p4);
		if (t2 == DATUM(db->notselftype)) {
			/* Nothing else is needed */
		} else if (t2 == DATUM(db->othertype)) {
			curr = cil_create_and_add_avrule(curr, s4, s3, p4);
		} else {
			struct cil_symtab_datum *s5, *s6, *tc, *td;
			rc = cil_create_attribute_d1_and_not_d2(db, s4, t2, &s5);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			rc = cil_create_attribute_d1_and_d2(db, s4, t2, &s6);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			rc = cil_create_attribute_d1_and_not_d2(db, s1, t2, &tc);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			rc = cil_create_attribute_d1_and_not_d2(db, s3, t2, &td);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			curr = cil_create_and_add_avrule(curr, s6, tc, p4);
			curr = cil_create_and_add_avrule(curr, s5, td, p4);
			if (cil_datum_cardinality(s5) > 1) {
				curr = cil_create_and_add_avrule(curr, s5, DATUM(db->othertype), p4);
			}
		}
	} else {
		struct cil_symtab_datum *s8;
		rc = cil_create_attribute_d1_and_d2(db, s4, t1, &s8);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		curr = cil_create_and_add_avrule(curr, s8, DATUM(db->selftype), p4);
		if (t2 == DATUM(db->notselftype)) {
			/* Nothing else is needed */
		} else { /* t2 == DATUM(db->othertype) */
			struct cil_symtab_datum *t;
			rc = cil_create_attribute_d1_and_not_d2(db, t1, s2, &t);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			curr = cil_create_and_add_avrule(curr, s4, t, p4);
		}
	}
	return SEPOL_OK;

exit:
	return rc;
}

static int cil_remove_permissions_from_rule(struct cil_db *db, struct cil_tree_node *allow_node, const struct cil_tree_node *deny_node)
{
	struct cil_avrule *allow_rule = allow_node->data;
	struct cil_deny_rule *deny_rule = deny_node->data;
	struct cil_symtab_datum *s1 = allow_rule->src;
	struct cil_symtab_datum *t1 = allow_rule->tgt;
	struct cil_list *p1 = allow_rule->perms.classperms;
	struct cil_symtab_datum *s2 = deny_rule->src;
	struct cil_symtab_datum *t2 = deny_rule->tgt;
	struct cil_list *p2 = deny_rule->classperms;
	struct cil_list *p3 = NULL;
	struct cil_list *p4 = NULL;
	struct cil_symtab_datum *s3, *s4;
	struct cil_tree_node *curr = allow_node;
	int rc;

	cil_classperms_list_andnot(&p3, p1, p2);
	if (!cil_list_is_empty(p3)) {;
		curr = cil_create_and_add_avrule(curr, s1, t1, p3);
	}
	cil_destroy_classperms_list(&p3);
	p3 = NULL;

	cil_classperms_list_and(&p4, p1, p2);
	if (cil_list_is_empty(p4)) {
		cil_tree_log(allow_node, CIL_ERR, "Allow rule did not match deny rule: No matching class and permissions");
		cil_tree_log((struct cil_tree_node *)deny_node, CIL_ERR, "Deny rule");
		rc = SEPOL_ERR;
		goto exit;
	}

	rc = cil_create_attribute_d1_and_not_d2(db, s1, s2, &s3);
	if (rc != SEPOL_OK) {
		goto exit;
	}
	curr = cil_create_and_add_avrule(curr, s3, t1, p4);

	if ((t1 == DATUM(db->selftype) && t2 == DATUM(db->selftype)) ||
		(t1 == DATUM(db->notselftype) && t2 == DATUM(db->notselftype))) {
		/* Nothing more needs to be done */
		rc = SEPOL_OK;
		goto exit;
	}

	rc = cil_create_attribute_d1_and_d2(db, s1, s2, &s4);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	if (t1 == DATUM(db->notselftype) || t1 == DATUM(db->othertype) ||
		t2 == DATUM(db->notselftype) || t2 == DATUM(db->othertype)) {
		rc = cil_remove_permissions_from_special_rule(db, curr, s1, t1, s2, t2, p4, s3, s4);
		goto exit;
	}

	if (t1 == DATUM(db->selftype) && t2 != DATUM(db->selftype)) {
		struct cil_symtab_datum *s5;
		rc = cil_create_attribute_d1_and_not_d2(db, s4, t2, &s5);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		curr = cil_create_and_add_avrule(curr, s5, DATUM(db->selftype), p4);
	} else if (t1 != DATUM(db->selftype) && t2 == DATUM(db->selftype)) {
		struct cil_symtab_datum *s7, *s8, *t8;
		rc = cil_create_attribute_d1_and_not_d2(db, s4, t1, &s7);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		rc = cil_create_attribute_d1_and_d2(db, s4, t1, &s8);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		rc = cil_create_attribute_d1_and_not_d2(db, t1, s4, &t8);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		curr = cil_create_and_add_avrule(curr, s7, t1, p4);
		curr = cil_create_and_add_avrule(curr, s8, t8, p4);
		if (cil_datum_cardinality(s8) > 1) {
			curr = cil_create_and_add_avrule(curr, s8, DATUM(db->othertype), p4);
		}
	} else {
		struct cil_symtab_datum *t3;
		rc = cil_create_attribute_d1_and_not_d2(db, t1, t2, &t3);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		curr = cil_create_and_add_avrule(curr, s4, t3, p4);
	}

exit:
	if (p4) {
		cil_destroy_classperms_list(&p4);
	}
	return rc;
}

static int cil_find_matching_allow_rules(struct cil_list *matching, struct cil_tree_node *start, struct cil_tree_node *deny_node)
{
	struct cil_deny_rule *deny_rule = deny_node->data;
	struct cil_avrule target;

	target.rule_kind = CIL_AVRULE_ALLOWED;
	target.is_extended = CIL_FALSE;
	target.src = deny_rule->src;
	target.tgt = deny_rule->tgt;
	target.perms.classperms = deny_rule->classperms;

	return cil_find_matching_avrule_in_ast(start, CIL_AVRULE, &target, matching, CIL_FALSE);
}

static int cil_process_deny_rule(struct cil_db *db, struct cil_tree_node *start, struct cil_tree_node *deny_node)
{
	struct cil_list *matching;
	struct cil_list_item *item;
	int rc;

	cil_list_init(&matching, CIL_NODE);

	rc = cil_find_matching_allow_rules(matching, start, deny_node);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	cil_list_for_each(item, matching) {
		struct cil_tree_node *allow_node = item->data;
		rc = cil_remove_permissions_from_rule(db, allow_node, deny_node);
		cil_tree_node_remove(allow_node);
		if (rc != SEPOL_OK) {
			goto exit;
		}

	}

exit:
	cil_list_destroy(&matching, CIL_FALSE);
	return rc;
}

static int cil_process_deny_rules(struct cil_db *db, struct cil_tree_node *start, struct cil_list *deny_rules)
{
	struct cil_list_item *item;
	int rc = SEPOL_OK;

	cil_list_for_each(item, deny_rules) {
		struct cil_tree_node *deny_node = item->data;
		rc = cil_process_deny_rule(db, start, deny_node);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		cil_tree_node_remove(deny_node);
	}

exit:
	return rc;
}

static int __cil_find_deny_rules(struct cil_tree_node *node,  uint32_t *finished, void *extra_args)
{
	struct cil_list *deny_rules = extra_args;

	if (node->flavor == CIL_BLOCK) {
		struct cil_block *block = node->data;
		if (block->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
	} else if (node->flavor == CIL_MACRO) {
		*finished = CIL_TREE_SKIP_HEAD;
	} else if (node->flavor == CIL_DENY_RULE) {
		cil_list_append(deny_rules, CIL_DENY_RULE, node);
	}
	return SEPOL_OK;
}

int cil_process_deny_rules_in_ast(struct cil_db *db)
{
	struct cil_tree_node *start;
	struct cil_list *deny_rules;
	int rc = SEPOL_ERR;

	cil_list_init(&deny_rules, CIL_DENY_RULE);

	if (!db) {
		cil_log(CIL_ERR, "No CIL db provided to process deny rules\n");
		goto exit;
	}

	start = db->ast->root;
	rc = cil_tree_walk(start, __cil_find_deny_rules, NULL, NULL, deny_rules);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "An error occurred while getting deny rules\n");
		goto exit;
	}

	rc = cil_process_deny_rules(db, start, deny_rules);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "An error occurred while processing deny rules\n");
		goto exit;
	}

exit:
	cil_list_destroy(&deny_rules, CIL_FALSE);
	return rc;
}
