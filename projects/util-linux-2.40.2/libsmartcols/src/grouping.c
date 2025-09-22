/*
 * Copyright (C) 2018 Karel Zak <kzak@redhat.com>
 */
#include "smartcolsP.h"

/**
 * SECTION: grouping
 * @title: Grouping
 * @short_description: lines grouing
 *
 * Lines groups manipulation API. The grouping API can be used to create M:N
 * relations between lines and on tree-like output it prints extra chart to
 * visualize these relations. The group has unlimited number of members and
 * group childs. See libsmartcols/sample/grouping* for more details.
 */

/* Private API */
void scols_ref_group(struct libscols_group *gr)
{
	if (gr)
		gr->refcount++;
}

void scols_group_remove_children(struct libscols_group *gr)
{
	if (!gr)
		return;

	while (!list_empty(&gr->gr_children)) {
		struct libscols_line *ln = list_entry(gr->gr_children.next,
						struct libscols_line, ln_children);

		DBG(GROUP, ul_debugobj(gr, "remove child"));
		list_del_init(&ln->ln_children);
		scols_ref_group(ln->parent_group);
		ln->parent_group = NULL;
		scols_unref_line(ln);
	}
}

void scols_group_remove_members(struct libscols_group *gr)
{
	if (!gr)
		return;

	while (!list_empty(&gr->gr_members)) {
		struct libscols_line *ln = list_entry(gr->gr_members.next,
						struct libscols_line, ln_groups);

		DBG(GROUP, ul_debugobj(gr, "remove member [%p]", ln));
		list_del_init(&ln->ln_groups);

		scols_unref_group(ln->group);
		ln->group->nmembers++;
		ln->group = NULL;

		scols_unref_line(ln);
	}
}

/* note group has to be already without members to deallocate */
void scols_unref_group(struct libscols_group *gr)
{
	if (gr && --gr->refcount <= 0) {
		DBG(GROUP, ul_debugobj(gr, "dealloc"));
		scols_group_remove_children(gr);
		list_del(&gr->gr_groups);
		free(gr);
		return;
	}
}


static void groups_fix_members_order(struct libscols_line *ln)
{
	struct libscols_iter itr;
	struct libscols_line *child;

	if (ln->group) {
		INIT_LIST_HEAD(&ln->ln_groups);
		list_add_tail(&ln->ln_groups, &ln->group->gr_members);
		DBG(GROUP, ul_debugobj(ln->group, "fixing member line=%p [%zu/%zu]",
					ln, ln->group->nmembers,
					list_count_entries(&ln->group->gr_members)));
	}

	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_line_next_child(ln, &itr, &child) == 0)
		groups_fix_members_order(child);

	/*
	 * We modify gr_members list, so is_last_group_member() does not have
	 * to provide reliable answer, we need to verify by list_count_entries().
	 */
	if (ln->group
	    && is_last_group_member(ln)
	    && ln->group->nmembers == list_count_entries(&ln->group->gr_members)) {

		DBG(GROUP, ul_debugobj(ln->group, "fixing childs"));
		scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
		while (scols_line_next_group_child(ln, &itr, &child) == 0)
			groups_fix_members_order(child);
	}
}

void scols_groups_fix_members_order(struct libscols_table *tb)
{
	struct libscols_iter itr;
	struct libscols_line *ln;
	struct libscols_group *gr;

	/* remove all from groups lists */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_group(tb, &itr, &gr) == 0) {
		while (!list_empty(&gr->gr_members)) {
			struct libscols_line *line = list_entry(gr->gr_members.next,
						struct libscols_line, ln_groups);
			list_del_init(&line->ln_groups);
		}
	}

	/* add again to the groups list in order we walk in tree */
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_line(tb, &itr, &ln) == 0) {
		if (ln->parent || ln->parent_group)
			continue;
		groups_fix_members_order(ln);
	}

	/* If group child is member of another group *
	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_group(tb, &itr, &gr) == 0) {
		struct libscols_iter xitr;
		struct libscols_line *child;

		scols_reset_iter(&xitr, SCOLS_ITER_FORWARD);
		while (scols_line_next_group_child(ln, &xitr, &child) == 0)
			groups_fix_members_order(child);
	}
	*/
}

static inline const char *group_state_to_string(int state)
{
	static const char *grpstates[] = {
		[SCOLS_GSTATE_NONE]		= "none",
		[SCOLS_GSTATE_FIRST_MEMBER]	= "1st-member",
		[SCOLS_GSTATE_MIDDLE_MEMBER]	= "middle-member",
		[SCOLS_GSTATE_LAST_MEMBER]	= "last-member",
		[SCOLS_GSTATE_MIDDLE_CHILD]	= "middle-child",
		[SCOLS_GSTATE_LAST_CHILD]	= "last-child",
		[SCOLS_GSTATE_CONT_MEMBERS]	= "continue-members",
		[SCOLS_GSTATE_CONT_CHILDREN]	= "continue-children"
	};

	assert(state >= 0);
	assert((size_t) state < ARRAY_SIZE(grpstates));

	return grpstates[state];
}
/*
static void grpset_debug(struct libscols_table *tb, struct libscols_line *ln)
{
	size_t i;

	for (i = 0; i < tb->grpset_size; i++) {
		if (tb->grpset[i]) {
			struct libscols_group *gr = tb->grpset[i];

			if (ln)
				DBG(LINE, ul_debugobj(ln, "grpset[%zu]: %p %s", i,
					gr, group_state_to_string(gr->state)));
			else
				DBG(LINE, ul_debug("grpset[%zu]: %p %s", i,
					gr, group_state_to_string(gr->state)));
		} else if (ln) {
			DBG(LINE, ul_debugobj(ln, "grpset[%zu]: free", i));
		} else
			DBG(LINE, ul_debug("grpset[%zu]: free", i));
	}
}
*/
static int group_state_for_line(struct libscols_group *gr, struct libscols_line *ln)
{
	if (gr->state == SCOLS_GSTATE_NONE &&
	    (ln->group != gr || !is_first_group_member(ln)))
		/*
		 * NONE is possible to translate to FIRST_MEMBER only, and only if
		 * line group matches with the current group.
		 */
		return SCOLS_GSTATE_NONE;

	if (ln->group != gr && ln->parent_group != gr) {
		/* Not our line, continue */
		if (gr->state == SCOLS_GSTATE_FIRST_MEMBER ||
		    gr->state == SCOLS_GSTATE_MIDDLE_MEMBER ||
		    gr->state == SCOLS_GSTATE_CONT_MEMBERS)
			return SCOLS_GSTATE_CONT_MEMBERS;

		if (gr->state == SCOLS_GSTATE_LAST_MEMBER ||
		    gr->state == SCOLS_GSTATE_MIDDLE_CHILD ||
		    gr->state == SCOLS_GSTATE_CONT_CHILDREN)
			return SCOLS_GSTATE_CONT_CHILDREN;

	} else if (ln->group == gr && is_first_group_member(ln)) {
		return SCOLS_GSTATE_FIRST_MEMBER;

	} else if (ln->group == gr && is_last_group_member(ln)) {
		return SCOLS_GSTATE_LAST_MEMBER;

	} else if (ln->group == gr && is_group_member(ln)) {
		return SCOLS_GSTATE_MIDDLE_MEMBER;

	} else if (ln->parent_group == gr && is_last_group_child(ln)) {
		return SCOLS_GSTATE_LAST_CHILD;

	} else if (ln->parent_group == gr && is_group_child(ln)) {
		return SCOLS_GSTATE_MIDDLE_CHILD;
	}

	return SCOLS_GSTATE_NONE;
}

/*
 * apply new @state to the chunk (addressed by @xx) of grpset used for the group (@gr)
 */
static void grpset_apply_group_state(struct libscols_group **xx, int state, struct libscols_group *gr)
{
	size_t i;

	DBG(GROUP, ul_debugobj(gr, "   applying state to grpset"));

	/* gr->state holds the old state, @state is the new state
	 */
	for (i = 0; i < SCOLS_GRPSET_CHUNKSIZ; i++)
		xx[i] = state == SCOLS_GSTATE_NONE ? NULL : gr;

	gr->state = state;
}

static struct libscols_group **grpset_locate_freespace(struct libscols_table *tb, int chunks, int prepend)
{
	size_t i, avail = 0;
	struct libscols_group **tmp, **first = NULL;
	const size_t wanted = chunks * SCOLS_GRPSET_CHUNKSIZ;

	if (!tb->grpset_size)
		prepend = 0;
	/*
	DBG(TAB, ul_debugobj(tb, "orig grpset:"));
	grpset_debug(tb, NULL);
	*/
	if (prepend) {
		for (i = tb->grpset_size - 1; ; i--) {
			if (tb->grpset[i] == NULL) {
				first = &tb->grpset[i];
				avail++;
			} else
				avail = 0;
			if (avail == wanted)
				goto done;
			if (i == 0)
				break;
		}
	} else {
		for (i = 0; i < tb->grpset_size; i++) {
			if (tb->grpset[i] == NULL) {
				if (avail == 0)
					first = &tb->grpset[i];
				avail++;
			} else
				avail = 0;
			if (avail == wanted)
				goto done;
		}
	}

	DBG(TAB, ul_debugobj(tb, "   realocate grpset [sz: old=%zu, new=%zu, new_chunks=%d]",
				tb->grpset_size, tb->grpset_size + wanted, chunks));

	tmp = reallocarray(tb->grpset, tb->grpset_size + wanted, sizeof(struct libscols_group *));
	if (!tmp)
		return NULL;

	tb->grpset = tmp;

	if (prepend) {
		DBG(TAB, ul_debugobj(tb, "   prepending free space"));
		char *dest = (char *) tb->grpset;

		memmove(	dest + (wanted * sizeof(struct libscols_group *)),
				tb->grpset,
				tb->grpset_size * sizeof(struct libscols_group *));
		first = tb->grpset;
	} else {
		first = tb->grpset + tb->grpset_size;
	}

	memset(first, 0, wanted * sizeof(struct libscols_group *));
	tb->grpset_size += wanted;

done:
	/*
	DBG(TAB, ul_debugobj(tb, "new grpset:"));
	grpset_debug(tb, NULL);
	*/
	return first;
}

static struct libscols_group **grpset_locate_group(struct libscols_table *tb, struct libscols_group *gr)
{
	size_t i;

	for (i = 0; i < tb->grpset_size; i++) {
		if (gr == tb->grpset[i])
			return &tb->grpset[i];
	}

	return NULL;
}


static int grpset_update(struct libscols_table *tb, struct libscols_line *ln, struct libscols_group *gr)
{
	struct libscols_group **xx;
	int state;

	DBG(LINE, ul_debugobj(ln, "   group [%p] grpset update [grpset size=%zu]", gr, tb->grpset_size));

	/* new state, note that gr->state still holds the original state */
	state = group_state_for_line(gr, ln);
	DBG(LINE, ul_debugobj(ln, "    state %s --> %s",
			group_state_to_string(gr->state),
			group_state_to_string(state)));

	if (state == SCOLS_GSTATE_FIRST_MEMBER && gr->state != SCOLS_GSTATE_NONE) {
		DBG(LINE, ul_debugobj(ln, "wrong group initialization (%s)", group_state_to_string(gr->state)));
		abort();
	}
	if (state != SCOLS_GSTATE_NONE && gr->state == SCOLS_GSTATE_LAST_CHILD) {
		DBG(LINE, ul_debugobj(ln, "wrong group termination (%s)", group_state_to_string(gr->state)));
		abort();
	}
	if (gr->state == SCOLS_GSTATE_LAST_MEMBER &&
	    !(state == SCOLS_GSTATE_LAST_CHILD ||
	      state == SCOLS_GSTATE_CONT_CHILDREN ||
	      state == SCOLS_GSTATE_MIDDLE_CHILD ||
	      state == SCOLS_GSTATE_NONE)) {
		DBG(LINE, ul_debugobj(ln, "wrong group member->child order"));
		abort();
	}

	/* should not happen; probably wrong line... */
	if (gr->state == SCOLS_GSTATE_NONE && state == SCOLS_GSTATE_NONE)
		return 0;

	/* locate place in grpset where we draw the group */
	if (!tb->grpset || gr->state == SCOLS_GSTATE_NONE)
		xx = grpset_locate_freespace(tb, 1, 1);
	else
		xx = grpset_locate_group(tb, gr);
	if (!xx) {
		DBG(LINE, ul_debugobj(ln, "failed to locate group or reallocate grpset"));
		return -ENOMEM;
	}

	grpset_apply_group_state(xx, state, gr);
	/*ON_DBG(LINE, grpset_debug(tb, ln));*/
	return 0;
}

static int grpset_update_active_groups(struct libscols_table *tb, struct libscols_line *ln)
{
	int rc = 0;
	size_t i;
	struct libscols_group *last = NULL;

	DBG(LINE, ul_debugobj(ln, "   update for active groups"));

	for (i = 0; i < tb->grpset_size; i++) {
		struct libscols_group *gr = tb->grpset[i];

		if (!gr || last == gr)
			continue;
		last = gr;
		rc = grpset_update(tb, ln, gr);
		if (rc)
			break;
	}

	DBG(LINE, ul_debugobj(ln, "   <- active groups updated [rc=%d]", rc));
	return rc;
}

int scols_groups_update_grpset(struct libscols_table *tb, struct libscols_line *ln)
{
	int rc = 0;

	DBG(LINE, ul_debugobj(ln, "  grpset update [line: group=%p, parent_group=%p",
				ln->group, ln->parent_group));

	rc = grpset_update_active_groups(tb, ln);
	if (!rc && ln->group && ln->group->state == SCOLS_GSTATE_NONE) {
		DBG(LINE, ul_debugobj(ln, " introduce a new group"));
		rc = grpset_update(tb, ln, ln->group);
	}
	return rc;
}

void scols_groups_reset_state(struct libscols_table *tb)
{
	struct libscols_iter itr;
	struct libscols_group *gr;

	DBG(TAB, ul_debugobj(tb, "reset groups states"));

	scols_reset_iter(&itr, SCOLS_ITER_FORWARD);
	while (scols_table_next_group(tb, &itr, &gr) == 0) {
		DBG(GROUP, ul_debugobj(gr, " reset to NONE"));
		gr->state = SCOLS_GSTATE_NONE;
	}

	if (tb->grpset) {
		DBG(TAB, ul_debugobj(tb, " zeroize grpset"));
		memset(tb->grpset, 0, tb->grpset_size * sizeof(struct libscols_group *));
	}
	tb->ngrpchlds_pending = 0;
}

static void add_member(struct libscols_group *gr, struct libscols_line *ln)
{
	DBG(GROUP, ul_debugobj(gr, "add member %p", ln));

	ln->group = gr;
	gr->nmembers++;
	scols_ref_group(gr);

	INIT_LIST_HEAD(&ln->ln_groups);
	list_add_tail(&ln->ln_groups, &gr->gr_members);
	scols_ref_line(ln);
}

/*
 * Returns first group which is ready to print group children.
 *
 * This function scans grpset[] in backward order and returns first group
 * with SCOLS_GSTATE_CONT_CHILDREN or SCOLS_GSTATE_LAST_MEMBER state.
 */
struct libscols_group *scols_grpset_get_printable_children(struct libscols_table *tb)
{
	size_t i;

	for (i = tb->grpset_size; i > 0; i -= SCOLS_GRPSET_CHUNKSIZ) {
		struct libscols_group *gr = tb->grpset[i-1];

		if (gr == NULL)
			continue;
		if (gr->state == SCOLS_GSTATE_CONT_CHILDREN ||
		    gr->state == SCOLS_GSTATE_LAST_MEMBER)
			return gr;
	}

	return NULL;
}


/**
 * scols_table_group_lines:
 * @tb: a pointer to a struct libscols_table instance
 * @ln: new group member
 * @member: group member
 * @id: group identifier (unused, not implemented yet), use zero.
 *
 * This function add line @ln to group of lines represented by @member.  If the
 * group is not yet defined (@member is not member of any group) than a new one
 * is allocated.
 *
 * The @ln maybe a NULL -- in this case only a new group is allocated if not
 * defined yet.
 *
 * Note that the same line cannot be member of more groups (not implemented
 * yet). The child of any group can be member of another group.
 *
 * The @id is not used for now, use 0. The plan is to use it to support
 * multi-group membership in future.
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.34
 */
int scols_table_group_lines(	struct libscols_table *tb,
				struct libscols_line *ln,
				struct libscols_line *member,
				__attribute__((__unused__)) int id)
{
	struct libscols_group *gr = NULL;

	if (!tb || !member) {
		DBG(GROUP, ul_debugobj(gr, "failed group lines (no table or member)"));
		return -EINVAL;
	}
	if (ln)  {
		if (ln->group && !member->group) {
			DBG(GROUP, ul_debugobj(gr, "failed group lines (new group, line member of another)"));
			return -EINVAL;
		}
		if (ln->group && member->group && ln->group != member->group) {
			DBG(GROUP, ul_debugobj(gr, "failed group lines (groups mismatch bwteen member and line"));
			return -EINVAL;
		}
	}

	gr = member->group;

	/* create a new group */
	if (!gr) {
		gr = calloc(1, sizeof(*gr));
		if (!gr)
			return -ENOMEM;
		DBG(GROUP, ul_debugobj(gr, "alloc"));
		gr->refcount = 1;
		INIT_LIST_HEAD(&gr->gr_members);
		INIT_LIST_HEAD(&gr->gr_children);
		INIT_LIST_HEAD(&gr->gr_groups);

		/* add group to the table */
		list_add_tail(&gr->gr_groups, &tb->tb_groups);

		/* add the first member */
		add_member(gr, member);
	}

	/* add to group */
	if (ln && !ln->group)
		add_member(gr, ln);

	return 0;
}

/**
 * scols_line_link_group:
 * @ln: line instance
 * @member: group member
 * @id: group identifier (unused, not implemented yet))
 *
 * Define @ln as child of group represented by group @member. The line @ln
 * cannot be child of any other line. It's possible to create group->child or
 * parent->child relationship, but no both for the same line (child).
 *
 * The @id is not used for now, use 0. The plan is to use it to support
 * multi-group membership in future.
 *
 * Returns: 0, a negative value in case of an error.
 *
 * Since: 2.34
 */
int scols_line_link_group(struct libscols_line *ln, struct libscols_line *member,
		 __attribute__((__unused__)) int id)
{
	if (!ln || !member || !member->group || ln->parent)
		return -EINVAL;

	if (!list_empty(&ln->ln_children))
		return -EINVAL;

	DBG(GROUP, ul_debugobj(member->group, "add child"));

	list_add_tail(&ln->ln_children, &member->group->gr_children);
	scols_ref_line(ln);

	ln->parent_group = member->group;
	scols_ref_group(member->group);

	return 0;
}
