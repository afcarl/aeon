#include "etl_localization.hpp"
#include "box.hpp"

using namespace std;
using namespace nervana;

template<typename T> string join(const T& v, const string& sep) {
    ostringstream ss;
    for(const auto& x : v) {
        if(&x != &v[0]) ss << sep;
        ss << x;
    }
    return ss.str();
}

bool nervana::localization::config::set_config(nlohmann::json js)
{
    bbox::config::set_config(js);

    parse_opt(images_per_batch, "images_per_batch", js);
    parse_opt(rois_per_image, "rois_per_image", js);
    parse_opt(min_size, "min_size", js);
    parse_opt(max_size, "max_size", js);
    parse_opt(base_size, "base_size", js);
    parse_opt(scaling_factor, "scaling_factor", js);
    parse_opt(ratios, "ratios", js);
    parse_opt(scales, "scales", js);
    parse_opt(negative_overlap, "negative_overlap", js);
    parse_opt(positive_overlap, "positive_overlap", js);
    parse_opt(foreground_fraction, "foreground_fraction", js);

    return validate();
}

bool nervana::localization::config::validate() {
    return max_size > min_size &&
           negative_overlap >= 0.0 &&
           negative_overlap <= 1.0 &&
           positive_overlap >= 0.0 &&
           positive_overlap <= 1.0 &&
           foreground_fraction <= 1.0;}

localization::extractor::extractor(std::shared_ptr<const localization::config> cfg) :
    bbox_extractor{cfg}
{

}


localization::transformer::transformer(std::shared_ptr<const localization::config> _cfg) :
    cfg{_cfg},
    _anchor{cfg}
{
}

shared_ptr<localization::decoded> localization::transformer::transform(
                    shared_ptr<image::params> txs,
                    shared_ptr<localization::decoded> mp) {
    cv::Size im_size{mp->width(), mp->height()};
    float im_scale;
    tie(im_scale, im_size) = calculate_scale_shape(im_size, cfg->min_size, cfg->max_size);
    mp->image_scale = im_scale;
    mp->image_size = im_size;

    vector<int> idx_inside = _anchor.inside_image_bounds(im_size.width, im_size.height);
    const vector<box> all_anchors = _anchor.get_all_anchors();
    vector<box> anchors_inside;
    for(int i : idx_inside) anchors_inside.push_back(all_anchors[i]);

    // compute bbox overlaps
    vector<box> scaled_bbox;
    for(const bbox::box& b : mp->boxes()) {
        box r = b*im_scale;
        scaled_bbox.push_back(r);
    }
    cv::Mat overlaps = bbox_overlaps(anchors_inside, scaled_bbox);

    vector<int> labels(overlaps.rows);
    fill_n(labels.begin(),labels.size(),-1.);

    // assign bg labels first
    vector<float> row_max(overlaps.rows);
    vector<float> column_max(overlaps.cols);
    fill_n(row_max.begin(),overlaps.rows,0);
    fill_n(column_max.begin(),overlaps.cols,0);
    for(int row=0; row<overlaps.rows; row++) {
        for(int col=0; col<overlaps.cols; col++) {
            auto value = overlaps.at<float>(row,col);
            row_max[row]    = std::max(row_max[row],   value);
            column_max[col] = std::max(column_max[col],value);
        }
    }

    for(int row=0; row<overlaps.rows; row++) {
        if(row_max[row] < cfg->negative_overlap) {
            labels[row] = 0;
        }
    }

    // assigning fg labels
    // 1. for each gt box, anchor with higher overlaps [including ties]
    for(int row=0; row<overlaps.rows; row++) {
        for(int col=0; col<overlaps.cols; col++) {
            // This should be fixed as it is comparing floats
            if(overlaps.at<float>(row,col) == column_max[col]) {
                labels[row] = 1;
            }
        }
    }

    // 2. any anchor above the overlap threshold with any gt box
    for(int row=0; row<overlaps.rows; row++) {
        if(row_max[row] >= cfg->positive_overlap) {
            labels[row] = 1;
        }
    }

    // For every anchor, compute the regression target compared
    // to the gt box that it has the highest overlap with
    // the indicies of labels should match these targets
    vector<box> argmax;
    for(int row=0; row<overlaps.rows; row++) {
        int index = 0;
        float max = 0;
        for(int col=0; col<overlaps.cols; col++) {
            auto value = overlaps.at<float>(row,col);
            if(value > max) {
                index = col;
                max = value;
            }
        }
        argmax.push_back(scaled_bbox[index]);
    }

    auto bbox_targets = compute_targets(argmax, anchors_inside);

    // map lists to original canvas
    {
        vector<int> t_labels(all_anchors.size());
        fill_n(t_labels.begin(), t_labels.size(), -1);
        vector<target> t_bbox_targets(all_anchors.size());
        for(int i=0; i<idx_inside.size(); i++) {
            int index = idx_inside[i];
            t_labels[index] = labels[i];
            t_bbox_targets[index] = bbox_targets[i];
        }
        labels = move(t_labels);
        bbox_targets = move(t_bbox_targets);
    }

    mp->anchor_index = sample_anchors(labels);
    mp->anchors = all_anchors;
    mp->labels = labels;
    mp->bbox_targets = bbox_targets;


//    cout << "labels size " << labels.size() << " all_anchors size " << all_anchors.size() << endl;
//    for(int i=0; i<mp->anchor_index.size(); i++) {
//        cout << i << " " << mp->anchor_index[i] << " " << labels[mp->anchor_index[i]] << " " << all_anchors[mp->anchor_index[i]] << endl;
//    }

    return mp;
}

vector<int> localization::transformer::sample_anchors(const vector<int>& labels) {
    // subsample labels if needed
    int num_fg = int(cfg->foreground_fraction * cfg->rois_per_image);
    vector<int> fg_idx;
    vector<int> bg_idx;
    for(int i=0; i<labels.size(); i++) {
        if(labels[i] >= 1) {
            fg_idx.push_back(i);
        } else if(labels[i] == 0) {
            bg_idx.push_back(i);
        }
    }
    shuffle(fg_idx.begin(), fg_idx.end(),random);
    if(fg_idx.size() > num_fg) {
        fg_idx.resize(num_fg);
    }
    int remainder = cfg->rois_per_image-fg_idx.size();
    shuffle(bg_idx.begin(), bg_idx.end(),random);
    if(bg_idx.size() > remainder) {
        bg_idx.resize(remainder);
    }

    vector<int> result_idx;
    result_idx.insert(result_idx.end(), fg_idx.begin(), fg_idx.end());
    result_idx.insert(result_idx.end(), bg_idx.begin(), bg_idx.end());

//    vector<int>    result_labels;
//    vector<target> result_targets;
//    for(int i : result_idx) {
//        result_labels.push_back(labels[i]);
//        result_targets.push_back(bbox_targets[i]);
//    }

//    return make_tuple(result_labels, result_targets, result_idx);
    return result_idx;
}

vector<localization::target> localization::transformer::compute_targets(const vector<box>& gt_bb, const vector<box>& rp_bb) {
    //  Given ground truth bounding boxes and proposed boxes, compute the regresssion
    //  targets according to:

    //  t_x = (x_gt - x) / w
    //  t_y = (y_gt - y) / h
    //  t_w = log(w_gt / w)
    //  t_h = log(h_gt / h)

    //  where (x,y) are bounding box centers and (w,h) are the box dimensions

    // the target will be how to adjust the bbox's center and width/height
    // note that the targets are generated based on the original RP, which has not
    // been scaled by the image resizing
    vector<target> targets;
    for(int i=0; i<gt_bb.size(); i++) {
        const box& gt = gt_bb[i];
        const box& rp = rp_bb[i];
        float dx = (gt.xcenter() - rp.xcenter()) / rp.width();
        float dy = (gt.ycenter() - rp.ycenter()) / rp.height();
        float dw = log(gt.width() / rp.width());
        float dh = log(gt.height() / rp.height());
        targets.emplace_back(dx, dy, dw, dh);
    }

    return targets;
}

cv::Mat localization::transformer::bbox_overlaps(const vector<box>& boxes, const vector<box>& bounding_boxes) {
    // Parameters
    // ----------
    // boxes: (N, 4) ndarray of float
    // query_boxes: (K, 4) ndarray of float
    // Returns
    // -------
    // overlaps: (N, K) ndarray of overlap between boxes and query_boxes
    uint32_t N = boxes.size();
    uint32_t K = bounding_boxes.size();
    cv::Mat overlaps(N,K,CV_32FC1);
    overlaps = 0.;
    float iw, ih, box_area;
    float ua;
    for(uint32_t k=0; k<bounding_boxes.size(); k++) {
        const box& bounding_box = bounding_boxes[k];
        box_area = (bounding_box.xmax - bounding_box.xmin + 1) *
                   (bounding_box.ymax - bounding_box.ymin + 1);
        for(uint32_t n=0; n<boxes.size(); n++) {
            const box& b = boxes[n];
            iw = min(b.xmax, bounding_box.xmax) -
                 max(b.xmin, bounding_box.xmin) + 1;
            if(iw > 0) {
                ih = min(b.ymax, bounding_box.ymax) -
                     max(b.ymin, bounding_box.ymin) + 1;
                if(ih > 0) {
                    ua = (b.xmax - b.xmin + 1.) *
                         (b.ymax - b.ymin + 1.) +
                          box_area - iw * ih;
                    overlaps.at<float>(n, k) = iw * ih / ua;
                }
            }
        }
    }
    return overlaps;
}

tuple<float,cv::Size> localization::transformer::calculate_scale_shape(cv::Size size, int min_size, int max_size) {
    int im_size_min = std::min(size.width,size.height);
    int im_size_max = max(size.width,size.height);
    float im_scale = float(min_size) / float(im_size_min);
    // Prevent the biggest axis from being more than FRCN_MAX_SIZE
    if(round(im_scale * im_size_max) > max_size) {
        im_scale = float(max_size) / float(im_size_max);
    }
    cv::Size im_shape{int(round(size.width*im_scale)), int(round(size.height*im_scale))};
    return make_tuple(im_scale, im_shape);
}





localization::loader::loader(std::shared_ptr<const localization::config> cfg)
{
    _channel_major = cfg->channel_major;
    _load_size     = 1;
    total_anchors = cfg->total_anchors();
//    _load_count    = cfg->width * cfg->height * cfg->channels * cfg->num_crops();
}

void localization::loader::load(char* buf, std::shared_ptr<localization::decoded> mp) {
    mp->labels;
    mp->bbox_targets;
    mp->anchor_index;
    mp->anchors;

    cout << "labels size " << mp->labels.size() << endl;
    cout << "bbox_targets size " << mp->bbox_targets.size() << endl;
    cout << "anchor_index size " << mp->anchor_index.size() << endl;
    cout << "anchors size " << mp->anchors.size() << endl;

//    for(auto index : mp->anchor_index) {
//        cout << "anchor index " << index << endl;
//    }



//    # map our labels and bbox_targets back to
//    # the full canvas (e.g. 9 * (62 * 62))
//    label.fill(0)
//    label[anchors_blob, :] = labels_blob[:, np.newaxis]
    //    for(int i=0; i<mp->anchor_index.size(); i++) labels[mp->anchor_index[i]] = mp->labels[i];

    //    for(int i=0; i<label.size(); i++) { cout << "label " << i << " [" << label[i] << "]" << endl; }

//    self.dev_y_labels_flat[:] = label.reshape((1, -1))
//    self.dev_y_labels_onehot[:] = self.be.onehot(self.dev_y_labels_flat, axis=0)
//    self.dev_y_labels = self.dev_y_labels_onehot.reshape((-1, 1))

//    label_mask.fill(0)
//    label_mask[anchors_blob, :] = 1
//    self.dev_y_labels_mask[:] = np.vstack([label_mask, label_mask])

//    bbtargets.fill(0)
//    bbtargets[anchors_blob, :] = bbox_targets_blob
//    self.dev_y_bbtargets[:] = bbtargets.T.reshape((-1, 1))

//    bbtargets_mask.fill(0)
//    bbtargets_mask[np.where(label == 1)[0]] = 1
//    self.dev_y_bbtargets_mask[:] = bbtargets_mask.T.reshape((-1, 1))

//    X = self.dev_X_img
//    Y = ((self.dev_y_labels, self.dev_y_labels_mask),
//         (self.dev_y_bbtargets, self.dev_y_bbtargets_mask))


//    sizeof dev_y_labels 69192
//    sizeof dev_y_labels_mask 69192
//    sizeof dev_y_bbtargets 138384
//    sizeof dev_y_bbtargets_mask 138384

}

void localization::loader::fill_info(nervana::count_size_type* cst) {
    cst->count   = _load_count;
    cst->size    = _load_size;
    cst->type[0] = 'u';
}







localization::anchor::anchor(std::shared_ptr<const localization::config> _cfg) :
    cfg{_cfg},
    conv_size{int(std::floor(cfg->max_size * cfg->scaling_factor))}
{
    all_anchors = add_anchors();
}

vector<int> localization::anchor::inside_image_bounds(int width, int height) {
    vector<int> rc;
    for(int i=0; i<all_anchors.size(); i++) {
        const box& b = all_anchors[i];
        if( b.xmin >= 0 && b.ymin >= 0 && b.xmax < width && b.ymax < height ) {
            rc.emplace_back(i);
        }
    }
    return rc;
}

vector<box> localization::anchor::add_anchors() {
    vector<box> anchors = generate_anchors();

    // generate shifts to apply to anchors
    // note: 1/self.SCALE is the feature stride
    vector<float> shift_x;
    vector<float> shift_y;
    for(float i=0; i<conv_size; i++) {
        shift_x.push_back(i * 1. / cfg->scaling_factor);
        shift_y.push_back(i * 1. / cfg->scaling_factor);
    }

    vector<box> shifts;
    for(int y=0; y<shift_y.size(); y++) {
        for(int x=0; x<shift_x.size(); x++) {
            shifts.emplace_back(shift_x[x], shift_y[y], shift_x[x], shift_y[y]);
        }
    }

    vector<box> all_anchors;
    for(const box& anchor_data : anchors ) {
        for(const box& row_data : shifts ) {
            box b = row_data+anchor_data;
            all_anchors.push_back(b);
        }
    }

    return all_anchors;
}

vector<box> localization::anchor::generate_anchors() {
    box anchor{0.,0.,(float)(cfg->base_size-1),(float)(cfg->base_size-1)};
    vector<box> ratio_anchors = ratio_enum(anchor, cfg->ratios);

    vector<box> result;
    for(const box& ratio_anchor : ratio_anchors) {
        for(const box& b : scale_enum(ratio_anchor, cfg->scales)) {
            result.push_back(b);
        }
    }

    return result;
}

vector<box> localization::anchor::mkanchors(const vector<float>& ws, const vector<float>& hs, float x_ctr, float y_ctr) {
    vector<box> rc;
    for(int i=0; i<ws.size(); i++) {
        rc.emplace_back(x_ctr - 0.5 *(ws[i]-1),
                        y_ctr - 0.5 *(hs[i]-1),
                        x_ctr + 0.5 *(ws[i]-1),
                        y_ctr + 0.5 *(hs[i]-1));
    }
    return rc;
}

vector<box> localization::anchor::ratio_enum(const box& anchor, const vector<float>& ratios) {
    float w;
    float h;
    float x_ctr;
    float y_ctr;
    tie(w,h,x_ctr,y_ctr) = whctrs(anchor);

    int size = w * h;
    vector<float> size_ratios;
    vector<float> ws;
    vector<float> hs;
    for(float ratio : ratios)      { size_ratios.push_back(size/ratio); }
    for(float sr : size_ratios)    { ws.push_back(round(sqrt(sr))); }
    for(int i=0; i<ws.size(); i++) { hs.push_back(round(ws[i]*ratios[i])); }

    return mkanchors(ws, hs, x_ctr, y_ctr);
}

vector<box> localization::anchor::scale_enum(const box& anchor, const vector<float>& scales) {
    float w;
    float h;
    float x_ctr;
    float y_ctr;
    tie(w,h,x_ctr,y_ctr) = whctrs(anchor);

    vector<float> ws;
    vector<float> hs;
    for(float scale : scales) {
        ws.push_back(w*scale);
        hs.push_back(h*scale);
    }
    return mkanchors(ws, hs, x_ctr, y_ctr);
}

tuple<float,float,float,float> localization::anchor::whctrs(const box& anchor) {
    float w = anchor.xmax - anchor.xmin + 1;
    float h = anchor.ymax - anchor.ymin + 1;
    float x_ctr = (float)anchor.xmin + 0.5 * (float)(w - 1);
    float y_ctr = (float)anchor.ymin + 0.5 * (float)(h - 1);
    return make_tuple(w, h, x_ctr, y_ctr);
}
