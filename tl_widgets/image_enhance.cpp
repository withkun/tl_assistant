#include "image_enhance.h"


Enhance::Enhance(const QImage &image) : image_(image) {
}

QImage Enhance::enhance(float factor) const {
    // Step 3: 统一格式为RGBA8888以确保混合一致性
    QImage original = image_.convertToFormat(QImage::Format_RGBA8888);
    QImage degenerate = degenerate_.convertToFormat(QImage::Format_RGBA8888);

    // Step 4: 手动线性插值：result = factor * original + (1 - factor) * degenerate
    for (int32_t y = 0; y < original.height(); ++y) {
        uchar *resultRow = original.scanLine(y);
        const uchar *degenerateRow = degenerate.scanLine(y);

        for (int32_t x = 0; x < original.width(); ++x) {
            // 对每个通道进行线性插值
            for (int32_t c = 0; c < 4; ++c) {
                int32_t origVal = resultRow[x * 4 + c];
                int32_t degVal = degenerateRow[x * 4 + c];
                float blended = factor * origVal + (1.0f - factor) * degVal;
                resultRow[x * 4 + c] = static_cast<uchar>(std::clamp(static_cast<int32_t>(blended), 0, 255));
            }
        }
    }

    return original;
}

Contrast::Contrast(const QImage &image) : Enhance(image) {
    // Step 1: 转为灰度图（等价于 image.convert("L")）
    QImage gray = image_.convertToFormat(QImage::Format_Grayscale8);

    // Step 2: 计算全局均值（等价于 ImageStat.Stat(image).mean）
    int32_t sum = 0;
    int32_t pixelCount = gray.width() * gray.height();
    for (int32_t y = 0; y < gray.height(); ++y) {
        const uchar *scanLine = gray.scanLine(y);
        for (int32_t x = 0; x < gray.width(); ++x) {
            sum += scanLine[x];
        }
    }
    int32_t mean = static_cast<int32_t>(1.0 * sum / pixelCount + 0.5); // 四舍五入

    // Step 3: 创建退化图像（均值填充的灰度图）
    degenerate_ = QImage(gray.size(), QImage::Format_Grayscale8);
    std::fill(degenerate_.bits(), degenerate_.bits() + degenerate_.sizeInBytes(), static_cast<uchar>(mean));

    // Step 4: 若原图含Alpha，将退化图升级为相同格式并注入原始Alpha
    if (image_.hasAlphaChannel()) {
        QImage alphaChannel = image_.convertToFormat(QImage::Format_Alpha8);
        degenerate_ = degenerate_.convertToFormat(QImage::Format_RGBA8888);
        for (int32_t y = 0; y < degenerate_.height(); ++y) {
            uchar *dstRow = degenerate_.scanLine(y);
            const uchar *alphaRow = alphaChannel.scanLine(y);
            for (int32_t x = 0; x < degenerate_.width(); ++x) {
                dstRow[x * 4 + 3] = alphaRow[x]; // 设置Alpha通道
            }
        }
    }
}

Brightness::Brightness(const QImage &image) : Enhance(image) {
    // Step 1: 创建退化图像（全零填充，等价于 Image.new(image.mode, image.size, 0)）
    if (image_.hasAlphaChannel()) {
        // 若含Alpha通道，退化图初始化为全零的RGBA格式
        degenerate_ = QImage(image_.size(), QImage::Format_RGBA8888);
        std::fill(degenerate_.bits(), degenerate_.bits() + degenerate_.sizeInBytes(), 0);

        // Step 2: 精确复制原始Alpha通道（等价于 putalpha(image.getchannel("A"))）
        QImage alphaChannel = image_.convertToFormat(QImage::Format_Alpha8);
        for (int32_t y = 0; y < degenerate_.height(); ++y) {
            uchar *dstRow = degenerate_.scanLine(y);
            const uchar *alphaRow = alphaChannel.scanLine(y);
            for (int32_t x = 0; x < degenerate_.width(); ++x) {
                dstRow[x * 4 + 3] = alphaRow[x]; // Alpha通道位于第4字节
            }
        }
    } else {
        // 无Alpha时，退化图保持为Grayscale8（全零）
        degenerate_ = QImage(image_.size(), QImage::Format_Grayscale8);
        std::fill(degenerate_.bits(), degenerate_.bits() + degenerate_.sizeInBytes(), 0);
    }
}