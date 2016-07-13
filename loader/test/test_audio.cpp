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

#include "etl_audio.hpp"
#include "gen_audio.hpp"

using namespace std;
using namespace nervana;

shared_ptr<audio::decoded> generate_decoded_audio(float frequencyHz, int duration) {
    // generate a decoded audio by first using gen_audio::encode to generate
    // the encoded audio and then decoding it with an extractor.
    // TODO: refactor audio signal generation from encoding step in
    //       gen_audio::encode
    gen_audio gen;
    vector<unsigned char> encoded_audio = gen.encode(frequencyHz, duration);
    nlohmann::json js;

    auto config = make_shared<audio::config>(js);

    audio::extractor extractor(config);
    return extractor.extract((char*)encoded_audio.data(), encoded_audio.size());
}

// This test was disabled because it does not properly configure the audio::config
// structure. This used to often pass but now always fails.
TEST(DISABLED_etl, audio_extract) {
    auto decoded_audio = generate_decoded_audio(1000, 2000);

    // because of the way encoded_audio is being generated, there
    // are 88704 samples instead of 44100 * 2 = 88200 like we would
    // expect ... a fix for another time
    ASSERT_EQ(decoded_audio->getSize(), 88704);
}

TEST(etl, write_wav) {
    auto sg = make_shared<sinewave_generator>(400, 500);
    wav_data wav(sg, 2, 16000, false);
    wav.write_to_file("blah.wav");
}

TEST(etl, audio_transform) {
    auto decoded_audio = generate_decoded_audio(1000, 2000);

    auto js = R"(
        {
            "max_duration": "2000 milliseconds",
            "frame_length": "1024 samples",
            "frame_stride": "256 samples",
            "sample_freq_hz": 44100,
            "feature_type": "mfcc",
            "num_filts": 64
        }
    )"_json;
    auto config = make_shared<audio::config>(js);

    audio::transformer _imageTransformer(config);
    audio::param_factory factory(config);

    auto audioParams = factory.make_params(decoded_audio);

    _imageTransformer.transform(audioParams, decoded_audio);

    ASSERT_EQ(config->get_shape()[0], 1);
    ASSERT_EQ(config->get_shape()[1], 40);
    ASSERT_EQ(config->get_shape()[1], 40);

}
