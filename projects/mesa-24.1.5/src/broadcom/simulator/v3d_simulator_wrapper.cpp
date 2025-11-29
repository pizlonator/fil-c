/*
 * Copyright © 2017 Broadcom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file
 *
 * Wraps bits of the V3D simulator interface in a C interface for the
 * v3d_simulator.c code to use.
 */

#ifdef USE_V3D_SIMULATOR

#include "v3d_simulator_wrapper.h"
#include "v3d_hw_auto.h"

extern "C" {

struct v3d_hw *v3d_hw_auto_new(void *in_params)
{
        return v3d_hw_auto_make_unique().release();
}

uint64_t v3d_hw_get_mem(const struct v3d_hw *hw, uint64_t *size)
{
        uint64_t addr;
        assert(hw->get_mem(&addr, size));
        return addr;
}

void v3d_hw_set_mem(struct v3d_hw *hw, uint64_t addr, uint8_t value, uint64_t size)
{
        hw->set_mem(addr, value, size);
}

void v3d_hw_write_mem(struct v3d_hw *hw, uint64_t addr, const void *p, uint64_t size)
{
        hw->write_mem(addr, p, size);
}

void v3d_hw_read_mem(struct v3d_hw *hw, void *p, uint64_t addr, uint64_t size)
{
        hw->read_mem(p, addr, size);
}

bool v3d_hw_alloc_mem(struct v3d_hw *hw, uint64_t min_size)
{
        return hw->alloc_mem(min_size) == V3D_HW_ALLOC_SUCCESS;
}

uint32_t v3d_hw_read_reg(struct v3d_hw *hw, uint32_t reg)
{
        return hw->read_reg(reg);
}

void v3d_hw_write_reg(struct v3d_hw *hw, uint32_t reg, uint32_t val)
{
        hw->write_reg(reg, val);
}

void v3d_hw_tick(struct v3d_hw *hw)
{
        return hw->tick();
}

int v3d_hw_get_version(struct v3d_hw *hw)
{
        const V3D_HUB_IDENT_T *ident = hw->get_hub_ident();

        return ident->tech_version * 10 + ident->revision;
}

void
v3d_hw_set_isr(struct v3d_hw *hw, void (*isr)(uint32_t status))
{
        hw->set_isr(isr);
}

uint32_t v3d_hw_get_hub_core()
{
        return V3D_HW_HUB_CORE;
}

}
#endif /* USE_V3D_SIMULATOR */
