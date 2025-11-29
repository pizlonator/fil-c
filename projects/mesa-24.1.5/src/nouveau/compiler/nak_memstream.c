/*
 * Copyright © 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 *
 * This file exposes a nice interface that can be consumed from Rust. We would
 * have to have Rust libc bindings otherwise.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "nak_private.h"
#include "nir.h"

void nak_open_memstream(struct nak_memstream *memstream)
{
    memstream->stream = open_memstream(&memstream->buffer, &memstream->written);
    fflush(memstream->stream);
    assert(memstream->stream);
    assert(memstream->buffer);
}

void nak_close_memstream(struct nak_memstream *memstream)
{
    fclose(memstream->stream);
    free(memstream->buffer);
}

void nak_nir_asprint_instr(struct nak_memstream *memstream, const nir_instr *instr)
{
    nir_print_instr(instr, memstream->stream);
    fflush(memstream->stream);
}

void nak_clear_memstream(struct nak_memstream *memstream)
{
    rewind(memstream->stream);
}