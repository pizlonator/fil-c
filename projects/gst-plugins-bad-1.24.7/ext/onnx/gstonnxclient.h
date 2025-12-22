/*
 * GStreamer gstreamer-onnxclient
 * Copyright (C) 2021 Collabora Ltd
 *
 * gstonnxclient.h
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
#ifndef __GST_ONNX_CLIENT_H__
#define __GST_ONNX_CLIENT_H__

#include <gst/gst.h>
#include <onnxruntime_cxx_api.h>
#include <gst/video/video.h>
#include "gstml.h"
#include "tensor/gsttensormeta.h"

GST_DEBUG_CATEGORY_EXTERN (onnx_inference_debug);

typedef enum
{
  GST_ONNX_OPTIMIZATION_LEVEL_DISABLE_ALL,
  GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_BASIC,
  GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_EXTENDED,
  GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_ALL,
} GstOnnxOptimizationLevel;

typedef enum
{
  GST_ONNX_EXECUTION_PROVIDER_CPU,
  GST_ONNX_EXECUTION_PROVIDER_CUDA,
} GstOnnxExecutionProvider;


namespace GstOnnxNamespace {

  class GstOnnxClient {
  public:
    GstOnnxClient(GstElement *debug_parent);
    ~GstOnnxClient(void);
    bool createSession(std::string modelFile, GstOnnxOptimizationLevel optim,
                       GstOnnxExecutionProvider provider);
    bool hasSession(void);
    void setInputImageFormat(GstMlInputImageFormat format);
    GstMlInputImageFormat getInputImageFormat(void);
    GstTensorDataType getInputImageDatatype(void);
    void setInputImageOffset (float offset);
    float getInputImageOffset ();
    void setInputImageScale (float offset);
    float getInputImageScale ();
    std::vector < Ort::Value > run (uint8_t * img_data, GstVideoInfo vinfo);
    std::vector < const char *> genOutputNamesRaw(void);
    bool isFixedInputImageSize(void);
    int32_t getWidth(void);
    int32_t getHeight(void);
    int32_t getChannels (void);
    GstTensorMeta *copy_tensors_to_meta (std::vector<Ort::Value> &outputs,
                                         GstBuffer *buffer);
    void parseDimensions(GstVideoInfo vinfo);
  private:

    GstElement *debug_parent;
    void setInputImageDatatype (GstTensorDataType datatype);
    template < typename T>
    void convert_image_remove_alpha (T *dest, GstMlInputImageFormat hwc,
        uint8_t **srcPtr, uint32_t srcSamplesPerPixel, uint32_t stride, T offset, T div);
    bool doRun(uint8_t * img_data, GstVideoInfo vinfo, std::vector < Ort::Value > &modelOutput);
    Ort::Env env;
    Ort::Session * session;
    int32_t width;
    int32_t height;
    int32_t channels;
    uint8_t *dest;
    GstOnnxExecutionProvider m_provider;
    std::vector < Ort::Value > modelOutput;
    std::vector < std::string > labels;
    std::vector < const char *> outputNamesRaw;
    std::vector < Ort::AllocatedStringPtr > outputNames;
    std::vector < GQuark > outputIds;
    GstMlInputImageFormat inputImageFormat;
    GstTensorDataType inputDatatype;
    size_t inputDatatypeSize;
    bool fixedInputImageSize;
    float inputTensorOffset;
    float inputTensorScale;
  };
}

#endif                          /* __GST_ONNX_CLIENT_H__ */
