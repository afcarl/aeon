#pragma once

#include <vector>
#include <tuple>
#include <random>

#include "etl_interface.hpp"
#include "etl_bbox.hpp"
#include "etl_image_var.hpp"
#include "params.hpp"
#include "util.hpp"
#include "box.hpp"

namespace nervana {

    namespace localization {
        class decoded;
        class params;
        class config;
        class target;
        class anchor;

        class extractor;
        class transformer;
        class loader;
    }

    class localization::target {
    public:
        target() : target(0,0,0,0){}
        target(float x, float y, float w, float h) :
            dx{x}, dy{y}, dw{w}, dh{h} {}
        float dx;
        float dy;
        float dw;
        float dh;
    };

    class localization::anchor {
    public:
        anchor(const localization::config&);

        std::vector<int> inside_image_bounds(int width, int height);
//        std::vector<int> inside_image_bounds(int width, int height);
        int total_anchors() const { return all_anchors.size(); }
        const std::vector<box>& get_all_anchors() const { return all_anchors; }
    private:
        anchor() = delete;
        //    Generate anchor (reference) windows by enumerating aspect ratios X
        //    scales wrt a reference (0, 0, 15, 15) window.
        std::vector<box> generate_anchors();

        //    Enumerate a set of anchors for each aspect ratio wrt an anchor.
        std::vector<box> ratio_enum(const box& anchor, const std::vector<float>& ratios);

        //    Given a vector of widths (ws) and heights (hs) around a center
        //    (x_ctr, y_ctr), output a set of anchors (windows).
        std::vector<box> mkanchors(const std::vector<float>& ws, const std::vector<float>& hs, float x_ctr, float y_ctr);

        //    Enumerate a set of anchors for each scale wrt an anchor.
        std::vector<box> scale_enum(const box& anchor, const std::vector<float>& scales);

        std::vector<box> add_anchors();

        const localization::config& cfg;
        int conv_size;

        std::vector<box> all_anchors;
    };

    class localization::config : public bbox::config {
    public:
        int images_per_batch = 1;
        int rois_per_image = 256;
        int min_size = 600;
        int max_size = 1000;
        int base_size = 16;
        float scaling_factor = 1.0 / 16.;
        std::vector<float> ratios = {0.5, 1, 2};
        std::vector<float> scales = {8, 16, 32};
        float negative_overlap = 0.3;  // negative anchors have < 0.3 overlap with any gt box
        float positive_overlap = 0.7;  // positive anchors have > 0.7 overlap with at least one gt box
        float foreground_fraction = 0.5;  // at most, positive anchors are 0.5 of the total rois

        // Derived values
        uint32_t output_buffer_size;

        config(nlohmann::json js);

        int total_anchors() const {
            return ratios.size() * scales.size() * (int)pow(int(std::floor(max_size * scaling_factor)),2);
        }

    private:
        config() = delete;
        void validate();
    };

    class localization::decoded : public bbox::decoded {
    public:
        decoded() {}
        virtual ~decoded() override {}

        // from transformer
        std::vector<int>    labels;
        std::vector<target> bbox_targets;
        std::vector<int>    anchor_index;
        std::vector<box>    anchors;

        float image_scale;
        cv::Size image_size;

    private:
    };

    class localization::extractor : public nervana::interface::extractor<localization::decoded> {
    public:
        extractor(const localization::config&);

        virtual std::shared_ptr<localization::decoded> extract(const char* data, int size) override {
            auto rc = std::make_shared<localization::decoded>();
            auto bb = std::static_pointer_cast<bbox::decoded>(rc);
            bbox_extractor.extract(data, size, bb);
            return rc;
        }

        virtual ~extractor() {}
    private:
        extractor() = delete;
        bbox::extractor bbox_extractor;
    };

    class localization::transformer : public interface::transformer<localization::decoded, image_var::params> {
    public:
        transformer(const localization::config&);

        virtual ~transformer() {}

        std::shared_ptr<localization::decoded> transform(
                            std::shared_ptr<image_var::params> txs,
                            std::shared_ptr<localization::decoded> mp) override;

        static std::tuple<float,cv::Size> calculate_scale_shape(cv::Size size, int min_size, int max_size);
    private:
        transformer() = delete;
        cv::Mat bbox_overlaps(const std::vector<box>& boxes, const std::vector<box>& query_boxes);
        static std::vector<target> compute_targets(const std::vector<box>& gt_bb, const std::vector<box>& anchors);
        std::vector<int> sample_anchors(const std::vector<int>& labels, bool debug=false);

        const localization::config& cfg;
        std::minstd_rand0 random;
        anchor  _anchor;
    };

    class localization::loader : public interface::loader<localization::decoded> {
    public:
        loader(const localization::config&);

        virtual ~loader() {}

        void load(char* buf, std::shared_ptr<localization::decoded> mp) override;
    private:
        loader() = delete;
        void build_output(std::shared_ptr<localization::decoded> mp, std::vector<float>& dev_y_labels, std::vector<float>& dev_y_labels_mask, std::vector<float>& dev_y_bbtargets, std::vector<float>& dev_y_bbtargets_mask);
        int total_anchors;
    };
}
