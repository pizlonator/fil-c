/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef R300_TEXTURE_H
#define R300_TEXTURE_H

#include "util/compiler.h"
#include "util/format/u_formats.h"
#include "pipe/p_screen.h"

struct pipe_screen;
struct pipe_context;
struct pipe_resource;
struct winsys_handle;
struct r300_texture_format_state;
struct r300_texture_desc;
struct r300_resource;
struct r300_screen;

unsigned r300_get_swizzle_combined(const unsigned char *swizzle_format,
                                   const unsigned char *swizzle_view,
                                   bool dxtc_swizzle);

uint32_t r300_translate_texformat(enum pipe_format format,
                                  const unsigned char *swizzle_view,
                                  bool is_r500,
                                  bool dxtc_swizzle);

uint32_t r500_tx_format_msb_bit(enum pipe_format format);

bool r300_is_colorbuffer_format_supported(enum pipe_format format);

bool r300_is_zs_format_supported(enum pipe_format format);

bool r300_is_sampler_format_supported(enum pipe_format format);

void r300_texture_setup_format_state(struct r300_screen *screen,
                                     struct r300_resource *tex,
                                     enum pipe_format format,
                                     unsigned level,
                                     unsigned width0_override,
                                     unsigned height0_override,
                                     struct r300_texture_format_state *out);

bool r300_resource_get_handle(struct pipe_screen* screen,
                              struct pipe_context *ctx,
                              struct pipe_resource *texture,
                              struct winsys_handle *whandle,
                              unsigned usage);

struct pipe_resource*
r300_texture_from_handle(struct pipe_screen* screen,
			 const struct pipe_resource* base,
			 struct winsys_handle *whandle,
                         unsigned usage);

struct pipe_resource*
r300_texture_create(struct pipe_screen* screen,
		    const struct pipe_resource* templ);

struct pipe_surface* r300_create_surface_custom(struct pipe_context * ctx,
                                         struct pipe_resource* texture,
                                         const struct pipe_surface *surf_tmpl,
                                         unsigned width0_override,
					 unsigned height0_override);

struct pipe_surface* r300_create_surface(struct pipe_context *ctx,
                                         struct pipe_resource* texture,
                                         const struct pipe_surface *surf_tmpl);

void r300_surface_destroy(struct pipe_context *ctx, struct pipe_surface* s);

#endif /* R300_TEXTURE_H */
