/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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
#include "config.h"
#endif

#include "gstasioobject.h"
#include <string.h>
#include <avrt.h>
#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <iasiodrv.h>

GST_DEBUG_CATEGORY_STATIC (gst_asio_object_debug);
#define GST_CAT_DEFAULT gst_asio_object_debug

/* List of GstAsioObject */
static GList *asio_object_list = nullptr;

/* *INDENT-OFF* */
/* Protect asio_object_list and other global values */
std::mutex global_lock;

/* Protect callback slots */
std::mutex slot_lock;
/* *INDENT-ON* */

static void gst_asio_object_buffer_switch (GstAsioObject * self,
    glong index, ASIOBool process_now);
static void gst_asio_object_sample_rate_changed (GstAsioObject * self,
    ASIOSampleRate rate);
static glong gst_asio_object_messages (GstAsioObject * self, glong selector,
    glong value, gpointer message, gdouble * opt);
static ASIOTime *gst_asio_object_buffer_switch_time_info (GstAsioObject * self,
    ASIOTime * time_info, glong index, ASIOBool process_now);

/* *INDENT-OFF* */
/* Object to delegate ASIO callbacks to dedicated GstAsioObject */
class GstAsioCallbacks
{
public:
  GstAsioCallbacks (GstAsioObject * object)
  {
    g_weak_ref_init (&object_, object);
  }

  virtual ~GstAsioCallbacks ()
  {
    g_weak_ref_clear (&object_);
  }

  void BufferSwitch (glong index, ASIOBool process_now)
  {
    GstAsioObject *obj = (GstAsioObject *) g_weak_ref_get (&object_);
    if (!obj)
      return;

    gst_asio_object_buffer_switch (obj, index, process_now);
    gst_object_unref (obj);
  }

  void SampleRateChanged (ASIOSampleRate rate)
  {
    GstAsioObject *obj = (GstAsioObject *) g_weak_ref_get (&object_);
    if (!obj)
      return;

    gst_asio_object_sample_rate_changed (obj, rate);
    gst_object_unref (obj);
  }

  glong Messages (glong selector, glong value, gpointer message, gdouble *opt)
  {
    GstAsioObject *obj = (GstAsioObject *) g_weak_ref_get (&object_);
    if (!obj)
      return 0;

    glong ret = gst_asio_object_messages (obj, selector, value, message, opt);
    gst_object_unref (obj);

    return ret;
  }

  ASIOTime * BufferSwitchTimeInfo (ASIOTime * time_info,
    glong index, ASIOBool process_now)
  {
    GstAsioObject *obj = (GstAsioObject *) g_weak_ref_get (&object_);
    if (!obj)
      return nullptr;

    ASIOTime * ret = gst_asio_object_buffer_switch_time_info (obj,
        time_info, index, process_now);
    gst_object_unref (obj);

    return ret;
  }

private:
  GWeakRef object_;
};

template <int instance_id>
class GstAsioCallbacksSlot
{
public:
  static void
  BufferSwitchStatic(glong index, ASIOBool process_now)
  {
    buffer_switch(index, process_now);
  }

  static void
  SampleRateChangedStatic (ASIOSampleRate rate)
  {
    sample_rate_changed(rate);
  }

  static glong
  MessagesStatic(glong selector, glong value, gpointer message, gdouble *opt)
  {
    return messages(selector, value, message, opt);
  }

  static ASIOTime *
  BufferSwitchTimeInfoStatic(ASIOTime * time_info, glong index,
      ASIOBool process_now)
  {
    return buffer_switch_time_info(time_info, index, process_now);
  }

  static std::function<void(glong, ASIOBool)> buffer_switch;
  static std::function<void(ASIOSampleRate)> sample_rate_changed;
  static std::function<glong(glong, glong, gpointer, gdouble *)> messages;
  static std::function<ASIOTime *(ASIOTime *, glong, ASIOBool)> buffer_switch_time_info;

  static bool bound;

  static void Init ()
  {
    buffer_switch = nullptr;
    sample_rate_changed = nullptr;
    messages = nullptr;
    buffer_switch_time_info = nullptr;
    bound = false;
  }

  static bool IsBound ()
  {
    return bound;
  }

  static void Bind (GstAsioCallbacks * cb, ASIOCallbacks * driver_cb)
  {
    buffer_switch = std::bind(&GstAsioCallbacks::BufferSwitch, cb,
      std::placeholders::_1, std::placeholders::_2);
    sample_rate_changed = std::bind(&GstAsioCallbacks::SampleRateChanged, cb,
      std::placeholders::_1);
    messages = std::bind(&GstAsioCallbacks::Messages, cb,
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
      std::placeholders::_4);
    buffer_switch_time_info = std::bind(&GstAsioCallbacks::BufferSwitchTimeInfo,
      cb, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    driver_cb->bufferSwitch = BufferSwitchStatic;
    driver_cb->sampleRateDidChange = SampleRateChangedStatic;
    driver_cb->asioMessage = MessagesStatic;
    driver_cb->bufferSwitchTimeInfo = BufferSwitchTimeInfoStatic;

    bound = true;
  }
};

template <int instance_id>
std::function<void(glong, ASIOBool)> GstAsioCallbacksSlot<instance_id>::buffer_switch;
template <int instance_id>
std::function<void(ASIOSampleRate)> GstAsioCallbacksSlot<instance_id>::sample_rate_changed;
template <int instance_id>
std::function<glong(glong, glong, gpointer, gdouble *)> GstAsioCallbacksSlot<instance_id>::messages;
template <int instance_id>
std::function<ASIOTime *(ASIOTime *, glong, ASIOBool)> GstAsioCallbacksSlot<instance_id>::buffer_switch_time_info;
template <int instance_id>
bool GstAsioCallbacksSlot<instance_id>::bound;

/* XXX: Create global slot objects,
 * because ASIO callback doesn't support user data, hum.... */
GstAsioCallbacksSlot<0> cb_slot_0;
GstAsioCallbacksSlot<1> cb_slot_1;
GstAsioCallbacksSlot<2> cb_slot_2;
GstAsioCallbacksSlot<3> cb_slot_3;
GstAsioCallbacksSlot<4> cb_slot_4;
GstAsioCallbacksSlot<5> cb_slot_5;
GstAsioCallbacksSlot<6> cb_slot_6;
GstAsioCallbacksSlot<7> cb_slot_7;

/* *INDENT-ON* */

typedef struct
{
  GstAsioObjectCallbacks callbacks;
  guint64 callback_id;
} GstAsioObjectCallbacksPrivate;

enum
{
  PROP_0,
  PROP_DEVICE_INFO,
};

typedef enum
{
  GST_ASIO_OBJECT_STATE_LOADED,
  GST_ASIO_OBJECT_STATE_INITIALIZED,
  GST_ASIO_OBJECT_STATE_PREPARED,
  GST_ASIO_OBJECT_STATE_RUNNING,
} GstAsioObjectState;

/* Protect singletone object */
struct _GstAsioObject
{
  GstObject parent;

  GstAsioDeviceInfo *device_info;

  GstAsioObjectState state;

  IASIO *asio_handle;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;

  GMutex thread_lock;
  GCond thread_cond;

  GMutex api_lock;

  /* called after init() done */
  glong max_num_input_channels;
  glong max_num_output_channels;

  glong min_buffer_size;
  glong max_buffer_size;
  glong preferred_buffer_size;
  glong buffer_size_granularity;

  glong selected_buffer_size;

  /* List of supported sample rate */
  GArray *supported_sample_rates;

  /* List of ASIOChannelInfo */
  ASIOChannelInfo *input_channel_infos;
  ASIOChannelInfo *output_channel_infos;

  /* Selected sample rate */
  ASIOSampleRate sample_rate;

  /* Input/Output buffer infors */
  ASIOBufferInfo *buffer_infos;

  /* Store requested channel before createbuffer */
  gboolean *input_channel_requested;
  gboolean *output_channel_requested;

  glong num_requested_input_channels;
  glong num_requested_output_channels;
  guint num_allocated_buffers;

  GList *src_client_callbacks;
  GList *sink_client_callbacks;
  GList *loopback_client_callbacks;
  guint64 next_callback_id;

  GstAsioCallbacks *callbacks;
  ASIOCallbacks driver_callbacks;
  int slot_id;

  gboolean occupy_all_channels;
};

static void gst_asio_object_constructed (GObject * object);
static void gst_asio_object_finalize (GObject * object);
static void gst_asio_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static gpointer gst_asio_object_thread_func (GstAsioObject * self);

#define gst_asio_object_parent_class parent_class
G_DEFINE_TYPE (GstAsioObject, gst_asio_object, GST_TYPE_OBJECT);

static void
gst_asio_object_class_init (GstAsioObjectClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gst_asio_object_constructed;
  gobject_class->finalize = gst_asio_object_finalize;
  gobject_class->set_property = gst_asio_object_set_property;

  g_object_class_install_property (gobject_class, PROP_DEVICE_INFO,
      g_param_spec_pointer ("device-info", "Device Info",
          "A pointer to GstAsioDeviceInfo struct",
          (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));

  GST_DEBUG_CATEGORY_INIT (gst_asio_object_debug,
      "asioobject", 0, "asioobject");
}

static void
gst_asio_object_init (GstAsioObject * self)
{
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  g_mutex_init (&self->thread_lock);
  g_cond_init (&self->thread_cond);

  g_mutex_init (&self->api_lock);

  self->supported_sample_rates = g_array_new (FALSE,
      FALSE, sizeof (ASIOSampleRate));

  self->slot_id = -1;
}

static void
gst_asio_object_constructed (GObject * object)
{
  GstAsioObject *self = GST_ASIO_OBJECT (object);

  if (!self->device_info) {
    GST_ERROR_OBJECT (self, "Device info was not configured");
    return;
  }

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstAsioObject",
      (GThreadFunc) gst_asio_object_thread_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);
}

static void
gst_asio_object_finalize (GObject * object)
{
  GstAsioObject *self = GST_ASIO_OBJECT (object);

  if (self->loop) {
    g_main_loop_quit (self->loop);
    g_thread_join (self->thread);
    g_main_loop_unref (self->loop);
    g_main_context_unref (self->context);
  }

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  g_mutex_clear (&self->thread_lock);
  g_cond_clear (&self->thread_cond);

  g_mutex_clear (&self->api_lock);

  g_array_unref (self->supported_sample_rates);

  gst_asio_device_info_free (self->device_info);
  g_free (self->input_channel_infos);
  g_free (self->output_channel_infos);
  g_free (self->input_channel_requested);
  g_free (self->output_channel_requested);

  if (self->src_client_callbacks)
    g_list_free_full (self->src_client_callbacks, (GDestroyNotify) g_free);
  if (self->sink_client_callbacks)
    g_list_free_full (self->sink_client_callbacks, (GDestroyNotify) g_free);
  if (self->loopback_client_callbacks)
    g_list_free_full (self->loopback_client_callbacks, (GDestroyNotify) g_free);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_asio_object_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAsioObject *self = GST_ASIO_OBJECT (object);

  switch (prop_id) {
    case PROP_DEVICE_INFO:
      g_clear_pointer (&self->device_info, gst_asio_device_info_free);
      self->device_info = gst_asio_device_info_copy ((GstAsioDeviceInfo *)
          g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static HWND
gst_asio_object_create_internal_hwnd (GstAsioObject * self)
{
  WNDCLASSEXW wc;
  ATOM atom = 0;
  HINSTANCE hinstance = GetModuleHandle (NULL);

  atom = GetClassInfoExW (hinstance, L"GstAsioInternalWindow", &wc);
  if (atom == 0) {
    GST_LOG_OBJECT (self, "Register internal window class");
    ZeroMemory (&wc, sizeof (WNDCLASSEX));

    wc.cbSize = sizeof (WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle (nullptr);
    wc.style = CS_OWNDC;
    wc.lpszClassName = L"GstAsioInternalWindow";

    atom = RegisterClassExW (&wc);

    if (atom == 0) {
      GST_ERROR_OBJECT (self, "Failed to register window class 0x%x",
          (unsigned int) GetLastError ());
      return nullptr;
    }
  }

  return CreateWindowExW (0, L"GstAsioInternalWindow", L"GstAsioInternal",
      WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandle (nullptr),
      nullptr);
}

static gboolean
hwnd_msg_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MSG msg;

  if (!PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    return G_SOURCE_CONTINUE;

  TranslateMessage (&msg);
  DispatchMessage (&msg);

  return G_SOURCE_CONTINUE;
}

static gboolean
gst_asio_object_main_loop_running_cb (GstAsioObject * self)
{
  GST_INFO_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gboolean
gst_asio_object_bind_callbacks (GstAsioObject * self)
{
  std::lock_guard < std::mutex > lk (slot_lock);
  gboolean ret = TRUE;

  if (!cb_slot_0.IsBound ()) {
    cb_slot_0.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 0;
  } else if (!cb_slot_1.IsBound ()) {
    cb_slot_1.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 1;
  } else if (!cb_slot_2.IsBound ()) {
    cb_slot_2.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 2;
  } else if (!cb_slot_3.IsBound ()) {
    cb_slot_3.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 3;
  } else if (!cb_slot_4.IsBound ()) {
    cb_slot_4.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 4;
  } else if (!cb_slot_5.IsBound ()) {
    cb_slot_5.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 5;
  } else if (!cb_slot_6.IsBound ()) {
    cb_slot_6.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 6;
  } else if (!cb_slot_7.IsBound ()) {
    cb_slot_7.Bind (self->callbacks, &self->driver_callbacks);
    self->slot_id = 7;
  } else {
    self->slot_id = -1;
    ret = FALSE;
  }

  return ret;
}

static void
gst_asio_object_unbind_callbacks (GstAsioObject * self)
{
  std::lock_guard < std::mutex > lk (slot_lock);

  if (!self->callbacks || self->slot_id < 0)
    return;

  switch (self->slot_id) {
    case 0:
      cb_slot_0.Init ();
      break;
    case 1:
      cb_slot_1.Init ();
      break;
    case 2:
      cb_slot_2.Init ();
      break;
    case 3:
      cb_slot_3.Init ();
      break;
    case 4:
      cb_slot_4.Init ();
      break;
    case 5:
      cb_slot_5.Init ();
      break;
    case 6:
      cb_slot_6.Init ();
      break;
    case 7:
      cb_slot_7.Init ();
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return;
}

static gpointer
gst_asio_object_thread_func (GstAsioObject * self)
{
  HANDLE avrt_handle = nullptr;
  static DWORD task_idx = 0;
  HWND hwnd;
  GSource *source = nullptr;
  GSource *hwnd_msg_source = nullptr;
  GIOChannel *msg_io_channel = nullptr;
  HRESULT hr;
  ASIOError asio_rst;
  IASIO *asio_handle = nullptr;
  GstAsioDeviceInfo *device_info = self->device_info;
  /* FIXME: check more sample rate */
  static ASIOSampleRate sample_rate_to_check[] = {
    48000.0, 44100.0, 192000.0, 96000.0, 88200.0,
  };

  g_assert (device_info);

  GST_INFO_OBJECT (self,
      "Enter loop, ThreadingModel: %s, driver-name: %s, driver-desc: %s",
      device_info->sta_model ? "STA" : "MTA",
      GST_STR_NULL (device_info->driver_name),
      GST_STR_NULL (device_info->driver_desc));

  if (device_info->sta_model)
    CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
  else
    CoInitializeEx (NULL, COINIT_MULTITHREADED);

  /* Our thread is unlikely different from driver's working thread though,
   * let's do this. It should not cause any problem */
  AvSetMmThreadCharacteristicsW (L"Pro Audio", &task_idx);
  g_main_context_push_thread_default (self->context);

  source = g_idle_source_new ();
  g_source_set_callback (source,
      (GSourceFunc) gst_asio_object_main_loop_running_cb, self, nullptr);
  g_source_attach (source, self->context);
  g_source_unref (source);

  /* XXX: not sure why ASIO API wants Windows handle for init().
   * Possibly it might be used for STA COM threading
   * but it's undocummented... */
  hwnd = gst_asio_object_create_internal_hwnd (self);
  if (!hwnd)
    goto run_loop;

  hr = CoCreateInstance (device_info->clsid, nullptr, CLSCTX_INPROC_SERVER,
      device_info->clsid, (gpointer *) & asio_handle);
  if (FAILED (hr)) {
    GST_WARNING_OBJECT (self, "Failed to create IASIO instance, hr: 0x%x",
        (guint) hr);
    goto run_loop;
  }

  if (!asio_handle->init (hwnd)) {
    GST_WARNING_OBJECT (self, "Failed to init IASIO instance");
    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  /* Query device information */
  asio_rst = asio_handle->getChannels (&self->max_num_input_channels,
      &self->max_num_output_channels);
  if (asio_rst != 0) {
    GST_WARNING_OBJECT (self, "Failed to query in/out channels, ret %ld",
        asio_rst);
    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  GST_INFO_OBJECT (self, "Input/Output channles: %ld/%ld",
      self->max_num_input_channels, self->max_num_output_channels);

  asio_rst = asio_handle->getBufferSize (&self->min_buffer_size,
      &self->max_buffer_size, &self->preferred_buffer_size,
      &self->buffer_size_granularity);
  if (asio_rst != 0) {
    GST_WARNING_OBJECT (self, "Failed to get buffer size, ret %ld", asio_rst);
    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  /* Use preferreed buffer size by default */
  self->selected_buffer_size = self->preferred_buffer_size;

  GST_INFO_OBJECT (self, "min-buffer-size %ld, max-buffer-size %ld, "
      "preferred-buffer-size %ld, buffer-size-granularity %ld",
      self->min_buffer_size, self->max_buffer_size,
      self->preferred_buffer_size, self->buffer_size_granularity);

  for (guint i = 0; i < G_N_ELEMENTS (sample_rate_to_check); i++) {
    asio_rst = asio_handle->canSampleRate (sample_rate_to_check[i]);
    if (asio_rst != 0)
      continue;

    GST_INFO_OBJECT (self, "SampleRate %.1lf is supported",
        sample_rate_to_check[i]);
    g_array_append_val (self->supported_sample_rates, sample_rate_to_check[i]);
  }

  if (self->supported_sample_rates->len == 0) {
    GST_WARNING_OBJECT (self, "Failed to query supported sample rate");
    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  /* Pick the first supported samplerate */
  self->sample_rate =
      g_array_index (self->supported_sample_rates, ASIOSampleRate, 0);
  if (asio_handle->setSampleRate (self->sample_rate) != 0) {
    GST_WARNING_OBJECT (self, "Failed to set samplerate %.1lf",
        self->sample_rate);
    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  if (self->max_num_input_channels > 0) {
    self->input_channel_infos = g_new0 (ASIOChannelInfo,
        self->max_num_input_channels);
    for (glong i = 0; i < self->max_num_input_channels; i++) {
      ASIOChannelInfo *info = &self->input_channel_infos[i];
      info->channel = i;
      info->isInput = TRUE;

      asio_rst = asio_handle->getChannelInfo (info);
      if (asio_rst != 0) {
        GST_WARNING_OBJECT (self, "Failed to %ld input channel info, ret %ld",
            i, asio_rst);
        asio_handle->Release ();
        asio_handle = nullptr;
        goto run_loop;
      }

      GST_INFO_OBJECT (self,
          "InputChannelInfo %ld: isActive %s, channelGroup %ld, "
          "ASIOSampleType %ld, name %s", i, info->isActive ? "true" : "false",
          info->channelGroup, info->type, GST_STR_NULL (info->name));
    }

    self->input_channel_requested =
        g_new0 (gboolean, self->max_num_input_channels);
  }

  if (self->max_num_output_channels > 0) {
    self->output_channel_infos = g_new0 (ASIOChannelInfo,
        self->max_num_output_channels);
    for (glong i = 0; i < self->max_num_output_channels; i++) {
      ASIOChannelInfo *info = &self->output_channel_infos[i];
      info->channel = i;
      info->isInput = FALSE;

      asio_rst = asio_handle->getChannelInfo (info);
      if (asio_rst != 0) {
        GST_WARNING_OBJECT (self, "Failed to %ld output channel info, ret %ld",
            i, asio_rst);
        asio_handle->Release ();
        asio_handle = nullptr;
        goto run_loop;
      }

      GST_INFO_OBJECT (self,
          "OutputChannelInfo %ld: isActive %s, channelGroup %ld, "
          "ASIOSampleType %ld, name %s", i, info->isActive ? "true" : "false",
          info->channelGroup, info->type, GST_STR_NULL (info->name));
    }

    self->output_channel_requested =
        g_new0 (gboolean, self->max_num_output_channels);
  }

  asio_rst = asio_handle->getSampleRate (&self->sample_rate);
  if (asio_rst != 0) {
    GST_WARNING_OBJECT (self,
        "Failed to get current samplerate, ret %ld", asio_rst);
    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  GST_INFO_OBJECT (self, "Current samplerate %.1lf", self->sample_rate);

  self->callbacks = new GstAsioCallbacks (self);
  if (!gst_asio_object_bind_callbacks (self)) {
    GST_ERROR_OBJECT (self, "Failed to bind callback to slot");
    delete self->callbacks;
    self->callbacks = nullptr;

    asio_handle->Release ();
    asio_handle = nullptr;
    goto run_loop;
  }

  msg_io_channel = g_io_channel_win32_new_messages ((guintptr) hwnd);
  hwnd_msg_source = g_io_create_watch (msg_io_channel, G_IO_IN);
  g_source_set_callback (hwnd_msg_source, (GSourceFunc) hwnd_msg_cb,
      self->context, nullptr);
  g_source_attach (hwnd_msg_source, self->context);

  self->state = GST_ASIO_OBJECT_STATE_INITIALIZED;
  self->asio_handle = asio_handle;

run_loop:
  g_main_loop_run (self->loop);

  if (self->asio_handle) {
    if (self->state > GST_ASIO_OBJECT_STATE_PREPARED)
      self->asio_handle->stop ();

    if (self->state > GST_ASIO_OBJECT_STATE_INITIALIZED)
      self->asio_handle->disposeBuffers ();
  }

  gst_asio_object_unbind_callbacks (self);
  if (self->callbacks) {
    delete self->callbacks;
    self->callbacks = nullptr;
  }

  if (hwnd_msg_source) {
    g_source_destroy (hwnd_msg_source);
    g_source_unref (hwnd_msg_source);
  }

  if (msg_io_channel)
    g_io_channel_unref (msg_io_channel);

  if (hwnd)
    DestroyWindow (hwnd);

  g_main_context_pop_thread_default (self->context);

  if (avrt_handle)
    AvRevertMmThreadCharacteristics (avrt_handle);

  if (asio_handle) {
    asio_handle->Release ();
    asio_handle = nullptr;
  }

  CoUninitialize ();

  GST_INFO_OBJECT (self, "Exit loop");

  return nullptr;
}

static void
gst_asio_object_weak_ref_notify (gpointer data, GstAsioObject * object)
{
  std::lock_guard < std::mutex > lk (global_lock);
  asio_object_list = g_list_remove (asio_object_list, object);
}

GstAsioObject *
gst_asio_object_new (const GstAsioDeviceInfo * info,
    gboolean occupy_all_channels)
{
  GstAsioObject *self = nullptr;
  GList *iter;
  std::lock_guard < std::mutex > lk (global_lock);

  g_return_val_if_fail (info != nullptr, nullptr);

  /* Check if we have object corresponding to CLSID, and if so return
   * already existing object instead of allocating new one */
  for (iter = asio_object_list; iter; iter = g_list_next (iter)) {
    GstAsioObject *object = (GstAsioObject *) iter->data;

    if (object->device_info->clsid == info->clsid) {
      GST_DEBUG_OBJECT (object, "Found configured ASIO object");
      self = (GstAsioObject *) gst_object_ref (object);
      break;
    }
  }

  if (self)
    return self;

  self = (GstAsioObject *) g_object_new (GST_TYPE_ASIO_OBJECT,
      "device-info", info, nullptr);

  if (!self->asio_handle) {
    GST_WARNING_OBJECT (self, "ASIO handle is not available");
    gst_object_unref (self);

    return nullptr;
  }

  self->occupy_all_channels = occupy_all_channels;

  gst_object_ref_sink (self);

  g_object_weak_ref (G_OBJECT (self),
      (GWeakNotify) gst_asio_object_weak_ref_notify, nullptr);
  asio_object_list = g_list_append (asio_object_list, self);

  return self;
}

static GstCaps *
gst_asio_object_create_caps_from_channel_info (GstAsioObject * self,
    ASIOChannelInfo * info, guint min_num_channels, guint max_num_channels)
{
  GstCaps *caps;
  std::string caps_str;
  GstAudioFormat fmt;
  const gchar *fmt_str;

  g_assert (info);
  g_assert (max_num_channels >= min_num_channels);

  fmt = gst_asio_sample_type_to_gst (info->type);
  if (fmt == GST_AUDIO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Unknown format");
    return nullptr;
  }

  fmt_str = gst_audio_format_to_string (fmt);

  /* Actually we are non-interleaved, but element will interlave data */
  caps_str = "audio/x-raw, layout = (string) interleaved, ";
  caps_str += "format = (string) " + std::string (fmt_str) + ", ";
  /* use fixated sample rate, otherwise get_caps/set_sample_rate() might
   * be racy in case that multiple sink/src are used */
  caps_str +=
      "rate = (int) " + std::to_string ((gint) self->sample_rate) + ", ";

  if (max_num_channels == min_num_channels)
    caps_str += "channels = (int) " + std::to_string (max_num_channels);
  else
    caps_str += "channels = (int) [ " + std::to_string (min_num_channels) +
        ", " + std::to_string (max_num_channels) + " ]";

  caps = gst_caps_from_string (caps_str.c_str ());
  if (!caps) {
    GST_ERROR_OBJECT (self, "Failed to create caps");
    return nullptr;
  }

  GST_DEBUG_OBJECT (self, "Create caps %" GST_PTR_FORMAT, caps);

  return caps;
}

/* FIXME: assuming all channels has the same format but it might not be true? */
GstCaps *
gst_asio_object_get_caps (GstAsioObject * obj, GstAsioDeviceClassType type,
    guint min_num_channels, guint max_num_channels)
{
  ASIOChannelInfo *infos;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), nullptr);

  if (type == GST_ASIO_DEVICE_CLASS_CAPTURE) {
    if (obj->max_num_input_channels == 0) {
      GST_WARNING_OBJECT (obj, "Device doesn't support input");
      return nullptr;
    }

    /* max_num_channels == 0 means [1, max-allowed-channles] */
    if (max_num_channels > 0) {
      if (max_num_channels > obj->max_num_input_channels) {
        GST_WARNING_OBJECT (obj, "Too many max channels");
        return nullptr;
      }
    } else {
      max_num_channels = obj->max_num_input_channels;
    }

    if (min_num_channels > 0) {
      if (min_num_channels > obj->max_num_input_channels) {
        GST_WARNING_OBJECT (obj, "Too many min channels");
        return nullptr;
      }
    } else {
      min_num_channels = 1;
    }

    infos = obj->input_channel_infos;
  } else {
    if (obj->max_num_output_channels == 0) {
      GST_WARNING_OBJECT (obj, "Device doesn't support output");
      return nullptr;
    }

    /* max_num_channels == 0 means [1, max-allowed-channles] */
    if (max_num_channels > 0) {
      if (max_num_channels > obj->max_num_output_channels) {
        GST_WARNING_OBJECT (obj, "Too many max channels");
        return nullptr;
      }
    } else {
      max_num_channels = obj->max_num_output_channels;
    }

    if (min_num_channels > 0) {
      if (min_num_channels > obj->max_num_output_channels) {
        GST_WARNING_OBJECT (obj, "Too many min channels");
        return nullptr;
      }
    } else {
      min_num_channels = 1;
    }

    infos = obj->output_channel_infos;
  }

  return gst_asio_object_create_caps_from_channel_info (obj,
      infos, min_num_channels, max_num_channels);
}

gboolean
gst_asio_object_get_max_num_channels (GstAsioObject * obj, glong * num_input_ch,
    glong * num_output_ch)
{
  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);

  if (num_input_ch)
    *num_input_ch = obj->max_num_input_channels;
  if (num_output_ch)
    *num_output_ch = obj->max_num_output_channels;

  return TRUE;
}

gboolean
gst_asio_object_get_buffer_size (GstAsioObject * obj, glong * min_size,
    glong * max_size, glong * preferred_size, glong * granularity)
{
  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);

  if (min_size)
    *min_size = obj->min_buffer_size;
  if (max_size)
    *max_size = obj->max_buffer_size;
  if (preferred_size)
    *preferred_size = obj->preferred_buffer_size;
  if (granularity)
    *granularity = obj->buffer_size_granularity;

  return TRUE;
}

typedef void (*GstAsioObjectThreadFunc) (GstAsioObject * obj, gpointer data);

typedef struct
{
  GstAsioObject *self;
  GstAsioObjectThreadFunc func;
  gpointer data;
  gboolean fired;
} GstAsioObjectThreadRunData;

static gboolean
gst_asio_object_thread_run_func (GstAsioObjectThreadRunData * data)
{
  GstAsioObject *self = data->self;

  if (data->func)
    data->func (self, data->data);

  g_mutex_lock (&self->thread_lock);
  data->fired = TRUE;
  g_cond_broadcast (&self->thread_cond);
  g_mutex_unlock (&self->thread_lock);

  return G_SOURCE_REMOVE;
}

static void
gst_asio_object_thread_add (GstAsioObject * self, GstAsioObjectThreadFunc func,
    gpointer data)
{
  GstAsioObjectThreadRunData thread_data;

  g_return_if_fail (GST_IS_ASIO_OBJECT (self));

  thread_data.self = self;
  thread_data.func = func;
  thread_data.data = data;
  thread_data.fired = FALSE;

  g_main_context_invoke (self->context,
      (GSourceFunc) gst_asio_object_thread_run_func, &thread_data);

  g_mutex_lock (&self->thread_lock);
  while (!thread_data.fired)
    g_cond_wait (&self->thread_cond, &self->thread_lock);
  g_mutex_unlock (&self->thread_lock);
}

static gboolean
gst_asio_object_validate_channels (GstAsioObject * self, gboolean is_input,
    guint * channel_indices, guint num_channels)
{
  if (is_input) {
    if (self->max_num_input_channels < num_channels) {
      GST_WARNING_OBJECT (self, "%d exceeds max input channels %ld",
          num_channels, self->max_num_input_channels);
      return FALSE;
    }

    for (guint i = 0; i < num_channels; i++) {
      guint ch = channel_indices[i];
      if (self->max_num_input_channels <= ch) {
        GST_WARNING_OBJECT (self, "%d exceeds max input channels %ld",
            ch, self->max_num_input_channels);

        return FALSE;
      }
    }
  } else {
    if (self->max_num_output_channels < num_channels) {
      GST_WARNING_OBJECT (self, "%d exceeds max output channels %ld",
          num_channels, self->max_num_output_channels);

      return FALSE;
    }

    for (guint i = 0; i < num_channels; i++) {
      guint ch = channel_indices[i];
      if (self->max_num_output_channels <= ch) {
        GST_WARNING_OBJECT (self, "%d exceeds max output channels %ld",
            ch, self->max_num_output_channels);

        return FALSE;
      }
    }
  }

  return TRUE;
}

static gboolean
gst_asio_object_check_buffer_reuse (GstAsioObject * self, ASIOBool is_input,
    guint * channel_indices, guint num_channels)
{
  guint num_found = 0;

  g_assert (self->buffer_infos);
  g_assert (self->num_allocated_buffers > 0);

  for (guint i = 0; i < self->num_allocated_buffers; i++) {
    ASIOBufferInfo *info = &self->buffer_infos[i];

    if (info->isInput != is_input)
      continue;

    for (guint j = 0; j < num_channels; j++) {
      if (info->channelNum == channel_indices[j]) {
        num_found++;

        break;
      }
    }
  }

  return num_found == num_channels;
}

static void
gst_asio_object_dispose_buffers_async (GstAsioObject * self, ASIOError * rst)
{
  g_assert (self->asio_handle);
  g_assert (rst);

  *rst = self->asio_handle->disposeBuffers ();
}

static gboolean
gst_asio_object_dispose_buffers (GstAsioObject * self)
{
  ASIOError rst;
  g_assert (self->asio_handle);

  if (!self->buffer_infos)
    return TRUE;

  if (!self->device_info->sta_model) {
    rst = self->asio_handle->disposeBuffers ();
  } else {
    gst_asio_object_thread_add (self,
        (GstAsioObjectThreadFunc) gst_asio_object_dispose_buffers_async, &rst);
  }

  g_clear_pointer (&self->buffer_infos, g_free);
  self->num_allocated_buffers = 0;

  return rst == 0;
}

static ASIOError
gst_asio_object_create_buffers_real (GstAsioObject * self, glong * buffer_size)
{
  ASIOError err;

  g_assert (buffer_size);

  err = self->asio_handle->createBuffers (self->buffer_infos,
      self->num_requested_input_channels + self->num_requested_output_channels,
      *buffer_size, &self->driver_callbacks);

  /* It failed and buffer size is not equal to preferred size,
   * try again with preferred size */
  if (err != 0 && *buffer_size != self->preferred_buffer_size) {
    GST_WARNING_OBJECT (self,
        "Failed to create buffer with buffer size %ld, try again with %ld",
        *buffer_size, self->preferred_buffer_size);

    err = self->asio_handle->createBuffers (self->buffer_infos,
        self->num_requested_input_channels +
        self->num_requested_output_channels, self->preferred_buffer_size,
        &self->driver_callbacks);

    if (!err) {
      *buffer_size = self->preferred_buffer_size;
    }
  }

  return err;
}

typedef struct
{
  glong buffer_size;
  ASIOError err;
} CreateBuffersAsyncData;

static void
gst_asio_object_create_buffers_async (GstAsioObject * self,
    CreateBuffersAsyncData * data)
{
  data->err = gst_asio_object_create_buffers_real (self, &data->buffer_size);
}

static gboolean
gst_asio_object_create_buffers_internal (GstAsioObject * self,
    glong * buffer_size)
{
  ASIOError err;
  g_assert (self->asio_handle);

  if (!self->device_info->sta_model) {
    err = gst_asio_object_create_buffers_real (self, buffer_size);
  } else {
    CreateBuffersAsyncData data;
    data.buffer_size = *buffer_size;

    gst_asio_object_thread_add (self,
        (GstAsioObjectThreadFunc) gst_asio_object_create_buffers_async, &data);

    err = data.err;
    *buffer_size = data.buffer_size;
  }

  return !err;
}

gboolean
gst_asio_object_create_buffers (GstAsioObject * obj,
    GstAsioDeviceClassType type,
    guint * channel_indices, guint num_channels, guint * buffer_size)
{
  gboolean can_reuse = FALSE;
  guint i, j;
  glong buf_size;
  glong prev_buf_size = 0;
  gboolean is_src;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);
  g_return_val_if_fail (channel_indices != nullptr, FALSE);
  g_return_val_if_fail (num_channels > 0, FALSE);

  GST_DEBUG_OBJECT (obj, "Create buffers");

  if (type == GST_ASIO_DEVICE_CLASS_CAPTURE)
    is_src = TRUE;
  else
    is_src = FALSE;

  g_mutex_lock (&obj->api_lock);
  if (!gst_asio_object_validate_channels (obj, is_src, channel_indices,
          num_channels)) {
    GST_ERROR_OBJECT (obj, "Invalid request");
    g_mutex_unlock (&obj->api_lock);

    return FALSE;
  }

  if (obj->buffer_infos) {
    GST_DEBUG_OBJECT (obj,
        "Have configured buffer infors, checking whether we can reuse it");
    can_reuse = gst_asio_object_check_buffer_reuse (obj,
        is_src ? TRUE : FALSE, channel_indices, num_channels);
  }

  if (can_reuse) {
    GST_DEBUG_OBJECT (obj, "We can reuse already allocated buffers");
    if (buffer_size)
      *buffer_size = obj->selected_buffer_size;

    g_mutex_unlock (&obj->api_lock);

    return TRUE;
  }

  /* Cannot re-allocated buffers once started... */
  if (obj->state > GST_ASIO_OBJECT_STATE_PREPARED) {
    GST_WARNING_OBJECT (obj, "We are running already");
    g_mutex_unlock (&obj->api_lock);

    return FALSE;
  }

  /* Use already configured buffer size */
  if (obj->buffer_infos)
    prev_buf_size = obj->selected_buffer_size;

  /* If we have configured buffers, dispose and re-allocate */
  if (!gst_asio_object_dispose_buffers (obj)) {
    GST_ERROR_OBJECT (obj, "Failed to dispose buffers");

    obj->state = GST_ASIO_OBJECT_STATE_INITIALIZED;

    g_mutex_unlock (&obj->api_lock);
    return FALSE;
  }

  if (obj->occupy_all_channels) {
    GST_INFO_OBJECT (obj,
        "occupy-all-channels mode, will allocate buffers for all channels");
    /* In this case, we will allocate buffer for all available input/output
     * channles, regardless of what requested here */
    for (guint i = 0; i < (guint) obj->max_num_input_channels; i++)
      obj->input_channel_requested[i] = TRUE;
    for (guint i = 0; i < (guint) obj->max_num_output_channels; i++)
      obj->output_channel_requested[i] = TRUE;

    obj->num_requested_input_channels = obj->max_num_input_channels;
    obj->num_requested_output_channels = obj->max_num_output_channels;
  } else {
    if (is_src) {
      for (guint i = 0; i < num_channels; i++) {
        guint ch = channel_indices[i];

        obj->input_channel_requested[ch] = TRUE;
      }

      obj->num_requested_input_channels = 0;
      for (guint i = 0; i < obj->max_num_input_channels; i++) {
        if (obj->input_channel_requested[i])
          obj->num_requested_input_channels++;
      }
    } else {
      for (guint i = 0; i < num_channels; i++) {
        guint ch = channel_indices[i];

        obj->output_channel_requested[ch] = TRUE;
      }

      obj->num_requested_output_channels = 0;
      for (guint i = 0; i < obj->max_num_output_channels; i++) {
        if (obj->output_channel_requested[i])
          obj->num_requested_output_channels++;
      }
    }
  }

  obj->num_allocated_buffers = obj->num_requested_input_channels +
      obj->num_requested_output_channels;

  obj->buffer_infos = g_new0 (ASIOBufferInfo, obj->num_allocated_buffers);
  for (i = 0, j = 0; i < obj->num_requested_input_channels; i++) {
    ASIOBufferInfo *info = &obj->buffer_infos[i];

    info->isInput = TRUE;
    while (!obj->input_channel_requested[j])
      j++;

    info->channelNum = j;
    j++;
  }

  for (i = obj->num_requested_input_channels, j = 0;
      i <
      obj->num_requested_input_channels + obj->num_requested_output_channels;
      i++) {
    ASIOBufferInfo *info = &obj->buffer_infos[i];

    info->isInput = FALSE;
    while (!obj->output_channel_requested[j])
      j++;

    info->channelNum = j;
    j++;
  }

  if (prev_buf_size > 0) {
    buf_size = prev_buf_size;
  } else if (buffer_size && *buffer_size > 0) {
    buf_size = *buffer_size;
  } else {
    buf_size = obj->preferred_buffer_size;
  }

  GST_INFO_OBJECT (obj, "Creating buffer with size %ld", buf_size);

  if (!gst_asio_object_create_buffers_internal (obj, &buf_size)) {
    GST_ERROR_OBJECT (obj, "Failed to create buffers");
    g_clear_pointer (&obj->buffer_infos, g_free);
    obj->num_allocated_buffers = 0;

    obj->state = GST_ASIO_OBJECT_STATE_INITIALIZED;

    g_mutex_unlock (&obj->api_lock);

    return FALSE;
  }

  GST_INFO_OBJECT (obj, "Selected buffer size %ld", buf_size);

  obj->selected_buffer_size = buf_size;
  if (buffer_size)
    *buffer_size = buf_size;

  obj->state = GST_ASIO_OBJECT_STATE_PREPARED;

  g_mutex_unlock (&obj->api_lock);

  return TRUE;
}

typedef struct
{
  glong arg[4];
  ASIOError ret;
} RunAsyncData;

static void
gst_asio_object_get_latencies_async (GstAsioObject * self, RunAsyncData * data)
{
  data->ret = self->asio_handle->getLatencies (&data->arg[0], &data->arg[1]);
}

gboolean
gst_asio_object_get_latencies (GstAsioObject * obj, glong * input_latency,
    glong * output_latency)
{
  RunAsyncData data = { 0 };
  ASIOError err;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);
  g_assert (obj->asio_handle);

  if (!obj->device_info->sta_model) {
    err = obj->asio_handle->getLatencies (input_latency, output_latency);
  } else {
    gst_asio_object_thread_add (obj,
        (GstAsioObjectThreadFunc) gst_asio_object_get_latencies_async, &data);

    *input_latency = data.arg[0];
    *output_latency = data.arg[1];
    err = data.ret;
  }

  return !err;
}

typedef struct
{
  ASIOSampleRate sample_rate;
  ASIOError err;
} SampleRateAsyncData;

static void
gst_asio_object_can_sample_rate_async (GstAsioObject * self,
    SampleRateAsyncData * data)
{
  data->err = self->asio_handle->canSampleRate (data->sample_rate);
}

gboolean
gst_asio_object_can_sample_rate (GstAsioObject * obj,
    ASIOSampleRate sample_rate)
{
  SampleRateAsyncData data = { 0 };
  ASIOError err = 0;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);
  g_assert (obj->asio_handle);

  g_mutex_lock (&obj->api_lock);
  for (guint i = 0; i < obj->supported_sample_rates->len; i++) {
    ASIOSampleRate val = g_array_index (obj->supported_sample_rates,
        ASIOSampleRate, i);
    if (val == sample_rate) {
      g_mutex_unlock (&obj->api_lock);
      return TRUE;
    }
  }

  if (!obj->device_info->sta_model) {
    err = obj->asio_handle->canSampleRate (sample_rate);

    if (!err)
      g_array_append_val (obj->supported_sample_rates, sample_rate);

    g_mutex_unlock (&obj->api_lock);
    return !err;
  }

  data.sample_rate = sample_rate;
  gst_asio_object_thread_add (obj,
      (GstAsioObjectThreadFunc) gst_asio_object_can_sample_rate_async, &data);

  if (!data.err)
    g_array_append_val (obj->supported_sample_rates, sample_rate);

  g_mutex_unlock (&obj->api_lock);

  return !data.err;
}

gboolean
gst_asio_object_get_sample_rate (GstAsioObject * obj,
    ASIOSampleRate * sample_rate)
{
  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);

  *sample_rate = obj->sample_rate;

  return 0;
}

static void
gst_asio_object_set_sample_rate_async (GstAsioObject * self,
    SampleRateAsyncData * data)
{
  data->err = self->asio_handle->setSampleRate (data->sample_rate);
  if (!data->err)
    self->sample_rate = data->sample_rate;
}

gboolean
gst_asio_object_set_sample_rate (GstAsioObject * obj,
    ASIOSampleRate sample_rate)
{
  SampleRateAsyncData data = { 0 };
  ASIOError err = 0;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);
  g_assert (obj->asio_handle);

  g_mutex_lock (&obj->api_lock);
  if (sample_rate == obj->sample_rate) {
    g_mutex_unlock (&obj->api_lock);
    return TRUE;
  }

  if (!obj->device_info->sta_model) {
    err = obj->asio_handle->setSampleRate (sample_rate);
    if (!err)
      obj->sample_rate = sample_rate;

    g_mutex_unlock (&obj->api_lock);
    return !err;
  }

  data.sample_rate = sample_rate;
  gst_asio_object_thread_add (obj,
      (GstAsioObjectThreadFunc) gst_asio_object_set_sample_rate_async, &data);
  g_mutex_unlock (&obj->api_lock);

  return !data.err;
}

static void
gst_asio_object_buffer_switch (GstAsioObject * self,
    glong index, ASIOBool process_now)
{
  ASIOTime time_info;
  ASIOTime *our_time_info = nullptr;
  ASIOError err = 0;

  memset (&time_info, 0, sizeof (ASIOTime));

  err =
      self->asio_handle->getSamplePosition (&time_info.timeInfo.samplePosition,
      &time_info.timeInfo.systemTime);
  if (!err)
    our_time_info = &time_info;

  gst_asio_object_buffer_switch_time_info (self,
      our_time_info, index, process_now);
}

static void
gst_asio_object_sample_rate_changed (GstAsioObject * self, ASIOSampleRate rate)
{
  GST_INFO_OBJECT (self, "SampleRate changed to %lf", rate);
}

static glong
gst_asio_object_messages (GstAsioObject * self,
    glong selector, glong value, gpointer message, gdouble * opt)
{
  GST_DEBUG_OBJECT (self, "ASIO message: %ld, %ld", selector, value);

  switch (selector) {
    case kAsioSelectorSupported:
      if (value == kAsioResetRequest || value == kAsioEngineVersion ||
          value == kAsioResyncRequest || value == kAsioLatenciesChanged ||
          value == kAsioSupportsTimeCode || value == kAsioSupportsInputMonitor)
        return 0;
      else if (value == kAsioSupportsTimeInfo)
        return 1;
      GST_WARNING_OBJECT (self, "Unsupported ASIO selector: %li", value);
      break;
    case kAsioBufferSizeChange:
      GST_WARNING_OBJECT (self,
          "Unsupported ASIO message: kAsioBufferSizeChange");
      break;
    case kAsioResetRequest:
      GST_WARNING_OBJECT (self, "Unsupported ASIO message: kAsioResetRequest");
      break;
    case kAsioResyncRequest:
      GST_WARNING_OBJECT (self, "Unsupported ASIO message: kAsioResyncRequest");
      break;
    case kAsioLatenciesChanged:
      GST_WARNING_OBJECT (self,
          "Unsupported ASIO message: kAsioLatenciesChanged");
      break;
    case kAsioEngineVersion:
      /* We target the ASIO v2 API, which includes ASIOOutputReady() */
      return 2;
    case kAsioSupportsTimeInfo:
      /* We use the new time info buffer switch callback */
      return 1;
    case kAsioSupportsTimeCode:
      /* We don't use the time code info right now */
      return 0;
    default:
      GST_WARNING_OBJECT (self, "Unsupported ASIO message: %li, %li", selector,
          value);
      break;
  }

  return 0;
}

#define PACK_ASIO_64(v) ((v).lo | ((guint64)((v).hi) << 32))

static ASIOTime *
gst_asio_object_buffer_switch_time_info (GstAsioObject * self,
    ASIOTime * time_info, glong index, ASIOBool process_now)
{
  GList *iter;

  if (time_info) {
    guint64 pos;
    guint64 system_time;

    pos = PACK_ASIO_64 (time_info->timeInfo.samplePosition);
    system_time = PACK_ASIO_64 (time_info->timeInfo.systemTime);

    GST_TRACE_OBJECT (self, "Sample Position: %" G_GUINT64_FORMAT
        ", System Time: %" GST_TIME_FORMAT, pos, GST_TIME_ARGS (system_time));
  }

  g_mutex_lock (&self->api_lock);
  if (!self->src_client_callbacks && !self->sink_client_callbacks &&
      !self->loopback_client_callbacks) {
    GST_WARNING_OBJECT (self, "No installed client callback");
    goto out;
  }

  for (iter = self->src_client_callbacks; iter;) {
    GstAsioObjectCallbacksPrivate *cb =
        (GstAsioObjectCallbacksPrivate *) iter->data;
    gboolean ret;

    ret = cb->callbacks.buffer_switch (self, index, self->buffer_infos,
        self->num_allocated_buffers, self->input_channel_infos,
        self->output_channel_infos, self->sample_rate,
        self->selected_buffer_size, time_info, cb->callbacks.user_data);
    if (!ret) {
      GST_INFO_OBJECT (self, "Remove callback for id %" G_GUINT64_FORMAT,
          cb->callback_id);
      GList *to_remove = iter;
      iter = g_list_next (iter);

      g_free (to_remove->data);
      g_list_free (to_remove);
    }

    iter = g_list_next (iter);
  }

  for (iter = self->sink_client_callbacks; iter;) {
    GstAsioObjectCallbacksPrivate *cb =
        (GstAsioObjectCallbacksPrivate *) iter->data;
    gboolean ret;

    ret = cb->callbacks.buffer_switch (self, index, self->buffer_infos,
        self->num_allocated_buffers, self->input_channel_infos,
        self->output_channel_infos, self->sample_rate,
        self->selected_buffer_size, time_info, cb->callbacks.user_data);
    if (!ret) {
      GST_INFO_OBJECT (self, "Remove callback for id %" G_GUINT64_FORMAT,
          cb->callback_id);
      GList *to_remove = iter;
      iter = g_list_next (iter);

      g_free (to_remove->data);
      g_list_free (to_remove);
    }

    iter = g_list_next (iter);
  }

  for (iter = self->loopback_client_callbacks; iter;) {
    GstAsioObjectCallbacksPrivate *cb =
        (GstAsioObjectCallbacksPrivate *) iter->data;
    gboolean ret;

    ret = cb->callbacks.buffer_switch (self, index, self->buffer_infos,
        self->num_allocated_buffers, self->input_channel_infos,
        self->output_channel_infos, self->sample_rate,
        self->selected_buffer_size, time_info, cb->callbacks.user_data);
    if (!ret) {
      GST_INFO_OBJECT (self, "Remove callback for id %" G_GUINT64_FORMAT,
          cb->callback_id);
      GList *to_remove = iter;
      iter = g_list_next (iter);

      g_free (to_remove->data);
      g_list_free (to_remove);
    }

    iter = g_list_next (iter);
  }

  self->asio_handle->outputReady ();

out:
  g_mutex_unlock (&self->api_lock);

  return nullptr;
}

static void
gst_asio_object_start_async (GstAsioObject * self, ASIOError * rst)
{
  *rst = self->asio_handle->start ();
}

gboolean
gst_asio_object_start (GstAsioObject * obj)
{
  ASIOError ret;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);

  g_mutex_lock (&obj->api_lock);
  if (obj->state > GST_ASIO_OBJECT_STATE_PREPARED) {
    GST_DEBUG_OBJECT (obj, "We are running already");
    g_mutex_unlock (&obj->api_lock);

    return TRUE;
  } else if (obj->state < GST_ASIO_OBJECT_STATE_PREPARED) {
    GST_ERROR_OBJECT (obj, "We are not prepared");
    g_mutex_unlock (&obj->api_lock);

    return FALSE;
  }

  /* Then start */
  if (!obj->device_info->sta_model) {
    ret = obj->asio_handle->start ();
  } else {
    gst_asio_object_thread_add (obj,
        (GstAsioObjectThreadFunc) gst_asio_object_start_async, &ret);
  }

  if (ret != 0) {
    GST_ERROR_OBJECT (obj, "Failed to start object");
    g_mutex_unlock (&obj->api_lock);

    return FALSE;
  }

  obj->state = GST_ASIO_OBJECT_STATE_RUNNING;
  g_mutex_unlock (&obj->api_lock);

  return TRUE;
}

gboolean
gst_asio_object_install_callback (GstAsioObject * obj,
    GstAsioDeviceClassType type,
    GstAsioObjectCallbacks * callbacks, guint64 * callback_id)
{
  GstAsioObjectCallbacksPrivate *cb;

  g_return_val_if_fail (GST_IS_ASIO_OBJECT (obj), FALSE);
  g_return_val_if_fail (callbacks != nullptr, FALSE);
  g_return_val_if_fail (callback_id != nullptr, FALSE);

  g_mutex_lock (&obj->api_lock);
  cb = g_new0 (GstAsioObjectCallbacksPrivate, 1);
  cb->callbacks = *callbacks;
  cb->callback_id = obj->next_callback_id;

  switch (type) {
    case GST_ASIO_DEVICE_CLASS_CAPTURE:
      obj->src_client_callbacks = g_list_append (obj->src_client_callbacks, cb);
      break;
    case GST_ASIO_DEVICE_CLASS_RENDER:
      obj->sink_client_callbacks =
          g_list_append (obj->sink_client_callbacks, cb);
      break;
    case GST_ASIO_DEVICE_CLASS_LOOPBACK_CAPTURE:
      obj->loopback_client_callbacks =
          g_list_append (obj->loopback_client_callbacks, cb);
      break;
    default:
      g_assert_not_reached ();
      g_free (cb);
      return FALSE;
  }

  *callback_id = cb->callback_id;
  g_mutex_unlock (&obj->api_lock);

  return TRUE;
}

void
gst_asio_object_uninstall_callback (GstAsioObject * obj, guint64 callback_id)
{
  GList *iter;

  g_return_if_fail (GST_IS_ASIO_OBJECT (obj));

  g_mutex_lock (&obj->api_lock);

  GST_DEBUG_OBJECT (obj, "Removing callback id %" G_GUINT64_FORMAT,
      callback_id);

  for (iter = obj->src_client_callbacks; iter; iter = g_list_next (iter)) {
    GstAsioObjectCallbacksPrivate *cb =
        (GstAsioObjectCallbacksPrivate *) iter->data;

    if (cb->callback_id != callback_id)
      continue;

    GST_DEBUG_OBJECT (obj, "Found src callback for id %" G_GUINT64_FORMAT,
        callback_id);

    obj->src_client_callbacks =
        g_list_remove_link (obj->src_client_callbacks, iter);
    g_free (iter->data);
    g_list_free (iter);
    g_mutex_unlock (&obj->api_lock);

    return;
  }

  for (iter = obj->sink_client_callbacks; iter; iter = g_list_next (iter)) {
    GstAsioObjectCallbacksPrivate *cb =
        (GstAsioObjectCallbacksPrivate *) iter->data;

    if (cb->callback_id != callback_id)
      continue;

    GST_DEBUG_OBJECT (obj, "Found sink callback for id %" G_GUINT64_FORMAT,
        callback_id);

    obj->sink_client_callbacks =
        g_list_remove_link (obj->sink_client_callbacks, iter);
    g_free (iter->data);
    g_list_free (iter);
    g_mutex_unlock (&obj->api_lock);

    return;
  }

  for (iter = obj->loopback_client_callbacks; iter; iter = g_list_next (iter)) {
    GstAsioObjectCallbacksPrivate *cb =
        (GstAsioObjectCallbacksPrivate *) iter->data;

    if (cb->callback_id != callback_id)
      continue;

    GST_DEBUG_OBJECT (obj, "Found loopback callback for id %" G_GUINT64_FORMAT,
        callback_id);

    obj->loopback_client_callbacks =
        g_list_remove_link (obj->loopback_client_callbacks, iter);
    g_free (iter->data);
    g_list_free (iter);
    break;
  }

  g_mutex_unlock (&obj->api_lock);
}
