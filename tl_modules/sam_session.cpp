#include "sam_session.h"
#include "sam_apis.h"

#include <future>
#include <fstream>

#include <QApplication>
#include <QProgressDialog>


SamSession::SamSession(const std::string &name, const int32_t cache_size) : model_name_(name), cache_size_(cache_size) {
    memoryInfo_ = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
}

GenerateResponse SamSession::run(const cv::Mat &image, const int64_t &image_id,
                                 const std::vector<cv::Point2f> &point_coords, const std::vector<float> &point_labels,
                                 const std::vector<std::string> &texts) {
    Prompt prompt;
    if (!point_coords.empty() && !point_labels.empty()) {
        prompt = Prompt(point_coords, point_labels);
    } else if (!texts.empty()) {
        prompt = Prompt(texts, 0.5, 0.1, 1000);
    } else {
        throw std::runtime_error("Either points and point_labels, or texts must be provided.");
    }

    cv::Mat dest = image;
    if (image.type() == CV_8UC1) {
        cv::cvtColor(image, dest, cv::COLOR_GRAY2BGR);
    }
    auto image_embedding = get_or_compute_embedding(dest, image_id);
    GenerateRequest request{model_name_, dest, prompt, std::move(image_embedding)};
    return model_->generate(request);
}

ImageEmbedding SamSession::get_or_compute_embedding(const cv::Mat &image, const int64_t &image_id) {
    //for key, embedding in self._embedding_cache:
    //    if key == image_id:
    //        return embedding
    for (auto &[key, cache_embedding] : embedding_cache_) {
        if (key != image_id) {
            continue;
        }
        SPDLOG_INFO("Get computing embedding for cache_key={}", image_id);
        ImageEmbedding image_embedding{cache_embedding.image_h, cache_embedding.image_w};
        CloneOrtValue(cache_embedding.embedding, image_embedding.embedding);

        image_embedding.extra_features.resize(cache_embedding.extra_features.size());
        for (size_t i = 0; i < cache_embedding.extra_features.size(); ++i) {
            CloneOrtValue(cache_embedding.extra_features[i], image_embedding.extra_features[i]);
        }
        return image_embedding;
    }

    QProgressDialog progressDialog("AI解码中, 请稍候...", "Cancel", 0, 0, nullptr, Qt::FramelessWindowHint);
    progressDialog.setWindowModality(Qt::WindowModal);  // 确保用户无法操作其他窗口
    progressDialog.setCancelButton(nullptr);    // 隐藏取消按钮
    progressDialog.setMinimumDuration(100);     // 延迟100ms显示, 避免闪屏
    progressDialog.show();                      // 启动对话框展示
    QApplication::processEvents();              // 确保对话框立即渲染

    std::future<ImageEmbedding> future = std::async(std::launch::async, [this, &image]() {
                                             auto *model = get_or_load_model();
                                             return model->encode_image(image);
                                         });
    while (future.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready) {
        //QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        //progressDialog.update();
    }
    ImageEmbedding image_embedding = future.get();

    progressDialog.close();     // 关闭销毁

    SPDLOG_INFO("Set computing embedding for cache_key={}", image_id);
    //self._embedding_cache.append((image_id, embedding))
    while (embedding_cache_.size() >= cache_size_) {
        embedding_cache_.pop_front();
    }

    embedding_cache_.push_back({});
    auto &[key, cache_embedding] = embedding_cache_.back();
    key = image_id;
    cache_embedding.image_h = image_embedding.image_h;
    cache_embedding.image_w = image_embedding.image_w;
    CloneOrtValue(image_embedding.embedding, cache_embedding.embedding);

    cache_embedding.extra_features.resize(image_embedding.extra_features.size());
    for (size_t i = 0; i < cache_embedding.extra_features.size(); ++i) {
        CloneOrtValue(image_embedding.extra_features[i], cache_embedding.extra_features[i]);
    }
    return image_embedding;
}

Model *SamSession::get_or_load_model() {
    if (model_ == nullptr) {
        // self._model = apis.get_model_type_by_name(self._model_name)  ()
        model_ = SamApis::instance().get_model_by_name(model_name_);
    }
    return model_;
}

void SamSession::CloneOrtValue(Ort::Value &tensor, Ort::Value &value)  const {
    if (tensor == nullptr || !tensor.IsTensor()) {
        return;
    }

    const auto typeShape = tensor.GetTypeInfo().GetTensorTypeAndShapeInfo();
    const auto b = tensor.GetTensorSizeInBytes();
    const auto s = typeShape.GetShape();
    const auto t = typeShape.GetElementType();
    value = Ort::Value::CreateTensor(memoryInfo_, tensor.GetTensorMutableRawData(), b, s.data(), s.size(), t);
    SPDLOG_INFO("copy image embedding: {}, element count: {}, size bytes: {}", typeShape.GetShape(), typeShape.GetElementCount(), b);
}

void toFile(const std::string &name, const Ort::Value &tensor) {
    const auto dataDims = tensor.GetTensorTypeAndShapeInfo().GetShape();
    std::ofstream ofs(name, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<const char *>(tensor.GetTensorData<float *>()), mult_size(dataDims) * sizeof(float));
    ofs.close();
}

void fromFile(const std::string &path, const cv::Mat &blob) {
    std::ifstream ifs(path, std::ios::in|std::ios::binary|std::ios::ate);
    const size_t model_size = ifs.tellg();
    ifs.seekg(0, ifs.beg);
    ifs.read(reinterpret_cast<char *>(blob.data), model_size);
}

void fromFile(const std::string &path, std::vector<float> &blob) {
    std::ifstream ifs(path, std::ios::in|std::ios::binary|std::ios::ate);
    const size_t model_size = ifs.tellg();
    ifs.seekg(0, ifs.beg);
    ifs.read(reinterpret_cast<char *>(blob.data()), model_size);
}
