
#include <inttypes.h>

#include "fdiskP.h"

/**
 * SECTION: item
 * @title: Labelitem
 * @short_description: disk label items
 *
 * The labelitem is label specific items stored in the partition table header.
 * The information provided by labelitems are not specific to the partitions.
 *
 * For example
 *
 * <informalexample>
 *  <programlisting>
 *	struct fdisk_labelitem *item = fdisk_new_labelitem();
 *
 *	fdisk_get_disklabel_item(cxt, GPT_LABELITEM_ALTLBA, item);
 *	print("Backup header LBA: %ju\n", fdisk_labelitem_get_data_u64(item));
 *
 *	fdisk_unref_labelitem(item);
 *  </programlisting>
 * </informalexample>
 *
 * returns LBA of the alternative GPT header.
 *
 * See also fdisk_get_disklabel_item(). The IDs are generic (e.g.
 * FDISK_LABEL_ITEM_*) and label specific ((e.g. GPT_LABELITEM_*).
 */

/**
 * fdisk_new_labelitem
 *
 * Returns: new instance.
 * Since: 2.29
 */
struct fdisk_labelitem *fdisk_new_labelitem(void)
{
	struct fdisk_labelitem *li = calloc(1, sizeof(*li));

	if (!li)
		return NULL;

	li->refcount = 1;
	DBG(ITEM, ul_debugobj(li, "alloc"));
	return li;
}

/**
 * fdisk_ref_labelitem:
 * @li: label item
 *
 * Increments reference counter.
 * Since: 2.29
 */
void fdisk_ref_labelitem(struct fdisk_labelitem *li)
{
	if (li) {
		/* me sure we do not use refcouting for static items */
		assert(li->refcount > 0);
		li->refcount++;
	}
}

/**
 * fdisk_reset_labelitem:
 * @li: label item
 *
 * Zeroize data stored in the @li (does not modify anything in disk label).
 *
 * Returns: 0 on success, or <0 in case of error
 * Since: 2.29
 */
int fdisk_reset_labelitem(struct fdisk_labelitem *li)
{
	int refcount;

	if (!li)
		return -EINVAL;
	if (li->type == 's')
		free(li->data.str);

	refcount = li->refcount;
	memset(li, 0, sizeof(*li));
	li->refcount = refcount;
	return 0;
}

/**
 * fdisk_unref_labelitem:
 * @li: label item
 *
 * Decrements reference counter, on zero the @li is automatically
 * deallocated.
 *
 * Since: 2.29
 */
void fdisk_unref_labelitem(struct fdisk_labelitem *li)
{
	if (!li)
		return;

	/* me sure we do not use refcouting for static items */
	assert(li->refcount > 0);

	li->refcount--;
	if (li->refcount <= 0) {
		DBG(ITEM, ul_debugobj(li, "free"));
		fdisk_reset_labelitem(li);
		free(li);
	}
}

/**
 * fdisk_labelitem_get_name:
 * @li: label item
 *
 * Returns: item name or NULL.
 * Since: 2.29
 */
const char *fdisk_labelitem_get_name(struct fdisk_labelitem *li)
{
	return li ? li->name : NULL;
}

/**
 * fdisk_labelitem_get_id:
 * @li: label item
 *
 * Returns: item Id or <0 in case of error.
 * Since: 2.29
 */
int fdisk_labelitem_get_id(struct fdisk_labelitem *li)
{
	return li ? li->id : -EINVAL;
}


/**
 * fdisk_labelitem_get_data_u64:
 * @li: label item
 * @data: returns data
 *
 * Returns: 0 on success, <0 on error
 * Since: 2.29
 */
int fdisk_labelitem_get_data_u64(struct fdisk_labelitem *li, uint64_t *data)
{
	if (!li || li->type != 'j')
		return -EINVAL;

	if (data)
		*data = li->data.num64;
	return 0;
}

/**
 * fdisk_labelitem_get_data_string:
 * @li: label item
 * @data: returns data
 *
 * Returns: 0 on success, <0 on error.
 * Since: 2.29
 */
int fdisk_labelitem_get_data_string(struct fdisk_labelitem *li, const char **data)
{
	if (!li || li->type != 's')
		return -EINVAL;

	if (data)
		*data = li->data.str;
	return 0;
}

/**
 * fdisk_labelitem_is_string:
 * @li: label item
 *
 * Returns: 0 or 1
 * Since: 2.29
 */
int fdisk_labelitem_is_string(struct fdisk_labelitem *li)
{
	return li && li->type == 's';
}

/**
 * fdisk_labelitem_is_number:
 * @li: label item
 *
 * Returns: 0 or 1
 * Since: 2.29
 */
int fdisk_labelitem_is_number(struct fdisk_labelitem *li)
{
	return li && li->type == 'j';
}

#ifdef TEST_PROGRAM
static int test_listitems(struct fdisk_test *ts __attribute__((unused)),
			  int argc, char *argv[])
{
	if (argc != 2)
		return -1;

	const char *disk = argv[1];
	struct fdisk_context *cxt;
	struct fdisk_labelitem *item;
	int i = 0, rc;

	cxt = fdisk_new_context();
	item = fdisk_new_labelitem();

	fdisk_assign_device(cxt, disk, 1);

	do {
		rc = fdisk_get_disklabel_item(cxt, i++, item);
		switch (rc) {
		case 0:	/* success */
		{
			const char *name = fdisk_labelitem_get_name(item);
			const char *str;
			uint64_t num;

			if (fdisk_labelitem_is_string(item)
			    && fdisk_labelitem_get_data_string(item, &str) == 0)
				printf("%s: %s\n", name, str);
			else if (fdisk_labelitem_get_data_u64(item, &num) == 0)
				printf("%s: %"PRIu64"\n", name, num);
			break;
		}
		case 1: /* item unsupported by label -- ignore */
			rc = 0;
			break;
		case 2:	/* end (out of range) */
			break;
		default: /* error */
			break;
		}
	} while (rc == 0);

	fdisk_unref_labelitem(item);
	fdisk_unref_context(cxt);
	return rc < 0 ? rc : 0;
}

int main(int argc, char *argv[])
{
	struct fdisk_test tss[] = {
		{ "--list-items",  test_listitems,  "<disk>             list items" },
		{ NULL }
	};

	return fdisk_run_test(tss, argc, argv);
}

#endif
