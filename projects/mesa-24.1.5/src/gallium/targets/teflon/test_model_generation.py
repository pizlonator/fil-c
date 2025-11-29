# MIT License
# 
# Copyright (c) 2021 VeriSilicon, INC.
# Copyright (c) 2023 Tomeu Vizoso
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import math
import os
import os.path
import re
import sys
import tempfile
import time

import numpy as np
import pytest
import json

import tensorflow as tf
from tensorflow import keras

MODEL_PATH = "conv2d.tflite"

def create_model_keras(batch_size, in_w, in_h, k_w, k_h, in_ch, out_ch, stride, padding, signed, seed, depthwise):
    tf.random.set_seed(seed)

    input_shape = [batch_size, in_h, in_w, in_ch]
    out_channel = out_ch
    kernel_shape = [k_w, k_h]
    input_dtype = tf.float32

    if depthwise:
       conv = keras.layers.DepthwiseConv2D(kernel_size=kernel_shape, strides=stride, padding=padding, depth_multiplier=1)
    else:
       conv = keras.layers.Conv2D(filters=out_channel, kernel_size=kernel_shape, strides=stride, padding=padding)

    model = keras.models.Sequential([
        keras.layers.InputLayer(input_shape=input_shape[1:], batch_size=input_shape[0]),
        conv
        ])
    model.build(input_shape=input_shape)

    if depthwise:
      weight_shape = [k_w, k_h, in_ch, 1]
    else:
      weight_shape = [k_w, k_h, in_ch, out_ch]

    weight_data = tf.random.normal(weight_shape, 0, 127, input_dtype, seed=seed)
    bias_data = tf.random.normal((out_ch, ), 0, 127, input_dtype, seed=seed)
    model.set_weights([np.asarray(weight_data, dtype=np.float32), np.asarray(bias_data, dtype=np.float32)])

    tmp = tempfile.NamedTemporaryFile(delete=False, prefix="conv2d-", suffix=".h5", mode="w")
    model.save(tmp.name)
    tmp.close()
    converter = tf.compat.v1.lite.TFLiteConverter.from_keras_model_file(tmp.name)
    os.unlink(tmp.name)

    converter.quantized_input_stats = {model.layers[0].input.name: (128, 128.0)}
    converter.default_ranges_stats = (0.0, 6.0)

    if signed:
      converter.inference_input_type = tf.int8
      converter.inference_output_type = tf.int8
      converter.inference_type = tf.int8
    else:
      converter.inference_input_type = tf.uint8
      converter.inference_output_type = tf.uint8
      converter.inference_type = tf.uint8

    tflite_model = converter.convert()

    fp = open(MODEL_PATH, "wb")
    fp.write(tflite_model)
    fp.flush()

    tf.lite.experimental.Analyzer.analyze(model_path=MODEL_PATH, gpu_compatibility=True)

    return MODEL_PATH

def tflite_to_json(file_path):
    ret = os.system("flatc --json src/gallium/frontends/teflon/tests/tflite_schema.fbs -- " + file_path)
    assert(ret == 0)
    return os.path.splitext(file_path)[0] + ".json"

WEIGHTS_BUFFER = 2
BIAS_BUFFER = 3
VERSION_BUFFER = 5

def zero_irrelevant_values(file_path, signed):
    model_data = open(file_path).read()
    model_data = re.sub("(\\\"(.*?)\\\"|(\\w+))(\\s*:\\s*(\\\".*?\\\"|.))", "\"\\2\\3\"\\4", model_data)
    model = json.loads(model_data)
    #print(json.dumps(model, indent=4))
    if "version" in model["operator_codes"][0].keys():
       del model["operator_codes"][0]["version"]
    for subgraph in model["subgraphs"]:
        for tensor in subgraph["tensors"]:
            tensor["name"] = ""
            if signed:
              tensor["quantization"]["scale"] = [0.0] * len(tensor["quantization"]["scale"])
            else:
              tensor["quantization"]["scale"] = [0.0]
            if signed:
              tensor["quantization"]["zero_point"] = [0] * len(tensor["quantization"]["zero_point"])
            else:
              tensor["quantization"]["zero_point"] = [0]

    model["buffers"][BIAS_BUFFER]["data"] = [0] * len(model["buffers"][BIAS_BUFFER]["data"])
    model["buffers"][WEIGHTS_BUFFER]["data"] = [0] * len(model["buffers"][WEIGHTS_BUFFER]["data"])
    model["buffers"][VERSION_BUFFER]["data"] = [0]

    if "signature_defs" in model:
      del model["signature_defs"]

    open(file_path, "w").write(json.dumps(model, indent=4))
    
def diff(file_1, file_2):
    ret = os.system("diff -U30 -u " + file_1 + " " + file_2)
    assert(ret == 0)

def create_model(batch_size, in_w, in_h, k_w, k_h, in_ch, out_ch, stride, padding, signed, seed, depthwise):
    args = ['build/src/gallium/targets/teflon/test_teflon',
            'generate_model',
            str(in_w),
            str(k_w),
            str(in_ch),
            str(out_ch),
            str(stride),
            "1" if padding == "same" else "0",
            str(int(signed)),
            str(int(depthwise)),
            str(seed)]
    print(' '.join(args))
    os.system(' '.join(args))
    return "model.tflite"

def convolution(batch_size, input_size, weight_size, in_ch, out_ch, stride, padding, signed, seed, depthwise):

    in_w = input_size
    in_h = input_size
    k_w = weight_size
    k_h = weight_size

    # Depthwise convolutions require the out channels to be a multiple of input channels
    assert not (depthwise and out_ch % in_ch != 0)

    # Depthwise convolutions with a single IFM don't make sense
    assert not (depthwise and in_ch == 1)

    # Depthwise convolutions with IFM != OFM are not supported
    assert not (depthwise and out_ch != in_ch)

    np.random.seed(seed)

    model_file = create_model_keras(batch_size, in_w, in_h, k_w, k_h, in_ch, out_ch, stride, padding, signed, seed, depthwise)
    model_file_2 = create_model(batch_size, in_w, in_h, k_w, k_h, in_ch, out_ch, stride, padding, signed, seed, depthwise)

    json_file = tflite_to_json(model_file)
    json_file_2 = tflite_to_json(model_file_2)

    os.unlink(model_file)
    os.unlink(model_file_2)

    zero_irrelevant_values(json_file, signed)
    zero_irrelevant_values(json_file_2, signed)

    #print(json.dumps(json.loads(open(json_file).read()), indent=4))

    diff(json_file, json_file_2)

    os.unlink(json_file)
    os.unlink(json_file_2)

@pytest.mark.parametrize("batch_size",  [1])
@pytest.mark.parametrize("input_size",  [4, 112])
@pytest.mark.parametrize("weight_size", [1, 3])
@pytest.mark.parametrize("in_ch",       [1, 32, 120, 128, 256])
@pytest.mark.parametrize("out_ch",      [1, 32, 120, 128, 256, 480])
@pytest.mark.parametrize("stride",      [1, 2])
@pytest.mark.parametrize("padding",     ["valid", "same"])
@pytest.mark.parametrize("signed",      [False])
@pytest.mark.parametrize("seed",        [4, 5])
def test_conv2d(batch_size, input_size, weight_size, in_ch, out_ch, stride, padding, signed, seed):
  s = "%r-%r-%s-%r-%r-%r-%r-%r-%r" % (seed, signed, padding, stride, out_ch, in_ch, weight_size, input_size, batch_size)
  print(s, file=sys.stderr)
  convolution(batch_size, input_size, weight_size, in_ch, out_ch, stride, padding, signed, seed, depthwise=False)

@pytest.mark.parametrize("batch_size",  [1])
@pytest.mark.parametrize("input_size",  [4, 112])
@pytest.mark.parametrize("weight_size", [3])
@pytest.mark.parametrize("channels",    [32, 128, 256])
@pytest.mark.parametrize("stride",      [1, 2])
@pytest.mark.parametrize("padding",     ["valid", "same"])
@pytest.mark.parametrize("signed",      [False])
@pytest.mark.parametrize("seed",        [4, 5])
def test_depthwise(batch_size, input_size, weight_size, channels, stride, padding, signed, seed):
   s = "%r-%s-%r-%r-%r-%r-%r-%r" % (seed, signed, padding, stride, channels, weight_size, input_size, batch_size)
   print(s, file=sys.stderr)
   convolution(batch_size, input_size, weight_size, channels, channels, stride, padding, signed, seed, depthwise=True)

test_conv2d(1, 80, 5, 16, 128, 2, "same", False, 4)