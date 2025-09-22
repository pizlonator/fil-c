/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "c.h"
#include "rpmatch.h"
#include "fdisk.h"
#include "pt-sun.h"
#include "pt-mbr.h"

struct menu_entry {
	const char	key;			/* command key */
	const char	*title;			/* help string */
	unsigned int	normal : 1,		/* normal mode */
			expert : 1,		/* expert mode */
			hidden : 1;		/* be sensitive for this key,
						   but don't print it in help */

	enum fdisk_labeltype	label;		/* only for this label */
	int                     exclude;    /* all labels except these */
	enum fdisk_labeltype	parent;		/* for nested PT */
};

#define IS_MENU_SEP(e)	((e)->key == '-')
#define IS_MENU_HID(e)	((e)->hidden)

struct menu {
	enum fdisk_labeltype	label;		/* only for this label */
	int                     exclude;    /* all labels except these */

	unsigned int		nonested : 1;	/* don't make this menu active in nested PT */

	int (*callback)(struct fdisk_context **,
			const struct menu *,
			const struct menu_entry *);

	struct menu_entry	entries[];	/* NULL terminated array */
};

struct menu_context {
	size_t		menu_idx;		/* the current menu */
	size_t		entry_idx;		/* index with in the current menu */
};

#define MENU_CXT_EMPTY	{ 0, 0 }
#define DECLARE_MENU_CB(x) \
	static int x(struct fdisk_context **, \
		     const struct menu *, \
		     const struct menu_entry *)

DECLARE_MENU_CB(gpt_menu_cb);
DECLARE_MENU_CB(sun_menu_cb);
DECLARE_MENU_CB(sgi_menu_cb);
DECLARE_MENU_CB(geo_menu_cb);
DECLARE_MENU_CB(dos_menu_cb);
DECLARE_MENU_CB(bsd_menu_cb);
DECLARE_MENU_CB(createlabel_menu_cb);
DECLARE_MENU_CB(generic_menu_cb);

/*
 * Menu entry macros:
 *	MENU_X*    expert mode only
 *      MENU_B*    both -- expert + normal mode
 *
 *      *_E  exclude this label
 *      *_H  hidden
 *      *_L  only for this label
 */

/* separator */
#define MENU_SEP(t)		{ .title = t, .key = '-', .normal = 1 }
#define MENU_XSEP(t)		{ .title = t, .key = '-', .expert = 1 }
#define MENU_BSEP(t)		{ .title = t, .key = '-', .expert = 1, .normal = 1 }

/* entry */
#define MENU_ENT(k, t)		{ .title = t, .key = k, .normal = 1 }
#define MENU_ENT_E(k, t, l)	{ .title = t, .key = k, .normal = 1, .exclude = l }
#define MENU_ENT_L(k, t, l)	{ .title = t, .key = k, .normal = 1, .label = l }

#define MENU_XENT(k, t)		{ .title = t, .key = k, .expert = 1 }
#define MENU_XENT_H(k, t)	{ .title = t, .key = k, .expert = 1, .hidden = 1 }

#define MENU_BENT(k, t)		{ .title = t, .key = k, .expert = 1, .normal = 1 }
#define MENU_BENT_E(k, t, l)	{ .title = t, .key = k, .expert = 1, .normal = 1, .exclude = l }

#define MENU_ENT_NEST(k, t, l, p)	{ .title = t, .key = k, .normal = 1, .label = l, .parent = p }
#define MENU_BENT_NEST_H(k, t, l, p)	{ .title = t, .key = k, .expert = 1, .normal = 1, .label = l, .parent = p, .hidden = 1 }

/* Generic menu */
static const struct menu menu_generic = {
	.callback	= generic_menu_cb,
	.entries	= {
		MENU_BSEP(N_("Generic")),
		MENU_ENT  ('d', N_("delete a partition")),
		MENU_ENT  ('F', N_("list free unpartitioned space")),
		MENU_ENT  ('l', N_("list known partition types")),
		MENU_ENT  ('n', N_("add a new partition")),
		MENU_BENT ('p', N_("print the partition table")),
		MENU_ENT  ('t', N_("change a partition type")),
		MENU_BENT_E('v', N_("verify the partition table"), FDISK_DISKLABEL_BSD),
		MENU_ENT  ('i', N_("print information about a partition")),
		MENU_ENT  ('e', N_("resize a partition")),

		MENU_XENT('d', N_("print the raw data of the first sector from the device")),
		MENU_XENT('D', N_("print the raw data of the disklabel from the device")),
		MENU_XENT('f', N_("fix partitions order")),

		MENU_SEP(N_("Misc")),
		MENU_BENT ('m', N_("print this menu")),
		MENU_ENT_E('u', N_("change display/entry units"), FDISK_DISKLABEL_GPT),
		MENU_ENT_E('x', N_("extra functionality (experts only)"), FDISK_DISKLABEL_BSD),

		MENU_SEP(N_("Script")),
		MENU_ENT  ('I', N_("load disk layout from sfdisk script file")),
		MENU_ENT  ('O', N_("dump disk layout to sfdisk script file")),

		MENU_BSEP(N_("Save & Exit")),
		MENU_ENT_E('w', N_("write table to disk and exit"), FDISK_DISKLABEL_BSD),
		MENU_ENT_L('w', N_("write table to disk"), FDISK_DISKLABEL_BSD),
		MENU_BENT ('q', N_("quit without saving changes")),
		MENU_XENT ('r', N_("return to main menu")),

		MENU_ENT_NEST('r', N_("return from BSD to DOS (MBR)"), FDISK_DISKLABEL_BSD, FDISK_DISKLABEL_DOS),

		MENU_ENT_NEST('r', N_("return from protective/hybrid MBR to GPT"), FDISK_DISKLABEL_DOS, FDISK_DISKLABEL_GPT),

		{ 0, NULL }
	}
};

static const struct menu menu_createlabel = {
	.callback = createlabel_menu_cb,
	.exclude = FDISK_DISKLABEL_BSD,
	.nonested = 1,
	.entries = {
		MENU_SEP(N_("Create a new label")),
		MENU_ENT('g', N_("create a new empty GPT partition table")),
		MENU_ENT('G', N_("create a new empty SGI (IRIX) partition table")),
		MENU_ENT('o', N_("create a new empty MBR (DOS) partition table")),
		MENU_ENT('s', N_("create a new empty Sun partition table")),

		/* backward compatibility -- be sensitive to 'g', but don't
		 * print it in the expert menu */
		MENU_XENT_H('g', N_("create an IRIX (SGI) partition table")),
		{ 0, NULL }
	}
};

static const struct menu menu_geo = {
	.callback = geo_menu_cb,
	.exclude = FDISK_DISKLABEL_GPT | FDISK_DISKLABEL_BSD,
	.entries = {
		MENU_XSEP(N_("Geometry (for the current label)")),
		MENU_XENT('c', N_("change number of cylinders")),
		MENU_XENT('h', N_("change number of heads")),
		MENU_XENT('s', N_("change number of sectors/track")),
		{ 0, NULL }
	}
};

static const struct menu menu_gpt = {
	.callback = gpt_menu_cb,
	.label = FDISK_DISKLABEL_GPT,
	.entries = {
		MENU_BSEP(N_("GPT")),
		MENU_XENT('i', N_("change disk GUID")),
		MENU_XENT('n', N_("change partition name")),
		MENU_XENT('u', N_("change partition UUID")),
		MENU_XENT('l', N_("change table length")),
		MENU_BENT('M', N_("enter protective/hybrid MBR")),

		MENU_XSEP(""),
		MENU_XENT('A', N_("toggle the legacy BIOS bootable flag")),
		MENU_XENT('B', N_("toggle the no block IO protocol flag")),
		MENU_XENT('R', N_("toggle the required partition flag")),
		MENU_XENT('S', N_("toggle the GUID specific bits")),

		{ 0, NULL }
	}
};

static const struct menu menu_sun = {
	.callback = sun_menu_cb,
	.label = FDISK_DISKLABEL_SUN,
	.entries = {
		MENU_BSEP(N_("Sun")),
		MENU_ENT('a', N_("toggle the read-only flag")),
		MENU_ENT('c', N_("toggle the mountable flag")),

		MENU_XENT('a', N_("change number of alternate cylinders")),
		MENU_XENT('e', N_("change number of extra sectors per cylinder")),
		MENU_XENT('i', N_("change interleave factor")),
		MENU_XENT('o', N_("change rotation speed (rpm)")),
		MENU_XENT('y', N_("change number of physical cylinders")),
		{ 0, NULL }
	}
};

static const struct menu menu_sgi = {
	.callback = sgi_menu_cb,
	.label = FDISK_DISKLABEL_SGI,
	.entries = {
		MENU_SEP(N_("SGI")),
		MENU_ENT('a', N_("select bootable partition")),
		MENU_ENT('b', N_("edit bootfile entry")),
		MENU_ENT('c', N_("select sgi swap partition")),
		MENU_ENT('i', N_("create SGI info")),
		{ 0, NULL }
	}
};

static const struct menu menu_dos = {
	.callback = dos_menu_cb,
	.label = FDISK_DISKLABEL_DOS,
	.entries = {
		MENU_BSEP(N_("DOS (MBR)")),
		MENU_ENT('a', N_("toggle a bootable flag")),
		MENU_ENT('b', N_("edit nested BSD disklabel")),
		MENU_ENT('c', N_("toggle the dos compatibility flag")),

		MENU_XENT('b', N_("move beginning of data in a partition")),
		MENU_XENT('F', N_("fix partitions C/H/S values")),
		MENU_XENT('i', N_("change the disk identifier")),

		MENU_BENT_NEST_H('M', N_("return from protective/hybrid MBR to GPT"), FDISK_DISKLABEL_DOS, FDISK_DISKLABEL_GPT),

		{ 0, NULL }
	}
};

static const struct menu menu_bsd = {
	.callback = bsd_menu_cb,
	.label = FDISK_DISKLABEL_BSD,
	.entries = {
		MENU_SEP(N_("BSD")),
		MENU_ENT('e', N_("edit drive data")),
		MENU_ENT('i', N_("install bootstrap")),
		MENU_ENT('s', N_("show complete disklabel")),
		MENU_ENT('x', N_("link BSD partition to non-BSD partition")),
		{ 0, NULL }
	}
};

static const struct menu *menus[] = {
	&menu_gpt,
	&menu_sun,
	&menu_sgi,
	&menu_dos,
	&menu_bsd,
	&menu_geo,
	&menu_generic,
	&menu_createlabel,
};

static const struct menu_entry *next_menu_entry(
			struct fdisk_context *cxt,
			struct menu_context *mc)
{
	struct fdisk_label *lb = fdisk_get_label(cxt, NULL);
	struct fdisk_context *parent = fdisk_get_parent(cxt);
	unsigned int type = 0, pr_type = 0;

	assert(cxt);

	if (lb)
		type = fdisk_label_get_type(lb);
	if (parent)
		pr_type = fdisk_label_get_type(fdisk_get_label(parent, NULL));

	while (mc->menu_idx < ARRAY_SIZE(menus)) {
		const struct menu *m = menus[mc->menu_idx];
		const struct menu_entry *e = &(m->entries[mc->entry_idx]);

		/*
		 * whole-menu filter
		 */

		/* no more entries */
		if (e->title == NULL ||
		/* menu wanted for specified labels only */
		    (m->label && (!lb || !(m->label & type))) ||
		/* unwanted for nested PT */
		    (m->nonested && parent) ||
		/* menu excluded for specified labels */
		    (m->exclude && lb && (m->exclude & type))) {
			mc->menu_idx++;
			mc->entry_idx = 0;
			continue;
		}

		/*
		 * per entry filter
		 */

		/* excluded for the current label */
		if ((e->exclude && lb && e->exclude & type) ||
		/* entry wanted for specified labels only */
		    (e->label && (!lb || !(e->label & type))) ||
		/* exclude non-expert entries in expect mode */
		    (e->expert == 0 && fdisk_is_details(cxt)) ||
		/* nested only */
		    (e->parent && (!parent || pr_type != e->parent)) ||
		/* exclude non-normal entries in normal mode */
		    (e->normal == 0 && !fdisk_is_details(cxt))) {
			mc->entry_idx++;
			continue;
		}
		mc->entry_idx++;
		return e;

	}
	return NULL;
}

/* returns @menu and menu entry for then @key */
static const struct menu_entry *get_fdisk_menu_entry(
		struct fdisk_context *cxt,
		int key,
		const struct menu **menu)
{
	struct menu_context mc = MENU_CXT_EMPTY;
	const struct menu_entry *e;

	while ((e = next_menu_entry(cxt, &mc))) {
		if (IS_MENU_SEP(e) || e->key != key)
			continue;

		if (menu)
			*menu = menus[mc.menu_idx];
		return e;
	}

	return NULL;
}

static int menu_detect_collisions(struct fdisk_context *cxt)
{
	struct menu_context mc = MENU_CXT_EMPTY;
	const struct menu_entry *e, *r;

	while ((e = next_menu_entry(cxt, &mc))) {
		if (IS_MENU_SEP(e))
			continue;

		r = get_fdisk_menu_entry(cxt, e->key, NULL);
		if (!r) {
			DBG(MENU, ul_debug("warning: not found "
					"entry for %c", e->key));
			return -1;
		}
		if (r != e) {
			DBG(MENU, ul_debug("warning: duplicate key '%c'",
						e->key));
			DBG(MENU, ul_debug("       : %s", e->title));
			DBG(MENU, ul_debug("       : %s", r->title));
			abort();
		}
	}

	return 0;
}

static int print_fdisk_menu(struct fdisk_context *cxt)
{
	struct menu_context mc = MENU_CXT_EMPTY;
	const struct menu_entry *e;

	ON_DBG(MENU, menu_detect_collisions(cxt));

	if (fdisk_is_details(cxt))
		printf(_("\nHelp (expert commands):\n"));
	else
		printf(_("\nHelp:\n"));

	while ((e = next_menu_entry(cxt, &mc))) {
		if (IS_MENU_HID(e))
			continue;	/* hidden entry */
		if (IS_MENU_SEP(e) && (!e->title || !*e->title))
			printf("\n");
		else if (IS_MENU_SEP(e)) {
			color_scheme_enable("help-title", UL_COLOR_BOLD);
			printf("\n  %s\n", _(e->title));
			color_disable();
		} else
			printf("   %c   %s\n", e->key, _(e->title));
	}
	fputc('\n', stdout);

	if (fdisk_get_parent(cxt)) {
		struct fdisk_label *l = fdisk_get_label(cxt, NULL),
				   *p = fdisk_get_label(fdisk_get_parent(cxt), NULL);

		fdisk_info(cxt, _("You're editing nested '%s' partition table, "
				  "primary partition table is '%s'."),
				fdisk_label_get_name(l),
				fdisk_label_get_name(p));
	}

	return 0;
}

/* Asks for command, verify the key and perform the command or
 * returns the command key if no callback for the command is
 * implemented.
 *
 * Note that this function might exchange the context pointer to
 * switch to another (nested) context.
 *
 * Returns: <0 on error
 *           0 on success (the command performed)
 *          >0 if no callback (then returns the key)
 */
int process_fdisk_menu(struct fdisk_context **cxt0)
{
	struct fdisk_context *cxt = *cxt0;
	const struct menu_entry *ent;
	const struct menu *menu;
	int key, rc;
	const char *prompt;
	char buf[BUFSIZ] = { '\0' };

	if (fdisk_is_details(cxt))
		prompt = _("Expert command (m for help): ");
	else
		prompt = _("Command (m for help): ");

	fputc('\n',stdout);
	rc = get_user_reply(prompt, buf, sizeof(buf));

	if (rc == -ECANCELED) {
		/* Map ^C and ^D in main menu to 'q' */
		if (is_interactive
		    && fdisk_label_is_changed(fdisk_get_label(cxt, NULL))) {
			/* TRANSLATORS: these yes no questions use rpmatch(),
			 * and should be translated.  */
			rc = get_user_reply(
				_("\nAll unwritten changes will be lost, do you really want to quit? (y/n)"),
				buf, sizeof(buf));
			if (rc || !rpmatch(buf))
				return 0;
		}
		key = 'q';
	} else if (rc) {
		return rc;
	} else
		key = buf[0];

	ent = get_fdisk_menu_entry(cxt, key, &menu);
	if (!ent) {
		fdisk_warnx(cxt, _("%c: unknown command"), key);
		return -EINVAL;
	}

	DBG(MENU, ul_debug("selected: key=%c, entry='%s'",
				key, ent->title));

	/* menu has implemented callback, use it */
	if (menu->callback)
		rc = menu->callback(cxt0, menu, ent);
	else {
		DBG(MENU, ul_debug("no callback for key '%c'", key));
		rc = -EINVAL;
	}

	DBG(MENU, ul_debug("process menu done [rc=%d]", rc));
	return rc;
}

static int script_read(struct fdisk_context *cxt)
{
	struct fdisk_script *sc = NULL;
	char *filename = NULL;
	int rc;

	rc = fdisk_ask_string(cxt, _("Enter script file name"), &filename);
	if (rc)
		return rc;

	errno = 0;
	sc = fdisk_new_script_from_file(cxt, filename);
	if (!sc && errno)
		fdisk_warn(cxt, _("Cannot open %s"), filename);
	else if (!sc)
		fdisk_warnx(cxt, _("Failed to parse script file %s"), filename);
	else if (fdisk_apply_script(cxt, sc) != 0) {
		fdisk_warnx(cxt, _("Failed to apply script %s"), filename);
		fdisk_warnx(cxt, _("Resetting fdisk!"));
		rc = fdisk_reassign_device(cxt);
                if (rc == 0 && !fdisk_has_label(cxt)) {
                        fdisk_info(cxt, _("Device does not contain a recognized partition table."));
                        rc = fdisk_create_disklabel(cxt, NULL);
		}
	} else
		fdisk_info(cxt, _("Script successfully applied."));

	fdisk_unref_script(sc);
	free(filename);
	return rc;
}

static int script_write(struct fdisk_context *cxt)
{
	struct fdisk_script *sc = NULL;
	char *filename = NULL;
	FILE *f = NULL;
	int rc;

	rc = fdisk_ask_string(cxt, _("Enter script file name"), &filename);
	if (rc)
		return rc;

	sc = fdisk_new_script(cxt);
	if (!sc) {
		fdisk_warn(cxt, _("Failed to allocate script handler"));
		goto done;
	}

	rc = fdisk_script_read_context(sc, NULL);
	if (rc) {
		fdisk_warnx(cxt, _("Failed to transform disk layout into script"));
		goto done;
	}

	f = fopen(filename, "w");
	if (!f) {
		fdisk_warn(cxt, _("Cannot open %s"), filename);
		goto done;
	}

	rc = fdisk_script_write_file(sc, f);
	if (rc)
		fdisk_warn(cxt, _("Failed to write script %s"), filename);
	else
		fdisk_info(cxt, _("Script successfully saved."));
done:
	if (f)
		fclose(f);
	fdisk_unref_script(sc);
	free(filename);
	return rc;
}

static int ask_for_wipe(struct fdisk_context *cxt, size_t partno)
{
	struct fdisk_partition *tmp = NULL;
	char *fstype = NULL;
	int rc, yes = 0;

	rc = fdisk_get_partition(cxt, partno, &tmp);
	if (rc)
		goto done;

	rc = fdisk_partition_to_string(tmp, cxt, FDISK_FIELD_FSTYPE, &fstype);
	if (rc || fstype == NULL)
		goto done;

	fdisk_warnx(cxt, _("Partition #%zu contains a %s signature."), partno + 1, fstype);

	if (pwipemode == WIPEMODE_AUTO && isatty(STDIN_FILENO))
		fdisk_ask_yesno(cxt, _("Do you want to remove the signature?"), &yes);
	else if (pwipemode == WIPEMODE_ALWAYS)
		yes = 1;

	if (yes) {
		fdisk_info(cxt, _("The signature will be removed by a write command."));
		rc = fdisk_wipe_partition(cxt, partno, TRUE);
	}
done:
	fdisk_unref_partition(tmp);
	free(fstype);
	return rc;
}

/*
 * Basic fdisk actions
 */
static int generic_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	int rc = 0;
	size_t n;

	/* actions shared between expert and normal mode */
	switch (ent->key) {
	case 'p':
		list_disk_geometry(cxt);
		list_disklabel(cxt);
		break;
	case 'w':
		if (fdisk_is_readonly(cxt)) {
			fdisk_warnx(cxt, _("Device is open in read-only mode."));
			break;
		}
		rc = fdisk_write_disklabel(cxt);
		if (rc)
			err(EXIT_FAILURE, _("failed to write disklabel"));

		fdisk_info(cxt, _("The partition table has been altered."));
		if (fdisk_get_parent(cxt))
			break; /* nested PT, don't leave */

		if (device_is_used)
			rc = fdisk_reread_changes(cxt, original_layout);
		else
			rc = fdisk_reread_partition_table(cxt);
		if (!rc)
			rc = fdisk_deassign_device(cxt, 0);
		/* fallthrough */
	case 'q':
		fdisk_unref_context(cxt);
		fputc('\n', stdout);
		exit(rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
	case 'm':
		rc = print_fdisk_menu(cxt);
		break;
	case 'v':
		rc = fdisk_verify_disklabel(cxt);
		break;
	case 'i':
		rc = print_partition_info(cxt);
		break;
	case 'F':
		list_freespace(cxt);
		break;
	}

	/* expert mode */
	if (ent->expert) {
		switch (ent->key) {
		case 'd':
			dump_firstsector(cxt);
			break;
		case 'D':
			dump_disklabel(cxt);
			break;
		case 'f':
			rc = fdisk_reorder_partitions(cxt);
			break;
		case 'r':
			rc = fdisk_enable_details(cxt, 0);
			break;
		}
		return rc;
	}

	/* normal mode */
	switch (ent->key) {
	case 'd':
		rc = fdisk_ask_partnum(cxt, &n, FALSE);
		if (rc)
			break; /* no partitions yet (or ENOMEM, ...) */

		rc = fdisk_delete_partition(cxt, n);
		if (rc)
			fdisk_warnx(cxt, _("Could not delete partition %zu"), n + 1);
		else
			fdisk_info(cxt, _("Partition %zu has been deleted."), n + 1);
		break;
	case 'I':
		script_read(cxt);
		break;
	case 'O':
		script_write(cxt);
		break;
	case 'l':
		list_partition_types(cxt);
		break;
	case 'n':
	{
		size_t partno;
		rc = fdisk_add_partition(cxt, NULL, &partno);
		if (!rc)
			rc = ask_for_wipe(cxt, partno);
		break;
	}
	case 'e':
		resize_partition(cxt);
		break;
	case 't':
		change_partition_type(cxt);
		break;
	case 'u':
		fdisk_set_unit(cxt,
			fdisk_use_cylinders(cxt) ? "sectors" :
							   "cylinders");
		if (fdisk_use_cylinders(cxt))
			fdisk_info(cxt, _("Changing display/entry units to cylinders (DEPRECATED!)."));
		else
			fdisk_info(cxt, _("Changing display/entry units to sectors."));
		break;
	case 'x':
		fdisk_enable_details(cxt, 1);
		break;
	case 'r':
		/* return from nested BSD to DOS or MBR to GPT */
		if (fdisk_get_parent(cxt)) {
			*cxt0 = fdisk_get_parent(cxt);

			fdisk_info(cxt, _("Leaving nested disklabel."));
			fdisk_unref_context(cxt);
		}
		break;
	}

	return rc;
}


/*
 * This is fdisk frontend for GPT specific libfdisk functions that
 * are not exported by generic libfdisk API.
 */
static int gpt_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	struct fdisk_context *mbr;
	struct fdisk_partition *pa = NULL;
	size_t n;
	int rc = 0;
	uintmax_t length = 0;

	assert(cxt);
	assert(ent);
	assert(fdisk_is_label(cxt, GPT));

	DBG(MENU, ul_debug("enter GPT menu"));

	if (ent->expert) {
		switch (ent->key) {
		case 'i':
			return fdisk_set_disklabel_id(cxt);
		case 'l':
	                rc =  fdisk_ask_number(cxt, 1, fdisk_get_npartitions(cxt),
	                                ~(uint32_t)0, _("New maximum entries"), &length);
			if (rc)
				return rc;
			return fdisk_gpt_set_npartitions(cxt, (uint32_t) length);
		case 'M':
			mbr = fdisk_new_nested_context(cxt, "dos");
			if (!mbr)
				return -ENOMEM;
			*cxt0 = cxt = mbr;
			if (fdisk_is_details(cxt))
				fdisk_enable_details(cxt, 1);	/* keep us in expert mode */
			fdisk_info(cxt, _("Entering protective/hybrid MBR disklabel."));
			return 0;
		}

		/* actions where is necessary partnum */
		rc = fdisk_ask_partnum(cxt, &n, FALSE);
		if (rc)
			return rc;

		switch(ent->key) {
		case 'u':
			pa = fdisk_new_partition();	/* new template */
			if (!pa)
				rc = -ENOMEM;
			else {
				char *str = NULL;
				rc = fdisk_ask_string(cxt, _("New UUID (in 8-4-4-4-12 format)"), &str);
				if (!rc)
					rc = fdisk_partition_set_uuid(pa, str);
				if (!rc)
					rc = fdisk_set_partition(cxt, n, pa);
				free(str);
				fdisk_unref_partition(pa);
			}
			break;
		case 'n':
			pa = fdisk_new_partition();	/* new template */
			if (!pa)
				rc = -ENOMEM;
			else {
				char *str = NULL;
				rc = fdisk_ask_string(cxt, _("New name"), &str);
				if (!rc)
					rc = fdisk_partition_set_name(pa, str);
				if (!rc)
					rc = fdisk_set_partition(cxt, n, pa);
				free(str);
				fdisk_unref_partition(pa);
			}
			break;
		case 'A':
			rc = fdisk_toggle_partition_flag(cxt, n, GPT_FLAG_LEGACYBOOT);
			break;
		case 'B':
			rc = fdisk_toggle_partition_flag(cxt, n, GPT_FLAG_NOBLOCK);
			break;
		case 'R':
			rc = fdisk_toggle_partition_flag(cxt, n, GPT_FLAG_REQUIRED);
			break;
		case 'S':
			rc = fdisk_toggle_partition_flag(cxt, n, GPT_FLAG_GUIDSPECIFIC);
			break;
		}
	}

	return rc;
}


/*
 * This is fdisk frontend for MBR specific libfdisk functions that
 * are not exported by generic libfdisk API.
 */
static int dos_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	int rc = 0;

	DBG(MENU, ul_debug("enter DOS menu"));

	if (!ent->expert) {
		switch (ent->key) {
		case 'a':
		{
			size_t n;
			rc = fdisk_ask_partnum(cxt, &n, FALSE);
			if (!rc)
				rc = fdisk_toggle_partition_flag(cxt, n, DOS_FLAG_ACTIVE);
			break;
		}
		case 'b':
		{
			struct fdisk_context *bsd
					= fdisk_new_nested_context(cxt, "bsd");
			if (!bsd)
				return -ENOMEM;
			if (!fdisk_has_label(bsd))
				rc = fdisk_create_disklabel(bsd, "bsd");
			if (rc)
				fdisk_unref_context(bsd);
			else {
				*cxt0 = cxt = bsd;
				fdisk_info(cxt,	_("Entering nested BSD disklabel."));
			}
			break;
		}
		case 'c':
			toggle_dos_compatibility_flag(cxt);
			break;
		}
		return rc;
	}

	/* expert mode */
	switch (ent->key) {
	case 'b':
	{
		size_t n;
		rc = fdisk_ask_partnum(cxt, &n, FALSE);
		if (!rc)
			rc = fdisk_dos_move_begin(cxt, n);
		break;
	}
	case 'i':
		rc = fdisk_set_disklabel_id(cxt);
		break;
	case 'M':
		/* return from nested MBR to GPT (backward compatibility only) */
		if (fdisk_get_parent(cxt)) {
			*cxt0 = fdisk_get_parent(cxt);

			fdisk_info(cxt, _("Leaving nested disklabel."));
			fdisk_unref_context(cxt);
		}
		break;
	case 'F':
		rc = fdisk_dos_fix_chs(cxt);
		if (rc)
			fdisk_info(cxt, _("C/H/S values fixed."));
		else
			fdisk_info(cxt, _("Nothing to do. C/H/S values are correct already."));
		break;
	}
	return rc;
}

static int sun_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	int rc = 0;

	DBG(MENU, ul_debug("enter SUN menu"));

	assert(cxt);
	assert(ent);
	assert(fdisk_is_label(cxt, SUN));

	DBG(MENU, ul_debug("enter SUN menu"));

	/* normal mode */
	if (!ent->expert) {
		size_t n;

		rc = fdisk_ask_partnum(cxt, &n, FALSE);
		if (rc)
			return rc;
		switch (ent->key) {
		case 'a':
			rc = fdisk_toggle_partition_flag(cxt, n, SUN_FLAG_RONLY);
			break;
		case 'c':
			rc = fdisk_toggle_partition_flag(cxt, n, SUN_FLAG_UNMNT);
			break;
		}
		return rc;
	}

	/* expert mode */
	switch (ent->key) {
	case 'a':
		rc = fdisk_sun_set_alt_cyl(cxt);
		break;
	case 'e':
		rc = fdisk_sun_set_xcyl(cxt);
		break;
	case 'i':
		rc = fdisk_sun_set_ilfact(cxt);
		break;
	case 'o':
		rc = fdisk_sun_set_rspeed(cxt);
		break;
	case 'y':
		rc = fdisk_sun_set_pcylcount(cxt);
		break;
	}
	return rc;
}

static int sgi_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	int rc = -EINVAL;
	size_t n = 0;

	DBG(MENU, ul_debug("enter SGI menu"));

	assert(cxt);
	assert(ent);
	assert(fdisk_is_label(cxt, SGI));

	if (ent->expert)
		return rc;

	switch (ent->key) {
	case 'a':
		rc = fdisk_ask_partnum(cxt, &n, FALSE);
		if (!rc)
			rc = fdisk_toggle_partition_flag(cxt, n, SGI_FLAG_BOOT);
		break;
	case 'b':
		fdisk_sgi_set_bootfile(cxt);
		break;
	case 'c':
		rc = fdisk_ask_partnum(cxt, &n, FALSE);
		if (!rc)
			rc = fdisk_toggle_partition_flag(cxt, n, SGI_FLAG_SWAP);
		break;
	case 'i':
		rc = fdisk_sgi_create_info(cxt);
		break;
	}

	return rc;
}

/*
 * This is fdisk frontend for BSD specific libfdisk functions that
 * are not exported by generic libfdisk API.
 */
static int bsd_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	int rc = 0, org;

	assert(cxt);
	assert(ent);
	assert(fdisk_is_label(cxt, BSD));

	DBG(MENU, ul_debug("enter BSD menu"));

	switch(ent->key) {
	case 'e':
		rc = fdisk_bsd_edit_disklabel(cxt);
		break;
	case 'i':
		rc = fdisk_bsd_write_bootstrap(cxt);
		break;
	case 's':
		org = fdisk_is_details(cxt);

		fdisk_enable_details(cxt, 1);
		list_disklabel(cxt);
		fdisk_enable_details(cxt, org);
		break;
	case 'x':
		rc = fdisk_bsd_link_partition(cxt);
		break;
	}
	return rc;
}

/* C/H/S commands
 *
 * The geometry setting from this dialog is not persistent and maybe reset by
 * fdisk_reset_device_properties() (for example when you create a new disk
 * label). Note that on command line specified -C/-H/-S setting is persistent
 * as it's based on fdisk_save_user_geometry().
 */
static int geo_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	struct fdisk_label *lb = fdisk_get_label(cxt, NULL);
	int rc = -EINVAL;
	uintmax_t c = 0, h = 0, s = 0;
	fdisk_sector_t mi, ma;

	DBG(MENU, ul_debug("enter GEO menu"));

	assert(cxt);
	assert(ent);

	/* default */
	if (!lb)
		lb = fdisk_get_label(cxt, "dos");

	switch (ent->key) {
	case 'c':
		fdisk_label_get_geomrange_cylinders(lb, &mi, &ma);
		rc =  fdisk_ask_number(cxt, mi, fdisk_get_geom_cylinders(cxt),
				ma, _("Number of cylinders"), &c);
		break;
	case 'h':
	{
		unsigned int i, a;
		fdisk_label_get_geomrange_heads(lb, &i, &a);
		rc =  fdisk_ask_number(cxt, i, fdisk_get_geom_heads(cxt),
				a, _("Number of heads"), &h);
		break;
	}
	case 's':
		fdisk_label_get_geomrange_sectors(lb, &mi, &ma);
		rc =  fdisk_ask_number(cxt, mi, fdisk_get_geom_sectors(cxt),
				ma, _("Number of sectors"), &s);
		break;
	}

	if (!rc)
		fdisk_override_geometry(cxt, c, h, s);
	return rc;
}

static int createlabel_menu_cb(struct fdisk_context **cxt0,
		       const struct menu *menu __attribute__((__unused__)),
		       const struct menu_entry *ent)
{
	struct fdisk_context *cxt = *cxt0;
	const char *wanted = NULL;
	int rc = -EINVAL;

	DBG(MENU, ul_debug("enter Create label menu"));

	assert(cxt);
	assert(ent);

	if (ent->expert) {
		switch (ent->key) {
		case 'g':
			/* Deprecated, use 'G' in main menu, just for backward
			 * compatibility only. */
			wanted = "sgi";
			break;
		}
	} else {
		switch (ent->key) {
			case 'g':
				wanted = "gpt";
				break;
			case 'G':
				wanted = "sgi";
				break;
			case 'o':
				wanted = "dos";
				break;
			case 's':
				wanted = "sun";
				break;
		}
	}

	if (wanted) {
		rc = fdisk_create_disklabel(cxt, wanted);
		if (rc) {
			errno = -rc;
			fdisk_warn(cxt, _("Failed to create '%s' disk label"), wanted);
		}
	}
	if (rc == 0 && fdisk_get_collision(cxt))
		follow_wipe_mode(cxt);

	return rc;
}
