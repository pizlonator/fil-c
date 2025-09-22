
#include "fdiskP.h"


/**
 * SECTION: label
 * @title: Label
 * @short_description: disk label (PT) specific data and functions
 *
 * The fdisk_new_context() initializes all label drivers, and allocate
 * per-label specific data struct. This concept can be used to store label specific
 * settings to the label driver independently on the currently active label
 * driver. Note that label struct cannot be deallocated, so there is no
 * reference counting for fdisk_label objects. All is destroyed by
 * fdisk_unref_context() only.
 *
 * Anyway, all label drives share in-memory first sector. The function
 * fdisk_create_disklabel() overwrites this in-memory sector. But it's possible that
 * label driver also uses another buffers, for example GPT reads more sectors
 * from the device.
 *
 * All label operations are in-memory only, except fdisk_write_disklabel().
 *
 * All functions that use "struct fdisk_context" rather than "struct
 * fdisk_label" use the currently active label driver.
 */


int fdisk_probe_labels(struct fdisk_context *cxt)
{
	size_t i;

	cxt->label = NULL;

	for (i = 0; i < cxt->nlabels; i++) {
		struct fdisk_label *lb = cxt->labels[i];
		struct fdisk_label *org = fdisk_get_label(cxt, NULL);
		int rc;

		if (!lb->op->probe)
			continue;
		if (lb->disabled) {
			DBG(CXT, ul_debugobj(cxt, "%s: disabled -- ignore", lb->name));
			continue;
		}
		DBG(CXT, ul_debugobj(cxt, "probing for %s", lb->name));

		cxt->label = lb;
		rc = lb->op->probe(cxt);
		cxt->label = org;

		if (rc != 1) {
			if (lb->op->deinit)
				lb->op->deinit(lb);	/* for sure */
			continue;
		}

		__fdisk_switch_label(cxt, lb);
		return 0;
	}

	DBG(CXT, ul_debugobj(cxt, "no label found"));
	return 1; /* not found */
}

/**
 * fdisk_label_get_name:
 * @lb: label
 *
 * Returns: label name
 */
const char *fdisk_label_get_name(const struct fdisk_label *lb)
{
	return lb ? lb->name : NULL;
}

/**
 * fdisk_label_is_labeltype:
 * @lb: label
 *
 * Returns: FDISK_DISKLABEL_*.
 */
int fdisk_label_get_type(const struct fdisk_label *lb)
{
	return lb->id;
}

/**
 * fdisk_label_require_geometry:
 * @lb: label
 *
 * Returns: 1 if label requires CHS geometry
 */
int fdisk_label_require_geometry(const struct fdisk_label *lb)
{
	assert(lb);

	return lb->flags & FDISK_LABEL_FL_REQUIRE_GEOMETRY ? 1 : 0;
}

/**
 * fdisk_label_get_fields_ids
 * @lb: label (or NULL for the current label)
 * @cxt: context
 * @ids: returns allocated array with FDISK_FIELD_* IDs
 * @nids: returns number of items in fields
 *
 * This function returns the default fields for the label.
 *
 * Note that the set of the default fields depends on fdisk_enable_details()
 * function. If the details are enabled then this function usually returns more
 * fields.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_label_get_fields_ids(
		const struct fdisk_label *lb,
		struct fdisk_context *cxt,
		int **ids, size_t *nids)
{
	size_t i, n;
	int *c;

	if (!cxt || (!lb && !cxt->label))
		return -EINVAL;

	lb = cxt->label;
	if (!lb->fields || !lb->nfields)
		return -ENOSYS;
	c = calloc(lb->nfields, sizeof(int));
	if (!c)
		return -ENOMEM;
	for (n = 0, i = 0; i < lb->nfields; i++) {
		int id = lb->fields[i].id;

		if ((fdisk_is_details(cxt) &&
				(lb->fields[i].flags & FDISK_FIELDFL_EYECANDY))
		     || (!fdisk_is_details(cxt) &&
				(lb->fields[i].flags & FDISK_FIELDFL_DETAIL))
		     || (id == FDISK_FIELD_SECTORS &&
			         fdisk_use_cylinders(cxt))
		     || (id == FDISK_FIELD_CYLINDERS &&
			         !fdisk_use_cylinders(cxt)))
			continue;

		c[n++] = id;
	}
	if (ids)
		*ids = c;
	else
		free(c);
	if (nids)
		*nids = n;
	return 0;
}

/**
 * fdisk_label_get_fields_ids_all
 * @lb: label (or NULL for the current label)
 * @cxt: context
 * @ids: returns allocated array with FDISK_FIELD_* IDs
 * @nids: returns number of items in fields
 *
 * This function returns all fields for the label.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_label_get_fields_ids_all(
		const struct fdisk_label *lb,
		struct fdisk_context *cxt,
		int **ids, size_t *nids)
{
	size_t i, n;
	int *c;

	if (!cxt || (!lb && !cxt->label))
		return -EINVAL;

	lb = cxt->label;
	if (!lb->fields || !lb->nfields)
		return -ENOSYS;
	c = calloc(lb->nfields, sizeof(int));
	if (!c)
		return -ENOMEM;
	for (n = 0, i = 0; i < lb->nfields; i++)
		c[n++] = lb->fields[i].id;
	if (ids)
		*ids = c;
	else
		free(c);
	if (nids)
		*nids = n;
	return 0;
}

/**
 * fdisk_label_get_field:
 * @lb: label
 * @id: FDISK_FIELD_*
 *
 * The field struct describes data stored in struct fdisk_partition. The info
 * about data is usable for example to generate human readable output (e.g.
 * fdisk 'p'rint command). See fdisk_partition_to_string() and fdisk code.
 *
 * Returns: pointer to static instance of the field.
 */
const struct fdisk_field *fdisk_label_get_field(const struct fdisk_label *lb, int id)
{
	size_t i;

	assert(lb);
	assert(id > 0);

	for (i = 0; i < lb->nfields; i++) {
		if (lb->fields[i].id == id)
			return &lb->fields[i];
	}

	return NULL;
}

/**
 * fdisk_label_get_field_by_name
 * @lb: label
 * @name: field name
 *
 * Returns: pointer to static instance of the field.
 */
const struct fdisk_field *fdisk_label_get_field_by_name(
				const struct fdisk_label *lb,
				const char *name)
{
	size_t i;

	assert(lb);
	assert(name);

	for (i = 0; i < lb->nfields; i++) {
		if (lb->fields[i].name && strcasecmp(lb->fields[i].name, name) == 0)
			return &lb->fields[i];
	}

	return NULL;
}

/**
 * fdisk_write_disklabel:
 * @cxt: fdisk context
 *
 * This function wipes the device (if enabled by fdisk_enable_wipe()) and then
 * it writes in-memory changes to disk. Be careful!
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_write_disklabel(struct fdisk_context *cxt)
{
	if (!cxt || !cxt->label || cxt->readonly)
		return -EINVAL;
	if (!cxt->label->op->write)
		return -ENOSYS;

	fdisk_do_wipe(cxt);
	return cxt->label->op->write(cxt);
}

/**
 * fdisk_verify_disklabel:
 * @cxt: fdisk context
 *
 * Verifies the partition table.
 *
 * Returns: 0 on success, <1 runtime or option errors, >0 number of detected issues
 */
int fdisk_verify_disklabel(struct fdisk_context *cxt)
{
	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->verify)
		return -ENOSYS;
	if (fdisk_missing_geometry(cxt))
		return -EINVAL;

	return cxt->label->op->verify(cxt);
}

/**
 * fdisk_list_disklabel:
 * @cxt: fdisk context
 *
 * Lists details about disklabel, but no partitions.
 *
 * This function is based on fdisk_get_disklabel_item() and prints all label
 * specific information by ASK interface (FDISK_ASKTYPE_INFO, aka fdisk_info()).
 * The function requires enabled "details" by fdisk_enable_details().
 *
 * It's recommended to use fdisk_get_disklabel_item() if you need better
 * control on output and formatting.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_list_disklabel(struct fdisk_context *cxt)
{
	int id = 0, rc = 0;
	struct fdisk_labelitem item = { .id = id };

	if (!cxt || !cxt->label)
		return -EINVAL;

	if (!cxt->display_details)
		return 0;

	/* List all label items */
	do {
		/* rc: < 0 error, 0 success, 1 unknown item, 2 out of range */
		rc = fdisk_get_disklabel_item(cxt, id++, &item);
		if (rc != 0)
			continue;
		switch (item.type) {
		case 'j':
			fdisk_info(cxt, "%s: %ju", item.name, item.data.num64);
			break;
		case 's':
			if (item.data.str && item.name)
				fdisk_info(cxt, "%s: %s", item.name, item.data.str);
			break;
		}
		fdisk_reset_labelitem(&item);
	} while (rc == 0 || rc == 1);

	return rc < 0 ? rc : 0;
}

/**
 * fdisk_create_disklabel:
 * @cxt: fdisk context
 * @name: label name
 *
 * Creates a new disk label of type @name. If @name is NULL, then it will
 * create a default system label type, either SUN or DOS. The function
 * automatically switches the current label driver to @name. The function
 * fdisk_get_label() returns the current label driver.
 *
 * The function modifies in-memory data only.
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_create_disklabel(struct fdisk_context *cxt, const char *name)
{
	int haslabel = 0;
	struct fdisk_label *lb;

	if (!cxt)
		return -EINVAL;

	if (!name) { /* use default label creation */
#ifdef __sparc__
		name = "sun";
#else
		name = "dos";
#endif
	}

	if (cxt->label) {
		fdisk_deinit_label(cxt->label);
		haslabel = 1;
	}

	lb = fdisk_get_label(cxt, name);
	if (!lb || lb->disabled)
		return -EINVAL;

	if (!haslabel || (lb && cxt->label != lb))
		fdisk_check_collisions(cxt);

	if (!lb->op->create)
		return -ENOSYS;

	__fdisk_switch_label(cxt, lb);
	assert(cxt->label == lb);

	if (haslabel && !cxt->parent)
		fdisk_reset_device_properties(cxt);

	DBG(CXT, ul_debugobj(cxt, "create a new %s label", lb->name));
	return lb->op->create(cxt);
}

/**
 * fdisk_locate_disklabel:
 * @cxt: context
 * @n: N item
 * @name: return item name
 * @offset: return offset where is item
 * @size: of the item
 *
 * Locate disklabel and returns info about @n item of the label.
 *
 * For example GPT is composed from three items, PMBR and GPT, n=0 return
 * offset to PMBR and n=1 return offset to GPT Header and n=2 returns offset to
 * GPT array of partitions, n=3 and n=4 returns location of the backup GPT
 * label at the end of the disk.
 *
 * The function returns the current in-memory situation. It's possible that a
 * header location is modified by write operation, for example when enabled
 * minimization (see fdisk_gpt_enable_minimize()). In this case it's better to
 * call this function after fdisk_write_disklabel().
 *
 * For more details see 'D' expert fdisk command.
 *
 * Returns: 0 on success, <0 on error, 1 no more items.
 */
int fdisk_locate_disklabel(struct fdisk_context *cxt, int n, const char **name,
			   uint64_t *offset, size_t *size)
{
	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->locate)
		return -ENOSYS;

	DBG(CXT, ul_debugobj(cxt, "locating %d chunk of %s.", n, cxt->label->name));
	return cxt->label->op->locate(cxt, n, name, offset, size);
}


/**
 * fdisk_get_disklabel_id:
 * @cxt: fdisk context
 * @id: returns pointer to allocated string (MBR Id or GPT dirk UUID)
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_get_disklabel_id(struct fdisk_context *cxt, char **id)
{
	struct fdisk_labelitem item = FDISK_LABELITEM_INIT;
	int rc;

	if (!cxt || !cxt->label || !id)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "asking for disk %s ID", cxt->label->name));

	rc = fdisk_get_disklabel_item(cxt, FDISK_LABELITEM_ID, &item);
	if (rc == 0) {
		*id = item.data.str;
		item.data.str = NULL;
	}
	fdisk_reset_labelitem(&item);
	if (rc > 0)
		rc = 0;
	return rc;
}

/**
 * fdisk_get_disklabel_item:
 * @cxt: fdisk context
 * @id: item ID (FDISK_LABELITEM_* or *_LABELITEM_*)
 * @item: specifies and returns the item
 *
 * Note that @id is always in range 0..N. It's fine to use the function in loop
 * until it returns error or 2, the result in @item should be ignored when
 * function returns 1. Don't forget to use fdisk_reset_labelitem() or fdisk_unref_labelitem().
 *
 * Returns: 0 on success, < 0 on error, 1 on unsupported item, 2 id out of range
 */
int fdisk_get_disklabel_item(struct fdisk_context *cxt, int id, struct fdisk_labelitem *item)
{
	if (!cxt || !cxt->label || !item)
		return -EINVAL;

	fdisk_reset_labelitem(item);
	item->id = id;
	DBG(CXT, ul_debugobj(cxt, "asking for disk %s item %d", cxt->label->name, item->id));

	if (!cxt->label->op->get_item)
		return -ENOSYS;

	return cxt->label->op->get_item(cxt, item);
}

/**
 * fdisk_set_disklabel_id:
 * @cxt: fdisk context
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_set_disklabel_id(struct fdisk_context *cxt)
{
	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->set_id)
		return -ENOSYS;

	DBG(CXT, ul_debugobj(cxt, "setting %s disk ID", cxt->label->name));
	return cxt->label->op->set_id(cxt, NULL);
}

/**
 * fdisk_set_disklabel_id_from_string
 * @cxt: fdisk context
 * @str: new Id
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 *
 * Since: 2.36
 */
int fdisk_set_disklabel_id_from_string(struct fdisk_context *cxt, const char *str)
{
	if (!cxt || !cxt->label || !str)
		return -EINVAL;
	if (!cxt->label->op->set_id)
		return -ENOSYS;

	DBG(CXT, ul_debugobj(cxt, "setting %s disk ID from '%s'", cxt->label->name, str));
	return cxt->label->op->set_id(cxt, str);
}

/**
 * fdisk_set_partition_type:
 * @cxt: fdisk context
 * @partnum: partition number
 * @t: new type
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_set_partition_type(struct fdisk_context *cxt,
			     size_t partnum,
			     struct fdisk_parttype *t)
{
	if (!cxt || !cxt->label || !t)
		return -EINVAL;


	if (cxt->label->op->set_part) {
		struct fdisk_partition *pa = fdisk_new_partition();
		int rc;

		if (!pa)
			return -ENOMEM;
		fdisk_partition_set_type(pa, t);

		DBG(CXT, ul_debugobj(cxt, "partition: %zd: set type", partnum));
		rc = cxt->label->op->set_part(cxt, partnum, pa);
		fdisk_unref_partition(pa);
		return rc;
	}

	return -ENOSYS;
}


/**
 * fdisk_toggle_partition_flag:
 * @cxt: fdisk context
 * @partnum: partition number
 * @flag: flag ID
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_toggle_partition_flag(struct fdisk_context *cxt,
			       size_t partnum,
			       unsigned long flag)
{
	int rc;

	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->part_toggle_flag)
		return -ENOSYS;

	rc = cxt->label->op->part_toggle_flag(cxt, partnum, flag);

	DBG(CXT, ul_debugobj(cxt, "partition: %zd: toggle: 0x%04lx [rc=%d]", partnum, flag, rc));
	return rc;
}

/**
 * fdisk_reorder_partitions
 * @cxt: fdisk context
 *
 * Sort partitions according to the partition start sector.
 *
 * Returns: 0 on success, 1 reorder unnecessary, otherwise a corresponding error.
 */
int fdisk_reorder_partitions(struct fdisk_context *cxt)
{
	int rc;

	if (!cxt || !cxt->label)
		return -EINVAL;
	if (!cxt->label->op->reorder)
		return -ENOSYS;

	rc = cxt->label->op->reorder(cxt);

	switch (rc) {
	case 0:
		fdisk_info(cxt, _("Partitions order fixed."));
		break;
	case 1:
		fdisk_info(cxt, _("Nothing to do. Ordering is correct already."));
		break;
	default:
		fdisk_warnx(cxt, _("Failed to fix partitions order."));
		break;
	}

	return rc;
}

/*
 * Resets the current used label driver to initial state
 */
void fdisk_deinit_label(struct fdisk_label *lb)
{
	assert(lb);

	/* private label information */
	if (lb->op->deinit)
		lb->op->deinit(lb);
}

/**
 * fdisk_label_set_changed:
 * @lb: label
 * @changed: 0/1
 *
 * Marks in-memory data as changed, to force fdisk_write_disklabel() to write
 * to device. This should be unnecessary by default, the library keeps track
 * about changes.
 */
void fdisk_label_set_changed(struct fdisk_label *lb, int changed)
{
	assert(lb);
	lb->changed = changed ? 1 : 0;
}

/**
 * fdisk_label_is_changed:
 * @lb: label
 *
 * Returns: 1 if in-memory data has been changed.
 */
int fdisk_label_is_changed(const struct fdisk_label *lb)
{
	return lb ? lb->changed : 0;
}

/**
 * fdisk_label_set_disabled:
 * @lb: label
 * @disabled: 0 or 1
 *
 * Mark label as disabled, then libfdisk is going to ignore the label when
 * probe device for labels.
 */
void fdisk_label_set_disabled(struct fdisk_label *lb, int disabled)
{
	assert(lb);

	DBG(LABEL, ul_debug("%s label %s",
				lb->name,
				disabled ? "DISABLED" : "ENABLED"));
	lb->disabled = disabled ? 1 : 0;
}

/**
 * fdisk_label_is_disabled:
 * @lb: label
 *
 * Returns: 1 if label driver disabled.
 */
int fdisk_label_is_disabled(const struct fdisk_label *lb)
{
	assert(lb);
	return lb ? lb->disabled : 0;
}

/**
 * fdisk_label_get_geomrange_sectors:
 * @lb: label
 * @mi: minimal number
 * @ma: maximal number
 *
 * The function provides minimal and maximal geometry supported for the label,
 * if no range defined by library then returns -ENOSYS.
 *
 * Since: 2.32
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_label_get_geomrange_sectors(const struct fdisk_label *lb,
					fdisk_sector_t *mi, fdisk_sector_t *ma)
{
	if (!lb || lb->geom_min.sectors == 0)
		return -ENOSYS;
	if (mi)
		*mi = lb->geom_min.sectors;
	if (ma)
		*ma = lb->geom_max.sectors;
	return 0;
}

/**
 * fdisk_label_get_geomrange_heads:
 * @lb: label
 * @mi: minimal number
 * @ma: maximal number
 *
 * The function provides minimal and maximal geometry supported for the label,
 * if no range defined by library then returns -ENOSYS.
 *
 * Since: 2.32
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_label_get_geomrange_heads(const struct fdisk_label *lb,
					unsigned int *mi, unsigned int *ma)
{
	if (!lb || lb->geom_min.heads == 0)
		return -ENOSYS;
	if (mi)
		*mi = lb->geom_min.heads;
	if (ma)
		*ma = lb->geom_max.heads;
	return 0;
}

/**
 * fdisk_label_get_geomrange_cylinders:
 * @lb: label
 * @mi: minimal number
 * @ma: maximal number
 *
 * The function provides minimal and maximal geometry supported for the label,
 * if no range defined by library then returns -ENOSYS.
 *
 * Since: 2.32
 *
 * Returns: 0 on success, otherwise, a corresponding error.
 */
int fdisk_label_get_geomrange_cylinders(const struct fdisk_label *lb,
					fdisk_sector_t *mi, fdisk_sector_t *ma)
{
	if (!lb || lb->geom_min.cylinders == 0)
		return -ENOSYS;
	if (mi)
		*mi = lb->geom_min.cylinders;
	if (ma)
		*ma = lb->geom_max.cylinders;
	return 0;
}

