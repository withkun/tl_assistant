#include "sam_base.h"


GenerateResponse SamBase::generate(GenerateRequest &request) {
    //image_embedding: types.ImageEmbedding
    //if request.image_embedding is None:
    //    if request.image is None:
    //        raise ValueError("request.image or request.image_embedding is required")
    //    image_embedding = self.encode_image(request.image)
    //else:
    //    image_embedding = request.image_embedding
    auto &image_embedding = request.image_embedding;

    Prompt prompt;
    if (request.prompt.empty()) {
        SPDLOG_WARN("Prompt is not given, so using the center point as prompt");
        prompt = Prompt({cv::Point2f(0.5 * image_embedding.image_w, 0.5 * image_embedding.image_w)}, {1});
    } else {
        prompt = request.prompt;
    }

    if (prompt.point_coords_.empty() || prompt.point_labels_.empty()) {
        throw std::runtime_error("Prompt must contain points and point_labels");
    }

    cv::Mat mask = generate_mask_from_image_embedding(image_embedding, prompt);

    Annotation annotation;
    annotation.mask = mask;
    return {name_, std::move(image_embedding), { annotation } };

    //bbox = imgviz.masks_to_bboxes(masks=[mask])[0].astype(int)
    //bounding_box: types.BoundingBox = types.BoundingBox(
    //    ymin=bbox[0], xmin=bbox[1], ymax=bbox[2], xmax=bbox[3]
    //)
    //
    //return {
    //    name_,
    //image_embedding,
    //annotations=[Annotation(mask=mask, bounding_box=bounding_box)]
    //};
    return {};
}

std::vector<cv::Rect> masks_to_bboxes(const std::vector<cv::Mat>& masks) {
    std::vector<cv::Rect> bboxes;
    bboxes.reserve(masks.size());

    for (const cv::Mat& mask : masks) {
        if (mask.empty() || cv::countNonZero(mask) == 0) {
            bboxes.emplace_back(0, 0, 0, 0); // 空掩膜返回零矩形
            continue;
        }

        cv::Mat points;
        cv::findNonZero(mask, points); // 获取非零像素坐标

        int xmin = points.at<cv::Point>(0).x;
        int xmax = xmin;
        int ymin = points.at<cv::Point>(0).y;
        int ymax = ymin;

        for (int i = 1; i < points.rows; ++i) {
            cv::Point p = points.at<cv::Point>(i);
            xmin = std::min(xmin, p.x);
            xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        }

        // 构造包含边界的矩形
        int width = xmax - xmin + 1;
        int height = ymax - ymin + 1;
        bboxes.emplace_back(xmin, ymin, width, height);
    }
    return bboxes;
}