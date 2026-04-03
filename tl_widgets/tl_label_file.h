#ifndef __INC_LABEL_FILE_H
#define __INC_LABEL_FILE_H

#include <QObject>
#include "tl_shape.h"

class ShapeDict {
public:
    QString                 label;
    QList<QPointF>          points;
    QString                 shape_type;
    QMap<QString, bool>     flags;
    QString                 description;
    int32_t                 group_id;
    cv::Mat                 mask;
    QMap<QString, QString>  other_data;
};

class LabelFileError : public std::exception {
  public:
    LabelFileError() {};
};

class TlLabelFile : public QObject {
public:
    explicit TlLabelFile(const QString &filename = "");

    static QString suffix;

    static QByteArray load_image_file(const QString &filename);

    static bool is_label_file(const QString &filename);

    void load(const QString &filename);

    void save(const QString &filename,
              const QList<TlShape> &shapes,
              const QString &imagePath,
              const QByteArray &imageData,
              int32_t imageHeight,
              int32_t imageWidth,
              const QString &otherData="",
              const QMap<QString, bool> &flags={});

private:
    std::pair<int32_t, int32_t> check_image_height_and_width(const QByteArray &imageData, int32_t imageHeight, int32_t imageWidth);

public:
    QString         flags_;
    QList<TlShape>  shapes_;
    QList<ShapeDict>  shapes1_;

    QString         filename_;
    QString         imagePath_;
    QByteArray      imageData_;
    QByteArray      otherData_;
};
#endif //__INC_LABEL_FILE_H