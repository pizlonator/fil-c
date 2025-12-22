/* HTTP source element for use in tests
 *
 * Copyright (c) <2015> YouView TV Ltd
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

#ifndef __GST_TEST_HTTP_SRC_H__
#define __GST_TEST_HTTP_SRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_TEST_HTTP_SRC            (gst_test_http_src_get_type ())

/**
 * TEST_HTTP_SRC_REQUEST_HEADERS_NAME:
 * The name of the #GstStructure that will contain all the HTTP request
 * headers
 */
#define TEST_HTTP_SRC_REQUEST_HEADERS_NAME "request-headers"

/**
 * TEST_HTTP_SRC_RESPONSE_HEADERS_NAME:
 * The name of the #GstStructure that will contain all the HTTP response
 * headers
 */
#define TEST_HTTP_SRC_RESPONSE_HEADERS_NAME "response-headers"

/* structure used by src_start function to configure the
 * GstTestHTTPSrc plugin.
 * It specifies information about a given URI.
 */
typedef struct _GstTestHTTPSrcInput
{
  gpointer context; /* opaque pointer that can be used in callbacks */
  guint64 size; /* size of resource, in bytes */
  GstStructure *request_headers;
  GstStructure *response_headers;
  guint status_code; /* HTTP status code */
} GstTestHTTPSrcInput;

/* Opaque structure used by GstTestHTTPSrc */
typedef struct _GstTestHTTPSrc GstTestHTTPSrc;

typedef struct _GstTestHTTPSrcCallbacks {
  /**
   * src_start:
   * @src: The #GstTestHTTPSrc calling this callback
   * @uri: The URI that is being requested
   * @input_data: (out) The implementation of this callback is
   * responsible for filling in this #GstTestHTTPSrcInput
   * with the appropriate information, return returning %TRUE.
   * If returning %FALSE, only GstTestHTTPSrcInput::status_code
   * should be updated.
   * Returns: %TRUE if GstTestHTTPSrc should respond to this URI,
   * using the supplied input_data.
   *
   * src_start is used to "open" the given URI. The callback must return
   * %TRUE to simulate a success, and set appropriate fields in input_data.
   * Returning %FALSE indicates that the request URI is not found.
   * In this situation GstTestHTTPSrc will cause the appropriate
   * 404 error to be posted to the bus 
   */
  gboolean (*src_start)(GstTestHTTPSrc *src,
                        const gchar *uri,
                        GstTestHTTPSrcInput *input_data,
                        gpointer user_data);
  /**
   * src_create:
   * @src: the #GstTestHTTPSrc calling this callback
   * @offset: the offset from the start of the resource
   * @length: requested number of bytes
   * @retbuf: (out) used to return a newly allocated #GstBuffer
   * @context: (allow none) the value of the context field
   * in #GstTestHTTPSrcInput.
   * @user_data: the value of user_data provided to 
   * #gst_test_http_src_install_callbacks
   * Returns: %GST_FLOW_OK to indicate success, or some other value of
   * #GstFlowReturn to indicate EOS or error.
   *
   * The src_create function is used to create a #GstBuffer for
   * simulating the data that is returned when accessing this
   * "open" stream. It can also be used to simulate various error
   * conditions by returning something other than %GST_FLOW_OK
   */
  GstFlowReturn (*src_create)(GstTestHTTPSrc *src,
                              guint64 offset,
                              guint length,
                              GstBuffer ** retbuf,
                              gpointer context,
                              gpointer user_data);
} GstTestHTTPSrcCallbacks;

GType gst_test_http_src_get_type (void);

/**
 * gst_test_http_src_register_plugin:
 * @registry: the #GstRegistry to use for registering this plugin
 * @name: the name to use for this plugin
 * Returns: true if successful
 *
 * Registers this plugin with the GstRegitry using the given name. It will
 * be given a high rank, so that it will be picked in preference to any
 * other element that implements #GstURIHandler.
 */
gboolean gst_test_http_src_register_plugin (GstRegistry * registry, const gchar * name);

/**
 * gst_test_http_src_install_callbacks:
 * @callbacks: the #GstTestHTTPSrcCallbacks callback functions that will
 * be called every time this element is asked to open a URI or provide data
 * for an open URI.
 * @user_data: a pointer that is passed to every callback
 */
void gst_test_http_src_install_callbacks (const GstTestHTTPSrcCallbacks *callbacks, gpointer user_data);

/**
 * gst_test_http_src_set_default_blocksize:
 * @blocksize: the default block size to use (0=use #GstBaseSrc default)
 *
 * Set the default blocksize that will be used by instances of
 * #GstTestHTTPSrc. It specifies the size (in bytes) that will be
 * returned in each #GstBuffer. This default can be overridden
 * by an instance of #GstTestHTTPSrc using the "blocksize" property
 * of #GstBaseSrc
 */
void gst_test_http_src_set_default_blocksize (guint blocksize);

G_END_DECLS

#endif /* __GST_TEST_HTTP_SRC_H__ */
