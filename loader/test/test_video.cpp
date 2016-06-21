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
    auto config = make_shared<video::config>();
    config->width = width;
    config->height = height;

    video::extractor extractor = video::extractor(config);
    auto decoded_vid = extractor.extract((char*)vid.data(), vid.size());

    ASSERT_EQ(decoded_vid->get_image_count(), 25);
    ASSERT_EQ(decoded_vid->get_image_size(), cv::Size2i(width, height));

    // transform
    video::transformer transformer = video::transformer(config);

    std::default_random_engine dre;
    image::param_factory factory(config, dre);
    auto imageParams = factory.make_params(decoded_vid);

    imageParams->output_size = cv::Size2i(width/2, height/2);
    auto params = make_shared<video::params>(imageParams);
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
    cout << "mat_image " << mat_image.size() << endl;
    decoded_image->add(mat_image);
    cout << "d " << decoded_image->get_image_size() << endl;

    auto config = make_shared<image::config>();
    config->width = width;
    config->height = height;

    image::transformer _imageTransformer(config);

    std::default_random_engine dre;
    image::param_factory factory(config, dre);
    auto imageParams = factory.make_params(decoded_image);
    imageParams->output_size = cv::Size2i(width/2, height/2);

    _imageTransformer.transform(imageParams, decoded_image);
}
