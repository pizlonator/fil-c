/*
 * GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/d3d11/gstd3d11.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl.h>
#include <string>
#include <locale>
#include <codecvt>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

struct OverlayContext
{
  GstElement *pipeline = nullptr;

  ID2D1Factory *d2d_factory = nullptr;
  IDWriteFactory *dwrite_factory = nullptr;

  IDWriteTextFormat *format = nullptr;
  IDWriteTextLayout *layout = nullptr;

  std::wstring text;

  FLOAT width;
  FLOAT height;
  FLOAT origin_y;
  gint text_width;
  gint last_position = 0;

  GMainLoop *loop = nullptr;
};
/* *INDENT-ON* */

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, OverlayContext * context)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      gst_printerrln ("ERROR %s", err->message);
      if (dbg)
        gst_printerrln ("ERROR debug information: %s", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (context->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      gst_println ("Got EOS");
      g_main_loop_quit (context->loop);
      break;
    default:
      break;
  }

  return TRUE;
}

/* This callback will be called with gst_d3d11_device_lock() taken by
 * d3d11overlay. We can perform GPU operation here safely */
static gboolean
on_draw (GstElement * overlay, GstD3D11Device * device,
    ID3D11RenderTargetView * rtv, GstClockTime pts, GstClockTime duration,
    OverlayContext * context)
{
  ComPtr < ID3D11Resource > resource;
  ComPtr < IDXGISurface > surface;
  ComPtr < ID2D1RenderTarget > d2d_target;
  ComPtr < ID2D1SolidColorBrush > text_brush;
  ComPtr < ID2D1SolidColorBrush > bg_brush;
  HRESULT hr;
  ID2D1Factory *d2d_factory;
  FLOAT position;

  rtv->GetResource (&resource);
  hr = resource.As (&surface);
  g_assert (SUCCEEDED (hr));

  d2d_factory = context->d2d_factory;
  D2D1_RENDER_TARGET_PROPERTIES props;
  props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
  props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
  props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
  /* default DPI */
  props.dpiX = 0;
  props.dpiY = 0;
  props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
  props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

  /* Creates D2D render target using swapchin's backbuffer */
  hr = d2d_factory->CreateDxgiSurfaceRenderTarget (surface.Get (), props,
      &d2d_target);
  g_assert (SUCCEEDED (hr));

  hr = d2d_target->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black),
      &bg_brush);
  g_assert (SUCCEEDED (hr));

  hr = d2d_target->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::White),
      &text_brush);
  g_assert (SUCCEEDED (hr));

  d2d_target->BeginDraw ();

  /* Draw background */
  d2d_target->FillRectangle (D2D1::RectF (0, context->origin_y, context->width,
          context->height), bg_brush.Get ());

  /* Draw text */
  position = -context->last_position;

  do {
    d2d_target->DrawTextLayout (D2D1::Point2F (position, context->origin_y),
        context->layout, text_brush.Get (),
        static_cast<D2D1_DRAW_TEXT_OPTIONS>(D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT));
    position = position + context->text_width;
  } while (position < context->width);

  d2d_target->EndDraw ();

  context->last_position += 2;
  if (context->last_position >= context->text_width) {
    context->last_position %= context->text_width;
  }

  return TRUE;
}

gint
main (gint argc, gchar ** argv)
{
  GOptionContext *option_ctx;
  GError *error = nullptr;
  gboolean ret;
  gchar *text = nullptr;
  gint width = 1280;
  gint height = 720;
  GOptionEntry options[] = {
    {"text", 0, 0, G_OPTION_ARG_STRING, &text, "Text to render", nullptr},
    {"width", 0, 0, G_OPTION_ARG_INT, &width, "Width of video stream", nullptr},
    {"height", 0, 0, G_OPTION_ARG_INT, &height, "Height of video stream",
        nullptr},
    {nullptr}
  };
  OverlayContext context;
  GstStateChangeReturn sret;
  HRESULT hr;
  std::wstring text_wide;
  std::string pipeline_str;
  GstElement *overlay;
  DWRITE_TEXT_METRICS metrics;
  FLOAT font_size;
  bool was_decreased = false;
  DWRITE_TEXT_RANGE range;
  FLOAT text_height;
  gchar **win32_argv;
  ComPtr < IDWriteTextFormat > text_format;
  ComPtr < IDWriteTextLayout > text_layout;

  win32_argv = g_win32_get_command_line ();

  option_ctx = g_option_context_new ("d3d11overlay example");
  g_option_context_add_main_entries (option_ctx, options, nullptr);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse_strv (option_ctx, &win32_argv, &error);
  g_option_context_free (option_ctx);
  g_strfreev (win32_argv);

  if (!ret) {
    gst_printerrln ("option parsing failed: %s", error->message);
    g_clear_error (&error);
    return 1;
  }

  /* Prepare device independent D2D objects */
  hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED,
      IID_PPV_ARGS (&context.d2d_factory));
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create D2D factory");
    return 1;
  }

  hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
      __uuidof (context.dwrite_factory),
      reinterpret_cast < IUnknown ** >(&context.dwrite_factory));
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create DirectWrite factory");
    return 1;
  }

  hr = context.dwrite_factory->CreateTextFormat (L"Arial", nullptr,
      DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &text_format);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create IDWriteTextFormat");
    return 1;
  }

  text_format->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_LEADING);
  text_format->SetParagraphAlignment (DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  text_format->SetWordWrapping (DWRITE_WORD_WRAPPING_NO_WRAP);

  if (text && text[0] != '\0') {
    std::wstring_convert < std::codecvt_utf8_utf16 < wchar_t >>converter;
    text_wide = converter.from_bytes (text);
  } else {
    /* *INDENT-OFF* */
    text_wide =
        std::wstring (L"Hello GStreamer! 😊 안녕하세요 GStreamer! 😉 ") +
        std::wstring (L"नमस्ते GStreamer! ❤️ Bonjour GStreamer! 😁 ") +
        std::wstring (L"Hallo GStreamer! 😎 Hola GStreamer! 😍 ") +
        std::wstring (L"こんにちは GStreamer! ✌️ 你好 GStreamer! 👍");
    /* *INDENT-ON* */
  }

  text_height = (FLOAT) height / 10.0f;

  hr = context.dwrite_factory->CreateTextLayout (text_wide.c_str (),
      text_wide.length (), text_format.Get (), G_MAXFLOAT, G_MAXFLOAT,
      &text_layout);
  if (FAILED (hr)) {
    gst_printerrln ("Couldn't create IDWriteTextLayout");
    return 1;
  }

  /* Calculate best font size */
  range.startPosition = 0;
  range.length = text_wide.length ();

  do {
    hr = text_layout->GetMetrics (&metrics);
    g_assert (SUCCEEDED (hr));

    text_layout->GetFontSize (0, &font_size);
    if (metrics.height >= (FLOAT) text_height) {
      if (font_size > 1.0f) {
        font_size -= 0.5f;
        was_decreased = true;
        hr = text_layout->SetFontSize (font_size, range);
        g_assert (SUCCEEDED (hr));
        continue;
      }

      break;
    }

    if (was_decreased)
      break;

    if (metrics.height < text_height) {
      if (metrics.height >= text_height * 0.9)
        break;

      font_size += 0.5f;
      hr = text_layout->SetFontSize (font_size, range);
      g_assert (SUCCEEDED (hr));
      continue;
    }
  } while (true);

  gst_println ("Calculated font size %lf", font_size);

  /* 10 pixels padding per loop */
  text_layout->SetMaxWidth (metrics.widthIncludingTrailingWhitespace + 10);
  text_layout->SetMaxHeight (metrics.height);

  context.text = text_wide;
  context.origin_y = (FLOAT) height - text_height;
  context.width = width;
  context.height = height;
  context.text_width = metrics.widthIncludingTrailingWhitespace + 10;
  context.layout = text_layout.Detach ();

  context.loop = g_main_loop_new (nullptr, FALSE);

  pipeline_str =
      "d3d11testsrc ! video/x-raw(memory:D3D11Memory),format=BGRA,width=" +
      std::to_string (width) + ",height=" + std::to_string (height) +
      ",framerate=60/1 ! d3d11overlay name=overlay ! queue ! d3d11videosink";

  context.pipeline = gst_parse_launch (pipeline_str.c_str (), nullptr);
  g_assert (context.pipeline);

  overlay = gst_bin_get_by_name (GST_BIN (context.pipeline), "overlay");
  g_assert (overlay);

  g_signal_connect (overlay, "draw", G_CALLBACK (on_draw), &context);
  gst_object_unref (overlay);

  gst_bus_add_watch (GST_ELEMENT_BUS (context.pipeline),
      (GstBusFunc) bus_msg, &context);

  sret = gst_element_set_state (context.pipeline, GST_STATE_PLAYING);
  g_assert (sret != GST_STATE_CHANGE_FAILURE);

  g_main_loop_run (context.loop);

  gst_element_set_state (context.pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (context.pipeline));
  gst_object_unref (context.pipeline);

  context.format->Release ();
  context.layout->Release ();
  context.d2d_factory->Release ();
  context.dwrite_factory->Release ();

  g_main_loop_unref (context.loop);
  g_free (text);

  return 0;
}
