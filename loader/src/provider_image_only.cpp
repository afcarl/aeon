#include "provider_image_only.hpp"

using namespace nervana;
using namespace std;

image_only::image_only(nlohmann::json js) :
    image_config(js["image"]),
    image_extractor(image_config),
    image_transformer(image_config),
    image_loader(image_config),
    image_factory(image_config)
{
    num_inputs = 1;
    oshapes.push_back(image_config.get_shape_type());
}

void image_only::provide(int idx, buffer_in_array& in_buf, buffer_out_array& out_buf) {
    std::vector<char>& datum_in  = in_buf[0]->get_item(idx);
    char* datum_out  = out_buf[0]->get_item(idx);

    if (datum_in.size() == 0) {
        std::stringstream ss;
        ss << "received encoded image with size 0, at idx " << idx;
        throw std::runtime_error(ss.str());
    }

    // Process image data
    auto image_dec = image_extractor.extract(datum_in.data(), datum_in.size());
    auto image_params = image_factory.make_params(image_dec);
    image_loader.load({datum_out}, image_transformer.transform(image_params, image_dec));
}
