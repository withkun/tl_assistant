#ifndef __INC_IMAGE_ENHANCE_H
#define __INC_IMAGE_ENHANCE_H


#include <QImage>


class Enhance {
public:
    explicit Enhance(const QImage &image);

    QImage enhance(float factor) const;

protected:
    QImage image_;
    QImage degenerate_;
};

class Contrast : public Enhance {
public:
    explicit Contrast(const QImage &image);
};

class Brightness : public Enhance {
public:
    explicit Brightness(const QImage &image);
};

#endif //__INC_IMAGE_ENHANCE_H