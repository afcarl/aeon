#include <sstream>
#include "etl_bbox.hpp"

using namespace std;
using namespace nervana;
using namespace nlohmann;   // json stuff

ostream& operator<<(ostream& out, const nervana::bbox::box& b) {
    out << b.name << " (" << b.xmax << "," << b.xmin << ")(" << b.ymax << "," << b.ymin << ") "
        << b.difficult << " " << b.truncated;
    return out;
}

cv::Rect nervana::bbox::box::rect() const {
    return cv::Rect(xmin,ymin,xmax-xmin,ymax-ymin);
}


nervana::bbox::decoded::decoded() {
}

nervana::bbox::extractor::extractor() {
}

media_ptr nervana::bbox::extractor::extract(char* data, int size) {
    shared_ptr<decoded> rc = make_shared<decoded>();
    string buffer( data, size );
    json j = json::parse(buffer);
    auto object_list = j["object"];
    auto image_size = j["size"];
    rc->_height = image_size["height"];
    rc->_width = image_size["width"];
    rc->_depth = image_size["depth"];
    for( auto object : object_list ) {
        auto bndbox = object["bndbox"];
        box b;
        b.xmax = bndbox["xmax"];
        b.xmin = bndbox["xmin"];
        b.ymax = bndbox["ymax"];
        b.ymin = bndbox["ymin"];
        if( !object["difficult"].is_null() ) b.difficult = object["difficult"];
        if( !object["truncated"].is_null() ) b.truncated = object["truncated"];
        b.name = object["name"];
        rc->_boxes.push_back(b);
    }
    return rc;
}

json nervana::bbox::extractor::create_box( const cv::Rect& rect, const string& label ) {
    json j = {{"bndbox",{{"xmax",rect.x+rect.width},{"xmin",rect.x},{"ymax",rect.y+rect.height},{"ymin",rect.y}}},{"name",label}};
    return j;
}

nlohmann::json nervana::bbox::extractor::create_metadata( const std::vector<nlohmann::json>& boxes ) {
    nlohmann::json j = nlohmann::json::object();
    j["object"] = boxes;
    j["size"] = {{"depth",3},{"height",256},{"width",256}};
    return j;
}

nervana::bbox::transformer::transformer() {

}

media_ptr nervana::bbox::transformer::transform(settings_ptr _sptr, const media_ptr& media) {
    shared_ptr<image::settings> sptr = static_pointer_cast<image::settings>(_sptr);
    shared_ptr<bbox::decoded> boxes = static_pointer_cast<bbox::decoded>(media);
    if( sptr->angle != 0 ) {
        return shared_ptr<bbox::decoded>();
    }
    shared_ptr<bbox::decoded> rc = make_shared<bbox::decoded>();
    cv::Rect crop = sptr->cropbox;
    for( box tmp : boxes->boxes() ) {
        box b = tmp;
        if( b.xmax <= crop.x ) {           // outside left
        } else if( b.xmin >= crop.x + crop.width ) {      // outside right
        } else if( b.ymax <= crop.y ) {   // outside above
        } else if( b.ymin >= crop.y + crop.height ) {     // outside below
        } else {
            if( b.xmin < crop.x ) {
                b.xmin = crop.x;
            }
            if( b.ymin < crop.y ) {
                b.ymin = crop.y;
            }
            if( b.xmax > crop.x + crop.width ) {
                b.xmax = crop.x + crop.width;
            }
            if( b.ymax > crop.y + crop.height ) {
                b.ymax = crop.y + crop.height;
            }
            rc->_boxes.push_back( b );
        }
        // cout << b.rect.x << ", " << b.rect.y << ", " << b.rect.width << ", " << b.rect.height << ", " << b.label << endl;
    }
    return rc;
}

void nervana::bbox::transformer::fill_settings(settings_ptr, const media_ptr&, std::default_random_engine &) {
}

void nervana::bbox::loader::load(char* data, int size, const media_ptr& media) {

}

