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

#include <vector>
#include <string>

#include "gtest/gtest.h"
#include "argtype.hpp"
#include "datagen.hpp"
#include "params.hpp"
#include "etl_interface.hpp"
#include "etl_image.hpp"
#include "etl_label.hpp"
#include "provider.hpp"

extern DataGen _datagen;

using namespace std;
using namespace nervana;

static param_ptr _ip1 = make_shared<image_params>();
static param_ptr _lp1 = make_shared<label_params>();

// An Integer Provider

class label_extractor ::

TEST(myloader, argtype) {
    map<string,shared_ptr<interface_ArgType> > args = _ip1->get_args();
    ASSERT_EQ(11, args.size());

    {
        string argString = "-h 220 -w 23 -s1 0.8";
        EXPECT_TRUE(_ip1->parse(argString)) << "**** missing required arguments in '" << argString << "'";
        auto ie_p = make_shared<image_extractor>(_ip1);
        EXPECT_EQ(ie_p->get_channel_count(), 3);
    }
    {
        BatchFileReader bf;
        string batchFileName = _datagen.GetDatasetPath() + "/archive-0.cpio";
        bf.open(batchFileName);

        // Just get a single item
        auto data = bf.read();
        auto labels = bf.read();
        bf.close();

        // Take the int and do provision with it.



    }

}
