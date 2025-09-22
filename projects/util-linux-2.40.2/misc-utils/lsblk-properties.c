
#include <blkid.h>

#ifdef HAVE_LIBUDEV
# include <libudev.h>
#endif

#include "c.h"
#include "xalloc.h"
#include "mangle.h"
#include "path.h"
#include "nls.h"
#include "strutils.h"

#include "lsblk.h"

#ifdef HAVE_LIBUDEV
static struct udev *udev;
#endif

void lsblk_device_free_properties(struct lsblk_devprop *p)
{
	if (!p)
		return;

	free(p->fstype);
	free(p->fsversion);
	free(p->uuid);
	free(p->ptuuid);
	free(p->pttype);
	free(p->label);
	free(p->parttype);
	free(p->partuuid);
	free(p->partlabel);
	free(p->partn);
	free(p->wwn);
	free(p->serial);
	free(p->model);
	free(p->partflags);
	free(p->idlink);
	free(p->revision);

	free(p->mode);
	free(p->owner);
	free(p->group);

	free(p);
}

#ifndef HAVE_LIBUDEV
static struct lsblk_devprop *get_properties_by_udev(struct lsblk_device *dev
				__attribute__((__unused__)))
{
	return NULL;
}
#else

#define LSBLK_UDEV_BYID_PREFIX "/dev/disk/by-id/"
#define LSBLK_UDEV_BYID_PREFIXSZ (sizeof(LSBLK_UDEV_BYID_PREFIX) - 1)

static struct lsblk_devprop *get_properties_by_udev(struct lsblk_device *ld)
{
	struct udev_device *dev;
	struct udev_list_entry *le;
	const char *data;
	struct lsblk_devprop *prop;
	size_t len;

	if (ld->udev_requested)
		return ld->properties;

	if (!udev)
		udev = udev_new();	/* global handler */
	if (!udev)
		goto done;

	dev = udev_device_new_from_subsystem_sysname(udev, "block", ld->name);
	if (!dev)
		goto done;

	DBG(DEV, ul_debugobj(ld, "%s: found udev properties", ld->name));

	if (ld->properties)
		lsblk_device_free_properties(ld->properties);
	prop = ld->properties = xcalloc(1, sizeof(*ld->properties));

	if ((data = udev_device_get_property_value(dev, "ID_FS_LABEL_ENC"))) {
		prop->label = xstrdup(data);
		unhexmangle_string(prop->label);
	}
	if ((data = udev_device_get_property_value(dev, "ID_FS_UUID_ENC"))) {
		prop->uuid = xstrdup(data);
		unhexmangle_string(prop->uuid);
	}
	if ((data = udev_device_get_property_value(dev, "ID_PART_TABLE_UUID")))
		prop->ptuuid = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_PART_TABLE_TYPE")))
		prop->pttype = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_PART_ENTRY_NAME"))) {
		prop->partlabel = xstrdup(data);
		unhexmangle_string(prop->partlabel);
	}
	if ((data = udev_device_get_property_value(dev, "ID_FS_TYPE")))
		prop->fstype = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_FS_VERSION")))
		prop->fsversion = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_PART_ENTRY_TYPE")))
		prop->parttype = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_PART_ENTRY_UUID")))
		prop->partuuid = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_PART_ENTRY_NUMBER")))
		prop->partn = xstrdup(data);
	if ((data = udev_device_get_property_value(dev, "ID_PART_ENTRY_FLAGS")))
		prop->partflags = xstrdup(data);

	data = udev_device_get_property_value(dev, "ID_WWN_WITH_EXTENSION");
	if (!data)
		data = udev_device_get_property_value(dev, "ID_WWN");
	if (data)
		prop->wwn = xstrdup(data);

	data = udev_device_get_property_value(dev, "SCSI_IDENT_SERIAL");	/* sg3_utils do not use I_D prefix */
	if (!data)
		data = udev_device_get_property_value(dev, "ID_SCSI_SERIAL");
	if(!data)
		data = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
	if(!data)
		data = udev_device_get_property_value(dev, "ID_SERIAL");
	if (data) {
		prop->serial = xstrdup(data);
		normalize_whitespace((unsigned char *) prop->serial);
	}

	if ((data = udev_device_get_property_value(dev, "ID_REVISION")))
		prop->revision = xstrdup(data);

	if ((data = udev_device_get_property_value(dev, "ID_MODEL_ENC"))) {
		prop->model = xstrdup(data);
		unhexmangle_string(prop->model);
		normalize_whitespace((unsigned char *) prop->model);
	} else if ((data = udev_device_get_property_value(dev, "ID_MODEL"))) {
		prop->model = xstrdup(data);
		normalize_whitespace((unsigned char *) prop->model);
	}

	/* select the shortest udev by-id symlink */
	len = 0;
	udev_list_entry_foreach(le, udev_device_get_devlinks_list_entry(dev)) {
		const char *name = udev_list_entry_get_name(le);
		size_t sz;

		if (!name || !startswith(name,  LSBLK_UDEV_BYID_PREFIX))
			continue;
		name += LSBLK_UDEV_BYID_PREFIXSZ;
		if (!*name)
			continue;
		sz = strlen(name);
		if (!len || sz < len) {
			len = sz;
			free(prop->idlink);
			prop->idlink = xstrdup(name);
		}
	}

	udev_device_unref(dev);
done:
	ld->udev_requested = 1;

	DBG(DEV, ul_debugobj(ld, " from udev"));
	return ld->properties;
}
#endif /* HAVE_LIBUDEV */


static int lookup(char *buf, char *pattern, char **value)
{
	char *p, *v;
	int len;

	/* do not re-fill value */
	if (!buf || *value)
		return 0;

	len = strlen(pattern);
	if (strncmp(buf, pattern, len) != 0)
		return 0;

	p = buf + len;
	if (*p != '=')
		return 0;
	p++;
	if (!*p || *p == '\n')
		return 0;
	v = p;
	for (; *p && *p != '\n'; p++) ;
	if (*p == '\n')
		*p = '\0';

	*value = xstrdup(v);
	return 1;
}

/* read device properties from fake text file (used on --sysroot) */
static struct lsblk_devprop *get_properties_by_file(struct lsblk_device *ld)
{
	struct lsblk_devprop *prop;
	struct path_cxt *pc;
	FILE *fp = NULL;
	struct stat sb;
	char buf[BUFSIZ];

	assert(lsblk->sysroot);

	if (ld->file_requested)
		return ld->properties;

	if (ld->properties || ld->filename) {
		lsblk_device_free_properties(ld->properties);
		ld->properties = NULL;
	}

	pc = ul_new_path("/");
	if (!pc)
		return NULL;
	if (ul_path_set_prefix(pc, lsblk->sysroot) != 0)
		goto done;
	if (ul_path_stat(pc, &sb, 0, ld->filename) != 0 || !S_ISREG(sb.st_mode))
		goto done;

	fp = ul_path_fopen(pc, "r", ld->filename);
	if (!fp)
		goto done;

	prop = ld->properties;
	if (!prop)
		prop = ld->properties = xcalloc(1, sizeof(*ld->properties));

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* udev based */
		if (lookup(buf, "ID_FS_LABEL_ENC", &prop->label))
			unhexmangle_string(prop->label);
		else if (lookup(buf, "ID_FS_UUID_ENC", &prop->uuid))
			unhexmangle_string(prop->uuid);
		else if (lookup(buf, "ID_PART_ENTRY_NAME", &prop->partlabel))
			unhexmangle_string(prop->partlabel);
		else if (lookup(buf, "ID_PART_TABLE_UUID", &prop->ptuuid)) ;
		else if (lookup(buf, "ID_PART_TABLE_TYPE", &prop->pttype)) ;
		else if (lookup(buf, "ID_FS_TYPE", &prop->fstype)) ;
		else if (lookup(buf, "ID_FS_VERSION", &prop->fsversion)) ;
		else if (lookup(buf, "ID_PART_ENTRY_TYPE", &prop->parttype)) ;
		else if (lookup(buf, "ID_PART_ENTRY_UUID", &prop->partuuid)) ;
		else if (lookup(buf, "ID_PART_ENTRY_FLAGS", &prop->partflags)) ;
		else if (lookup(buf, "ID_PART_ENTRY_NUMBER", &prop->partn)) ;
		else if (lookup(buf, "ID_MODEL", &prop->model)) ;
		else if (lookup(buf, "ID_WWN_WITH_EXTENSION", &prop->wwn)) ;
		else if (lookup(buf, "ID_WWN", &prop->wwn)) ;
		else if (lookup(buf, "SCSI_IDENT_SERIAL", &prop->serial)) ;	/* serial from sg3_utils */
		else if (lookup(buf, "ID_SCSI_SERIAL", &prop->serial)) ;
		else if (lookup(buf, "ID_SERIAL_SHORT", &prop->serial)) ;
		else if (lookup(buf, "ID_SERIAL", &prop->serial)) ;
		else if (lookup(buf, "ID_REVISION", &prop->revision)) ;

		/* lsblk specific */
		else if (lookup(buf, "MODE", &prop->mode)) ;
		else if (lookup(buf, "OWNER", &prop->owner)) ;
		else if (lookup(buf, "GROUP", &prop->group)) ;

		else
			continue;
	}
done:
	if (fp)
		fclose(fp);
	ul_unref_path(pc);
	ld->file_requested = 1;

	DBG(DEV, ul_debugobj(ld, " from fake-file"));
	return ld->properties;
}


static struct lsblk_devprop *get_properties_by_blkid(struct lsblk_device *dev)
{
	blkid_probe pr = NULL;

	if (dev->blkid_requested)
		return dev->properties;

	if (!dev->size)
		goto done;
	if (getuid() != 0)
		goto done;;				/* no permissions to read from the device */

	pr = blkid_new_probe_from_filename(dev->filename);
	if (!pr)
		goto done;

	blkid_probe_enable_superblocks(pr, 1);
	blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_LABEL |
					      BLKID_SUBLKS_UUID |
					      BLKID_SUBLKS_TYPE);
	blkid_probe_enable_partitions(pr, 1);
	blkid_probe_set_partitions_flags(pr, BLKID_PARTS_ENTRY_DETAILS);

	if (!blkid_do_safeprobe(pr)) {
		const char *data = NULL;
		struct lsblk_devprop *prop;

		if (dev->properties)
			lsblk_device_free_properties(dev->properties);
		prop = dev->properties = xcalloc(1, sizeof(*dev->properties));

		if (!blkid_probe_lookup_value(pr, "TYPE", &data, NULL))
			prop->fstype = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "UUID", &data, NULL))
			prop->uuid = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PTUUID", &data, NULL))
			prop->ptuuid = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PTTYPE", &data, NULL))
			prop->pttype = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "LABEL", &data, NULL))
			prop->label = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "VERSION", &data, NULL))
			prop->fsversion = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PART_ENTRY_TYPE", &data, NULL))
			prop->parttype = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PART_ENTRY_UUID", &data, NULL))
			prop->partuuid = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PART_ENTRY_NAME", &data, NULL))
			prop->partlabel = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PART_ENTRY_FLAGS", &data, NULL))
			prop->partflags = xstrdup(data);
		if (!blkid_probe_lookup_value(pr, "PART_ENTRY_NUMBER", &data, NULL))
			prop->partn = xstrdup(data);

		DBG(DEV, ul_debugobj(dev, "%s: found blkid properties", dev->name));
	}

done:
	blkid_free_probe(pr);

	DBG(DEV, ul_debugobj(dev, " from blkid"));
	dev->blkid_requested = 1;
	return dev->properties;
}

struct lsblk_devprop *lsblk_device_get_properties(struct lsblk_device *dev)
{
	struct lsblk_devprop *p = NULL;

	DBG(DEV, ul_debugobj(dev, "%s: properties requested", dev->filename));
	if (lsblk->sysroot)
		return get_properties_by_file(dev);

	p = get_properties_by_udev(dev);
	if (!p)
		p = get_properties_by_blkid(dev);
	return p;
}

void lsblk_properties_deinit(void)
{
#ifdef HAVE_LIBUDEV
	udev_unref(udev);
#endif
}



/*
 * Partition types
 */
struct lsblk_parttype {
	unsigned int	code;		/* type as number or zero */
	char		*name;		/* description */
	char		*typestr;	/* type as string or NULL */
};

static const struct lsblk_parttype mbr_types[] =
{
	#include "pt-mbr-partnames.h"
};

#define DEF_GUID(_u, _n) \
	{ \
		.typestr = (_u), \
		.name = (_n),    \
	}
static const struct lsblk_parttype gpt_types[] =
{
	#include "pt-gpt-partnames.h"
};

const char *lsblk_parttype_code_to_string(const char *code, const char *pttype)
{
	size_t i;

	if (!code || !pttype)
		return NULL;

	if (strcmp(pttype, "dos") == 0 || strcmp(pttype, "mbr") == 0) {
		char *end = NULL;
		unsigned int xcode;

		errno = 0;
		xcode = strtol(code, &end, 16);

		if (errno || *end != '\0')
			return NULL;

		for (i = 0; i < ARRAY_SIZE(mbr_types); i++) {
			const struct lsblk_parttype *t = &mbr_types[i];

			if (t->name && t->code == xcode)
				return t->name;
		}

	} else if (strcmp(pttype, "gpt") == 0) {
		for (i = 0; i < ARRAY_SIZE(gpt_types); i++) {
			const struct lsblk_parttype *t = &gpt_types[i];

			if (t->name && t->typestr &&
			    strcasecmp(code, t->typestr) == 0)
				return t->name;
		}
	}

	return NULL;
}
