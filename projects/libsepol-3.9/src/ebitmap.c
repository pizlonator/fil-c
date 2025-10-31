
/* Author : Stephen Smalley, <stephen.smalley.work@gmail.com> */

/* FLASK */

/* 
 * Implementation of the extensible bitmap type.
 */

#include <stdlib.h>

#include <sepol/policydb/ebitmap.h>
#include <sepol/policydb/policydb.h>

#include "debug.h"
#include "private.h"

int ebitmap_or(ebitmap_t * dst, const ebitmap_t * e1, const ebitmap_t * e2)
{
	const ebitmap_node_t *n1, *n2;
	ebitmap_node_t *new = NULL, **prev;

	ebitmap_init(dst);

	prev = &dst->node;
	n1 = e1->node;
	n2 = e2->node;
	while (n1 || n2) {
		new = (ebitmap_node_t *) malloc(sizeof(ebitmap_node_t));
		if (!new) {
			ebitmap_destroy(dst);
			return -ENOMEM;
		}
		if (n1 && n2 && n1->startbit == n2->startbit) {
			new->startbit = n1->startbit;
			new->map = n1->map | n2->map;
			n1 = n1->next;
			n2 = n2->next;
		} else if (!n2 || (n1 && n1->startbit < n2->startbit)) {
			new->startbit = n1->startbit;
			new->map = n1->map;
			n1 = n1->next;
		} else {
			new->startbit = n2->startbit;
			new->map = n2->map;
			n2 = n2->next;
		}

		new->next = NULL;

		*prev = new;
		prev = &new->next;
	}

	dst->highbit = (e1->highbit > e2->highbit) ? e1->highbit : e2->highbit;
	return 0;
}

int ebitmap_union(ebitmap_t * dst, const ebitmap_t * e1)
{
	ebitmap_t tmp;

	if (ebitmap_or(&tmp, dst, e1))
		return -1;
	ebitmap_destroy(dst);
	dst->node = tmp.node;
	dst->highbit = tmp.highbit;

	return 0;
}

int ebitmap_and(ebitmap_t *dst, const ebitmap_t *e1, const ebitmap_t *e2)
{
	const ebitmap_node_t *n1, *n2;
	ebitmap_node_t *new = NULL, **prev;

	ebitmap_init(dst);

	prev = &dst->node;
	n1 = e1->node;
	n2 = e2->node;
	while (n1 && n2) {
		if (n1->startbit == n2->startbit) {
			if (n1->map & n2->map) {
				new = malloc(sizeof(ebitmap_node_t));
				if (!new) {
					ebitmap_destroy(dst);
					return -ENOMEM;
				}
				new->startbit = n1->startbit;
				new->map = n1->map & n2->map;
				new->next = NULL;

				*prev = new;
				prev = &new->next;
			}

			n1 = n1->next;
			n2 = n2->next;
		} else if (n1->startbit > n2->startbit) {
			n2 = n2->next;
		} else {
			n1 = n1->next;
		}
	}

	if (new)
		dst->highbit = new->startbit + MAPSIZE;

	return 0;
}

int ebitmap_xor(ebitmap_t *dst, const ebitmap_t *e1, const ebitmap_t *e2)
{
	const ebitmap_node_t *n1, *n2;
	ebitmap_node_t *new = NULL, **prev;
	uint32_t startbit;
	MAPTYPE map;

	ebitmap_init(dst);

	prev = &dst->node;
	n1 = e1->node;
	n2 = e2->node;
	while (n1 || n2) {
		if (n1 && n2 && n1->startbit == n2->startbit) {
			startbit = n1->startbit;
			map = n1->map ^ n2->map;
			n1 = n1->next;
			n2 = n2->next;
		} else if (!n2 || (n1 && n1->startbit < n2->startbit)) {
			startbit = n1->startbit;
			map = n1->map;
			n1 = n1->next;
		} else {
			startbit = n2->startbit;
			map = n2->map;
			n2 = n2->next;
		}

		if (map != 0) {
			new = malloc(sizeof(ebitmap_node_t));
			if (!new) {
				ebitmap_destroy(dst);
				return -ENOMEM;
			}
			new->startbit = startbit;
			new->map = map;
			new->next = NULL;

			*prev = new;
			prev = &new->next;
		}
	}

	if (new)
		dst->highbit = new->startbit + MAPSIZE;

	return 0;
}

int ebitmap_not(ebitmap_t *dst, const ebitmap_t *e1, unsigned int maxbit)
{
	const ebitmap_node_t *n;
	ebitmap_node_t *new = NULL, **prev;
	uint32_t startbit, cur_startbit;
	MAPTYPE map;

	ebitmap_init(dst);

	prev = &dst->node;
	n = e1->node;
	for (cur_startbit = 0; cur_startbit < maxbit; cur_startbit += MAPSIZE) {
		if (n && n->startbit == cur_startbit) {
			startbit = n->startbit;
			map = ~n->map;

			n = n->next;
		} else {
			startbit = cur_startbit;
			map = ~((MAPTYPE) 0);
		}

		if (maxbit - cur_startbit < MAPSIZE)
			map &= (((MAPTYPE)1) << (maxbit - cur_startbit)) - 1;

		if (map != 0) {
			new = malloc(sizeof(ebitmap_node_t));
			if (!new) {
				ebitmap_destroy(dst);
				return -ENOMEM;
			}

			new->startbit = startbit;
			new->map = map;
			new->next = NULL;

			*prev = new;
			prev = &new->next;
		}
	}

	if (new)
		dst->highbit = new->startbit + MAPSIZE;

	return 0;
}

int ebitmap_andnot(ebitmap_t *dst, const ebitmap_t *e1, const ebitmap_t *e2, unsigned int maxbit)
{
	int rc;
	ebitmap_t e3;
	ebitmap_init(dst);
	rc = ebitmap_not(&e3, e2, maxbit);
	if (rc < 0)
		return rc;
	rc = ebitmap_and(dst, e1, &e3);
	ebitmap_destroy(&e3);
	if (rc < 0)
		return rc;
	return 0;
}

unsigned int ebitmap_cardinality(const ebitmap_t *e1)
{
	unsigned int count = 0;
	const ebitmap_node_t *n;

	for (n = e1->node; n; n = n->next) {
		count += __builtin_popcountll(n->map);
	}
	return count;
}

int ebitmap_hamming_distance(const ebitmap_t * e1, const ebitmap_t * e2)
{
	int rc;
	ebitmap_t tmp;
	int distance;
	if (ebitmap_cmp(e1, e2))
		return 0;
	rc = ebitmap_xor(&tmp, e1, e2);
	if (rc < 0)
		return -1;
	distance = ebitmap_cardinality(&tmp);
	ebitmap_destroy(&tmp);
	return distance;
}

int ebitmap_cmp(const ebitmap_t * e1, const ebitmap_t * e2)
{
	const ebitmap_node_t *n1, *n2;

	if (e1->highbit != e2->highbit)
		return 0;

	n1 = e1->node;
	n2 = e2->node;
	while (n1 && n2 &&
	       (n1->startbit == n2->startbit) && (n1->map == n2->map)) {
		n1 = n1->next;
		n2 = n2->next;
	}

	if (n1 || n2)
		return 0;

	return 1;
}

int ebitmap_cpy(ebitmap_t * dst, const ebitmap_t * src)
{
	const ebitmap_node_t *n;
	ebitmap_node_t *new = NULL, **prev;

	ebitmap_init(dst);
	n = src->node;
	prev = &dst->node;
	while (n) {
		new = (ebitmap_node_t *) malloc(sizeof(ebitmap_node_t));
		if (!new) {
			ebitmap_destroy(dst);
			return -ENOMEM;
		}
		new->startbit = n->startbit;
		new->map = n->map;
		new->next = NULL;

		*prev = new;
		prev = &new->next;

		n = n->next;
	}

	dst->highbit = src->highbit;
	return 0;
}

int ebitmap_contains(const ebitmap_t * e1, const ebitmap_t * e2)
{
	const ebitmap_node_t *n1, *n2;

	if (e1->highbit < e2->highbit)
		return 0;

	n1 = e1->node;
	n2 = e2->node;
	while (n1 && n2 && (n1->startbit <= n2->startbit)) {
		if (n1->startbit < n2->startbit) {
			n1 = n1->next;
			continue;
		}
		if ((n1->map & n2->map) != n2->map)
			return 0;

		n1 = n1->next;
		n2 = n2->next;
	}

	if (n2)
		return 0;

	return 1;
}

int ebitmap_match_any(const ebitmap_t *e1, const ebitmap_t *e2)
{
	const ebitmap_node_t *n1 = e1->node;
	const ebitmap_node_t *n2 = e2->node;

	while (n1 && n2) {
		if (n1->startbit < n2->startbit) {
			n1 = n1->next;
		} else if (n2->startbit < n1->startbit) {
			n2 = n2->next;
		} else {
			if (n1->map & n2->map) {
				return 1;
			}
			n1 = n1->next;
			n2 = n2->next;
		}
	}

	return 0;
}

int ebitmap_get_bit(const ebitmap_t * e, unsigned int bit)
{
	const ebitmap_node_t *n;

	if (e->highbit < bit)
		return 0;

	n = e->node;
	while (n && (n->startbit <= bit)) {
		if ((n->startbit + MAPSIZE) > bit) {
			if (n->map & (MAPBIT << (bit - n->startbit)))
				return 1;
			else
				return 0;
		}
		n = n->next;
	}

	return 0;
}

int ebitmap_set_bit(ebitmap_t * e, unsigned int bit, int value)
{
	ebitmap_node_t *n, *prev, *new;
	uint32_t startbit = bit & ~(MAPSIZE - 1);
	uint32_t highbit = startbit + MAPSIZE;

	if (highbit == 0) {
		ERR(NULL, "bitmap overflow, bit 0x%x", bit);
		return -EINVAL;
	}

	prev = 0;
	n = e->node;
	while (n && n->startbit <= bit) {
		if ((n->startbit + MAPSIZE) > bit) {
			if (value) {
				n->map |= (MAPBIT << (bit - n->startbit));
			} else {
				n->map &= ~(MAPBIT << (bit - n->startbit));
				if (!n->map) {
					/* drop this node from the bitmap */

					if (!n->next) {
						/*
						 * this was the highest map
						 * within the bitmap
						 */
						if (prev)
							e->highbit =
							    prev->startbit +
							    MAPSIZE;
						else
							e->highbit = 0;
					}
					if (prev)
						prev->next = n->next;
					else
						e->node = n->next;

					free(n);
				}
			}
			return 0;
		}
		prev = n;
		n = n->next;
	}

	if (!value)
		return 0;

	new = (ebitmap_node_t *) malloc(sizeof(ebitmap_node_t));
	if (!new)
		return -ENOMEM;

	new->startbit = startbit;
	new->map = (MAPBIT << (bit - new->startbit));

	if (!n) {
		/* this node will be the highest map within the bitmap */
		e->highbit = highbit;
	}

	if (prev) {
		new->next = prev->next;
		prev->next = new;
	} else {
		new->next = e->node;
		e->node = new;
	}

	return 0;
}

int ebitmap_init_range(ebitmap_t * e, unsigned int minbit, unsigned int maxbit)
{
	ebitmap_node_t *new = NULL, **prev;
	uint32_t minstartbit = minbit & ~(MAPSIZE - 1);
	uint32_t maxstartbit = maxbit & ~(MAPSIZE - 1);
	uint32_t minhighbit = minstartbit + MAPSIZE;
	uint32_t maxhighbit = maxstartbit + MAPSIZE;
	uint32_t startbit;
	MAPTYPE mask;

	ebitmap_init(e);

	if (minbit > maxbit)
		return -EINVAL;

	if (minhighbit == 0 || maxhighbit == 0)
		return -EOVERFLOW;

	prev = &e->node;

	for (startbit = minstartbit; startbit <= maxstartbit; startbit += MAPSIZE) {
		new = malloc(sizeof(ebitmap_node_t));
		if (!new)
			return -ENOMEM;

		new->next = NULL;
		new->startbit = startbit;

		if (startbit != minstartbit && startbit != maxstartbit) {
			new->map = ~((MAPTYPE)0);
		} else if (startbit != maxstartbit) {
			new->map = ~((MAPTYPE)0) << (minbit - startbit);
		} else if (startbit != minstartbit) {
			new->map = ~((MAPTYPE)0) >> (MAPSIZE - (maxbit - startbit + 1));
		} else {
			mask = ~((MAPTYPE)0) >> (MAPSIZE - (maxbit - minbit + 1));
			new->map = (mask << (minbit - startbit));
		}

		*prev = new;
		prev = &new->next;
	}

	e->highbit = maxhighbit;

	return 0;
}

unsigned int ebitmap_highest_set_bit(const ebitmap_t * e)
{
	const ebitmap_node_t *n;
	MAPTYPE map;
	unsigned int pos = 0;

	n = e->node;
	if (!n)
		return 0;

	while (n->next)
		n = n->next;

	map = n->map;
	while (map >>= 1)
		pos++;

	return n->startbit + pos;
}

void ebitmap_destroy(ebitmap_t * e)
{
	ebitmap_node_t *n, *temp;

	if (!e)
		return;

	n = e->node;
	while (n) {
		temp = n;
		n = n->next;
		free(temp);
	}

	e->highbit = 0;
	e->node = 0;
	return;
}

int ebitmap_read(ebitmap_t * e, void *fp)
{
	int rc;
	ebitmap_node_t *n, *l;
	uint32_t buf[3], mapsize, count, i;
	uint64_t map;

	ebitmap_init(e);

	rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
	if (rc < 0)
		goto bad;

	mapsize = le32_to_cpu(buf[0]);
	e->highbit = le32_to_cpu(buf[1]);
	count = le32_to_cpu(buf[2]);

	if (mapsize != MAPSIZE) {
		ERR(NULL, "security: ebitmap: map size %d does not match my size %zu (high bit was %d)",
		     mapsize, MAPSIZE, e->highbit);
		goto bad;
	}
	if (!e->highbit) {
		e->node = NULL;
		goto ok;
	}
	if (e->highbit & (MAPSIZE - 1)) {
		ERR(NULL, "security: ebitmap: high bit (%d) is not a multiple of the map size (%zu)",
		     e->highbit, MAPSIZE);
		goto bad;
	}

	if (e->highbit && !count)
		goto bad;

	l = NULL;
	for (i = 0; i < count; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0) {
			ERR(NULL, "security: ebitmap: truncated map");
			goto bad;
		}
		n = (ebitmap_node_t *) malloc(sizeof(ebitmap_node_t));
		if (!n) {
			ERR(NULL, "security: ebitmap: out of memory");
			rc = -ENOMEM;
			goto bad;
		}
		memset(n, 0, sizeof(ebitmap_node_t));

		n->startbit = le32_to_cpu(buf[0]);

		if (n->startbit & (MAPSIZE - 1)) {
			ERR(NULL, "security: ebitmap start bit (%d) is not a multiple of the map size (%zu)",
			     n->startbit, MAPSIZE);
			goto bad_free;
		}
		if (n->startbit > (e->highbit - MAPSIZE)) {
			ERR(NULL, "security: ebitmap start bit (%d) is beyond the end of the bitmap (%zu)",
			     n->startbit, (e->highbit - MAPSIZE));
			goto bad_free;
		}
		rc = next_entry(&map, fp, sizeof(uint64_t));
		if (rc < 0) {
			ERR(NULL, "security: ebitmap: truncated map");
			goto bad_free;
		}
		n->map = le64_to_cpu(map);

		if (!n->map) {
			ERR(NULL, "security: ebitmap: null map in ebitmap (startbit %d)",
			     n->startbit);
			goto bad_free;
		}
		if (l) {
			if (n->startbit <= l->startbit) {
				ERR(NULL, "security: ebitmap: start bit %d comes after start bit %d",
				     n->startbit, l->startbit);
				goto bad_free;
			}
			l->next = n;
		} else
			e->node = n;

		l = n;
	}
	if (count && l->startbit + MAPSIZE != e->highbit) {
		ERR(NULL, "security: ebitmap: high bit %u has not the expected value %zu",
		     e->highbit, l->startbit + MAPSIZE);
		goto bad;
	}

      ok:
	rc = 0;
      out:
	return rc;
      bad_free:
	free(n);
      bad:
	if (!rc)
		rc = -EINVAL;
	ebitmap_destroy(e);
	goto out;
}

/* FLASK */
