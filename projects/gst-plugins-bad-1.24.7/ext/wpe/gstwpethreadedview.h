/* Copyright (C) <2018> Philippe Normand <philn@igalia.com>
 * Copyright (C) <2018> Žan Doberšek <zdobersek@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <EGL/egl.h>
#include <glib.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <wpe/fdo.h>
#include <wpe/fdo-egl.h>
#include <wpe/webkit.h>
#include "gstwpevideosrc.h"

typedef struct _GstGLContext GstGLContext;
typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstEGLImage GstEGLImage;

class GstWPEThreadedView {
public:
    GstWPEThreadedView(WebKitWebContext*, GstWpeVideoSrc*, GstGLContext*, GstGLDisplay*, int width, int height);
    ~GstWPEThreadedView();

    bool operator!() const { return m_isValid; }

    /*  Used by wpevideosrc */
    void resize(int width, int height);
    void loadUri(const gchar*);
    void loadData(GBytes*);
    void runJavascript(const gchar*);
    void setDrawBackground(gboolean);

    GstEGLImage* image();
    GstBuffer* buffer();

    void dispatchKeyboardEvent(struct wpe_input_keyboard_event&);
    void dispatchPointerEvent(struct wpe_input_pointer_event&);
    void dispatchAxisEvent(struct wpe_input_axis_event&);
    void dispatchTouchEvent(struct wpe_input_touch_event&);

    /*  Used by GstWPEContextThread */
    bool hasUri() const { return webkit.uri; }
    void disconnectLoadFailedSignal();
    void waitLoadCompletion();

    GstWpeVideoSrc *src() const { return m_src; }

    void notifyLoadFinished();

protected:
    void handleExportedImage(gpointer);
    void handleExportedBuffer(struct wpe_fdo_shm_exported_buffer*);

private:
    struct wpe_view_backend* backend() const;
    void frameComplete();
    void loadUriUnlocked(const gchar*);

    void releaseImage(gpointer);
    void releaseSHMBuffer(gpointer);
    static void s_releaseSHMBuffer(gpointer);

    struct {
        GstGLContext* context;
        GstGLDisplay* display;
        GstGLDisplayEGL* display_egl;
    } gst { nullptr, nullptr, nullptr };

    static struct wpe_view_backend_exportable_fdo_egl_client s_exportableEGLClient;
    static struct wpe_view_backend_exportable_fdo_client s_exportableClient;

    static void s_releaseImage(GstEGLImage*, gpointer);
    struct {
        struct wpe_view_backend_exportable_fdo* exportable;
        int width;
        int height;
    } wpe { nullptr, 0, 0, };

    struct {
        gchar* uri;
        WebKitWebView* view;
    } webkit = { nullptr, nullptr };

    bool m_isValid { false };

    struct {
        GMutex ready_mutex;
        GCond ready_cond;
        gboolean ready;
    } threading;

    // This mutex guards access to either egl or shm resources declared below,
    // depending on the runtime behavior.
    GMutex images_mutex;

    struct {
        GstEGLImage* pending;
        GstEGLImage* committed;
    } egl { nullptr, nullptr };

    struct {
        GstBuffer* pending;
        GstBuffer* committed;
    } shm { nullptr, nullptr };

    struct {
        gulong init_ext_sigid;
        gulong extension_msg_sigid;
    } audio {0, 0};

    GstWpeVideoSrc *m_src { nullptr };
};

class GstWPEContextThread {
public:
    static GstWPEContextThread& singleton();

    GstWPEContextThread();
    ~GstWPEContextThread();

    GstWPEThreadedView* createWPEView(GstWpeVideoSrc*, GstGLContext*, GstGLDisplay*, int width, int height);

    template<typename Function>
    void dispatch(Function);

private:
    static gpointer s_viewThread(gpointer);
    struct {
        GMutex mutex;
        GCond cond;
        gboolean ready;
        GThread* thread { nullptr };
    } threading;

    struct {
        GMainContext* context;
        GMainLoop* loop;
        WebKitWebContext* web_context;
    } glib { nullptr, nullptr, nullptr };
};
