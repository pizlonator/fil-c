/* A set of utility functions that are common between elements
 * based upon GstAdaptiveDemux
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

#ifndef __GST_ADAPTIVE_DEMUX_COMMON_TEST_H__
#define __GST_ADAPTIVE_DEMUX_COMMON_TEST_H__

#include <gst/gst.h>
#include "adaptive_demux_engine.h"

G_BEGIN_DECLS

#define GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE \
  (gst_adaptive_demux_test_case_get_type())
#define GST_ADAPTIVE_DEMUX_TEST_CASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE, GstAdaptiveDemuxTestCase))
#define GST_ADAPTIVE_DEMUX_TEST_CASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE, GstAdaptiveDemuxTestCaseClass))
#define GST_ADAPTIVE_DEMUX_TEST_CASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE, GstAdaptiveDemuxTestCaseClass))
#define GST_IS_ADAPTIVE_DEMUX_TEST_CASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE))
#define GST_IS_ADAPTIVE_DEMUX_TEST_CASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ADAPTIVE_DEMUX_TEST_CASE))

/**
 * TestTaskState:
 * The seek test uses a separate task to perform the seek operation.
 * After starting the task, the caller blocks until the seek task
 * flushes the AppSink and changes the GstFakeSoupHTTPSrc element state from
 * PLAYING to PAUSED.
 * When that event is detected, the caller is allowed to resume.
 * Any data that will be sent to AppSink after resume will be rejected because
 * AppSink is in flushing mode.
 */
typedef enum
{
  TEST_TASK_STATE_NOT_STARTED,
  TEST_TASK_STATE_WAITING_FOR_TESTSRC_STATE_CHANGE,
  TEST_TASK_STATE_EXITING,
} TestTaskState;

/**
 * GstAdaptiveDemuxTestExpectedOutput:
 * Structure used to store output stream related data.
 * It is used by the test during output validation.
 * The fields are set by the testcase function prior
 * to starting the test.
 */
typedef struct _GstAdaptiveDemuxTestExpectedOutput
{
  /* the name of the demux src pad generating this stream */
  const char *name;
  /* the expected size on this stream */
  guint64 expected_size;
  /* the expected data on this stream (optional) */
  const guint8* expected_data;

  GstSegment post_seek_segment;
  gboolean segment_verification_needed;
} GstAdaptiveDemuxTestExpectedOutput;

typedef struct _GstAdaptiveDemuxTestCaseClass
{
  GObjectClass parent_class;
} GstAdaptiveDemuxTestCaseClass;

typedef struct _GstAdaptiveDemuxTestCase
{
  GObject parent;

  /* output data used to validate the test
   * list of GstAdaptiveDemuxTestExpectedOutput, one entry per stream
   */
  GList *output_streams; /*GList<GstAdaptiveDemuxTestExpectedOutput>*/

  /* the number of streams that finished.
   * Main thread will stop the pipeline when all streams are finished
   * (i.e. count_of_finished_streams == g_list_length(output_streams) )
   */
  guint count_of_finished_streams;

  /* taskTesk... is a set of variables that can be used by a test
   * that needs to perform operations from another thread
   * For example, it is used by the seek test to perform the seek
   * operation
   */
  GstTask *test_task;
  GRecMutex test_task_lock;
  TestTaskState test_task_state;
  GMutex test_task_state_lock;
  GCond test_task_state_cond;

  /* seek test will wait for this amount of bytes to be sent by 
   * demux to AppSink before triggering a seek request
   */
  guint64 threshold_for_seek;
  GstEvent *seek_event;
  gboolean seeked;

  gpointer signal_context;
} GstAdaptiveDemuxTestCase;

/* high-level unit test functions */

/**
 * gst_adaptive_demux_test_setup:
 * Causes the test HTTP src element to be registered
 */
void gst_adaptive_demux_test_setup (void);

void gst_adaptive_demux_test_teardown (void);

GType gst_adaptive_demux_test_case_get_type (void);

/**
 * gst_adaptive_demux_test_case_new: creates new #GstAdaptiveDemuxTestCase
 * object. Use #g_object_unref to free.
 */
GstAdaptiveDemuxTestCase * gst_adaptive_demux_test_case_new (void) G_GNUC_MALLOC;

/**
 * gst_adaptive_demux_test_seek: test that element supports seeking
 * @element_name: The name of the demux element (e.g. "dashdemux")
 * @manifest_uri: The URI of the manifest to load
 * @testData: The #GstAdaptiveDemuxTestCase that the test expects the
 * demux element to produce.
 * 
 * Creates a pipeline and starts it. Once data is flowing, request a
 * seek to almost the start of the stream.
 */
void gst_adaptive_demux_test_seek (const gchar * element_name,
    const gchar * manifest_uri,
    GstAdaptiveDemuxTestCase *testData);

/* Utility functions for use within a unit test */

/**
 * gst_adaptive_demux_test_unexpected_eos:
 * @engine: The #GstAdaptiveDemuxTestEngine that caused this callback
 * @stream: The #GstAdaptiveDemuxTestOutputStream that caused this callback
 * @user_data: A pointer to a #GstAdaptiveDemuxTestCase object
 *
 * This function can be used as an EOS callback by tests that don't expect
 * AppSink to receive EOS.
 */
void
gst_adaptive_demux_test_unexpected_eos (GstAdaptiveDemuxTestEngine *
    engine, GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data);

/**
 * gst_adaptive_demux_test_check_size_of_received_data:
 * @engine: The #GstAdaptiveDemuxTestEngine that caused this callback
 * @stream: The #GstAdaptiveDemuxTestOutputStream that caused this callback
 * @user_data: A pointer to a #GstAdaptiveDemuxTestCase object
 *
 * This function can be used as an EOS callback to check that the
 * size of received data equals expected_size. It should be used with
 * a test that expects the entire file to be downloaded.
 */
void gst_adaptive_demux_test_check_size_of_received_data(
    GstAdaptiveDemuxTestEngine *engine,
    GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data);

/**
 * gst_adaptive_demux_test_download_error_size_of_received_data:
 * @engine: The #GstAdaptiveDemuxTestEngine that caused this callback
 * @stream: The #GstAdaptiveDemuxTestOutputStream that caused this callback
 * @user_data: A pointer to a #GstAdaptiveDemuxTestCase object
 *
 * This function can be used as an EOS callback to check that the
 * size of received data is >0 && <expected_size. It should be used with
 * a test that does not expect the entire file to be downloaded.
 */
void gst_adaptive_demux_test_download_error_size_of_received_data (
    GstAdaptiveDemuxTestEngine *engine,
    GstAdaptiveDemuxTestOutputStream * stream, gpointer user_data);

/**
 * gst_adaptive_demux_test_check_received_data:
 * @engine: The #GstAdaptiveDemuxTestEngine that caused this callback
 * @stream: The #GstAdaptiveDemuxTestOutputStream that caused this callback
 * @buffer: The #GstBuffer containing the data to check
 * @user_data: A pointer to a #GstAdaptiveDemuxTestCase object
 * Returns: TRUE if buffer contains the expected data.
 *
 * This function can be used as an appSinkGotData callback, to check
 * that the contents of the received data matches the expected data
 */
gboolean gst_adaptive_demux_test_check_received_data (
    GstAdaptiveDemuxTestEngine *engine,
    GstAdaptiveDemuxTestOutputStream * stream,
    GstBuffer * buffer,
    gpointer user_data);

/**
 * gst_adaptive_demux_test_find_test_data_by_stream:
 * @testData: The #GstAdaptiveDemuxTestCase object that contains the
 * output_streams list to search
 * @stream: the #GstAdaptiveDemuxTestOutputStream to search for
 * @index: (out) (allow none) the index of the entry in output_streams
 * Returns: The #GstAdaptiveDemuxTestExpectedOutput that matches @stream, or
 * %NULL if not found
 *
 * Search the list of output test data for the entry that matches stream.
 */
GstAdaptiveDemuxTestExpectedOutput * gst_adaptive_demux_test_find_test_data_by_stream (
    GstAdaptiveDemuxTestCase * testData,
    GstAdaptiveDemuxTestOutputStream * stream,
    guint * index);

G_END_DECLS
#endif /* __GST_ADAPTIVE_DEMUX_COMMON_TEST_H__ */
