#ifndef LK_ARRAY_H
#define LK_ARRAY_H

#include <sys/param.h>
#include <sys/types.h>

/**
 * @brief Basic structure for array implementation.
 * @details The array is designed to store an arbitrary number of similar items.
 */
struct lk_array {
	char *array;   /**< Data pointer. */
	ssize_t memb;  /**< One element size. */
	ssize_t count; /**< Number of elements. */
	ssize_t total; /**< Total number of allocated elements. */
};

int lk_array_init(struct lk_array *a, ssize_t memb, ssize_t size);
int lk_array_free(struct lk_array *a);

int lk_array_empty(struct lk_array *a);

int lk_array_append(struct lk_array *a, const void *e);

int lk_array_set(struct lk_array *a, ssize_t i, const void *e);
void *lk_array_get(struct lk_array *a, ssize_t i);
void *lk_array_get_ptr(struct lk_array *a, ssize_t i);

int lk_array_unset(struct lk_array *a, ssize_t i);
int lk_array_exists(struct lk_array *a, ssize_t i);

#endif /* LK_ARRAY_H */
