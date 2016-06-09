#include "etl_image.hpp"

using namespace std;

void nervana::image::settings::dump(ostream & ostr)
{
    ostr << "Angle: " << setw(3) << angle << " ";
    ostr << "Flip: " << flip << " ";
    ostr << "Filled: " << filled << " ";
    ostr << "Lighting: ";
    for_each (lighting.begin(), lighting.end(), [&ostr](float &l) {ostr << l << " ";});
    ostr << "Photometric: ";
    for_each (photometric.begin(), photometric.end(), [&ostr](float &p) {ostr << p << " ";});
    ostr << "\n" << "Crop Box: " << cropbox << "\n";
}



/* Extract */
nervana::image::extractor::extractor(shared_ptr<const nervana::image::config> cfg)
{
    if (!(cfg->num_channels == 1 || cfg->num_channels == 3))
    {
        std::stringstream ss;
        ss << "Unsupported number of channels in image: " << cfg->num_channels;
        throw std::runtime_error(ss.str());
    } else {
        _pixel_type = cfg->num_channels == 1 ? CV_8UC1 : CV_8UC3;
        _color_mode = cfg->num_channels == 1 ? CV_LOAD_IMAGE_GRAYSCALE : CV_LOAD_IMAGE_COLOR;
    }

}

std::shared_ptr<nervana::image::decoded> nervana::image::extractor::extract(char* inbuf, int insize)
{
    cv::Mat output_img;
    cv::Mat input_img(1, insize, _pixel_type, inbuf);
    cv::imdecode(input_img, _color_mode, &output_img);
    return make_shared<nervana::image::decoded>(output_img);
}


/* Transform:
    image::config will be a supplied bunch of settings used by this provider.
    on each record, the transformer will use the config along with the supplied
    record to fill a transform_settings structure which will have

    Spatial distortion settings:
    randomly sampled crop box (based on params->center, params->aspect_ratio, params->scale_pct, record size)
    randomly determined flip (based on params->flip)
    randomly sampled rotation angle (based on params->angle)

    Photometric distortion settings:
    randomly sampled contrast, brightness, saturation, lighting values (based on params->cbs, lighting bounds)

*/
nervana::image::transformer::transformer(shared_ptr<nervana::image::config> cfg)
{
    _size = cv::Size2i(cfg->width, cfg->height);
    _do_area_scale = cfg->do_area_scale;

    _icp = cfg;
}

std::shared_ptr<nervana::image::decoded> nervana::image::transformer::transform(settings_ptr transform_settings,
                                                 std::shared_ptr<nervana::image::decoded> img)
{
    auto img_xform = static_pointer_cast<nervana::image::settings>(transform_settings);
    cv::Mat rotatedImage;
    rotate(img->get_image(0), rotatedImage, img_xform->angle);
    cv::Mat croppedImage = rotatedImage(img_xform->cropbox);

    cv::Mat resizedImage;
    resize(croppedImage, resizedImage, _size);
    cbsjitter(resizedImage, img_xform->photometric);
    lighting(resizedImage, img_xform->lighting);

    cv::Mat *finalImage = &resizedImage;
    cv::Mat flippedImage;
    if (img_xform->flip) {
        cv::flip(resizedImage, flippedImage, 1);
        finalImage = &flippedImage;
    }

    return make_shared<nervana::image::decoded>(*finalImage);
}

void nervana::image::transformer::fill_settings(settings_ptr transform_settings,
                                                std::shared_ptr<nervana::image::decoded> input,
                                                default_random_engine &dre)
{
    auto imgstgs = static_pointer_cast<nervana::image::settings>(transform_settings);

    imgstgs->angle = _icp->angle(dre);
    imgstgs->flip  = _icp->flip(dre);

    cv::Size2f in_size = input->get_image_size();

    float scale = _icp->scale(dre);
    float aspect_ratio = _icp->aspect_ratio(dre);
    cout << "ASPECT_RATIO CHOSEN " << aspect_ratio << "\n";
    scale_cropbox(in_size, imgstgs->cropbox, aspect_ratio, scale);

    float c_off_x = _icp->crop_offset(dre);
    float c_off_y = _icp->crop_offset(dre);
    shift_cropbox(in_size, imgstgs->cropbox, c_off_x, c_off_y);

    for_each(imgstgs->lighting.begin(),
             imgstgs->lighting.end(),
             [this, &dre] (float &n) {n = _icp->lighting(dre);});
    for_each(imgstgs->photometric.begin(),
             imgstgs->photometric.end(),
             [this, &dre] (float &n) {n = _icp->photometric(dre);});
}

void nervana::image::transformer::shift_cropbox(
                            const cv::Size2f &in_size,
                            cv::Rect &crop_box,
                            float off_x,
                            float off_y )
{
    crop_box.x = (in_size.width - crop_box.width) * off_x;
    crop_box.y = (in_size.height - crop_box.height) * off_y;
}

void nervana::image::transformer::scale_cropbox(
                            const cv::Size2f &in_size,
                            cv::Rect &crop_box,
                            float tgt_aspect_ratio,
                            float tgt_scale )
{
    cv::Size2f out_size = _size;
    float out_a_r = out_size.width / out_size.height;
    float in_a_r  = in_size.width / in_size.height;

    float crop_a_r = out_a_r * tgt_aspect_ratio;

    if (_do_area_scale) {
        // Area scaling -- use pctge of original area subject to aspect ratio constraints
        float max_scale = in_a_r > crop_a_r ? crop_a_r /  in_a_r : in_a_r / crop_a_r;
        float tgt_area  = std::min(tgt_scale, max_scale) * in_size.area();

        crop_box.height = sqrt(tgt_area / crop_a_r);
        crop_box.width  = crop_box.height * crop_a_r;
    } else {
        // Linear scaling -- make the long crop box side  the scale pct of the short orig side
        float short_side = std::min(in_size.width, in_size.height);

        if (crop_a_r < 1) { // long side is height
            crop_box.height = tgt_scale * short_side;
            crop_box.width  = crop_box.height * crop_a_r;
        } else {
            crop_box.width  = tgt_scale * short_side;
            crop_box.height = crop_box.width / crop_a_r;
        }
    }
}


void nervana::image::transformer::rotate(const cv::Mat& input, cv::Mat& output, int angle)
{
    if (angle == 0) {
        output = input;
    } else {
        cv::Point2i pt(input.cols / 2, input.rows / 2);
        cv::Mat rot = cv::getRotationMatrix2D(pt, angle, 1.0);
        cv::warpAffine(input, output, rot, input.size());
    }
}

void nervana::image::transformer::resize(const cv::Mat& input, cv::Mat& output, const cv::Size2i& size)
{
    if (size == input.size()) {
        output = input;
    } else {
        int inter = input.size().area() < size.area() ? CV_INTER_CUBIC : CV_INTER_AREA;
        cv::resize(input, output, size, 0, 0, inter);
    }
}

void nervana::image::transformer::lighting(cv::Mat& inout, vector<float> lighting)
{
}

void nervana::image::transformer::cbsjitter(cv::Mat& inout, vector<float> photometric)
{
}


nervana::image::loader::loader(shared_ptr<const nervana::image::config> cfg)
{
    _channel_major = cfg->channel_major;
}

void nervana::image::loader::load(char* outbuf, int outsize, std::shared_ptr<nervana::image::decoded> input)
{
    auto img = input->get_image(0);
    int all_pixels = img.channels() * img.total();

    if (all_pixels > outsize) {
        throw std::runtime_error("Load failed - buffer too small");
    }

    if (_channel_major) {
        this->split(img, outbuf);
    } else {
        memcpy(outbuf, img.data, all_pixels);
    }
}

void nervana::image::loader::split(cv::Mat& img, char* buf)
{
    int pix_per_channel = img.total();
    int num_channels = img.channels();

    if (num_channels == 1) {
        memcpy(buf, img.data, pix_per_channel);
    } else {
        // Split into separate channels
        cv::Size2i size = img.size();
        cv::Mat b(size, CV_8U, buf);
        cv::Mat g(size, CV_8U, buf + pix_per_channel);
        cv::Mat r(size, CV_8U, buf + 2 * pix_per_channel);

        cv::Mat channels[3] = {b, g, r};
        cv::split(img, channels);
    }

}

