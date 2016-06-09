#include "gtest/gtest.h"
#include "argtype.hpp"
#include "datagen.hpp"
#include "batchfile.hpp"

#include "params.hpp"
#include "etl_interface.hpp"
#include "etl_image.hpp"
#include "etl_label.hpp"
#include "etl_bbox.hpp"
#include "etl_lmap.hpp"
#include "provider.hpp"
#include "json.hpp"

extern DataGen _datagen;

using namespace std;
using namespace nervana;

TEST(provider, argtype) {

    {
        /* Create extractor with default num channels param */
        string cfgString = "{\"height\":10, \"width\":10}";
        auto ic = make_shared<image::extractor>(make_shared<image::config>(cfgString));
        EXPECT_EQ(ic->get_channel_count(), 3);
    }


    {
        string cfgString = R"(
            {
                "height": 30,
                "width" : 30,
                "dist_params/angle" : [-20, 20],
                "dist_params/scale" : [0.2, 0.8],
                "dist_params/lighting" : [0.0, 0.1],
                "dist_params/aspect_ratio" : [0.75, 1.33],
                "dist_params/flip" : [false]
            }
        )";

        auto itpj = make_shared<image::config>(cfgString);

        // output the fixed parameters
        cout << "HEIGHT: " << itpj->height << endl;
        cout << "WIDTH: "  << itpj->width  << endl;

        // output the random parameters
        default_random_engine r_eng(0);
        image::param_factory img_prm_maker(itpj);
        auto imgt = make_shared<image::transformer>(itpj);

        auto input_img_ptr = make_shared<image::decoded>(cv::Mat(256, 320, CV_8UC3));

        auto its = img_prm_maker.make_params(input_img_ptr, r_eng);
        its->dump();
    }


    {
        string cfgString = R"(
            {
                "extract offset":  20,
                "dist_params/transform scale":  [-8, 8],
                "dist_params/transform shift":  [-5, 5]
            }
        )";
        auto lblcfg = make_shared<label::config>(cfgString);

        BatchFileReader bf;
        auto dataFiles = _datagen.GetFiles();
        ASSERT_GT(dataFiles.size(),0);
        string batchFileName = dataFiles[0];
        bf.open(batchFileName);

        // Just get a single item
        auto data = bf.read();
        auto labels = bf.read();
        bf.close();


        label::param_factory lbl_prm_maker(lblcfg);

        default_random_engine r_eng(0);
        // Note we don't need the media to get params
        auto lstg = lbl_prm_maker.make_params(nullptr, r_eng);

        cout << "Set scale: " << lstg->scale << " ";
        cout << "Set shift: " << lstg->shift << endl;

        int reference = ((int) (*labels)[0] + lblcfg->ex_offset)* lstg->scale + lstg->shift;

        // Take the int and do provision with it.
        auto lble = make_shared<label::extractor>(lblcfg);
        auto lblt = make_shared<label::transformer>(lblcfg);

        {
            auto lbll = make_shared<label::loader>(lblcfg);

            int reference_target = reference;
            int loaded_target = 0;
            provider<nervana::label::decoded, nervana::label::params> pp{lble, lblt, lbll};
            pp.provide(labels->data(), 4, (char *)(&loaded_target), 4, lstg);
            EXPECT_EQ(reference_target, loaded_target);
        }

        {
            // This is a float loader that loads into a float with an offset
            string lArgString = R"(
                {
                    "load do float":  true,
                    "load offset": 0.8
                }
            )";
            auto flt_lbl_config = make_shared<label::config>(lArgString);

            auto lbll = make_shared<label::loader>(flt_lbl_config);

            float reference_target = reference + 0.8;
            float loaded_target = 0.0;
            provider<nervana::label::decoded, nervana::label::params> pp{lble, lblt, lbll};
            pp.provide(labels->data(), 4, (char *)(&loaded_target), 4, lstg);
            EXPECT_EQ(reference_target, loaded_target);
        }
    }
}
