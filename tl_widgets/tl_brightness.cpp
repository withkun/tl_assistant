#include "tl_brightness.h"

#include <QLabel>
#include <QHBoxLayout>

int32_t BrightnessContrast::base_value_ = 50;

BrightnessContrast::BrightnessContrast(const QImage &img, std::function<void()> callback, QWidget *parent) : QDialog(parent) {
    setModal(true);
    setWindowTitle("Brightness/Contrast");

    const auto fSlider = [=](auto title, auto *parent) {
        auto *const layout = new QHBoxLayout();
        auto *const title_label = new QLabel(tr(title));
        title_label->setFixedWidth(75);
        layout->addWidget(title_label);

        auto *const slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 3 * base_value_);
        slider->setValue(base_value_);
        layout->addWidget(slider);

        auto *const value_label = new QLabel(QString("%1").arg(slider->value() / base_value_));
        value_label->setAlignment(Qt::AlignRight);
        layout->addWidget(value_label);

        QObject::connect(slider, &QSlider::valueChanged, this, &BrightnessContrast::onNewValue);
        QObject::connect(slider, &QSlider::valueChanged, [=]() {
            value_label->setText(QString("%1").arg(slider->value() / base_value_)); }
        );
        parent->addLayout(layout);
        return slider;
    };

    auto *v_layout = new QVBoxLayout();
    slider_brightness_ = fSlider("Brightness:", v_layout);
    slider_contrast_ = fSlider("Contrast:", v_layout);

    //if (img.mode != "RGB"):
    //    raise ValueError("Image mode must be RGB")
    //self.img = img
    callback_ = callback;
}

void BrightnessContrast::onNewValue(int32_t value) {
    double brightness = 1.0*slider_brightness_->value() / base_value_;
    double contrast = 1.0*slider_contrast_->value() / base_value_;

    //img = self.img;
    //if (brightness != 1)
    //    img = PIL.ImageEnhance.Brightness(img).enhance(brightness);
    //if (contrast != 1)
    //    img = PIL.ImageEnhance.Contrast(img).enhance(contrast);
    //
    //qimage = QImage(
    //    img.tobytes(), img.width, img.height, img.width * 3, QImage.Format_RGB888
    //)
    //self.callback(qimage);
}