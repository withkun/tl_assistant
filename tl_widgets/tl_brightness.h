//
// Created by njtl007 on 2024/11/21.
//

#ifndef TL_ASSISTANT_TL_UTILS_TL_BRIGHTNESS_H_
#define TL_ASSISTANT_TL_UTILS_TL_BRIGHTNESS_H_

#include <QDialog>

class TlBrightness : public QDialog {
public:
    explicit TlBrightness(const QImage &img, std::function<void()> callback, QWidget *parent = nullptr);
    ~TlBrightness();

    void onNewValue();


    static int32_t base_value_;
};
#endif// TL_ASSISTANT_TL_UTILS_TL_BRIGHTNESS_H_
