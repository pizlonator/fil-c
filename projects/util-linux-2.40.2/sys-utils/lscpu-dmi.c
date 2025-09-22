/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Copyright (C) 2020 FUJITSU LIMITED.  All rights reserved.
 */
#include "lscpu.h"

void to_dmi_header(struct lscpu_dmi_header *h, uint8_t *data)
{
	h->type = data[0];
	h->length = data[1];
	memcpy(&h->handle, data + 2, sizeof(h->handle));
	h->data = data;
}

char *dmi_string(const struct lscpu_dmi_header *dm, uint8_t s)
{
	char *bp = (char *)dm->data;

	if (!s || !bp)
		return NULL;

	bp += dm->length;
	while (s > 1 && *bp) {
		bp += strlen(bp);
		bp++;
		s--;
	}

	return !*bp ? NULL : bp;
}

int parse_dmi_table(uint16_t len, uint16_t num,
				uint8_t *data,
				struct dmi_info *di)
{
	uint8_t *buf = data;
	int rc = -1;
	int i = 0;

	 /* 4 is the length of an SMBIOS structure header */
	while (i < num && data + 4 <= buf + len) {
		uint8_t *next;
		struct lscpu_dmi_header h;

		to_dmi_header(&h, data);

		/*
		 * If a short entry is found (less than 4 bytes), not only it
		 * is invalid, but we cannot reliably locate the next entry.
		 * Better stop at this point.
		 */
		if (h.length < 4)
			goto done;

		/* look for the next handle */
		next = data + h.length;
		while (next - buf + 1 < len && (next[0] != 0 || next[1] != 0))
			next++;
		next += 2;
		switch (h.type) {
		case 0:
			di->vendor = dmi_string(&h, data[0x04]);
			break;
		case 1:
			di->manufacturer = dmi_string(&h, data[0x04]);
			di->product = dmi_string(&h, data[0x05]);
			break;
		case 4:
			/* Get the first processor information */
			if (di->sockets == 0) {
				di->processor_manufacturer = dmi_string(&h, data[0x7]);
				di->processor_version = dmi_string(&h, data[0x10]);
				di->current_speed = *((uint16_t *)(&data[0x16]));
				di->part_num = dmi_string(&h, data[0x22]);

				if (data[0x6] == 0xfe)
					di->processor_family = *((uint16_t *)(&data[0x28]));
				else
					di->processor_family = data[0x6];
			}
			di->sockets++;
			break;
		default:
			break;
		}

		data = next;
		i++;
	}
	rc = 0;
done:
	return rc;
}

int dmi_decode_cputype(struct lscpu_cputype *ct)
{
	static char const sys_fw_dmi_tables[] = _PATH_SYS_DMI;
	struct dmi_info di = { };
	struct stat st;
	uint8_t *data;
	int rc = 0;
	char buf[100] = { };

	if (stat(sys_fw_dmi_tables, &st))
		return rc;

	data = get_mem_chunk(0, st.st_size, sys_fw_dmi_tables);
	if (!data)
		return rc;

	rc = parse_dmi_table(st.st_size, st.st_size/4, data, &di);
	if (rc < 0) {
		free(data);
		return rc;
	}

	if (di.processor_manufacturer)
		ct->bios_vendor = xstrdup(di.processor_manufacturer);

	snprintf(buf, sizeof(buf), "%s %s CPU @ %d.%dGHz",
			(di.processor_version ?: ""), (di.part_num ?: ""),
			di.current_speed/1000, (di.current_speed % 1000) / 100);
	ct->bios_modelname = xstrdup(buf);

	/* Get CPU family */
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%d", di.processor_family);
	ct->bios_family = xstrdup(buf);

	free(data);
	return 0;
}

size_t get_number_of_physical_sockets_from_dmi(void)
{
	static char const sys_fw_dmi_tables[] = _PATH_SYS_DMI;
	struct dmi_info di;
	struct stat st;
	uint8_t *data;
	int rc = 0;

	if (stat(sys_fw_dmi_tables, &st))
		return rc;

	data = get_mem_chunk(0, st.st_size, sys_fw_dmi_tables);
	if (!data)
		return rc;

	memset(&di, 0, sizeof(struct dmi_info));
	rc = parse_dmi_table(st.st_size, st.st_size/4, data, &di);

	free(data);

	if ((rc < 0) || !di.sockets)
		return 0;
	else
		return di.sockets;
}
