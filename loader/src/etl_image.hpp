#pragma once
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "etl_interface.hpp"
#include "params.hpp"


namespace nervana {
    namespace image {
        class config;
        class settings;
        class decoded;

        class extractor;
        class transformer;
        class loader;
    }
}


class nervana::image::config : public nervana::json_config_parser {
public:
    int height;
    int width;

    std::uniform_real_distribution<float> scale{1.0f, 1.0f};
    std::uniform_int_distribution<int>    angle{0, 0};
    std::normal_distribution<float>       lighting{0.0f, 0.0f};
    std::uniform_real_distribution<float> aspect_ratio{1.0f, 1.0f};
    std::uniform_real_distribution<float> photometric{0.0f, 0.0f};
    std::uniform_real_distribution<float> crop_offset{0.5f, 0.5f};
    std::bernoulli_distribution           flip{0};

    bool do_area_scale = false;
    bool channel_major = true;
    int num_channels = 3;

    config(std::string argString) {
        auto js = nlohmann::json::parse(argString);

        parse_req(height, "height", js);
        parse_req(width, "width", js);

        parse_opt(do_area_scale, "do_area_scale", js);
        parse_opt(num_channels, "num_channels", js);
        parse_opt(channel_major, "channel_major", js);

        parse_dist(angle, "dist_params/angle", js);
        parse_dist(scale, "dist_params/scale", js);
        parse_dist(lighting, "dist_params/lighting", js);
        parse_dist(aspect_ratio, "dist_params/aspect_ratio", js);
        parse_dist(photometric, "dist_params/photometric", js);
        parse_dist(crop_offset, "dist_params/crop_offset", js);
        parse_dist(flip, "dist_params/flip", js);
        validate();
    }

private:
    bool validate() {
        return crop_offset.param().a() <= crop_offset.param().b();
    }
};

class nervana::image::settings : public nervana::settings {
public:

    settings() {}
    void dump(std::ostream & = std::cout);

    cv::Rect cropbox;
    cv::Size2i output_size;
    int angle = 0;
    bool flip = false;
    std::vector<float> lighting{0.0, 0.0, 0.0};  //pixelwise random values
    std::vector<float> photometric{0.0, 0.0, 0.0};  // contrast, brightness, saturation
    bool filled = false;
};


class nervana::image::decoded : public nervana::decoded_media {
public:
    decoded() {}
    decoded(cv::Mat img) : _img(img) { _images.push_back(_img); }
    virtual ~decoded() override {}

    virtual MediaType get_type() override { return MediaType::IMAGE; }
    cv::Mat& get_image(int index) { return _images[index]; }
    cv::Size2i get_image_size() {return _images[0].size(); }
    size_t size() const { return _images.size(); }

private:
    cv::Mat _img;
    std::vector<cv::Mat> _images;
};


class nervana::image::extractor : public nervana::interface::extractor {
public:
    extractor(config_ptr);
    ~extractor() {}
    virtual media_ptr extract(char*, int) override;

    const int get_channel_count() {return _channel_count;}
private:
    int _channel_count;
    int _pixel_type;
    int _color_mode;
};


class nervana::image::transformer : public nervana::interface::transformer {
public:
    transformer(config_ptr);
    ~transformer() {}
    virtual media_ptr transform(settings_ptr, const media_ptr&) override;
    virtual void fill_settings(settings_ptr, const media_ptr&, std::default_random_engine &) override;

private:
    void rotate(const cv::Mat& input, cv::Mat& output, int angle);
    void resize(const cv::Mat& input, cv::Mat& output, const cv::Size2i& size);
    void lighting(cv::Mat& inout, std::vector<float>);
    void cbsjitter(cv::Mat& inout, std::vector<float>);
    void scale_cropbox(const cv::Size2f&, cv::Rect&, float, float);
    void shift_cropbox(const cv::Size2f&, cv::Rect&, float, float);

    std::shared_ptr<image::config> _icp;
};


class nervana::image::loader : public nervana::interface::loader {
public:
    loader(config_ptr);
    ~loader() {}
    virtual void load(char*, int, const media_ptr&) override;

private:
    void split(cv::Mat&, char*);
    bool _channel_major;
};

