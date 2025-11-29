#ifndef GDI_SW_WINSYS_H
#define GDI_SW_WINSYS_H

#include <windows.h>

#include "util/compiler.h"
#include "frontend/sw_winsys.h"

void gdi_sw_display( struct sw_winsys *winsys,
                     struct sw_displaytarget *dt,
                     HDC hDC );

struct sw_winsys *
gdi_create_sw_winsys(
    /* Following functions are used to acquire HDC to draw on
     * from winsys_drawable_handle argument of screen->flush_frontbuffer
     */
    HDC (*acquire_hdc)(void *winsys_drawable_handle),
    void (*release_hdc)(void *winsys_drawable_handle, HDC hdc)
);

/* Used when winsys_drawable_handle is HDC itself */
HDC gdi_sw_acquire_hdc_by_value(void *context_private);
void gdi_sw_release_hdc_by_value(void *context_private, HDC hdc);
#endif
