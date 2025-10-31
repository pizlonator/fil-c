/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#include <sepol/policydb/ebitmap.h>

#include "cil_internal.h"
#include "cil_find.h"
#include "cil_flavor.h"
#include "cil_list.h"
#include "cil_log.h"
#include "cil_symtab.h"

struct cil_args_find {
	enum cil_flavor flavor;
	void *target;
	struct cil_list *matching;
	int match_self;
};

static int cil_type_match_any(struct cil_symtab_datum *d1, struct cil_symtab_datum *d2)
{
	enum cil_flavor f1 = FLAVOR(d1);
	enum cil_flavor f2 = FLAVOR(d2);

	if (f1 != CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
		struct cil_type *t1 = (struct cil_type *)d1;
		struct cil_type *t2 = (struct cil_type *)d2;
		if (t1->value == t2->value) {
			return CIL_TRUE;
		}
	} else if (f1 == CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *a = (struct cil_typeattribute *)d1;
		struct cil_type *t = (struct cil_type *)d2;
		if (ebitmap_get_bit(a->types, t->value)) {
			return CIL_TRUE;
		}
	} else if (f1 != CIL_TYPEATTRIBUTE && f2 == CIL_TYPEATTRIBUTE) {
		struct cil_type *t = (struct cil_type *)d1;
		struct cil_typeattribute *a = (struct cil_typeattribute *)d2;
		if (ebitmap_get_bit(a->types, t->value)) {
			return CIL_TRUE;
		}
	} else {
		/* Both are attributes */
		struct cil_typeattribute *a1 = (struct cil_typeattribute *)d1;
		struct cil_typeattribute *a2 = (struct cil_typeattribute *)d2;
		if (d1 == d2) {
			return CIL_TRUE;
		} else if (ebitmap_match_any(a1->types, a2->types)) {
			return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

static int cil_type_matches(ebitmap_t *matches, struct cil_symtab_datum *d1, struct cil_symtab_datum *d2)
{
	int rc = SEPOL_OK;
	enum cil_flavor f1 = FLAVOR(d1);
	enum cil_flavor f2 = FLAVOR(d2);

	if (f1 == CIL_TYPEATTRIBUTE && f2 == CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *a1 = (struct cil_typeattribute *)d1;
		struct cil_typeattribute *a2 = (struct cil_typeattribute *)d2;
		rc = ebitmap_and(matches, a1->types, a2->types);
	} else {
		ebitmap_init(matches);
		if (f1 != CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
			struct cil_type *t1 = (struct cil_type *)d1;
			struct cil_type *t2 = (struct cil_type *)d2;
			if (t1->value == t2->value) {
				rc = ebitmap_set_bit(matches, t1->value, 1);
			}
		} else if (f1 == CIL_TYPEATTRIBUTE && f2 != CIL_TYPEATTRIBUTE) {
			struct cil_typeattribute *a = (struct cil_typeattribute *)d1;
			struct cil_type *t = (struct cil_type *)d2;
			if (ebitmap_get_bit(a->types, t->value)) {
				rc = ebitmap_set_bit(matches, t->value, 1);
			}
		} else { // f1 != CIL_TYPEATTRIBUTE && f2 == CIL_TYPEATTRIBUTE
			struct cil_type *t = (struct cil_type *)d1;
			struct cil_typeattribute *a = (struct cil_typeattribute *)d2;
			if (ebitmap_get_bit(a->types, t->value)) {
				rc = ebitmap_set_bit(matches, t->value, 1);
			}
		}
		if (rc != SEPOL_OK) {
			ebitmap_destroy(matches);
		}
	}

	return rc;
}

/* s1 is the src type that is matched with a self
 * s2, and t2 are the source and type of the other rule
 * Assumes there is a match between s1 and s2
 */
static int cil_self_match_any(struct cil_symtab_datum *s1, struct cil_symtab_datum *s2, struct cil_symtab_datum *t2)
{
	int rc;

	if (FLAVOR(s1) != CIL_TYPEATTRIBUTE) {
		rc = cil_type_match_any(s1, t2);
	} else {
		struct cil_typeattribute *a = (struct cil_typeattribute *)s1;
		ebitmap_t map;
		rc = cil_type_matches(&map, s2, t2);
		if (rc < 0) {
			return rc;
		}
		if (ebitmap_is_empty(&map)) {
			return CIL_FALSE;
		}
		rc = ebitmap_match_any(&map, a->types);
		ebitmap_destroy(&map);
	}

	return rc;
}

/* s1 is the src type that is matched with a notself
 * s2 and t2 are the source and type of the other rule
 * Assumes there is a match between s1 and s2
 */
static int cil_notself_match_any(struct cil_symtab_datum *s1, struct cil_symtab_datum *s2, struct cil_symtab_datum *t2)
{
	int rc;
	ebitmap_node_t *snode, *tnode;
	unsigned int s,t;

	if (FLAVOR(s1) != CIL_TYPEATTRIBUTE) {
		struct cil_type *ts1 = (struct cil_type *)s1;
		if (FLAVOR(t2) != CIL_TYPEATTRIBUTE) {
			struct cil_type *tt2 = (struct cil_type *)t2;
			if (ts1->value != tt2->value) {
				return CIL_TRUE;
			}
		} else {
			struct cil_typeattribute *at2 = (struct cil_typeattribute *)t2;
			ebitmap_for_each_positive_bit(at2->types, tnode, t) {
				if (t != (unsigned int)ts1->value) {
					return CIL_TRUE;
				}
			}
		}
	} else {
		ebitmap_t smap;
		rc = cil_type_matches(&smap, s1, s2);
		if (rc < 0) {
			return rc;
		}
		if (ebitmap_is_empty(&smap)) {
			return CIL_FALSE;
		}
		if (FLAVOR(t2) != CIL_TYPEATTRIBUTE) {
			struct cil_type *tt2 = (struct cil_type *)t2;
			ebitmap_for_each_positive_bit(&smap, snode, s) {
				if (s != (unsigned int)tt2->value) {
					ebitmap_destroy(&smap);
					return CIL_TRUE;
				}
			}
		} else {
			struct cil_typeattribute *at2 = (struct cil_typeattribute *)t2;
			ebitmap_for_each_positive_bit(&smap, snode, s) {
				ebitmap_for_each_positive_bit(at2->types, tnode, t) {
					if (s != t) {
						ebitmap_destroy(&smap);
						return CIL_TRUE;
					}
				}
			}
		}
		ebitmap_destroy(&smap);
	}

	return CIL_FALSE;
}

/* s1 is the src type that is matched with an other
 * s2, and t2 are the source and type of the other rule
 * Assumes there is a match between s1 and s2
 */
static int cil_other_match_any(struct cil_symtab_datum *s1, struct cil_symtab_datum *s2, struct cil_symtab_datum *t2)
{
	int rc;
	ebitmap_t smap, tmap;
	ebitmap_node_t *snode, *tnode;
	unsigned int s,t;

	if (FLAVOR(s1) != CIL_TYPEATTRIBUTE) {
		return CIL_FALSE;
	}

	rc = cil_type_matches(&smap, s1, s2);
	if (rc < 0) {
		return rc;
	}

	if (ebitmap_is_empty(&smap)) {
		return CIL_FALSE;
	}

	rc = cil_type_matches(&tmap, s1, t2);
	if (rc < 0) {
		ebitmap_destroy(&smap);
		return rc;
	}

	if (ebitmap_is_empty(&tmap)) {
		ebitmap_destroy(&smap);
		return CIL_FALSE;
	}

	ebitmap_for_each_positive_bit(&smap, snode, s) {
		ebitmap_for_each_positive_bit(&tmap, tnode, t) {
			if (s != t) {
				rc = CIL_TRUE;
				goto exit;
			}
		}
	}

	rc = CIL_FALSE;

exit:
	ebitmap_destroy(&smap);
	ebitmap_destroy(&tmap);
	return rc;
}

/* s2 is the src type that is matched with an other
 * Assumes there is a match between s1 and s2
 * s1 is not needed, since it is known that there is a match
 */
static int cil_notself_other_match_any(struct cil_symtab_datum *s2)
{
	if (FLAVOR(s2) == CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *as2 = (struct cil_typeattribute *)s2;
		if (ebitmap_cardinality(as2->types) > 1) {
			return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

static int cil_classperms_match_any(struct cil_classperms *cp1, struct cil_classperms *cp2)
{
	struct cil_class *c1 = cp1->class;
	struct cil_class *c2 = cp2->class;
	struct cil_list_item *i1, *i2;

	if (&c1->datum != &c2->datum) return CIL_FALSE;

	cil_list_for_each(i1, cp1->perms) {
		struct cil_perm *p1 = i1->data;
		cil_list_for_each(i2, cp2->perms) {
			struct cil_perm *p2 = i2->data;
			if (&p1->datum == &p2->datum) return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

static int __cil_classperms_list_match_any(struct cil_classperms *cp1, struct cil_list *cpl2)
{
	int rc;
	struct cil_list_item *curr;

	cil_list_for_each(curr, cpl2) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				rc = cil_classperms_match_any(cp1, cp);
				if (rc == CIL_TRUE) return CIL_TRUE;
			} else { /* MAP */
				struct cil_list_item *i = NULL;
				cil_list_for_each(i, cp->perms) {
					struct cil_perm *cmp = i->data;
					rc = __cil_classperms_list_match_any(cp1, cmp->classperms);
					if (rc == CIL_TRUE) return CIL_TRUE;
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			rc = __cil_classperms_list_match_any(cp1, cp->classperms);
			if (rc == CIL_TRUE) return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

static int cil_classperms_list_match_any(struct cil_list *cpl1, struct cil_list *cpl2)
{
	int rc;
	struct cil_list_item *curr;

	cil_list_for_each(curr, cpl1) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				rc = __cil_classperms_list_match_any(cp, cpl2);
				if (rc == CIL_TRUE) return CIL_TRUE;
			} else { /* MAP */
				struct cil_list_item *i = NULL;
				cil_list_for_each(i, cp->perms) {
					struct cil_perm *cmp = i->data;
					rc = cil_classperms_list_match_any(cmp->classperms, cpl2);
					if (rc == CIL_TRUE) return CIL_TRUE;
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			rc = cil_classperms_list_match_any(cp->classperms, cpl2);
			if (rc == CIL_TRUE) return CIL_TRUE;
		}
	}
	return CIL_FALSE;
}

static void __add_classes_from_classperms_list(struct cil_list *classperms, struct cil_list *class_list)
{
	struct cil_list_item *curr;

	cil_list_for_each(curr, classperms) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				cil_list_append(class_list, CIL_CLASS, cp->class);
			} else { /* MAP */
				struct cil_list_item *i = NULL;
				cil_list_for_each(i, cp->perms) {
					struct cil_perm *cmp = i->data;
					__add_classes_from_classperms_list(cmp->classperms, class_list);
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			__add_classes_from_classperms_list(cp->classperms, class_list);
		}
	}
}

static int __add_classes_from_map_perms(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct cil_list *class_list = args;
	struct cil_perm *cmp = (struct cil_perm *)d;

	__add_classes_from_classperms_list(cmp->classperms, class_list);

	return SEPOL_OK;
}

struct cil_list *cil_expand_class(struct cil_class *class)
{
	struct cil_list *class_list;

	cil_list_init(&class_list, CIL_CLASS);

	if (FLAVOR(class) == CIL_CLASS) {
		cil_list_append(class_list, CIL_CLASS, class);
	} else { /* MAP */
		cil_symtab_map(&class->perms, __add_classes_from_map_perms, class_list);
	}

	return class_list;
}

static int cil_permissionx_match_any(struct cil_permissionx *px1, struct cil_permissionx *px2)
{
	int rc = CIL_FALSE;
	struct cil_list *cl1 = NULL;
	struct cil_list *cl2 = NULL;

	if (px1->kind != px2->kind) goto exit;

	if (!ebitmap_match_any(px1->perms, px2->perms)) goto exit;

	cl1 = cil_expand_class(px1->obj);
	cl2 = cil_expand_class(px2->obj);

	if (!cil_list_match_any(cl1, cl2)) goto exit;

	rc = CIL_TRUE;

exit:
	cil_list_destroy(&cl1, CIL_FALSE);
	cil_list_destroy(&cl2, CIL_FALSE);

	return rc;
}

static int cil_find_matching_avrule(struct cil_tree_node *node, struct cil_avrule *avrule, struct cil_avrule *target, struct cil_list *matching, int match_self)
{
	int rc = SEPOL_OK;
	struct cil_symtab_datum *s1 = avrule->src;
	struct cil_symtab_datum *t1 = avrule->tgt;
	struct cil_symtab_datum *s2 = target->src;
	struct cil_symtab_datum *t2 = target->tgt;

	if (match_self != CIL_TRUE && avrule == target) goto exit;

	if (avrule->rule_kind != target->rule_kind) goto exit;

	if (avrule->is_extended != target->is_extended) goto exit;

	if (!cil_type_match_any(s1, s2)) goto exit;

	if (t1->fqn == CIL_KEY_SELF) {
		if (t2->fqn == CIL_KEY_SELF) {
			/* The earlier check whether s1 and s2 matches is all that is needed */
			rc = CIL_TRUE;
		} else if (t2->fqn == CIL_KEY_NOTSELF || t2->fqn == CIL_KEY_OTHER) {
			rc = CIL_FALSE;
		} else {
			rc = cil_self_match_any(s1, s2, t2);
		}
	} else if (t1->fqn == CIL_KEY_NOTSELF) {
		if (t2->fqn == CIL_KEY_SELF) {
			rc = CIL_FALSE;
		} else if (t2->fqn == CIL_KEY_NOTSELF) {
			/* The earlier check whether s1 and s2 matches is all that is needed */
			rc = CIL_TRUE;
		} else if (t2->fqn == CIL_KEY_OTHER) {
			rc = cil_notself_other_match_any(s2);
		} else {
			rc = cil_notself_match_any(s1, s2, t2);
		}
	} else if (t1->fqn == CIL_KEY_OTHER) {
		if (t2->fqn == CIL_KEY_SELF) {
			rc = CIL_FALSE;
		} else if (t2->fqn == CIL_KEY_NOTSELF) {
			rc = cil_notself_other_match_any(s1);
		} else if (t2->fqn == CIL_KEY_OTHER) {
			/* The earlier check whether s1 and s2 matches is all that is needed */
			rc = CIL_TRUE;
		} else {
			rc = cil_other_match_any(s1, s2, t2);
		}
	} else {
		if (t2->fqn == CIL_KEY_SELF) {
			rc = cil_self_match_any(s2, s1, t1);
		} else if (t2->fqn == CIL_KEY_NOTSELF) {
			rc = cil_notself_match_any(s2, s1, t1);
		} else if (t2->fqn == CIL_KEY_OTHER) {
			rc = cil_other_match_any(s2, s1, t1);
		} else {
			rc = cil_type_match_any(t1, t2);
		}
	}

	if (rc < 0) {
		goto exit;
	} else if (rc == CIL_FALSE) {
		rc = SEPOL_OK;
		goto exit;
	}

	if (!target->is_extended) {
		if (cil_classperms_list_match_any(avrule->perms.classperms, target->perms.classperms)) {
			cil_list_append(matching, CIL_NODE, node);
		}
	} else {
		if (cil_permissionx_match_any(avrule->perms.x.permx, target->perms.x.permx)) {
			cil_list_append(matching, CIL_NODE, node);
		}
	}

	rc = SEPOL_OK;

exit:
	return rc;
}

static int __cil_find_matching_avrule_in_ast(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_OK;
	struct cil_args_find *args = extra_args;

	if (node->flavor == CIL_BLOCK) {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
			goto exit;
		}
	} else if (node->flavor == CIL_MACRO) {
		*finished = CIL_TREE_SKIP_HEAD;
		goto exit;
	} else if (node->flavor == CIL_AVRULE || node->flavor == CIL_AVRULEX) {
		if (node->flavor == args->flavor) {
			rc = cil_find_matching_avrule(node, node->data, args->target, args->matching, args->match_self);
		}
	}

exit:
	return rc;
}

int cil_find_matching_avrule_in_ast(struct cil_tree_node *current, enum cil_flavor flavor, void *target, struct cil_list *matching, int match_self)
{
	int rc;
	struct cil_args_find args;

	args.flavor = flavor;
	args.target = target;
	args.matching = matching;
	args.match_self = match_self;

	rc = cil_tree_walk(current, __cil_find_matching_avrule_in_ast, NULL, NULL, &args);
	if (rc) {
		cil_log(CIL_ERR, "An error occurred while searching for avrule in AST\n");
	}

	return rc;
}
