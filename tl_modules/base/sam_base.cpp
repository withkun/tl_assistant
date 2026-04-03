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
