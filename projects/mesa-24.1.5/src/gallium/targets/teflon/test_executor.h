/*
 * Copyright (c) 2023-2024 Tomeu Vizoso <tomeu@tomeuvizoso.net>
 * SPDX-License-Identifier: MIT
 */

enum executor {
   EXECUTOR_CPU,
   EXECUTOR_NPU,
};

struct TfLiteModel;

std::vector<uint8_t> conv2d_generate_model(int input_size,
                                           int weight_size,
                                           int input_channels,
                                           int output_channels,
                                           int stride,
                                           bool padding_same,
                                           bool is_signed,
                                           bool depthwise);

std::vector<uint8_t> add_generate_model(int input_size,
                                        int weight_size,
                                        int input_channels,
                                        int output_channels,
                                        int stride,
                                        bool padding_same,
                                        bool is_signed,
                                        bool depthwise);

std::vector<std::vector<uint8_t>> run_model(TfLiteModel *model, enum executor executor, std::vector<std::vector<uint8_t>> &input);