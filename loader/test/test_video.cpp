/*
 Copyright 2016 Nervana Systems Inc.
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

#include "gtest/gtest.h"

#include "etl_video.hpp"
#include "gen_video.hpp"

using namespace std;
using namespace nervana;

TEST(etl, video_extract_transform) {
    int width = 352;
    int height = 288;

    // 1 second video at 25 FPS
    vector<unsigned char> vid = gen_video().encode(1000);

    // extract
    nlohmann::json js = {{"width", width},{"height",height},{"frame_count",5}};
    video::config config(js);

    video::extractor extractor{config};
    auto decoded_vid = extractor.extract((char*)vid.data(), vid.size());

    ASSERT_EQ(decoded_vid->get_image_count(), 25);
    ASSERT_EQ(decoded_vid->get_image_size(), cv::Size2i(width, height));

    // transform
    video::transformer transformer = video::transformer(config);

    video::param_factory factory(config);
    auto params = factory.make_params(decoded_vid);

    params->output_size = cv::Size2i(width/2, height/2);
    auto transformed_vid = transformer.transform(params, decoded_vid);
    ASSERT_NE(nullptr, transformed_vid);

    // make sure we still have the same number of images and that the
    // size has been reduced
    ASSERT_EQ(transformed_vid->get_image_count(), 25);
    ASSERT_EQ(transformed_vid->get_image_size(), cv::Size2i(width/2, height/2));
}

TEST(etl, video_image_transform) {
    int width = 352;
    int height = 288;

    auto decoded_image = make_shared<image::decoded>();
    cv::Mat mat_image(width, height, CV_8UC3, 0.0);
    decoded_image->add(mat_image);

    nlohmann::json js = {{"width", width},{"height",height}};
    image::config config(js);

    image::transformer _imageTransformer(config);

    image::param_factory factory(config);
    auto imageParams = factory.make_params(decoded_image);
    imageParams->output_size = cv::Size2i(width/2, height/2);

    _imageTransformer.transform(imageParams, decoded_image);
}

unsigned char expected_value(int d, int h, int w, int c) {
    // set up expected_value in final outbuf so that viewed in
    // order in memory you see 0, 1, 2, 3, ...
    // the expected value of outbuf at index expected_value is expected_value
    return (((((c * 5) + d) * 4) + h) * 2) + w;
}

TEST(etl, video_loader) {
    // set up video::decoded with specific values
    // the color of any pixel channel should
    // = channel
    // + width * 3
    // + height * 2 * 3
    // + depth * 4 * 2 * 3
    // each dimension is unique to help debug and detect incorrect
    // dimension ordering
    // extract
    nlohmann::json js = {{"height",4},{"width",2},{"channels",3},{"frame_count",5}};
    video::config vconfig{js};

    int channels, height, width, depth;
    tie(channels, height, width, depth) = make_tuple(vconfig.channels,
                                                     vconfig.height,
                                                     vconfig.width,
                                                     vconfig.frame_count);

    shared_ptr<video::decoded> decoded = make_shared<video::decoded>();

    for(int d = 0; d < depth; ++d) {
        cv::Mat image(height, width, CV_8UC3, 0.0);
        for(int w = 0; w < width; ++w) {
            for(int h = 0; h < height; ++h) {
                for(int c = 0; c < channels; ++c) {
                    image.at<cv::Vec3b>(h, w).val[c] = expected_value(d, h, w, c);
                }
            }
        }
        decoded->add(image);
    }

    // now run the loader
    vector<unsigned char> outbuf;

    int outbuf_size = channels * width * height * depth;
    outbuf.resize(outbuf_size);

    video::loader loader(vconfig);
    loader.load({outbuf.data()}, decoded);

    // make sure outbuf has data in it like we expect
    for(int c = 0; c < channels; ++c) {
        for(int d = 0; d < depth; ++d) {
            for(int h = 0; h < height; ++h) {
                for(int w = 0; w < width; ++w) {
                    unsigned char v = expected_value(d, h, w, c);
                    ASSERT_EQ(outbuf[v], v);
                }
            }
        }
    }
}
