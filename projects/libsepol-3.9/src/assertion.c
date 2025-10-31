/* Authors: Joshua Brindle <jbrindle@tresys.com>
 *
 * Assertion checker for avtab entries, taken from
 * checkpolicy.c by Stephen Smalley <stephen.smalley.work@gmail.com>
 *
 * Copyright (C) 2005 Tresys Technology, LLC
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdbool.h>
#include <sepol/policydb/avtab.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/util.h>

#include "private.h"
#include "debug.h"

struct avtab_match_args {
	sepol_handle_t *handle;
	policydb_t *p;
	const avrule_t *narule;
	unsigned long errors;
	bool conditional;
};

static const char* policy_name(const policydb_t *p) {
	return p->name ?: "policy.conf";
}

static void report_failure(sepol_handle_t *handle, const policydb_t *p, const avrule_t *narule,
			   unsigned int stype, unsigned int ttype,
			   const class_perm_node_t *curperm, uint32_t perms)
{
	char *permstr = sepol_av_to_string(p, curperm->tclass, perms);

	if (narule->source_filename) {
		ERR(handle, "neverallow on line %lu of %s (or line %lu of %s) violated by allow %s %s:%s {%s };",
		    narule->source_line, narule->source_filename, narule->line, policy_name(p),
		    p->p_type_val_to_name[stype],
		    p->p_type_val_to_name[ttype],
		    p->p_class_val_to_name[curperm->tclass - 1],
		    permstr ?: "<format-failure>");
	} else if (narule->line) {
		ERR(handle, "neverallow on line %lu violated by allow %s %s:%s {%s };",
		    narule->line, p->p_type_val_to_name[stype],
		    p->p_type_val_to_name[ttype],
		    p->p_class_val_to_name[curperm->tclass - 1],
		    permstr ?: "<format-failure>");
	} else {
		ERR(handle, "neverallow violated by allow %s %s:%s {%s };",
		    p->p_type_val_to_name[stype],
		    p->p_type_val_to_name[ttype],
		    p->p_class_val_to_name[curperm->tclass - 1],
		    permstr ?: "<format-failure>");
	}

	free(permstr);
}

static bool match_any_class_permissions(const class_perm_node_t *cp, uint32_t class, uint32_t data)
{
	for (; cp; cp = cp->next) {
		if ((cp->tclass == class) && (cp->data & data))
			return true;
	}

	return false;
}

static bool extended_permissions_and(const uint32_t *perms1, const uint32_t *perms2) {
	size_t i;
	for (i = 0; i < EXTENDED_PERMS_LEN; i++) {
		if (perms1[i] & perms2[i])
			return true;
	}

	return false;
}

static bool check_extended_permissions(const av_extended_perms_t *neverallow, const avtab_extended_perms_t *allow)
{
	bool rc = false;
	if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		if (neverallow->driver == allow->driver)
			rc = extended_permissions_and(neverallow->perms, allow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		rc = xperm_test(neverallow->driver, allow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		rc = xperm_test(allow->driver, neverallow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		rc = extended_permissions_and(neverallow->perms, allow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_NLMSG)
			&& (allow->specified == AVTAB_XPERMS_NLMSG)) {
		if (neverallow->driver == allow->driver)
			rc = extended_permissions_and(neverallow->perms, allow->perms);
	}

	return rc;
}

/* Compute which allowed extended permissions violate the neverallow rule */
static void extended_permissions_violated(avtab_extended_perms_t *result,
					const av_extended_perms_t *neverallow,
					const avtab_extended_perms_t *allow)
{
	size_t i;
	if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		result->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		result->driver = allow->driver;
		for (i = 0; i < EXTENDED_PERMS_LEN; i++)
			result->perms[i] = neverallow->perms[i] & allow->perms[i];
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		result->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		result->driver = neverallow->driver;
		memcpy(result->perms, neverallow->perms, sizeof(result->perms));
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		result->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		result->driver = allow->driver;
		memcpy(result->perms, allow->perms, sizeof(result->perms));
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		result->specified = AVTAB_XPERMS_IOCTLDRIVER;
		for (i = 0; i < EXTENDED_PERMS_LEN; i++)
			result->perms[i] = neverallow->perms[i] & allow->perms[i];
	} else if ((neverallow->specified == AVRULE_XPERMS_NLMSG)
			&& (allow->specified == AVTAB_XPERMS_NLMSG)) {
		result->specified = AVTAB_XPERMS_NLMSG;
		result->driver = allow->driver;
		for (i = 0; i < EXTENDED_PERMS_LEN; i++)
			result->perms[i] = neverallow->perms[i] & allow->perms[i];
	}
}

static bool match_node_key(const struct avtab_node *node, const avtab_key_t *key)
{
	return node->key.source_type == key->source_type
		&& node->key.target_type == key->target_type
		&& node->key.target_class == key->target_class;
}

/* Same scenarios of interest as check_assertion_extended_permissions */
static int report_assertion_extended_permissions(sepol_handle_t *handle,
				policydb_t *p, const avrule_t *narule,
				unsigned int stype, unsigned int ttype,
				const class_perm_node_t *curperm, uint32_t perms,
				const avtab_key_t *k, bool conditional)
{
	avtab_ptr_t node;
	avtab_key_t tmp_key;
	avtab_extended_perms_t *xperms;
	avtab_extended_perms_t error;
	const ebitmap_t *sattr = &p->type_attr_map[stype];
	const ebitmap_t *tattr = &p->type_attr_map[ttype];
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	bool found_xperm = false, found_cond_conflict = false;
	int errors = 0;

	memcpy(&tmp_key, k, sizeof(avtab_key_t));
	tmp_key.specified = AVTAB_XPERMS_ALLOWED;

	ebitmap_for_each_positive_bit(sattr, snode, i) {
		tmp_key.source_type = i + 1;
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			tmp_key.target_type = j + 1;
			for (node = avtab_search_node(&p->te_avtab, &tmp_key);
			     node;
			     node = avtab_search_node_next(node, tmp_key.specified)) {
				xperms = node->datum.xperms;
				if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
						&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER)
						&& (xperms->specified != AVTAB_XPERMS_NLMSG))
					continue;
				found_xperm = true;
				/* failure on the extended permission check_extended_permissions */
				if (check_extended_permissions(narule->xperms, xperms)) {
					char *permstring;

					extended_permissions_violated(&error, narule->xperms, xperms);
					permstring = sepol_extended_perms_to_string(&error);

					ERR(handle, "neverallowxperm on line %lu of %s (or line %lu of %s) violated by\n"
							"  allowxperm %s %s:%s %s;",
							narule->source_line, narule->source_filename, narule->line, policy_name(p),
							p->p_type_val_to_name[i],
							p->p_type_val_to_name[j],
							p->p_class_val_to_name[curperm->tclass - 1],
							permstring ?: "<format-failure>");

					free(permstring);
					errors++;
				}
			}

			for (const cond_list_t *cl = p->cond_list; cl; cl = cl->next) {
				bool found_true_base = false, found_true_xperm = false;
				bool found_false_base = false, found_false_xperm = false;

				for (const cond_av_list_t *cal = cl->true_list; cal; cal = cal->next) {
					node = cal->node; /* node->next is not from the same condition */
					if (!node)
						continue;

					if (!match_node_key(node, &tmp_key))
						continue;

					if (match_any_class_permissions(narule->perms, node->key.target_class, node->datum.data)) {
						found_true_base = true;
						continue;
					}

					if (!(node->key.specified & AVTAB_XPERMS_ALLOWED))
						continue;

					xperms = node->datum.xperms;
					if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
							&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER)
							&& (xperms->specified != AVTAB_XPERMS_NLMSG))
						continue;
					found_true_xperm = true;
					/* failure on the extended permission check_extended_permissions */
					if (check_extended_permissions(narule->xperms, xperms)) {
						char *permstring;

						extended_permissions_violated(&error, narule->xperms, xperms);
						permstring = sepol_extended_perms_to_string(&error);

						ERR(handle, "neverallowxperm on line %lu of %s (or line %lu of %s) violated by\n"
								"  allowxperm %s %s:%s %s;",
								narule->source_line, narule->source_filename, narule->line, policy_name(p),
								p->p_type_val_to_name[i],
								p->p_type_val_to_name[j],
								p->p_class_val_to_name[curperm->tclass - 1],
								permstring ?: "<format-failure>");

						free(permstring);
						errors++;
					}
				}

				for (const cond_av_list_t *cal = cl->false_list; cal; cal = cal->next) {
					node = cal->node; /* node->next is not from the same condition */
					if (!node)
						continue;

					if (!match_node_key(node, &tmp_key))
						continue;

					if (match_any_class_permissions(narule->perms, node->key.target_class, node->datum.data)) {
						found_false_base = true;
						continue;
					}

					if (!(node->key.specified & AVTAB_XPERMS_ALLOWED))
						continue;

					xperms = node->datum.xperms;
					if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
							&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER)
							&& (xperms->specified != AVTAB_XPERMS_NLMSG))
						continue;
					found_false_xperm = true;
					/* failure on the extended permission check_extended_permissions */
					if (check_extended_permissions(narule->xperms, xperms)) {
						char *permstring;

						extended_permissions_violated(&error, narule->xperms, xperms);
						permstring = sepol_extended_perms_to_string(&error);

						ERR(handle, "neverallowxperm on line %lu of %s (or line %lu of %s) violated by\n"
								"  allowxperm %s %s:%s %s;",
								narule->source_line, narule->source_filename, narule->line, policy_name(p),
								p->p_type_val_to_name[i],
								p->p_type_val_to_name[j],
								p->p_class_val_to_name[curperm->tclass - 1],
								permstring ?: "<format-failure>");

						free(permstring);
						errors++;
					}
				}

				if (found_true_xperm && found_false_xperm)
					found_xperm = true;
				else if (conditional && ((found_true_base && !found_true_xperm) || (found_false_base && !found_false_xperm)))
					found_cond_conflict = true;
			}
		}
	}

	if ((!found_xperm && !conditional) || found_cond_conflict) {
		/* failure on the regular permissions */
		char *permstr = sepol_av_to_string(p, curperm->tclass, perms);

		ERR(handle, "neverallowxperm on line %lu of %s (or line %lu of %s) violated by\n"
				"  allow %s %s:%s {%s };",
				narule->source_line, narule->source_filename, narule->line, policy_name(p),
				p->p_type_val_to_name[stype],
				p->p_type_val_to_name[ttype],
				p->p_class_val_to_name[curperm->tclass - 1],
				permstr ?: "<format-failure>");

		free(permstr);
		errors++;
	}

	return errors;
}

static int report_assertion_avtab_matches(avtab_key_t *k, avtab_datum_t *d, void *args)
{
	int rc = 0;
	struct avtab_match_args *a = (struct avtab_match_args *)args;
	sepol_handle_t *handle = a->handle;
	policydb_t *p = a->p;
	const avrule_t *narule = a->narule;
	const class_perm_node_t *cp;
	uint32_t perms;
	ebitmap_t src_matches, tgt_matches, self_matches;
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	const bool is_narule_self = (narule->flags & RULE_SELF) != 0;
	const bool is_narule_notself = (narule->flags & RULE_NOTSELF) != 0;

	if ((k->specified & AVTAB_ALLOWED) == 0)
		return 0;

	if (!match_any_class_permissions(narule->perms, k->target_class, d->data))
		return 0;

	ebitmap_init(&src_matches);
	ebitmap_init(&tgt_matches);
	ebitmap_init(&self_matches);

	rc = ebitmap_and(&src_matches, &narule->stypes.types,
			 &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (ebitmap_is_empty(&src_matches))
		goto exit;

	if (is_narule_notself) {
		if (ebitmap_is_empty(&narule->ttypes.types)) {
			/* avrule tgt is of the form ~self */
			rc = ebitmap_cpy(&tgt_matches, &p->attr_type_map[k->target_type -1]);
		} else {
			/* avrule tgt is of the form {ATTR -self} */
			rc = ebitmap_and(&tgt_matches, &narule->ttypes.types, &p->attr_type_map[k->target_type - 1]);
		}
		if (rc)
			goto oom;
	} else {
		rc = ebitmap_and(&tgt_matches, &narule->ttypes.types, &p->attr_type_map[k->target_type -1]);
		if (rc < 0)
			goto oom;

		if (is_narule_self) {
			rc = ebitmap_and(&self_matches, &src_matches, &p->attr_type_map[k->target_type - 1]);
			if (rc < 0)
				goto oom;

			if (!ebitmap_is_empty(&self_matches)) {
				rc = ebitmap_union(&tgt_matches, &self_matches);
				if (rc < 0)
					goto oom;
			}
		}
	}

	if (ebitmap_is_empty(&tgt_matches))
		goto exit;

	for (cp = narule->perms; cp; cp = cp->next) {

		perms = cp->data & d->data;
		if ((cp->tclass != k->target_class) || !perms) {
			continue;
		}

		ebitmap_for_each_positive_bit(&src_matches, snode, i) {
			ebitmap_for_each_positive_bit(&tgt_matches, tnode, j) {
				if (is_narule_self && i != j)
					continue;
				if (is_narule_notself && i == j)
					continue;
				if (narule->specified == AVRULE_XPERMS_NEVERALLOW) {
					a->errors += report_assertion_extended_permissions(handle,p, narule,
											i, j, cp, perms, k,
											a->conditional);
				} else {
					a->errors++;
					report_failure(handle, p, narule, i, j, cp, perms);
				}
			}
		}
	}

oom:
exit:
	ebitmap_destroy(&src_matches);
	ebitmap_destroy(&tgt_matches);
	ebitmap_destroy(&self_matches);
	return rc;
}

static int report_assertion_failures(sepol_handle_t *handle, policydb_t *p, const avrule_t *narule)
{
	int rc;
	struct avtab_match_args args = {
		.handle = handle,
		.p = p,
		.narule = narule,
		.errors = 0,
	};

	args.conditional = false;
	rc = avtab_map(&p->te_avtab, report_assertion_avtab_matches, &args);
	if (rc < 0)
		goto oom;

	args.conditional = true;
	rc = avtab_map(&p->te_cond_avtab, report_assertion_avtab_matches, &args);
	if (rc < 0)
		goto oom;

	return args.errors;

oom:
	return rc;
}

/*
 * Look up the extended permissions in avtab and verify that neverallowed
 * permissions are not granted.
 */
static bool check_assertion_extended_permissions_avtab(const avrule_t *narule,
						unsigned int stype, unsigned int ttype,
						const avtab_key_t *k, policydb_t *p,
						bool conditional)
{
	avtab_ptr_t node;
	avtab_key_t tmp_key;
	const avtab_extended_perms_t *xperms;
	const av_extended_perms_t *neverallow_xperms = narule->xperms;
	const ebitmap_t *sattr = &p->type_attr_map[stype];
	const ebitmap_t *tattr = &p->type_attr_map[ttype];
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	bool found_xperm = false, found_cond_conflict = false;

	memcpy(&tmp_key, k, sizeof(avtab_key_t));
	tmp_key.specified = AVTAB_XPERMS_ALLOWED;

	ebitmap_for_each_positive_bit(sattr, snode, i) {
		tmp_key.source_type = i + 1;
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			tmp_key.target_type = j + 1;
			for (node = avtab_search_node(&p->te_avtab, &tmp_key);
			     node;
			     node = avtab_search_node_next(node, tmp_key.specified)) {
				xperms = node->datum.xperms;

				if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
						&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER)
						&& (xperms->specified != AVTAB_XPERMS_NLMSG))
					continue;
				found_xperm = true;
				if (check_extended_permissions(neverallow_xperms, xperms))
					return true;
			}

			for (const cond_list_t *cl = p->cond_list; cl; cl = cl->next) {
				bool found_true_base = false, found_true_xperm = false;
				bool found_false_base = false, found_false_xperm = false;

				for (const cond_av_list_t *cal = cl->true_list; cal; cal = cal->next) {
					node = cal->node; /* node->next is not from the same condition */
					if (!node)
						continue;

					if (!match_node_key(node, &tmp_key))
						continue;

					if ((node->key.specified & AVTAB_ALLOWED) && match_any_class_permissions(narule->perms, node->key.target_class, node->datum.data)) {
						found_true_base = true;
						continue;
					}

					if (!(node->key.specified & AVTAB_XPERMS_ALLOWED))
						continue;

					xperms = node->datum.xperms;

					if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
							&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER)
							&& (xperms->specified != AVTAB_XPERMS_NLMSG))
						continue;
					found_true_xperm = true;
					if (check_extended_permissions(neverallow_xperms, xperms))
						return true;
				}

				for (const cond_av_list_t *cal = cl->false_list; cal; cal = cal->next) {
					node = cal->node; /* node->next is not from the same condition */
					if (!node)
						continue;

					if (!match_node_key(node, &tmp_key))
						continue;

					if ((node->key.specified & AVTAB_ALLOWED) && match_any_class_permissions(narule->perms, node->key.target_class, node->datum.data)) {
						found_false_base = true;
						continue;
					}

					if (!(node->key.specified & AVTAB_XPERMS_ALLOWED))
						continue;

					xperms = node->datum.xperms;

					if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
							&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER)
							&& (xperms->specified != AVTAB_XPERMS_NLMSG))
						continue;
					found_false_xperm = true;
					if (check_extended_permissions(neverallow_xperms, xperms))
						return true;
				}

				if (found_true_xperm && found_false_xperm)
					found_xperm = true;
				else if (conditional && ((found_true_base && !found_true_xperm) || (found_false_base && !found_false_xperm)))
					found_cond_conflict = true;
			}
		}
	}

	return (!conditional && !found_xperm) || found_cond_conflict;
}

/*
 * When the ioctl permission is granted on an avtab entry that matches an
 * avrule neverallowxperm entry, enumerate over the matching
 * source/target/class sets to determine if the extended permissions exist
 * and if the neverallowed ioctls are granted.
 *
 * Four scenarios of interest:
 * 1. PASS - the ioctl permission is not granted for this source/target/class
 *    This case is handled in check_assertion_avtab_match
 * 2. PASS - The ioctl permission is granted AND the extended permission
 *    is NOT granted
 * 3. FAIL - The ioctl permission is granted AND no extended permissions
 *    exist
 * 4. FAIL - The ioctl permission is granted AND the extended permission is
 *    granted
 */
static int check_assertion_extended_permissions(const avrule_t *narule,
						const avtab_key_t *k, policydb_t *p,
						bool conditional)
{
	ebitmap_t src_matches, tgt_matches, self_matches;
	unsigned int i, j;
	ebitmap_node_t *snode, *tnode;
	const bool is_narule_self = (narule->flags & RULE_SELF) != 0;
	const bool is_narule_notself = (narule->flags & RULE_NOTSELF) != 0;
	int rc;

	ebitmap_init(&src_matches);
	ebitmap_init(&tgt_matches);
	ebitmap_init(&self_matches);

	rc = ebitmap_and(&src_matches, &narule->stypes.types,
			 &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (ebitmap_is_empty(&src_matches)) {
		rc = 0;
		goto exit;
	}

	if (is_narule_notself) {
		if (ebitmap_is_empty(&narule->ttypes.types)) {
			/* avrule tgt is of the form ~self */
			rc = ebitmap_cpy(&tgt_matches, &p->attr_type_map[k->target_type -1]);
		} else {
			/* avrule tgt is of the form {ATTR -self} */
			rc = ebitmap_and(&tgt_matches, &narule->ttypes.types, &p->attr_type_map[k->target_type - 1]);
		}
		if (rc < 0)
			goto oom;
	} else {
		rc = ebitmap_and(&tgt_matches, &narule->ttypes.types, &p->attr_type_map[k->target_type -1]);
		if (rc < 0)
			goto oom;

		if (is_narule_self) {
			rc = ebitmap_and(&self_matches, &src_matches, &p->attr_type_map[k->target_type - 1]);
			if (rc < 0)
				goto oom;

			if (!ebitmap_is_empty(&self_matches)) {
				rc = ebitmap_union(&tgt_matches, &self_matches);
				if (rc < 0)
					goto oom;
			}
		}
	}

	if (ebitmap_is_empty(&tgt_matches)) {
		rc = 0;
		goto exit;
	}

	ebitmap_for_each_positive_bit(&src_matches, snode, i) {
		ebitmap_for_each_positive_bit(&tgt_matches, tnode, j) {
			if (is_narule_self && i != j)
				continue;
			if (is_narule_notself && i == j)
				continue;
			if (check_assertion_extended_permissions_avtab(narule, i, j, k, p, conditional)) {
				rc = 1;
				goto exit;
			}
		}
	}

	rc = 0;

oom:
exit:
	ebitmap_destroy(&src_matches);
	ebitmap_destroy(&tgt_matches);
	ebitmap_destroy(&self_matches);
	return rc;
}

static int check_assertion_notself_match(const avtab_key_t *k, const avrule_t *narule, policydb_t *p)
{
	ebitmap_t src_matches, tgt_matches;
	unsigned int num_src_matches, num_tgt_matches;
	int rc;

	ebitmap_init(&src_matches);
	ebitmap_init(&tgt_matches);

	rc = ebitmap_and(&src_matches, &narule->stypes.types, &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (ebitmap_is_empty(&narule->ttypes.types)) {
		/* avrule tgt is of the form ~self */
		rc = ebitmap_cpy(&tgt_matches, &p->attr_type_map[k->target_type - 1]);
	} else {
		/* avrule tgt is of the form {ATTR -self} */
		rc = ebitmap_and(&tgt_matches, &narule->ttypes.types, &p->attr_type_map[k->target_type - 1]);
	}
	if (rc < 0)
		goto oom;

	num_src_matches = ebitmap_cardinality(&src_matches);
	num_tgt_matches = ebitmap_cardinality(&tgt_matches);
	if (num_src_matches == 0 || num_tgt_matches == 0) {
		rc = 0;
		goto nomatch;
	}
	if (num_src_matches == 1 && num_tgt_matches == 1) {
		ebitmap_t matches;
		unsigned int num_matches;
		rc = ebitmap_and(&matches, &src_matches, &tgt_matches);
		if (rc < 0) {
			ebitmap_destroy(&matches);
			goto oom;
		}
		num_matches = ebitmap_cardinality(&matches);
		ebitmap_destroy(&matches);
		if (num_matches == 1) {
			/* The only non-match is of the form TYPE TYPE */
			rc = 0;
			goto nomatch;
		}
	}

	rc = 1;

oom:
nomatch:
	ebitmap_destroy(&src_matches);
	ebitmap_destroy(&tgt_matches);
	return rc;
}

static int check_assertion_self_match(const avtab_key_t *k, const avrule_t *narule, policydb_t *p)
{
	ebitmap_t src_matches;
	int rc;

	/* The key's target must match something in the matches of the avrule's source
	 * and the key's source.
	 */

	rc = ebitmap_and(&src_matches, &narule->stypes.types, &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (!ebitmap_match_any(&src_matches, &p->attr_type_map[k->target_type - 1])) {
		rc = 0;
		goto nomatch;
	}

	rc = 1;

oom:
nomatch:
	ebitmap_destroy(&src_matches);
	return rc;
}

static int check_assertion_avtab_match(avtab_key_t *k, avtab_datum_t *d, void *args)
{
	int rc;
	struct avtab_match_args *a = (struct avtab_match_args *)args;
	policydb_t *p = a->p;
	const avrule_t *narule = a->narule;

	if ((k->specified & AVTAB_ALLOWED) == 0)
		goto nomatch;

	if (!match_any_class_permissions(narule->perms, k->target_class, d->data))
		goto nomatch;

	if (!ebitmap_match_any(&narule->stypes.types, &p->attr_type_map[k->source_type - 1]))
		goto nomatch;

	if (narule->flags & RULE_NOTSELF) {
		rc = check_assertion_notself_match(k, narule, p);
		if (rc < 0)
			goto oom;
		if (rc == 0)
			goto nomatch;
	} else {
		/* neverallow may have tgts even if it uses SELF */
		if (!ebitmap_match_any(&narule->ttypes.types, &p->attr_type_map[k->target_type -1])) {
			if (narule->flags == RULE_SELF) {
				rc = check_assertion_self_match(k, narule, p);
				if (rc < 0)
					goto oom;
				if (rc == 0)
					goto nomatch;
			} else {
				goto nomatch;
			}
		}
	}

	if (narule->specified == AVRULE_XPERMS_NEVERALLOW) {
		rc = check_assertion_extended_permissions(narule, k, p, a->conditional);
		if (rc < 0)
			goto oom;
		if (rc == 0)
			goto nomatch;
	}
	return 1;

nomatch:
	return 0;

oom:
	return rc;
}

int check_assertion(policydb_t *p, const avrule_t *narule)
{
	int rc;
	struct avtab_match_args args = {
		.handle = NULL,
		.p = p,
		.narule = narule,
		.errors = 0,
	};

	args.conditional = false;
	rc = avtab_map(&p->te_avtab, check_assertion_avtab_match, &args);

	if (rc == 0) {
		args.conditional = true;
		rc = avtab_map(&p->te_cond_avtab, check_assertion_avtab_match, &args);
	}

	return rc;
}

int check_assertions(sepol_handle_t * handle, policydb_t * p,
		     const avrule_t * narules)
{
	int rc;
	const avrule_t *a;
	unsigned long errors = 0;

	for (a = narules; a != NULL; a = a->next) {
		if (!(a->specified & (AVRULE_NEVERALLOW | AVRULE_XPERMS_NEVERALLOW)))
			continue;
		rc = check_assertion(p, a);
		if (rc < 0) {
			ERR(handle, "Error occurred while checking neverallows");
			return -1;
		}
		if (rc) {
			rc = report_assertion_failures(handle, p, a);
			if (rc < 0) {
				ERR(handle, "Error occurred while checking neverallows");
				return -1;
			}
			errors += rc;
		}
	}

	if (errors)
		ERR(handle, "%lu neverallow failures occurred", errors);

	return errors ? -1 : 0;
}
