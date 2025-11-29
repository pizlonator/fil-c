/*
 * Copyright 2009 VMware, Inc.
 * Copyright 2016 Axel Davy <axel.davy@ens.fr>
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_BUFFER_UPLOAD_H_
#define _NINE_BUFFER_UPLOAD_H_

#include "pipe/p_defines.h"

struct nine_buffer_upload;
struct nine_subbuffer;

struct nine_buffer_upload *
nine_upload_create(struct pipe_context *pipe, unsigned buffers_size,
                   unsigned num_buffers);

void
nine_upload_destroy(struct nine_buffer_upload *upload);

struct nine_subbuffer *
nine_upload_create_buffer(struct nine_buffer_upload *upload,
                          unsigned buffer_size);

void
nine_upload_release_buffer(struct nine_buffer_upload *upload,
                           struct nine_subbuffer *buf);

uint8_t *
nine_upload_buffer_get_map(struct nine_subbuffer *buf);

struct pipe_resource *
nine_upload_buffer_resource_and_offset(struct nine_subbuffer *buf,
                                       unsigned *offset);

#endif /* _NINE_BUFFER_UPLOAD_H_ */
