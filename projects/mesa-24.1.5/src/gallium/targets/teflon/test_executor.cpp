/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

#include <dlfcn.h>
#include <stdio.h>
#include <vector>
#include <gtest/gtest.h>
#include <xtensor/xrandom.hpp>

#include "util/macros.h"

#include "tensorflow/lite/c/c_api.h"
#include "tensorflow/lite/c/common.h"

#include <fcntl.h>
#include "test_executor.h"
#include "tflite-schema-v2.15.0_generated.h"

static float
randf(float min, float max)
{
   return ((max - min) * ((float)rand() / (float)RAND_MAX)) + min;
}

static void
read_model(const char *file_name, tflite::ModelT &model)
{
   std::ostringstream file_path;
   assert(getenv("TEFLON_TEST_DATA"));
   file_path << getenv("TEFLON_TEST_DATA") << "/" << file_name;

   FILE *f = fopen(file_path.str().c_str(), "rb");
   assert(f);
   fseek(f, 0, SEEK_END);
   long fsize = ftell(f);
   fseek(f, 0, SEEK_SET);
   void *buf = malloc(fsize);
   fread(buf, fsize, 1, f);
   fclose(f);

   tflite::GetModel(buf)->UnPackTo(&model);
}

static void
patch_conv2d(unsigned operation_index,
             tflite::ModelT *model,
             int input_size,
             int weight_size,
             int input_channels,
             int output_channels,
             int stride,
             bool padding_same,
             bool is_signed,
             bool depthwise)
{
   unsigned output_size = 0;
   unsigned input_index;
   unsigned weights_index;
   unsigned bias_index;
   unsigned output_index;
   unsigned weights_buffer_index;
   unsigned bias_buffer_index;

   auto subgraph = model->subgraphs[0];

   /* Operation */
   if (depthwise) {
      auto value = new tflite::DepthwiseConv2DOptionsT();
      value->depth_multiplier = 1;
      value->padding = padding_same ? tflite::Padding_SAME : tflite::Padding_VALID;
      value->stride_w = stride;
      value->stride_h = stride;
      value->dilation_w_factor = 1;
      value->dilation_h_factor = 1;
      subgraph->operators[operation_index]->builtin_options.value = value;
      subgraph->operators[operation_index]->builtin_options.type = tflite::BuiltinOptions_DepthwiseConv2DOptions;

      model->operator_codes[0]->deprecated_builtin_code = 4;
      model->operator_codes[0]->builtin_code = tflite::BuiltinOperator_DEPTHWISE_CONV_2D;
   } else {
      auto value = new tflite::Conv2DOptionsT();
      value->padding = padding_same ? tflite::Padding_SAME : tflite::Padding_VALID;
      value->stride_w = stride;
      value->stride_h = stride;
      subgraph->operators[operation_index]->builtin_options.value = value;
   }

   input_index = subgraph->operators[operation_index]->inputs.data()[0];
   weights_index = subgraph->operators[operation_index]->inputs.data()[1];
   bias_index = subgraph->operators[operation_index]->inputs.data()[2];
   output_index = subgraph->operators[operation_index]->outputs.data()[0];

   /* Input */
   auto input_tensor = subgraph->tensors[input_index];
   input_tensor->shape.data()[0] = 1;
   input_tensor->shape.data()[1] = input_size;
   input_tensor->shape.data()[2] = input_size;
   input_tensor->shape.data()[3] = input_channels;
   input_tensor->type = is_signed ? tflite::TensorType_INT8 : tflite::TensorType_UINT8;

   /* Bias */
   auto bias_tensor = subgraph->tensors[bias_index];
   bias_buffer_index = bias_tensor->buffer;
   bias_tensor->shape.data()[0] = output_channels;

   auto bias_data = &model->buffers[bias_buffer_index]->data;
   xt::xarray<int32_t> bias_array = xt::random::randint<int32_t>({output_channels}, -20000, 20000);
   bias_data->resize(bias_array.size() * sizeof(int32_t));
   memcpy(bias_data->data(), bias_array.data(), bias_array.size() * sizeof(int32_t));

   /* Weight */
   auto weight_tensor = subgraph->tensors[weights_index];
   weights_buffer_index = weight_tensor->buffer;
   if (depthwise) {
      weight_tensor->shape.data()[0] = 1;
      weight_tensor->shape.data()[1] = weight_size;
      weight_tensor->shape.data()[2] = weight_size;
      weight_tensor->shape.data()[3] = output_channels;
   } else {
      weight_tensor->shape.data()[0] = output_channels;
      weight_tensor->shape.data()[1] = weight_size;
      weight_tensor->shape.data()[2] = weight_size;
      weight_tensor->shape.data()[3] = input_channels;
   }
   weight_tensor->type = is_signed ? tflite::TensorType_INT8 : tflite::TensorType_UINT8;

   auto weights_data = &model->buffers[weights_buffer_index]->data;
   std::vector<int> weight_shape;
   if (depthwise)
      weight_shape = {1, weight_size, weight_size, output_channels};
   else
      weight_shape = {output_channels, weight_size, weight_size, input_channels};

   xt::xarray<uint8_t> weights_array = xt::random::randint<uint8_t>(weight_shape, 0, 255);
   weights_data->resize(weights_array.size());
   memcpy(weights_data->data(), weights_array.data(), weights_array.size());

   /* Output */
   if (padding_same)
      output_size = (input_size + stride - 1) / stride;
   else
      output_size = (input_size + stride - weight_size) / stride;

   auto output_tensor = subgraph->tensors[output_index];
   output_tensor->shape.data()[0] = 1;
   output_tensor->shape.data()[1] = output_size;
   output_tensor->shape.data()[2] = output_size;
   output_tensor->shape.data()[3] = output_channels;
   output_tensor->type = is_signed ? tflite::TensorType_INT8 : tflite::TensorType_UINT8;
}

std::vector<uint8_t>
conv2d_generate_model(int input_size,
                      int weight_size,
                      int input_channels,
                      int output_channels,
                      int stride,
                      bool padding_same,
                      bool is_signed,
                      bool depthwise)
{
   tflite::ModelT model;
   read_model("conv2d.tflite", model);

   patch_conv2d(0, &model, input_size, weight_size, input_channels, output_channels, stride, padding_same, is_signed, depthwise);

   flatbuffers::FlatBufferBuilder builder;
   builder.Finish(tflite::Model::Pack(builder, &model), "TFL3");

   return {builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize()};
}

static void
patch_quant_for_add(tflite::ModelT *model)
{
   auto subgraph = model->subgraphs[0];
   auto add_op = subgraph->operators[2];

   auto input_index = add_op->inputs.data()[0];
   auto input_tensor = subgraph->tensors[input_index];
   input_tensor->quantization->scale[0] = randf(0.0078125, 0.4386410117149353);
   input_tensor->quantization->zero_point[0] = rand() % 255;

   input_index = add_op->inputs.data()[1];
   input_tensor = subgraph->tensors[input_index];
   input_tensor->quantization->scale[0] = randf(0.0078125, 0.4386410117149353);
   input_tensor->quantization->zero_point[0] = rand() % 255;
}

std::vector<uint8_t>
add_generate_model(int input_size,
                   int weight_size,
                   int input_channels,
                   int output_channels,
                   int stride,
                   bool padding_same,
                   bool is_signed,
                   bool depthwise)
{
   tflite::ModelT model;
   read_model("add.tflite", model);

   patch_conv2d(0, &model, input_size, weight_size, input_channels, output_channels, stride, padding_same, is_signed, depthwise);
   patch_conv2d(1, &model, input_size, weight_size, input_channels, output_channels, stride, padding_same, is_signed, depthwise);
   patch_quant_for_add(&model);

   /* Output */
   auto subgraph = model.subgraphs[0];
   unsigned input_index = subgraph->operators[2]->inputs.data()[0];
   unsigned output_index = subgraph->operators[2]->outputs.data()[0];

   auto input_tensor = subgraph->tensors[input_index];
   auto output_tensor = subgraph->tensors[output_index];
   output_tensor->shape.data()[0] = input_tensor->shape.data()[0];
   output_tensor->shape.data()[1] = input_tensor->shape.data()[1];
   output_tensor->shape.data()[2] = input_tensor->shape.data()[2];
   output_tensor->shape.data()[3] = input_tensor->shape.data()[3];
   output_tensor->type = is_signed ? tflite::TensorType_INT8 : tflite::TensorType_UINT8;

   flatbuffers::FlatBufferBuilder builder;
   builder.Finish(tflite::Model::Pack(builder, &model), "TFL3");

   return {builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize()};
}

static void
tflite_error_cb(void *user_data, const char *format, va_list args)
{
   vfprintf(stderr, format, args);
}

TfLiteDelegate *(*tflite_plugin_create_delegate)(char **options_keys,
                                                 char **options_values,
                                                 size_t num_options,
                                                 void (*report_error)(const char *));

void (*tflite_plugin_destroy_delegate)(TfLiteDelegate *delegate);

static void
load_delegate()
{
   const char *delegate_path = getenv("TEFLON_TEST_DELEGATE");
   assert(delegate_path);

   void *delegate_lib = dlopen(delegate_path, RTLD_LAZY | RTLD_LOCAL);
   assert(delegate_lib);

   tflite_plugin_create_delegate = reinterpret_cast<TfLiteDelegate *(*)(char **options_keys,
                                                                        char **options_values,
                                                                        size_t num_options,
                                                                        void (*report_error)(const char *))>(
      dlsym(delegate_lib, "tflite_plugin_create_delegate"));
   assert(tflite_plugin_create_delegate);

   tflite_plugin_destroy_delegate = reinterpret_cast<void (*)(TfLiteDelegate *delegate)>(
      dlsym(delegate_lib, "tflite_plugin_destroy_delegate"));
   assert(tflite_plugin_destroy_delegate);
}

std::vector<std::vector<uint8_t>>
run_model(TfLiteModel *model, enum executor executor, std::vector<std::vector<uint8_t>> &input)
{
   TfLiteDelegate *delegate = NULL;
   TfLiteInterpreterOptions *options = TfLiteInterpreterOptionsCreate();
   bool generate_random_input = input.empty();
   std::vector<std::vector<uint8_t>> output;

   if (executor == EXECUTOR_NPU) {
      load_delegate();
      delegate = tflite_plugin_create_delegate(NULL, NULL, 0, NULL);
      TfLiteInterpreterOptionsAddDelegate(options, delegate);
   }

   TfLiteInterpreterOptionsSetErrorReporter(options, tflite_error_cb, NULL);

   TfLiteInterpreter *interpreter = TfLiteInterpreterCreate(model, options);
   assert(interpreter);

   TfLiteInterpreterAllocateTensors(interpreter);

   unsigned input_tensors = TfLiteInterpreterGetInputTensorCount(interpreter);
   for (unsigned i = 0; i < input_tensors; i++) {
      TfLiteTensor *input_tensor = TfLiteInterpreterGetInputTensor(interpreter, i);

      if (generate_random_input) {
         int shape[4] = {input_tensor->dims->data[0],
                         input_tensor->dims->data[1],
                         input_tensor->dims->data[2],
                         input_tensor->dims->data[3]};
         xt::xarray<uint8_t> a = xt::random::randint<uint8_t>(shape, 0, 255);
         input.push_back({a.begin(), a.end()});
      }

      TfLiteTensorCopyFromBuffer(input_tensor, input[i].data(), input_tensor->bytes);
   }

   EXPECT_EQ(TfLiteInterpreterInvoke(interpreter), kTfLiteOk);

   unsigned output_tensors = TfLiteInterpreterGetOutputTensorCount(interpreter);
   for (unsigned i = 0; i < output_tensors; i++) {
      const TfLiteTensor *output_tensor = TfLiteInterpreterGetOutputTensor(interpreter, i);

      std::vector<uint8_t> out;
      out.resize(output_tensor->bytes);
      EXPECT_EQ(TfLiteTensorCopyToBuffer(output_tensor, out.data(), output_tensor->bytes), kTfLiteOk);

      output.push_back(out);
   }

   TfLiteInterpreterDelete(interpreter);
   if (executor == EXECUTOR_NPU)
      tflite_plugin_destroy_delegate(delegate);
   TfLiteInterpreterOptionsDelete(options);

   return output;
}
