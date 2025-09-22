#include "smartcolsP.h"

static int walk_line(struct libscols_table *tb,
		     struct libscols_line *ln,
		     struct libscols_column *cl,
		     int (*callback)(struct libscols_table *,
			            struct libscols_line *,
				    struct libscols_column *,
				    void *),
		    void *data)
{
	int rc = 0;

/*	DBG(LINE, ul_debugobj(ln, " wall line")); */

	/* we list group children in __scols_print_tree() after tree root node */
	if (is_group_member(ln) && is_last_group_member(ln) && has_group_children(ln))
		tb->ngrpchlds_pending++;

	if (has_groups(tb))
		rc = scols_groups_update_grpset(tb, ln);
	if (rc == 0)
		rc = callback(tb, ln, cl, data);

	/* children */
	if (rc == 0 && has_children(ln)) {
		struct list_head *p;

/*		DBG(LINE, ul_debugobj(ln, " children walk"));*/

		list_for_each(p, &ln->ln_branch) {
			struct libscols_line *chld = list_entry(p,
					struct libscols_line, ln_children);

			rc = walk_line(tb, chld, cl, callback, data);
			if (rc)
				break;
		}
	}

/*	DBG(LINE, ul_debugobj(ln, "<- walk line done [rc=%d]", rc)); */
	return rc;
}

/* last line in the tree? */
int scols_walk_is_last(struct libscols_table *tb, struct libscols_line *ln)
{
	if (tb->walk_last_done == 0)
		return 0;
	if (tb->ngrpchlds_pending > 0)
		return 0;
	if (has_children(ln))
		return 0;
	if (is_tree_root(ln) && !is_last_tree_root(tb, ln))
		return 0;
	if (is_group_member(ln) && (!is_last_group_member(ln) || has_group_children(ln)))
		return 0;
	if (is_child(ln)) {
		struct libscols_line *parent = ln->parent;

		if (!is_last_child(ln))
			return 0;
		while (parent) {
			if (is_child(parent) && !is_last_child(parent))
				return 0;
			if (!parent->parent)
				break;
			parent = parent->parent;
		}
		if (is_tree_root(parent) && !is_last_tree_root(tb, parent))
			return 0;
	}
	if (is_group_child(ln) && !is_last_group_child(ln))
		return 0;

	DBG(LINE, ul_debugobj(ln, "last in table"));
	return 1;
}

int scols_walk_tree(struct libscols_table *tb,
		    struct libscols_column *cl,
		    int (*callback)(struct libscols_table *,
			            struct libscols_line *,
				    struct libscols_column *,
				    void *),
		    void *data)
{
	int rc = 0;
	struct libscols_line *ln;
	struct libscols_iter itr;

	assert(tb);
/*	DBG(TAB, ul_debugobj(tb, ">> walk start"));*/

	/* init */
	tb->ngrpchlds_pending = 0;
	tb->walk_last_tree_root = NULL;
	tb->walk_last_done = 0;

	if (has_groups(tb))
		scols_groups_reset_state(tb);

	/* set pointer to last tree root */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_line(tb, &itr, &ln) == 0) {
		if (!tb->walk_last_tree_root)
			tb->walk_last_tree_root = ln;
		if (is_child(ln) || is_group_child(ln))
			continue;
		tb->walk_last_tree_root = ln;
	}

	/* walk */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (rc == 0 && scols_table_next_line(tb, &itr, &ln) == 0) {
		if (ln->parent || ln->parent_group)
			continue;

		if (tb->walk_last_tree_root == ln)
			tb->walk_last_done = 1;
		rc = walk_line(tb, ln, cl, callback, data);

		/* walk group's children */
		while (rc == 0 && tb->ngrpchlds_pending) {
			struct libscols_group *gr = scols_grpset_get_printable_children(tb);
			struct list_head *p;

			DBG(LINE, ul_debugobj(ln, " walk group children [pending=%zu]", tb->ngrpchlds_pending));
			if (!gr) {
				DBG(LINE, ul_debugobj(ln, " *** ngrpchlds_pending counter invalid"));
				tb->ngrpchlds_pending = 0;
				break;
			}

			tb->ngrpchlds_pending--;

			list_for_each(p, &gr->gr_children) {
				struct libscols_line *chld =
					list_entry(p, struct libscols_line, ln_children);

				rc = walk_line(tb, chld, cl, callback, data);
				if (rc)
					break;
			}
		}
	}

	tb->ngrpchlds_pending = 0;
	tb->walk_last_done = 0;
/*	DBG(TAB, ul_debugobj(tb, "<< walk end [rc=%d]", rc));*/
	return rc;
}
