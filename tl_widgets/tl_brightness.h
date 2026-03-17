#ifndef __INC_BRIGHTNESS_CONTRAST_H
#define __INC_BRIGHTNESS_CONTRAST_H

#include <QDialog>
#include <QSlider>

class BrightnessContrast : public QDialog {
public:
    explicit BrightnessContrast(const QImage &img, std::function<void()> callback, QWidget *parent = nullptr);
    ~BrightnessContrast() override = default;

    void onNewValue(int32_t value);


    std::function<void()> callback_;
    static int32_t  base_value_;

    QSlider        *slider_brightness_;
    QSlider        *slider_contrast_;
};
#endif // __INC_BRIGHTNESS_CONTRAST_H