/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_DEBUG_H_
#define _NINE_DEBUG_H_

#include "util/u_debug.h"
#include "util/compiler.h"

void
_nine_debug_printf( unsigned long flag,
                    const char *func,
                    const char *fmt,
                    ... ) _util_printf_format(3,4);

#define ERR(fmt, ...) _nine_debug_printf(DBG_ERROR, __func__, fmt, ## __VA_ARGS__)

#if MESA_DEBUG || !defined(NDEBUG)
#define WARN(fmt, ...) _nine_debug_printf(DBG_WARN, __func__, fmt, ## __VA_ARGS__)
#define WARN_ONCE(fmt, ...) \
    do { \
        static bool once = true; \
        if (once) { \
            once = false; \
            _nine_debug_printf(DBG_WARN, __func__, fmt, ## __VA_ARGS__); \
        } \
    } while(0)
#else
#define WARN(fmt, ...) do {} while(0)
#define WARN_ONCE(fmt, ...) do {} while(0)
#endif

#if MESA_DEBUG || !defined(NDEBUG)
#define DBG_FLAG(flag, fmt, ...) \
    _nine_debug_printf(flag, __func__, fmt, ## __VA_ARGS__)
#else
#define DBG_FLAG(flag, fmt, ...) do {} while(0)
#endif
#define DBG(fmt, ...) DBG_FLAG(DBG_CHANNEL, fmt, ## __VA_ARGS__)

#define DBG_UNKNOWN              (1<< 0)
#define DBG_ADAPTER              (1<< 1)
#define DBG_OVERLAYEXTENSION     (1<< 2)
#define DBG_AUTHENTICATEDCHANNEL (1<< 3)
#define DBG_BASETEXTURE          (1<< 4)
#define DBG_CRYPTOSESSION        (1<< 5)
#define DBG_CUBETEXTURE          (1<< 6)
#define DBG_DEVICE               (1<< 7)
#define DBG_DEVICEVIDEO          (1<< 8)
#define DBG_INDEXBUFFER          (1<< 9)
#define DBG_PIXELSHADER          (1<<10)
#define DBG_QUERY                (1<<11)
#define DBG_RESOURCE             (1<<12)
#define DBG_STATEBLOCK           (1<<13)
#define DBG_SURFACE              (1<<14)
#define DBG_SWAPCHAIN            (1<<15)
#define DBG_TEXTURE              (1<<16)
#define DBG_VERTEXBUFFER         (1<<17)
#define DBG_VERTEXDECLARATION    (1<<18)
#define DBG_VERTEXSHADER         (1<<19)
#define DBG_VOLUME               (1<<20)
#define DBG_VOLUMETEXTURE        (1<<21)
#define DBG_SHADER               (1<<22)
#define DBG_FF                   (1<<23)
#define DBG_USER                 (1<<24)
#define DBG_ERROR                (1<<25)
#define DBG_WARN                 (1<<26)
#define DBG_TID                  (1<<27)

void
_nine_stub( const char *file,
            const char *func,
            unsigned line );

#if MESA_DEBUG || !defined(NDEBUG)
#define STUB(ret) \
    do { \
        _nine_stub(__FILE__, __func__, __LINE__); \
        return ret; \
    } while (0)
#else
#define STUB(ret) do { return ret; } while (0)
#endif

/* the expression for this macro is equivalent of that to assert, however this
 * macro is designed to be used in conditionals ala
 * if (user_error(required condition)) { assertion failed }
 * It also prints debug message if the assertion fails. */
#if MESA_DEBUG || !defined(NDEBUG)
#define user_error(x) \
    (!(x) ? (DBG_FLAG(DBG_USER, "User assertion failed: `%s'\n", #x), true) \
          : false)
#else
#define user_error(x) (!(x) ? TRUE : FALSE)
#endif

#if MESA_DEBUG || !defined(NDEBUG)
#define user_warn(x) \
    if ((x)) { DBG_FLAG(DBG_USER, "User warning: `%s'\n", #x); }
#else
#define user_warn(x) do {} while(0)
#endif

/* nonfatal assert */
#define user_assert(x, r) \
    do { \
        if (user_error(x)) { \
            return r; \
        } \
    } while (0)

#define ret_err(x, r) \
    do { \
        ERR(x); \
        return r; \
    } while(0)

#endif /* _NINE_DEBUG_H_ */
