/*
 * Copyright 2016 Patrick Rudolph <siro@das-labor.org>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_QUEUE_H_
#define _NINE_QUEUE_H_

#include "util/compiler.h"

struct nine_queue_pool;

void
nine_queue_wait_flush(struct nine_queue_pool* ctx);

void *
nine_queue_get(struct nine_queue_pool* ctx);

void
nine_queue_flush(struct nine_queue_pool* ctx);

void *
nine_queue_alloc(struct nine_queue_pool* ctx, unsigned space);

bool
nine_queue_no_flushed_work(struct nine_queue_pool* ctx);

bool
nine_queue_isempty(struct nine_queue_pool* ctx);

struct nine_queue_pool*
nine_queue_create(void);

void
nine_queue_delete(struct nine_queue_pool *ctx);

#endif /* _NINE_QUEUE_H_ */
