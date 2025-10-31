#include "test-ebitmap.h"

#include <stdlib.h>
#include <time.h>

#include <sepol/debug.h>
#include <sepol/policydb/ebitmap.h>

#define RANDOM_ROUNDS 10


static int ebitmap_init_random(ebitmap_t *e, unsigned int length, int set_chance)
{
	unsigned int i;
	int rc;

	if (set_chance <= 0 || set_chance > 100)
		return -EINVAL;

	ebitmap_init(e);

	for (i = 0; i < length; i++) {
		if ((random() % 100) < set_chance) {
			rc = ebitmap_set_bit(e, i, 1);
			if (rc)
				return rc;
		}
	}

	return 0;
}

static void test_ebitmap_init_destroy(void)
{
	ebitmap_t e;

	/* verify idempotence */
	ebitmap_init(&e);
	ebitmap_init(&e);
	ebitmap_init(&e);

	CU_ASSERT(ebitmap_is_empty(&e));
	CU_ASSERT_PTR_NULL(ebitmap_startnode(&e));

	/* verify idempotence */
	ebitmap_destroy(&e);
	ebitmap_destroy(&e);
	ebitmap_destroy(&e);

	CU_ASSERT(ebitmap_is_empty(&e));
	CU_ASSERT_PTR_NULL(ebitmap_startnode(&e));
}

static void test_ebitmap_cmp(void)
{
	ebitmap_t e1, e2;

	ebitmap_init(&e1);
	ebitmap_init(&e2);

	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 10, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 10, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 63, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 63, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 64, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 64, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1022, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 1022, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1023, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 1023, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1024, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 1024, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1025, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 1025, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 255, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 255, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 256, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 256, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 639, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 639, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 640, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 640, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 900, 1), 0);
	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 900, 1), 0);
	CU_ASSERT(ebitmap_cmp(&e1, &e2));

	ebitmap_destroy(&e2);

	CU_ASSERT_FALSE(ebitmap_cmp(&e1, &e2));

	ebitmap_destroy(&e1);

	CU_ASSERT(ebitmap_cmp(&e1, &e2));
}

static void test_ebitmap_set_and_get(void)
{
	ebitmap_t e;

	ebitmap_init(&e);

	CU_ASSERT(ebitmap_is_empty(&e));
	CU_ASSERT_TRUE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 0);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 10), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, UINT32_MAX, 1), -EINVAL);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 10, 0), 0);
	CU_ASSERT(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 0);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 10), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 10, 1), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 1);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 10);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 10), 1);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 100, 1), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 2);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 100);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 100), 1);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 50, 1), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 3);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 100);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 50), 1);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 1023, 1), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 4);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1023);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 1023), 1);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 1024, 1), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 5);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1024);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 1024), 1);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 1050, 1), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 6);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1050);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 1050), 1);

	{
		ebitmap_node_t *n;
		unsigned int bit, count;

		/* iterate all allocated bits */
		count = 0;
		ebitmap_for_each_bit(&e, n, bit) {
			CU_ASSERT(                bit <   64  ||
			          (64   <= bit && bit <  128) ||
			          (960  <= bit && bit < 1024) ||
			          (1024 <= bit && bit < 1088));
			count++;
		}
		CU_ASSERT_EQUAL(count, 4 * 64);

		count = 0;
		ebitmap_for_each_positive_bit(&e, n, bit) {
			CU_ASSERT(bit == 10 ||
			          bit == 50 ||
			          bit == 100 ||
			          bit == 1023 ||
			          bit == 1024 ||
			          bit == 1050);
			CU_ASSERT_EQUAL(ebitmap_get_bit(&e, bit), 1);
			count++;
		}
		CU_ASSERT_EQUAL(count, 6);
	}

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 1024, 0), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 5);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1050);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 1024), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 1050, 0), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 4);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1023);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 1050), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 100, 0), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 3);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1023);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 100), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 10, 0), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 2);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1023);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 10), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 50, 0), 0);
	CU_ASSERT_FALSE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 1);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 1023);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 50), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e, 1023, 0), 0);
	CU_ASSERT_TRUE(ebitmap_is_empty(&e));
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e), 0);
	CU_ASSERT_EQUAL(ebitmap_get_bit(&e, 1023), 0);

	ebitmap_destroy(&e);
}

static void test_ebitmap_init_range(void)
{
	ebitmap_t e1, e2, e3, e4, e5, e6;

	CU_ASSERT_EQUAL(ebitmap_init_range(&e1, 0, 0), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e1), 0);
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e1), 1);

	CU_ASSERT_EQUAL(ebitmap_init_range(&e2, 0, 5), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e2), 5);
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e2), 6);

	CU_ASSERT_EQUAL(ebitmap_init_range(&e3, 20, 100), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e3), 100);
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e3), 81);

	CU_ASSERT_EQUAL(ebitmap_init_range(&e4, 100, 400), 0);
	CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&e4), 400);
	CU_ASSERT_EQUAL(ebitmap_cardinality(&e4), 301);

	CU_ASSERT_EQUAL(ebitmap_init_range(&e5, 10, 5), -EINVAL);
	CU_ASSERT_EQUAL(ebitmap_init_range(&e6, 0, UINT32_MAX), -EOVERFLOW);

	ebitmap_destroy(&e6);
	ebitmap_destroy(&e5);
	ebitmap_destroy(&e4);
	ebitmap_destroy(&e3);
	ebitmap_destroy(&e2);
	ebitmap_destroy(&e1);
}

static void test_ebitmap_or(void)
{
	ebitmap_t e1, e2, e3, e4;

	ebitmap_init(&e1);
	ebitmap_init(&e2);
	ebitmap_init(&e3);
	ebitmap_init(&e4);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 10, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 100, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 101, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 318, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 319, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 383, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 384, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 449, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1013, 1), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 11, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 101, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 430, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 665, 1), 0);

	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 10, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 11, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 100, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 101, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 318, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 319, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 383, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 384, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 430, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 449, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 665, 1), 0);
	CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 1013, 1), 0);

	{
		ebitmap_t dst;

		CU_ASSERT_EQUAL(ebitmap_or(&dst, &e1, &e1), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e1));

		ebitmap_destroy(&dst);
	}

	{
		ebitmap_t dst;

		CU_ASSERT_EQUAL(ebitmap_or(&dst, &e2, &e2), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e2));

		ebitmap_destroy(&dst);
	}

	{
		ebitmap_t dst;

		CU_ASSERT_EQUAL(ebitmap_or(&dst, &e1, &e2), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e3));

		ebitmap_destroy(&dst);
	}

	{
		ebitmap_t dst;

		CU_ASSERT_EQUAL(ebitmap_or(&dst, &e3, &e3), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e3));

		ebitmap_destroy(&dst);
	}

	{
		ebitmap_t dst;

		CU_ASSERT_EQUAL(ebitmap_or(&dst, &e3, &e4), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e3));

		ebitmap_destroy(&dst);
	}

	{
		ebitmap_t dst;

		CU_ASSERT_EQUAL(ebitmap_or(&dst, &e4, &e4), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e4));

		ebitmap_destroy(&dst);
	}

	ebitmap_destroy(&e4);
	ebitmap_destroy(&e3);
	ebitmap_destroy(&e2);
	ebitmap_destroy(&e1);
}

static void test_ebitmap_and(void)
{
	{
		ebitmap_t e1, e2, e12, e3, e4;

		ebitmap_init(&e1);
		ebitmap_init(&e2);
		ebitmap_init(&e12);
		ebitmap_init(&e3);
		ebitmap_init(&e4);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 10, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 100, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 319, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 383, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 384, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 449, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1013, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 11, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 319, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 665, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e12, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e12, 319, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 10, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 11, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 100, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 319, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 383, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 384, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 430, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 449, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 665, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 1013, 1), 0);

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e1, &e1), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e1));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e2, &e2), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e2));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e1, &e2), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e12));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e3, &e3), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e3));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e1, &e3), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e1));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e2, &e3), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e2));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e4, &e4), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_and(&dst, &e3, &e4), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		ebitmap_destroy(&e4);
		ebitmap_destroy(&e3);
		ebitmap_destroy(&e12);
		ebitmap_destroy(&e2);
		ebitmap_destroy(&e1);
	}

	{
		ebitmap_t e5;
		ebitmap_t e6;
		ebitmap_t e56;
		ebitmap_t dst;

		ebitmap_init(&e5);
		ebitmap_init(&e6);
		ebitmap_init(&e56);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 63, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 191, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 319, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 64, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 319, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 320, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 319, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_and(&dst, &e5, &e6), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e56));

		ebitmap_destroy(&dst);
		ebitmap_destroy(&e56);
		ebitmap_destroy(&e6);
		ebitmap_destroy(&e5);
	}
}

static void test_ebitmap_xor(void)
{
	{
		ebitmap_t e1, e2, e3, e4;

		ebitmap_init(&e1);
		ebitmap_init(&e2);
		ebitmap_init(&e3);
		ebitmap_init(&e4);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 5, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 10, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 1, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 3, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 6, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 9, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 3, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 5, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 6, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 9, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 10, 1), 0);

		{
			ebitmap_t dst1, dst2;

			CU_ASSERT_EQUAL(ebitmap_xor(&dst1, &e1, &e1), 0);
			CU_ASSERT(ebitmap_cmp(&dst1, &e4));
			CU_ASSERT_EQUAL(ebitmap_xor(&dst2, &dst1, &e1), 0);
			CU_ASSERT(ebitmap_cmp(&dst2, &e1));

			ebitmap_destroy(&dst2);
			ebitmap_destroy(&dst1);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_xor(&dst, &e2, &e2), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_xor(&dst, &e3, &e3), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_xor(&dst, &e4, &e4), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_xor(&dst, &e1, &e2), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e3));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_xor(&dst, &e2, &e4), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e2));

			ebitmap_destroy(&dst);
		}

		ebitmap_destroy(&e4);
		ebitmap_destroy(&e3);
		ebitmap_destroy(&e2);
		ebitmap_destroy(&e1);
	}

	{
		ebitmap_t e5;
		ebitmap_t e6;
		ebitmap_t e56;
		ebitmap_t dst;

		ebitmap_init(&e5);
		ebitmap_init(&e6);
		ebitmap_init(&e56);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 63, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 191, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 319, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 64, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 319, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 320, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 63, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 64, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 191, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 320, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_xor(&dst, &e5, &e6), 0);
		CU_ASSERT(ebitmap_cmp(&dst, &e56));

		ebitmap_destroy(&dst);
		ebitmap_destroy(&e56);
		ebitmap_destroy(&e6);
		ebitmap_destroy(&e5);
	}
}

static void test_ebitmap_not(void)
{
	{
		ebitmap_t e1, e2, e3, e4;

		ebitmap_init(&e1);
		ebitmap_init(&e2);
		ebitmap_init(&e3);
		ebitmap_init(&e4);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 0, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 5, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 10, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 2, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 3, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 4, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 6, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 7, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 8, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 9, 1), 0);

		{
			ebitmap_t dst1, dst2;

			CU_ASSERT_EQUAL(ebitmap_not(&dst1, &e3, 10), 0);
			CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&dst1), 9);
			CU_ASSERT_EQUAL(ebitmap_cardinality(&dst1), 10);

			CU_ASSERT_EQUAL(ebitmap_not(&dst2, &dst1, 10), 0);
			CU_ASSERT(ebitmap_cmp(&dst2, &e3));

			ebitmap_destroy(&dst2);
			ebitmap_destroy(&dst1);
		}

		{
			ebitmap_t dst1, dst2;

			CU_ASSERT_EQUAL(ebitmap_not(&dst1, &e1, 11), 0);
			CU_ASSERT(ebitmap_cmp(&dst1, &e2));
			CU_ASSERT_EQUAL(ebitmap_not(&dst2, &dst1, 11), 0);
			CU_ASSERT(ebitmap_cmp(&dst2, &e1));

			ebitmap_destroy(&dst2);
			ebitmap_destroy(&dst1);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_not(&dst, &e1, 8), 0);
			CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&dst), 7);
			CU_ASSERT_EQUAL(ebitmap_cardinality(&dst), 5);

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_not(&dst, &e1, 12), 0);
			CU_ASSERT_EQUAL(ebitmap_highest_set_bit(&dst), 11);
			CU_ASSERT_EQUAL(ebitmap_cardinality(&dst), 8);

			ebitmap_destroy(&dst);
		}

		ebitmap_destroy(&e3);
		ebitmap_destroy(&e2);
		ebitmap_destroy(&e1);
	}

	{
		ebitmap_t e5;
		ebitmap_t e5not;
		ebitmap_node_t *n;
		unsigned int bit;

		ebitmap_init(&e5);
		ebitmap_init(&e5not);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 63, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 191, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 319, 1), 0);

		for (bit = 0; bit < 317; bit++)
			CU_ASSERT_EQUAL(ebitmap_set_bit(&e5not, bit, 1), 0);
		ebitmap_for_each_positive_bit(&e5, n, bit)
			CU_ASSERT_EQUAL(ebitmap_set_bit(&e5not, bit, 0), 0);

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_not(&dst, &e5, 317), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e5not));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_not(&dst, &e5, 318), 0);
			CU_ASSERT_FALSE(ebitmap_cmp(&dst, &e5not));

			CU_ASSERT_EQUAL(ebitmap_set_bit(&e5not, 317, 1), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e5not));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_not(&dst, &e5, 319), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e5not));

			ebitmap_destroy(&dst);
		}

		ebitmap_destroy(&e5not);
		ebitmap_destroy(&e5);
	}
}

static void test_ebitmap_andnot(void)
{
	{
		ebitmap_t e1, e2, e12, e3, e4;

		ebitmap_init(&e1);
		ebitmap_init(&e2);
		ebitmap_init(&e12);
		ebitmap_init(&e3);
		ebitmap_init(&e4);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 10, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 100, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 430, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e1, 1013, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 11, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e2, 665, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e12, 10, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e12, 100, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e12, 430, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e12, 1013, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 10, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 11, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 100, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 101, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 430, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 665, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e3, 1013, 1), 0);

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e1, &e1, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e2, &e2, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e1, &e2, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e12));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e3, &e3, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e1, &e3, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e2, &e12, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e2));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e4, &e4, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e4));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e3, &e4, 1024), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e3));

			ebitmap_destroy(&dst);
		}

		ebitmap_destroy(&e4);
		ebitmap_destroy(&e3);
		ebitmap_destroy(&e12);
		ebitmap_destroy(&e2);
		ebitmap_destroy(&e1);
	}

	{
		ebitmap_t e5;
		ebitmap_t e6;
		ebitmap_t e56;

		ebitmap_init(&e5);
		ebitmap_init(&e6);
		ebitmap_init(&e56);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 63, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 191, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 318, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e5, 319, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 64, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 192, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 319, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e6, 320, 1), 0);

		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 63, 1), 0);
		CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 191, 1), 0);

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e5, &e6, 317), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e56));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e5, &e6, 318), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e56));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e5, &e6, 319), 0);
			CU_ASSERT_FALSE(ebitmap_cmp(&dst, &e56));

			CU_ASSERT_EQUAL(ebitmap_set_bit(&e56, 318, 1), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e56));

			ebitmap_destroy(&dst);
		}

		{
			ebitmap_t dst;

			CU_ASSERT_EQUAL(ebitmap_andnot(&dst, &e5, &e6, 320), 0);
			CU_ASSERT(ebitmap_cmp(&dst, &e56));

			ebitmap_destroy(&dst);
		}

		ebitmap_destroy(&e56);
		ebitmap_destroy(&e6);
		ebitmap_destroy(&e5);
	}
}

static void test_ebitmap__random_impl(unsigned int length, int set_chance)
{
	ebitmap_t e1, e2, dst_cpy, dst_or, dst_and, dst_xor1, dst_xor2, dst_not1, dst_not2, dst_andnot;
	unsigned int i;

	CU_ASSERT_EQUAL(ebitmap_init_random(&e1, length, set_chance), 0);
	CU_ASSERT_EQUAL(ebitmap_init_random(&e2, length, set_chance), 0);

	CU_ASSERT_EQUAL(ebitmap_cpy(&dst_cpy, &e1), 0);
	CU_ASSERT(ebitmap_cmp(&dst_cpy, &e1));

	CU_ASSERT_EQUAL(ebitmap_or(&dst_or, &e1, &e2), 0);
	for (i = 0; i < length; i++)
		CU_ASSERT_EQUAL(ebitmap_get_bit(&dst_or, i), ebitmap_get_bit(&e1, i) | ebitmap_get_bit(&e2, i));

	CU_ASSERT_EQUAL(ebitmap_and(&dst_and, &e1, &e2), 0);
	for (i = 0; i < length; i++)
		CU_ASSERT_EQUAL(ebitmap_get_bit(&dst_and, i), ebitmap_get_bit(&e1, i) & ebitmap_get_bit(&e2, i));

	CU_ASSERT_EQUAL(ebitmap_xor(&dst_xor1, &e1, &e2), 0);
	for (i = 0; i < length; i++)
		CU_ASSERT_EQUAL(ebitmap_get_bit(&dst_xor1, i), ebitmap_get_bit(&e1, i) ^ ebitmap_get_bit(&e2, i));
	CU_ASSERT_EQUAL(ebitmap_xor(&dst_xor2, &dst_xor1, &e2), 0);
	CU_ASSERT(ebitmap_cmp(&dst_xor2, &e1));

	CU_ASSERT_EQUAL(ebitmap_not(&dst_not1, &e1, length), 0);
	for (i = 0; i < length; i++)
		CU_ASSERT_EQUAL(ebitmap_get_bit(&dst_not1, i), !ebitmap_get_bit(&e1, i));
	CU_ASSERT_EQUAL(ebitmap_not(&dst_not2, &dst_not1, length), 0);
	CU_ASSERT(ebitmap_cmp(&dst_not2, &e1));

	CU_ASSERT_EQUAL(ebitmap_andnot(&dst_andnot, &e1, &e2, length), 0);
	for (i = 0; i < length; i++)
		CU_ASSERT_EQUAL(ebitmap_get_bit(&dst_andnot, i), ebitmap_get_bit(&e1, i) & !ebitmap_get_bit(&e2, i));

	ebitmap_destroy(&dst_andnot);
	ebitmap_destroy(&dst_not2);
	ebitmap_destroy(&dst_not1);
	ebitmap_destroy(&dst_xor2);
	ebitmap_destroy(&dst_xor1);
	ebitmap_destroy(&dst_and);
	ebitmap_destroy(&dst_or);
	ebitmap_destroy(&dst_cpy);
	ebitmap_destroy(&e2);
	ebitmap_destroy(&e1);
}

static void test_ebitmap__random(void)
{
	unsigned int i;

	for (i = 0; i < RANDOM_ROUNDS; i++)
		test_ebitmap__random_impl(5, 10);

	for (i = 0; i < RANDOM_ROUNDS; i++)
		test_ebitmap__random_impl(5, 90);

	for (i = 0; i < RANDOM_ROUNDS; i++)
		test_ebitmap__random_impl(1024, 50);

	for (i = 0; i < RANDOM_ROUNDS; i++)
		test_ebitmap__random_impl(8000, 5);

	for (i = 0; i < RANDOM_ROUNDS; i++)
		test_ebitmap__random_impl(8000, 95);
}

/*
 * External hooks
 */

int ebitmap_test_init(void)
{
	srandom(time(NULL));

	/* silence ebitmap_set_bit() failure message */
	sepol_debug(0);

	return 0;
}

int ebitmap_test_cleanup(void)
{
	return 0;
}

#define ADD_TEST(name) \
	do { \
		if (NULL == CU_add_test(suite, #name, test_##name)) { \
			return CU_get_error(); \
		} \
	} while (0)

int ebitmap_add_tests(CU_pSuite suite)
{
	ADD_TEST(ebitmap_init_destroy);
	ADD_TEST(ebitmap_cmp);
	ADD_TEST(ebitmap_set_and_get);
	ADD_TEST(ebitmap_init_range);
	ADD_TEST(ebitmap_or);
	ADD_TEST(ebitmap_and);
	ADD_TEST(ebitmap_xor);
	ADD_TEST(ebitmap_not);
	ADD_TEST(ebitmap_andnot);
	ADD_TEST(ebitmap__random);
	return 0;
}
