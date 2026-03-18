#ifndef __INC_SAM_SESSION_H
#define __INC_SAM_SESSION_H

#include "base/model.h"

extern std::vector<cv::Point2i> compute_polygon_from_mask(const cv::Mat &mask);
using CacheValue = std::pair<int64_t, ImageEmbedding>;

class SamSession {
public:
    explicit SamSession(const std::string &name, int32_t cache_size = 3);
    ~SamSession() = default;

    GenerateResponse run(const cv::Mat &image, const int64_t &image_id,
                         const std::vector<cv::Point2f> &point_coords, const std::vector<float> &point_labels,
                         const std::vector<std::string> &texts = {});

    [[nodiscard]] std::string model_name() const { return model_name_; }

protected:
    ImageEmbedding get_or_compute_embedding(const cv::Mat &image, const int64_t &image_id);
    Model *get_or_load_model();

    std::string                 model_name_;
    Model                      *model_{nullptr};
    int32_t                     cache_size_;

private:
    Ort::MemoryInfo             memoryInfo_{nullptr};
    std::deque<CacheValue>      embedding_cache_;
    void CloneOrtValue(Ort::Value &tensor, Ort::Value &value) const;
};
#endif// __INC_SAM_SESSION_H