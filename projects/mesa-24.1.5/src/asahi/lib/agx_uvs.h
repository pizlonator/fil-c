/*
 * Copyright 2024 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include "agx_pack.h"
#include "shader_enums.h"

struct nir_shader;

/* Matches the hardware order */
enum uvs_group {
   UVS_POSITION,
   UVS_VARYINGS,
   UVS_PSIZ,
   UVS_LAYER_VIEWPORT,
   UVS_CLIP_DIST,
   UVS_NUM_GROUP,
};

/**
 * Represents an "unlinked" UVS layout. This is computable from an unlinked
 * vertex shader without knowing the associated fragment shader. The various UVS
 * groups have fixed offsets, but the varyings within the varying group have
 * indeterminate order since we don't yet know the fragment shader interpolation
 * qualifiers.
 */
struct agx_unlinked_uvs_layout {
   /* Offset of each group in the UVS in words. */
   uint8_t group_offs[UVS_NUM_GROUP];

   /* Size of the UVS allocation in words. >= last group_offs element */
   uint8_t size;

   /* Size of the UVS_VARYINGS */
   uint8_t user_size;

   /* Number of 32-bit components written for each slot. TODO: Model 16-bit.
    *
    * Invariant: sum_{slot} (components[slot]) =
    *            group_offs[PSIZ] - group_offs[VARYINGS]
    */
   uint8_t components[VARYING_SLOT_MAX];

   /* Bit i set <===> components[i] != 0 && i != POS && i != PSIZ. For fast
    * iteration of user varyings.
    */
   uint64_t written;

   /* Fully packed data structure */
   struct agx_vdm_state_vertex_outputs_packed vdm;

   /* Partial data structure, must be merged with FS selects */
   struct agx_output_select_packed osel;
};

bool agx_nir_lower_uvs(struct nir_shader *s,
                       struct agx_unlinked_uvs_layout *layout);

/**
 * Represents a linked UVS layout.
 */
struct agx_varyings_vs {
   /* Associated linked hardware data structures */
   struct agx_varying_counts_packed counts_32, counts_16;

   /* If the user varying slot is written, this is the base index that the first
    * component of the slot is written to. The next components are found in the
    * next indices. Otherwise 0, aliasing position.
    */
   unsigned slots[VARYING_SLOT_MAX];
};

void agx_assign_uvs(struct agx_varyings_vs *varyings,
                    struct agx_unlinked_uvs_layout *layout, uint64_t flat_mask,
                    uint64_t linear_mask);
