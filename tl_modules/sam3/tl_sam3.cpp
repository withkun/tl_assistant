#include "tl_sam3.h"
#include "sam_apis.h"
#include "base/tokenizer.h"

AUTO_REGISTER_MODEL(Sam3, "sam3:latest");

ImageEmbedding Sam3::encode_image(const cv::Mat &image) {
    SPDLOG_INFO("Run encode image");
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(input_size_, input_size_), 0, 0, cv::INTER_LINEAR);

    cv::Mat dst;
    std::vector<uint8_t> dst_data;
    std::vector<cv::Mat> bgr_channels(3);
    cv::split(resized, bgr_channels);
    for (auto i = 0; i < bgr_channels.size(); i++) {
        std::vector<uint8_t> data = std::vector<uint8_t>(bgr_channels[i].reshape(1, 1));
        dst_data.insert(dst_data.end(), data.begin(), data.end());
    }

    auto &session = sessions_["image_encoder"];
    const std::vector<int64_t> inputDims_{3, input_size_, input_size_};
    session.input_tensors_[0] = Ort::Value::CreateTensor<uint8_t>(memoryInfo_, dst_data.data(), dst_data.size(), inputDims_.data(), inputDims_.size());

    session.RunInference();

    ImageEmbedding image_embedding{image.rows, image.cols};
    image_embedding.extra_features.resize(session.output_tensors_.size());
    for (auto i = 0; i < session.output_tensors_.size(); ++i) {
        image_embedding.extra_features[i] = std::move(session.output_tensors_[i]);
    }
    return image_embedding;
}

GenerateResponse Sam3::generate(GenerateRequest &request) {
    SPDLOG_INFO("Run generate encode texts");
    std::vector<int64_t> tokens;
    for (const auto &token : tokenize(request.prompt.texts_, TOKEN_W_)) {
        tokens.insert(tokens.end(), token.begin(), token.end());
    }

    auto &lang_session = sessions_["language_encoder"];
    const std::vector<int64_t> langDims_{static_cast<int64_t>(request.prompt.texts_.size()), TOKEN_W_};
    lang_session.input_tensors_[0] = Ort::Value::CreateTensor<int64_t>(memoryInfo_, tokens.data(), TOKEN_W_, langDims_.data(), langDims_.size());
    lang_session.RunInference();

    SPDLOG_INFO("Run generate decode image");
    ImageEmbedding &image_embedding = request.image_embedding;
    const std::vector<int64_t> dim_scalar{ };
    const std::vector<int64_t> dim_coords{ 1, 1, 4 };
    const std::vector<int64_t> dim_labels{ 1, 1 };
    const std::vector<int64_t> dim_masks{ 1, 1 };
    int64_t original_h[] = { image_embedding.image_h };
    int64_t original_w[] = { image_embedding.image_w };
    float   box_coords[] = { 0, 0, 0, 0 };
    int64_t box_labels[] = { 1 };
    bool    box_masks[]  = { true };

    auto &session = sessions_["decoder"];
    session.input_tensors_[0] = Ort::Value::CreateTensor<int64_t>(memoryInfo_, original_h, 1, dim_scalar.data(), dim_scalar.size());
    session.input_tensors_[1] = Ort::Value::CreateTensor<int64_t>(memoryInfo_, original_w, 1, dim_scalar.data(), dim_scalar.size());
    session.input_tensors_[2] = std::move(image_embedding.extra_features[2]);    // vision_pos_enc_2
    session.input_tensors_[3] = std::move(image_embedding.extra_features[3]);    // backbone_fpn_0
    session.input_tensors_[4] = std::move(image_embedding.extra_features[4]);    // backbone_fpn_1
    session.input_tensors_[5] = std::move(image_embedding.extra_features[5]);    // backbone_fpn_2
    session.input_tensors_[6] = std::move(lang_session.output_tensors_[0]);      // language_mask
    session.input_tensors_[7] = std::move(lang_session.output_tensors_[1]);      // language_features
    session.input_tensors_[8] = Ort::Value::CreateTensor<float>(memoryInfo_, box_coords, 4, dim_coords.data(), dim_coords.size());
    session.input_tensors_[9] = Ort::Value::CreateTensor<int64_t>(memoryInfo_, box_labels, 1, dim_labels.data(), dim_labels.size());
    session.input_tensors_[10]= Ort::Value::CreateTensor<bool>(memoryInfo_, box_masks, 1, dim_masks.data(), dim_masks.size());

    session.RunInference();
    image_embedding.extra_features[2] = std::move(session.input_tensors_[2]);
    image_embedding.extra_features[3] = std::move(session.input_tensors_[3]);
    image_embedding.extra_features[4] = std::move(session.input_tensors_[4]);
    image_embedding.extra_features[5] = std::move(session.input_tensors_[5]);
    lang_session.output_tensors_[0]   = std::move(session.input_tensors_[6]);
    lang_session.output_tensors_[1]   = std::move(session.input_tensors_[7]);

    GenerateResponse response;
    const auto scoreInfo = session.output_tensors_[1].GetTensorTypeAndShapeInfo();
    const auto scoreDims = scoreInfo.GetShape();
    const auto PROBES_N = static_cast<int32_t>(scoreDims[0]);
    response.annotations.reserve(PROBES_N);

    const cv::Mat boxes(PROBES_N, 4, CV_32F, session.output_tensors_[0].GetTensorMutableRawData());
    const cv::Mat scores(PROBES_N, 1, CV_32F, session.output_tensors_[1].GetTensorMutableRawData());
    const cv::Mat masks(PROBES_N, image_embedding.image_h*image_embedding.image_w, CV_8U, session.output_tensors_[2].GetTensorMutableRawData());
    for (int32_t n = 0; n < PROBES_N; ++n) {
        const float score = scores.at<float>(n, 0);
        if (score < request.prompt.score_threshold_) {
            continue;
        }

        response.annotations.push_back({});
        auto &annotation = response.annotations.back();
        annotation.score = score;
        annotation.text = request.prompt.texts_.front();

        const auto x1 = static_cast<int32_t>(std::round(std::max(0.0f, std::min(boxes.at<float>(n, 0), 1.0f * image_embedding.image_w))));
        const auto y1 = static_cast<int32_t>(std::round(std::max(0.0f, std::min(boxes.at<float>(n, 1), 1.0f * image_embedding.image_h))));
        const auto x2 = static_cast<int32_t>(std::round(std::max(0.0f, std::min(boxes.at<float>(n, 2), 1.0f * image_embedding.image_w))));
        const auto y2 = static_cast<int32_t>(std::round(std::max(0.0f, std::min(boxes.at<float>(n, 3), 1.0f * image_embedding.image_h))));
        annotation.bbox = {x1, y1, x2, y2};

        cv::Mat mask = masks.row(n).reshape(1, {image_embedding.image_h, image_embedding.image_w});
        annotation.mask = mask(cv::Rect(x1, y1, x2-x1, y2-y1)).clone();
    }

    return response;
}