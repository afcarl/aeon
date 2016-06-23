/*
 Copyright 2015 Nervana Systems Inc.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#pragma once

#if HAS_IMGLIB
#include <opencv2/core/core.hpp>
#endif

#define UNSUPPORTED_MEDIA_MESSAGE "support not built-in. Please install the " \
                                  "pre-requisites and re-run the installer."
class Matrix {
public:
    static void transpose(char* data, int height, int width, int elemLen) {
#if HAS_IMGLIB
        int elemType;
        if (elemLen == 1) {
            elemType = CV_8UC1;
        } else if (elemLen == 4) {
            elemType = CV_32F;
        } else {
            throw std::runtime_error("Unsupported type in transpose\n");
        }
        cv::Mat input = cv::Mat(height, width, elemType, data).clone();
        cv::Mat output = cv::Mat(width, height, elemType, data);
        cv::transpose(input, output);
#else
#warning ("OpenCV support not built-in. Certain features will not work.")
        std::string message = "OpenCV " UNSUPPORTED_MEDIA_MESSAGE;
        throw std::runtime_error(message);
#endif
    }
};
