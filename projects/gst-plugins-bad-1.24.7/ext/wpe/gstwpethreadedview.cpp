/* Copyright (C) <2018, 2019, 2020> Philippe Normand <philn@igalia.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwpe.h"
#include "gstwpesrcbin.h"
#include "gstwpethreadedview.h"

#include <gst/gl/gl.h>
#include <gst/gl/egl/gsteglimage.h>
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <wayland-server.h>

#include <cstdio>
#include <mutex>

#include <wpe/unstable/fdo-shm.h>

GST_DEBUG_CATEGORY_EXTERN (wpe_view_debug);
#define GST_CAT_DEFAULT wpe_view_debug

class GMutexHolder {
public:
    GMutexHolder(GMutex& mutex)
        : m(mutex)
    {
        g_mutex_lock(&m);
    }
    ~GMutexHolder()
    {
        g_mutex_unlock(&m);
    }

private:
    GMutex& m;
};

static GstWPEContextThread *s_view = NULL;

GstWPEContextThread& GstWPEContextThread::singleton()
{
    static gsize initialized = 0;

    if (g_once_init_enter (&initialized)) {
        s_view = new GstWPEContextThread;

        g_once_init_leave (&initialized, 1);
    }

    return *s_view;
}

GstWPEContextThread::GstWPEContextThread()
{
    g_mutex_init(&threading.mutex);
    g_cond_init(&threading.cond);
    threading.ready = FALSE;

    {
        GMutexHolder lock(threading.mutex);
        threading.thread = g_thread_new("GstWPEContextThread", s_viewThread, this);
        while (!threading.ready) {
            g_cond_wait(&threading.cond, &threading.mutex);
        }
        GST_DEBUG("thread spawned");
    }
}

GstWPEContextThread::~GstWPEContextThread()
{
    if (threading.thread) {
        g_thread_unref(threading.thread);
        threading.thread = nullptr;
    }

    g_mutex_clear(&threading.mutex);
    g_cond_clear(&threading.cond);
}

template<typename Function>
void GstWPEContextThread::dispatch(Function func)
{
    struct Job {
        Job(Function& f)
            : func(f)
        {
            g_mutex_init(&mutex);
            g_cond_init(&cond);
            dispatched = FALSE;
        }
        ~Job() {
            g_mutex_clear(&mutex);
            g_cond_clear(&cond);
        }

        void dispatch() {
            GMutexHolder lock(mutex);
            func();
            dispatched = TRUE;
            g_cond_signal(&cond);
        }

        void waitCompletion() {
            GMutexHolder lock(mutex);
            while(!dispatched) {
                g_cond_wait(&cond, &mutex);
            }
        }

        Function& func;
        GMutex mutex;
        GCond cond;
        gboolean dispatched;
    };

    struct Job job(func);
    GSource* source = g_idle_source_new();
    g_source_set_callback(source, [](gpointer data) -> gboolean {
        auto* job = static_cast<struct Job*>(data);
        job->dispatch();
        return G_SOURCE_REMOVE;
    }, &job, nullptr);
    g_source_set_priority(source, G_PRIORITY_DEFAULT);
    g_source_attach(source, glib.context);
    job.waitCompletion();
    g_source_unref(source);
}

gpointer GstWPEContextThread::s_viewThread(gpointer data)
{
    auto& view = *static_cast<GstWPEContextThread*>(data);

    view.glib.context = g_main_context_new();
    view.glib.loop = g_main_loop_new(view.glib.context, FALSE);

    g_main_context_push_thread_default(view.glib.context);

    {
        GSource* source = g_idle_source_new();
        g_source_set_callback(source,
            [](gpointer data) -> gboolean {
                auto& view = *static_cast<GstWPEContextThread*>(data);
                GMutexHolder lock(view.threading.mutex);
                view.threading.ready = TRUE;
                g_cond_signal(&view.threading.cond);
                return G_SOURCE_REMOVE;
            },
            &view, nullptr);
        g_source_attach(source, view.glib.context);
        g_source_unref(source);
    }

    g_main_loop_run(view.glib.loop);

    g_main_loop_unref(view.glib.loop);
    view.glib.loop = nullptr;

    g_main_context_pop_thread_default(view.glib.context);
    g_main_context_unref(view.glib.context);
    view.glib.context = nullptr;
    return nullptr;
}

#ifdef G_OS_UNIX
static void
initialize_web_extensions (WebKitWebContext *context)
{
    const gchar *local_path = gst_wpe_get_devenv_extension_path ();
    const gchar *path = g_file_test (local_path, G_FILE_TEST_IS_DIR) ? local_path : G_STRINGIFY (WPE_EXTENSION_INSTALL_DIR);
    GST_INFO ("Loading WebExtension from %s", path);
#if USE_WPE2
    webkit_web_context_set_web_process_extensions_directory (context, path);
#else
    webkit_web_context_set_web_extensions_directory (context, path);
#endif
}

static void
webkit_extension_gerror_msg_received (GstWpeSrc *src, GVariant *params)
{
    GstStructure *structure;
    GstMessage *forwarded;
    const gchar *src_path, *src_type, *src_name, *error_domain, *msg, *debug_str, *details_str;
    gint message_type;
    guint32 error_code;

    g_variant_get (params, "(issssusss)",
       &message_type,
       &src_type,
       &src_name,
       &src_path,
       &error_domain,
       &error_code,
       &msg,
       &debug_str,
       &details_str
    );

    GError *error = g_error_new(g_quark_from_string(error_domain), error_code, "%s", msg);
    GstStructure *details = (details_str[0] != '\0') ? gst_structure_new_from_string(details_str) : NULL;
    gchar * our_message = g_strdup_printf(
        "`%s` posted from %s running inside the web page",
        debug_str, src_path
    );


    if (message_type == GST_MESSAGE_ERROR) {
        forwarded =
            gst_message_new_error_with_details(GST_OBJECT(src), error,
                                               our_message, details);
    } else if (message_type == GST_MESSAGE_WARNING) {
        forwarded =
            gst_message_new_warning_with_details(GST_OBJECT(src), error,
                                                 our_message, details);
    } else {
        forwarded =
            gst_message_new_info_with_details(GST_OBJECT(src), error, our_message, details);
    }

    structure = gst_structure_new ("WpeForwarded",
        "message", GST_TYPE_MESSAGE, forwarded,
        "wpe-original-src-name", G_TYPE_STRING, src_name,
        "wpe-original-src-type", G_TYPE_STRING, src_type,
        "wpe-original-src-path", G_TYPE_STRING, src_path,
        NULL
    );

    g_free (our_message);
    gst_element_post_message(GST_ELEMENT(src), gst_message_new_custom(GST_MESSAGE_ELEMENT,
                                                                      GST_OBJECT(src), structure));
    g_error_free(error);
    gst_message_unref (forwarded);
}

static void
webkit_extension_bus_message_received (GstWpeSrc *src, GVariant *params)
{
    GstStructure *original_structure, *structure;
    const gchar *src_name, *src_type, *src_path, *struct_str;
    GstMessageType message_type;
    GstMessage *forwarded;

    g_variant_get (params, "(issss)",
       &message_type,
       &src_name,
       &src_type,
       &src_path,
       &struct_str
    );

    original_structure = (struct_str[0] != '\0') ? gst_structure_new_from_string(struct_str) : NULL;
    if (!original_structure)
    {
        if (struct_str[0] != '\0')
            GST_ERROR_OBJECT(src, "Could not deserialize: %s", struct_str);
        original_structure = gst_structure_new_empty("wpesrc");

    }

    forwarded = gst_message_new_custom(message_type,
        GST_OBJECT (src), original_structure);
    structure = gst_structure_new ("WpeForwarded",
        "message", GST_TYPE_MESSAGE, forwarded,
        "wpe-original-src-name", G_TYPE_STRING, src_name,
        "wpe-original-src-type", G_TYPE_STRING, src_type,
        "wpe-original-src-path", G_TYPE_STRING, src_path,
        NULL
    );

    gst_element_post_message(GST_ELEMENT(src), gst_message_new_custom(GST_MESSAGE_ELEMENT,
                                                                      GST_OBJECT(src), structure));

    gst_message_unref (forwarded);
}

static gboolean
webkit_extension_msg_received (WebKitWebContext  *context,
               WebKitUserMessage *message,
               GstWpeSrc           *src)
{
    const gchar *name = webkit_user_message_get_name (message);
    GVariant *params = webkit_user_message_get_parameters (message);
    gboolean res = TRUE;

    GST_TRACE_OBJECT(src, "Handling message %s", name);
    if (!g_strcmp0(name, "gstwpe.new_stream")) {
        guint32 id = g_variant_get_uint32 (g_variant_get_child_value (params, 0));
        const gchar *capsstr = g_variant_get_string (g_variant_get_child_value (params, 1), NULL);
        GstCaps *caps = gst_caps_from_string (capsstr);
        const gchar *stream_id = g_variant_get_string (g_variant_get_child_value (params, 2), NULL);
        gst_wpe_src_new_audio_stream(src, id, caps, stream_id);
        gst_caps_unref (caps);
    } else if (!g_strcmp0(name, "gstwpe.set_shm")) {
        auto fdlist = webkit_user_message_get_fd_list (message);
        gint id = g_variant_get_uint32 (g_variant_get_child_value (params, 0));
        gst_wpe_src_set_audio_shm (src, fdlist, id);
    } else if (!g_strcmp0(name, "gstwpe.new_buffer")) {
        guint32 id = g_variant_get_uint32 (g_variant_get_child_value (params, 0));
        guint64 size = g_variant_get_uint64 (g_variant_get_child_value (params, 1));
        gst_wpe_src_push_audio_buffer (src, id, size);

        webkit_user_message_send_reply(message, webkit_user_message_new ("gstwpe.buffer_processed", NULL));
    } else if (!g_strcmp0(name, "gstwpe.pause")) {
        guint32 id = g_variant_get_uint32 (params);

        gst_wpe_src_pause_audio_stream (src, id);
    } else if (!g_strcmp0(name, "gstwpe.stop")) {
        guint32 id = g_variant_get_uint32 (params);

        gst_wpe_src_stop_audio_stream (src, id);
    } else if (!g_strcmp0(name, "gstwpe.bus_gerror_message")) {
        webkit_extension_gerror_msg_received (src, params);
    } else if (!g_strcmp0(name, "gstwpe.bus_message")) {
        webkit_extension_bus_message_received (src, params);
    } else if (!g_strcmp0(name, "gstwpe.console_message")) {
        const gchar *message = g_variant_get_string (g_variant_get_child_value (params, 0), NULL);
        GstStructure *structure = gst_structure_new ("wpe-console-message",
            "message", G_TYPE_STRING, message,
            NULL);

        gst_element_post_message(GST_ELEMENT(src), gst_message_new_custom(GST_MESSAGE_ELEMENT,
            GST_OBJECT(src), structure));
    } else {
        res = FALSE;
        g_error("Unknown event: %s", name);
    }

    return res;
}
#endif

GstWPEThreadedView* GstWPEContextThread::createWPEView(GstWpeVideoSrc* src, GstGLContext* context, GstGLDisplay* display, int width, int height)
{
    GST_DEBUG("context %p display %p, size (%d,%d)", context, display, width, height);

    static std::once_flag s_loaderFlag;
    std::call_once(s_loaderFlag,
        [] {
            wpe_loader_init("libWPEBackend-fdo-1.0.so");
        });

    GstWPEThreadedView* view = nullptr;
    dispatch([&]() mutable {
        if (!glib.web_context) {
#if USE_WPE2
            glib.web_context = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, nullptr));
#else
            auto *manager = webkit_website_data_manager_new(NULL);
            glib.web_context =
                webkit_web_context_new_with_website_data_manager(manager);
            g_object_unref(manager);
#endif
        }
        view = new GstWPEThreadedView(glib.web_context, src, context, display, width, height);
    });

    if (view && view->hasUri()) {
        GST_DEBUG("waiting load to finish");
        view->waitLoadCompletion();
        GST_DEBUG("done");
    }

    return view;
}

static gboolean s_loadFailed(WebKitWebView*, WebKitLoadEvent, gchar* failing_uri, GError* error, gpointer data)
{
    GstWpeVideoSrc* src = GST_WPE_VIDEO_SRC(data);

    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)) {
        GST_INFO_OBJECT (src, "Loading cancelled.");

        return FALSE;
    }

    GST_ELEMENT_ERROR (GST_ELEMENT_CAST(src), RESOURCE, FAILED, (NULL), ("Failed to load %s (%s)", failing_uri, error->message));
    return FALSE;
}

static gboolean s_loadFailedWithTLSErrors(WebKitWebView*,  gchar* failing_uri, GTlsCertificate*, GTlsCertificateFlags, gpointer data)
{
    // Defer to load-failed.
    return FALSE;
}

static void s_loadProgressChanged(GObject* object, GParamSpec*, gpointer data)
{
    GstElement* src = GST_ELEMENT_CAST (data);
    // The src element is locked already so we can't call
    // gst_element_post_message(). Instead retrieve the bus manually and use it
    // directly.
    GstBus* bus = GST_ELEMENT_BUS (src);
    double estimatedProgress;
    g_object_get(object, "estimated-load-progress", &estimatedProgress, nullptr);
    gst_object_ref (bus);
    gst_bus_post (bus, gst_message_new_element(GST_OBJECT_CAST(src), gst_structure_new("wpe-stats", "estimated-load-progress", G_TYPE_DOUBLE, estimatedProgress * 100, nullptr)));
    gst_object_unref (bus);
}

static void s_webProcessCrashed(WebKitWebView*, WebKitWebProcessTerminationReason reason, gpointer data)
{
    auto &view = *static_cast<GstWPEThreadedView *>(data);
    auto *src = view.src();
    gchar *reason_str =
        g_enum_to_string (WEBKIT_TYPE_WEB_PROCESS_TERMINATION_REASON, reason);

    // In case the crash happened while doing the initial URL loading, unlock
    // the load completion waiting.
    view.notifyLoadFinished();

    // TODO: Emit a signal here and fallback to error system if signal wasn't handled by application?

    GST_ELEMENT_ERROR(GST_ELEMENT_CAST(src), RESOURCE, FAILED, (NULL), ("%s", reason_str));

    g_free (reason_str);
}

GstWPEThreadedView::GstWPEThreadedView(WebKitWebContext* web_context, GstWpeVideoSrc* src, GstGLContext* context, GstGLDisplay* display, int width, int height)
  : m_src(src)
{
#ifdef G_OS_UNIX
{
        GstObject *parent = gst_object_get_parent (GST_OBJECT (src));

        if (parent && GST_IS_WPE_SRC (parent)) {
            audio.init_ext_sigid = g_signal_connect (web_context,
#if USE_WPE2
                              "initialize-web-process-extensions",
#else
                              "initialize-web-extensions",
#endif
                              G_CALLBACK (initialize_web_extensions),
                              NULL);
            audio.extension_msg_sigid = g_signal_connect (web_context,
                                "user-message-received",
                                G_CALLBACK (webkit_extension_msg_received),
                                parent);
            GST_INFO_OBJECT (parent, "Enabled audio");
        }

        gst_clear_object (&parent);
}
#endif // G_OS_UNIX

    g_mutex_init(&threading.ready_mutex);
    g_cond_init(&threading.ready_cond);
    threading.ready = FALSE;

    g_mutex_init(&images_mutex);
    if (context)
        gst.context = GST_GL_CONTEXT(gst_object_ref(context));
    if (display) {
        gst.display = GST_GL_DISPLAY(gst_object_ref(display));
    }

    wpe.width = width;
    wpe.height = height;

    if (context && display) {
      if (gst_gl_context_get_gl_platform(context) == GST_GL_PLATFORM_EGL) {
        gst.display_egl = gst_gl_display_egl_from_gl_display (gst.display);
      } else {
        GST_DEBUG ("Available GStreamer GL Context is not EGL - not creating an EGL display from it");
      }
    }

    if (gst.display_egl) {
        EGLDisplay eglDisplay = (EGLDisplay)gst_gl_display_get_handle (GST_GL_DISPLAY(gst.display_egl));
        GST_DEBUG("eglDisplay %p", eglDisplay);

        m_isValid = wpe_fdo_initialize_for_egl_display(eglDisplay);
        GST_DEBUG("FDO EGL display initialisation result: %d", m_isValid);
    } else {
        m_isValid = wpe_fdo_initialize_shm();
        GST_DEBUG("FDO SHM initialisation result: %d", m_isValid);
    }
    if (!m_isValid)
        return;

    if (gst.display_egl) {
        wpe.exportable = wpe_view_backend_exportable_fdo_egl_create(&s_exportableEGLClient, this, wpe.width, wpe.height);
    } else {
        wpe.exportable = wpe_view_backend_exportable_fdo_create(&s_exportableClient, this, wpe.width, wpe.height);
    }

    auto* wpeViewBackend = wpe_view_backend_exportable_fdo_get_view_backend(wpe.exportable);
    auto* viewBackend = webkit_web_view_backend_new(wpeViewBackend, (GDestroyNotify) wpe_view_backend_exportable_fdo_destroy, wpe.exportable);
    wpe_view_backend_add_activity_state(wpeViewBackend, wpe_view_activity_state_visible | wpe_view_activity_state_focused | wpe_view_activity_state_in_window);

    webkit.view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", web_context,
        "backend", viewBackend,
        nullptr));

    g_signal_connect(webkit.view, "load-failed", G_CALLBACK(s_loadFailed), src);
    g_signal_connect(webkit.view, "load-failed-with-tls-errors", G_CALLBACK(s_loadFailedWithTLSErrors), src);
    g_signal_connect(webkit.view, "notify::estimated-load-progress", G_CALLBACK(s_loadProgressChanged), src);
    g_signal_connect(webkit.view, "web-process-terminated", G_CALLBACK(s_webProcessCrashed), this);

    auto* settings = webkit_web_view_get_settings(webkit.view);
    webkit_settings_set_enable_webaudio(settings, TRUE);

    gst_wpe_video_src_configure_web_view(src, webkit.view);

    gchar* location;
    gboolean drawBackground = TRUE;
    g_object_get(src, "location", &location, "draw-background", &drawBackground, nullptr);
    setDrawBackground(drawBackground);
    if (location) {
        loadUriUnlocked(location);
        g_free(location);
    }
}

GstWPEThreadedView::~GstWPEThreadedView()
{
    GstEGLImage *egl_pending = NULL;
    GstEGLImage *egl_committed = NULL;
    GstBuffer *shm_pending = NULL;
    GstBuffer *shm_committed = NULL;
    GST_TRACE ("%p destroying", this);

    g_mutex_clear(&threading.ready_mutex);
    g_cond_clear(&threading.ready_cond);

    {
        GMutexHolder lock(images_mutex);

        if (egl.pending) {
            egl_pending = egl.pending;
            egl.pending = nullptr;
        }
        if (egl.committed) {
            egl_committed = egl.committed;
            egl.committed = nullptr;
        }
        if (shm.pending) {
            GST_TRACE ("%p freeing shm pending %" GST_PTR_FORMAT, this, shm.pending);
            shm_pending = shm.pending;
            shm.pending = nullptr;
        }
        if (shm.committed) {
            GST_TRACE ("%p freeing shm commited %" GST_PTR_FORMAT, this, shm.committed);
            shm_committed = shm.committed;
            shm.committed = nullptr;
        }
    }

    if (egl_pending)
        gst_egl_image_unref (egl_pending);
    if (egl_committed)
        gst_egl_image_unref (egl_committed);
    if (shm_pending)
        gst_buffer_unref (shm_pending);
    if (shm_committed)
        gst_buffer_unref (shm_committed);

    if (audio.init_ext_sigid) {
        WebKitWebContext* web_context = webkit_web_view_get_context (webkit.view);

        g_signal_handler_disconnect(web_context, audio.init_ext_sigid);
        g_signal_handler_disconnect(web_context, audio.extension_msg_sigid);
        audio.init_ext_sigid = 0;
        audio.extension_msg_sigid = 0;
    }

    GstWPEContextThread::singleton().dispatch([&]() {
        if (webkit.view) {
            g_object_unref(webkit.view);
            webkit.view = nullptr;
        }
    });

    if (gst.display_egl) {
        gst_object_unref(gst.display_egl);
        gst.display_egl = nullptr;
    }

    if (gst.display) {
        gst_object_unref(gst.display);
        gst.display = nullptr;
    }

    if (gst.context) {
        gst_object_unref(gst.context);
        gst.context = nullptr;
    }
    if (webkit.uri) {
        g_free(webkit.uri);
        webkit.uri = nullptr;
    }

    g_mutex_clear(&images_mutex);
    GST_TRACE ("%p destroyed", this);
}

void GstWPEThreadedView::notifyLoadFinished()
{
    GMutexHolder lock(threading.ready_mutex);
    if (!threading.ready) {
        threading.ready = TRUE;
        g_cond_signal(&threading.ready_cond);
    }
}

void GstWPEThreadedView::waitLoadCompletion()
{
    GMutexHolder lock(threading.ready_mutex);
    while (!threading.ready)
        g_cond_wait(&threading.ready_cond, &threading.ready_mutex);
}

GstEGLImage* GstWPEThreadedView::image()
{
    GstEGLImage* ret = nullptr;
    bool dispatchFrameComplete = false;
    GstEGLImage *prev_image = NULL;

    {
        GMutexHolder lock(images_mutex);

        GST_TRACE("pending %" GST_PTR_FORMAT " (%d) committed %" GST_PTR_FORMAT " (%d)", egl.pending,
                  GST_IS_EGL_IMAGE(egl.pending) ? GST_MINI_OBJECT_REFCOUNT_VALUE(GST_MINI_OBJECT_CAST(egl.pending)) : 0,
                  egl.committed,
                  GST_IS_EGL_IMAGE(egl.committed) ? GST_MINI_OBJECT_REFCOUNT_VALUE(GST_MINI_OBJECT_CAST(egl.committed)) : 0);

        if (egl.pending) {
            prev_image = egl.committed;
            egl.committed = egl.pending;
            egl.pending = nullptr;

            dispatchFrameComplete = true;
        }

        if (egl.committed)
            ret = egl.committed;
    }

    if (prev_image)
        gst_egl_image_unref(prev_image);

    if (dispatchFrameComplete)
        frameComplete();

    return ret;
}

GstBuffer* GstWPEThreadedView::buffer()
{
    GstBuffer* ret = nullptr;
    bool dispatchFrameComplete = false;
    GstBuffer *prev_image = NULL;

    {
        GMutexHolder lock(images_mutex);

        GST_TRACE("pending %" GST_PTR_FORMAT " (%d) committed %" GST_PTR_FORMAT " (%d)", shm.pending,
                  GST_IS_BUFFER(shm.pending) ? GST_MINI_OBJECT_REFCOUNT_VALUE(GST_MINI_OBJECT_CAST(shm.pending)) : 0,
                  shm.committed,
                  GST_IS_BUFFER(shm.committed) ? GST_MINI_OBJECT_REFCOUNT_VALUE(GST_MINI_OBJECT_CAST(shm.committed)) : 0);

        if (shm.pending) {
            prev_image = shm.committed;
            shm.committed = shm.pending;
            shm.pending = nullptr;

            dispatchFrameComplete = true;
        }

        if (shm.committed)
            ret = shm.committed;
    }

    if (prev_image)
        gst_buffer_unref(prev_image);

    if (dispatchFrameComplete)
        frameComplete();

    return ret;
}

void GstWPEThreadedView::resize(int width, int height)
{
    GST_DEBUG("resize to %dx%d", width, height);
    wpe.width = width;
    wpe.height = height;

    s_view->dispatch([&]() {
        if (wpe.exportable && wpe_view_backend_exportable_fdo_get_view_backend(wpe.exportable))
          wpe_view_backend_dispatch_set_size(wpe_view_backend_exportable_fdo_get_view_backend(wpe.exportable), wpe.width, wpe.height);
    });
}

void GstWPEThreadedView::frameComplete()
{
    GST_TRACE("frame complete");
    s_view->dispatch([&]() {
        GST_TRACE("dispatching");
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(wpe.exportable);
    });
}

void GstWPEThreadedView::loadUriUnlocked(const gchar* uri)
{
    if (webkit.uri)
        g_free(webkit.uri);

    GST_DEBUG("loading %s", uri);
    webkit.uri = g_strdup(uri);
    webkit_web_view_load_uri(webkit.view, webkit.uri);
}

void GstWPEThreadedView::loadUri(const gchar* uri)
{
    s_view->dispatch([&]() {
        loadUriUnlocked(uri);
    });
}

static void s_runJavascriptFinished(GObject* object, GAsyncResult* result, gpointer user_data)
{
    GError *error = NULL;
#if USE_WPE2
    g_autoptr(JSCValue) js_result = webkit_web_view_evaluate_javascript_finish(
#else
    g_autoptr(WebKitJavascriptResult) js_result = webkit_web_view_run_javascript_finish(
#endif
        WEBKIT_WEB_VIEW(object), result, &error);

    // TODO: Pass result back to signal call site using a GstPromise?
    (void) js_result;

    if (error) {
        GST_WARNING("Error running javascript: %s", error->message);
        g_error_free(error);
    }
}

void GstWPEThreadedView::runJavascript(const char* script)
{
    s_view->dispatch([&]() {
#if USE_WPE2
        webkit_web_view_evaluate_javascript(webkit.view, script, -1, nullptr, nullptr, nullptr,
                                            s_runJavascriptFinished, nullptr);
#else
        webkit_web_view_run_javascript(webkit.view, script, nullptr, s_runJavascriptFinished, nullptr);
#endif
    });
}

void GstWPEThreadedView::loadData(GBytes* bytes)
{
    s_view->dispatch([this, bytes = g_bytes_ref(bytes)]() {
        webkit_web_view_load_bytes(webkit.view, bytes, nullptr, nullptr, nullptr);
        g_bytes_unref(bytes);
    });
}

void GstWPEThreadedView::setDrawBackground(gboolean drawsBackground)
{
    GST_DEBUG("%s background rendering", drawsBackground ? "Enabling" : "Disabling");
    WebKitColor color;
    webkit_color_parse(&color, drawsBackground ? "white" : "transparent");
    webkit_web_view_set_background_color(webkit.view, &color);
}

void GstWPEThreadedView::releaseImage(gpointer imagePointer)
{
    s_view->dispatch([&]() {
        GST_TRACE("Dispatch release exported image %p", imagePointer);
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(wpe.exportable,
                                                                            static_cast<struct wpe_fdo_egl_exported_image*>(imagePointer));
    });
}

struct ImageContext {
    GstWPEThreadedView* view;
    gpointer image;
};

void GstWPEThreadedView::handleExportedImage(gpointer image)
{
    ImageContext* imageContext = g_new (ImageContext, 1);
    imageContext->view = this;
    imageContext->image = static_cast<gpointer>(image);
    EGLImageKHR eglImage = wpe_fdo_egl_exported_image_get_egl_image(static_cast<struct wpe_fdo_egl_exported_image*>(image));

    auto* gstImage = gst_egl_image_new_wrapped(gst.context, eglImage, GST_GL_RGBA, imageContext, s_releaseImage);
    {
      GMutexHolder lock(images_mutex);

      GST_TRACE("EGLImage %p wrapped in GstEGLImage %" GST_PTR_FORMAT, eglImage, gstImage);
      gst_clear_mini_object ((GstMiniObject **) &egl.pending);
      egl.pending = gstImage;

      notifyLoadFinished();
    }
}

struct SHMBufferContext {
    GstWPEThreadedView* view;
    struct wpe_fdo_shm_exported_buffer* buffer;
};

void GstWPEThreadedView::releaseSHMBuffer(gpointer data)
{
    SHMBufferContext* context = static_cast<SHMBufferContext*>(data);
    s_view->dispatch([&]() {
        auto* buffer = static_cast<struct wpe_fdo_shm_exported_buffer*>(context->buffer);
        GST_TRACE("Dispatch release exported buffer %p", buffer);
        wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(wpe.exportable, buffer);
    });
}

void GstWPEThreadedView::s_releaseSHMBuffer(gpointer data)
{
    SHMBufferContext* context = static_cast<SHMBufferContext*>(data);
    context->view->releaseSHMBuffer(data);
    g_free (context);
}

void GstWPEThreadedView::handleExportedBuffer(struct wpe_fdo_shm_exported_buffer* buffer)
{
    struct wl_shm_buffer* shmBuffer = wpe_fdo_shm_exported_buffer_get_shm_buffer(buffer);
    auto format = wl_shm_buffer_get_format(shmBuffer);
    if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888) {
        GST_ERROR("Unsupported pixel format: %d", format);
        return;
    }

    int32_t width = wl_shm_buffer_get_width(shmBuffer);
    int32_t height = wl_shm_buffer_get_height(shmBuffer);
    gint stride = wl_shm_buffer_get_stride(shmBuffer);
    gsize size = width * height * 4;
    auto* data = static_cast<uint8_t*>(wl_shm_buffer_get_data(shmBuffer));

    SHMBufferContext* bufferContext = g_new (SHMBufferContext, 1);
    bufferContext->view = this;
    bufferContext->buffer = buffer;

    auto* gstBuffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, data, size, 0, size, bufferContext, s_releaseSHMBuffer);
    gsize offsets[1];
    gint strides[1];
    offsets[0] = 0;
    strides[0] = stride;
    gst_buffer_add_video_meta_full(gstBuffer, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_BGRA, width, height, 1, offsets, strides);

    {
        GMutexHolder lock(images_mutex);
        GST_TRACE("SHM buffer %p wrapped in buffer %" GST_PTR_FORMAT, buffer, gstBuffer);
        gst_clear_buffer (&shm.pending);
        shm.pending = gstBuffer;
        notifyLoadFinished();
    }
}

struct wpe_view_backend_exportable_fdo_egl_client GstWPEThreadedView::s_exportableEGLClient = {
    // export_egl_image
    nullptr,
    [](void* data, struct wpe_fdo_egl_exported_image* image) {
        auto& view = *static_cast<GstWPEThreadedView*>(data);
        view.handleExportedImage(static_cast<gpointer>(image));
    },
    nullptr,
    // padding
    nullptr, nullptr
};

struct wpe_view_backend_exportable_fdo_client GstWPEThreadedView::s_exportableClient = {
    nullptr,
    nullptr,
    // export_shm_buffer
    [](void* data, struct wpe_fdo_shm_exported_buffer* buffer) {
        auto& view = *static_cast<GstWPEThreadedView*>(data);
        view.handleExportedBuffer(buffer);
    },
    nullptr,
    nullptr,
};

void GstWPEThreadedView::s_releaseImage(GstEGLImage* image, gpointer data)
{
    ImageContext* context = static_cast<ImageContext*>(data);
    context->view->releaseImage(context->image);
    g_free (context);
}

struct wpe_view_backend* GstWPEThreadedView::backend() const
{
    return wpe.exportable ? wpe_view_backend_exportable_fdo_get_view_backend(wpe.exportable) : nullptr;
}

void GstWPEThreadedView::dispatchKeyboardEvent(struct wpe_input_keyboard_event& wpe_event)
{
    s_view->dispatch([&]() {
        wpe_view_backend_dispatch_keyboard_event(backend(), &wpe_event);
    });
}

void GstWPEThreadedView::dispatchPointerEvent(struct wpe_input_pointer_event& wpe_event)
{
    s_view->dispatch([&]() {
        wpe_view_backend_dispatch_pointer_event(backend(), &wpe_event);
    });
}

void GstWPEThreadedView::dispatchAxisEvent(struct wpe_input_axis_event& wpe_event)
{
    s_view->dispatch([&]() {
        wpe_view_backend_dispatch_axis_event(backend(), &wpe_event);
    });
}

void GstWPEThreadedView::dispatchTouchEvent(struct wpe_input_touch_event& wpe_event)
{
    s_view->dispatch([&]() {
        wpe_view_backend_dispatch_touch_event(backend(), &wpe_event);
    });
}
