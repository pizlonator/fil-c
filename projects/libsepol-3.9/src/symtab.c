
/* Author : Stephen Smalley, <stephen.smalley.work@gmail.com> */

/* FLASK */

/*
 * Implementation of the symbol table type.
 */

#include <string.h>

#include "private.h"

#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/symtab.h>

ignore_unsigned_overflow_
static unsigned int symhash(hashtab_t h, const_hashtab_key_t key)
{
	unsigned int hash = 5381;
	unsigned char c;

	while ((c = *(unsigned const char *)key++))
		hash = ((hash << 5) + hash) ^ c;

	return hash & (h->size - 1);
}

static int symcmp(hashtab_t h
		  __attribute__ ((unused)), const_hashtab_key_t key1,
		  const_hashtab_key_t key2)
{
	return strcmp(key1, key2);
}

int symtab_init(symtab_t * s, unsigned int size)
{
	s->table = hashtab_create(symhash, symcmp, size);
	if (!s->table)
		return -1;
	s->nprim = 0;
	return 0;
}

void symtab_destroy(symtab_t * s)
{
	if (!s)
		return;
	if (s->table)
		hashtab_destroy(s->table);
	return;
}
/* FLASK */
