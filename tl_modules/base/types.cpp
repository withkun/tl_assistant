#include "types.h"


Prompt::Prompt(const std::vector<cv::Point2f> &points, const std::vector<float> &point_labels) {
    if (points.empty() || points.size() != point_labels.size()) {
        throw std::runtime_error("points and point_labels not equal");
    }
    point_coords_.insert(point_coords_.end(), points.begin(), points.end());
    point_labels_.insert(point_labels_.end(), point_labels.begin(), point_labels.end());
}

Prompt::Prompt(const std::vector<std::string> &texts, float iou_threshold, float score_threshold, int32_t max_annotations) {
    texts_.insert(texts_.end(), texts.begin(), texts.end());
    iou_threshold_   = iou_threshold;
    score_threshold_ = score_threshold;
    max_annotations_ = max_annotations;
}

bool Prompt::empty() const {
    return texts_.empty() && point_coords_.empty();
}