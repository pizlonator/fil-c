
#include "fdiskP.h"

/**
 * SECTION: field
 * @title: Field
 * @short_description: description of the partition fields
 *
 * The fdisk fields are static user-friendly descriptions of the partition. The
 * fields are used to avoid label specific stuff in the functions that list disk
 * partitions (e.g. fdisk -l). The field Id is the same as Id for fdisk_partition_to_string().
 *
 * <informalexample>
 *   <programlisting>
 * int *ids;
 * size_t nids;
 * struct fdisk_partition *pa = NULL;
 * struct fdisk_label *lb = fdisk_get_label(cxt, NULL);
 *
 * fdisk_label_get_fields_ids(lb, cxt, &ids, &nids);
 *
 * fdisk_get_partition(cxt, 0, &pa);
 *
 * for (i = 0; i < nids; i++) {
 *	const struct fdisk_field *field = fdisk_label_get_field(lb, ids[i]);
 *
 *	int id = fdisk_field_get_id(fl);
 *	const char *name = fdisk_field_get_name(fl);
 *	char *data;
 *
 *	fdisk_partition_to_string(pa, id, &data);
 *	printf("%s: %s\n", name, data);
 *	free(data);
 * }
 * free(ids);
 *    </programlisting>
 * </informalexample>
 *
 * This example lists all information about the first partition. It will work
 * for MBR as well as for GPT because fields are not hardcoded in the example.
 *
 * See also fdisk_label_get_field_by_name(), fdisk_label_get_fields_ids_all()
 * and fdisk_label_get_fields_ids().
 */

/**
 * fdisk_field_get_id:
 * @field: field instance
 *
 * Returns: field Id (FDISK_FIELD_*)
 */
int fdisk_field_get_id(const struct fdisk_field *field)
{
	return field ? field->id : -EINVAL;
}

/**
 * fdisk_field_get_name:
 * @field: field instance
 *
 * Returns: field name
 */
const char *fdisk_field_get_name(const struct fdisk_field *field)
{
	return field ? field->name : NULL;
}

/**
 * fdisk_field_get_width:
 * @field: field instance
 *
 * Returns: libsmartcols compatible width.
 */
double fdisk_field_get_width(const struct fdisk_field *field)
{
	return field ? field->width : -EINVAL;
}

/**
 * fdisk_field_is_number:
 * @field: field instance
 *
 * Returns: 1 if field represent number
 */
int fdisk_field_is_number(const struct fdisk_field *field)
{
	return field->flags ? field->flags & FDISK_FIELDFL_NUMBER : 0;
}
