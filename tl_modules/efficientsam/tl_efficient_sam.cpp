#include "tl_efficient_sam.h"
#include "sam_apis.h"


AUTO_REGISTER_MODEL(EfficientSam10m, "efficientsam:10m");
AUTO_REGISTER_MODEL(EfficientSam30m, "efficientsam:latest");

//blob:33] ===> path: C:\Users\njtl007\.cache\osam\models\blobs\efficient_sam_vits_encoder.onnx
//model:84] Input Name: batched_images, Dimensions: [-1, 3, -1, -1], Type: FLOAT
//model:93] Output Name: image_embeddings, Dimensions: [-1, 256, -1, -1], Type: FLOAT
//blob:33] ===> path: C:\Users\njtl007\.cache\osam\models\blobs\efficient_sam_vits_decoder.onnx
//model:84] Input Name: image_embeddings, Dimensions: [-1, 256, 64, 64], Type: FLOAT
//model:84] Input Name: batched_point_coords, Dimensions: [1, 1, -1, 2], Type: FLOAT
//model:84] Input Name: batched_point_labels, Dimensions: [1, 1, -1], Type: FLOAT
//model:84] Input Name: orig_im_size, Dimensions: [2], Type: INT64
//model:93] Output Name: output_masks, Dimensions: [-1, -1, -1, -1, -1], Type: FLOAT
//model:93] Output Name: iou_predictions, Dimensions: [1, 1, -1], Type: FLOAT
//model:93] Output Name: onnx::Shape_1830, Dimensions: [-1, -1, -1, -1], Type: FLOAT
//model:25] ===> output: image_embeddings, ElementType: FLOAT, Shape: [1, 256, 64, 64], elements: 1048576
//model:30] Run success, usage: 4089704
//model:25] ===> output: output_masks, ElementType: FLOAT, Shape: [1, 1, 3, 1600, 2560], elements: 12288000
//model:25] ===> output: iou_predictions, ElementType: FLOAT, Shape: [1, 1, 3], elements: 3
//model:25] ===> output: onnx::Shape_1830, ElementType: FLOAT, Shape: [1, 3, 256, 256], elements: 196608
//model:30] Run success, usage: 74135
ImageEmbedding EfficientSam::compute_image_embedding_from_image(InferenceSession &session, const cv::Mat &image) {
    SPDLOG_INFO("compute_image_embedding_from_image start");
    //if image.ndim == 2:
    //    raise ValueError("Grayscale images are not supported")
    //if image.ndim == 3 and image.shape[2] == 4:
    //    raise ValueError("RGBA images are not supported")

    // 图像归一化
    cv::Mat batched_images = cv::dnn::blobFromImage(image, 1/255.0, cv::Size(), cv::Scalar(), true, false, CV_32F);
    const std::vector<int64_t> inputDims_{1, 3, image.rows, image.cols};   // NCHW
    const size_t inputTensorSize = mult_size(inputDims_);

    session.input_tensors_[0] = Ort::Value::CreateTensor<float>(memoryInfo_, batched_images.ptr<float>(), inputTensorSize, inputDims_.data(), inputDims_.size());

    SPDLOG_INFO("compute image embedding: {}", inputDims_);
    session.RunInference();

    //toFile(std::format("output_0_{}.hex", cv::getTickCount()), session.output_tensors_[0]);
    return {image.rows, image.cols, std::move(session.output_tensors_[0]) };
}

cv::Mat EfficientSam::generate_mask_from_image_embedding(InferenceSession &session, ImageEmbedding &image_embedding, const Prompt &prompt) {
    SPDLOG_INFO("generate mask from image embedding start");
    //1.image_embeddings
    //const std::vector<int64_t> inputDims1_{1, 256, 64, 64};
    const auto tensorInfo = image_embedding.embedding.GetTensorTypeAndShapeInfo();
    const auto embeddingDims = tensorInfo.GetShape();
    session.input_tensors_[0] = std::move(image_embedding.embedding);

    //2.batched_point_coords    [[[[580. 350.],   [650. 350.]]]]
    //const std::vector<int64_t> inputDims2_{1, 1, 2, 2};
    //std::vector<float> batched_point_coords{580.,350., 650.,350.};
    const std::vector<int64_t> inputDims2_{1, 1, static_cast<int64_t>(prompt.point_coords_.size()), 2};
    std::vector<float> batched_point_coords;
    for (const auto &point : prompt.point_coords_) {
        batched_point_coords.push_back(point.x);
        batched_point_coords.push_back(point.y);
    }
    session.input_tensors_[1] = Ort::Value::CreateTensor<float>(memoryInfo_, batched_point_coords.data(), batched_point_coords.size(), inputDims2_.data(), inputDims2_.size());

    //3.batched_point_labels   [[[1. 1.]]]
    //const std::vector<int64_t> inputDims3_{1, 1, 2};
    //std::vector<float> batched_point_labels{1, 1};
    const std::vector<int64_t> inputDims3_{1, 1, static_cast<int64_t>(prompt.point_labels_.size())};
    std::vector<float> batched_point_labels{prompt.point_labels_.begin(), prompt.point_labels_.end()};
    session.input_tensors_[2]  = Ort::Value::CreateTensor<float>(memoryInfo_, batched_point_labels.data(), batched_point_labels.size(), inputDims3_.data(), inputDims3_.size());

    //4.orig_im_size    [ 603 1072]
    const std::vector<int64_t> inputDims4_{2};
    std::vector<int64_t> orig_im_size{image_embedding.image_h, image_embedding.image_w}; //H,W
    session.input_tensors_[3]  = Ort::Value::CreateTensor<int64_t>(memoryInfo_, orig_im_size.data(), 2, inputDims4_.data(), inputDims4_.size());

    //static int32_t round = 0;
    //auto t1 = cv::getTickCount();
    //for (auto i = 0; i < session.input_tensors_.size(); ++i) {
    //    toFile(std::format("input_{}_{}_{}.hex", i, round, t1), session.input_tensors_[i]);
    //}
    //round++;
    session.RunInference();

    // 推理完成恢复输入数据
    image_embedding.embedding = std::move(session.input_tensors_[0]);

    auto &output_masks = session.output_tensors_[0];   //[1, 1, 3, H, W]
    auto &iou_predicts = session.output_tensors_[1];   //[1, 1, 3]
    auto &x_shape_1830 = session.output_tensors_[2];   //[1, 3, 256, 256]
    const auto dims = output_masks.GetTensorTypeAndShapeInfo().GetShape();
    auto *const x_data = output_masks.GetTensorMutableData<float>();

    const auto image_b = static_cast<int32_t>(dims[0]);
    const auto image_n = static_cast<int32_t>(dims[1]);
    const auto image_c = static_cast<int32_t>(dims[2]);
    const auto image_h = static_cast<int32_t>(dims[3]);
    const auto image_w = static_cast<int32_t>(dims[4]);

    //(1, 1, 3, H, W) -> (H, W)
    return cv::Mat(image_h, image_w, CV_32FC1, x_data) > 0;
}