#ifndef __INC_UTILS_H
#define __INC_UTILS_H

#include <QPoint>
#include <QAction>
#include <QToolBar>
#include <QPushButton>
#include <QValidator>

#include <opencv2/opencv.hpp>

inline constexpr int32_t None = std::numeric_limits<int32_t>::min();

using QKey = std::set<QString>;

class TlUtils {
  public:
    static QIcon newIcon(QString icon);
    static QPushButton *newButton(QString text, QString icon="", const std::function<void()> &slot=nullptr);

    static QValidator *labelValidator();

    static QString fmtShortcut(const QList<QString> &text);

    static QAction *newAction(const QString &text, const QList<QString> &shortcut={}, const QString &file="", const QString &tip="", bool checkable=false, bool enabled=true, bool checked=false);

    static void addActions(QMenu *menu, const std::list<QObject *> &actions);
    static void addActions(QToolBar *tool, const std::list<QAction *> &actions);

    static float distance(const QPointF &p);
    static float distance(const QPointF &p1, const QPointF &p2);
    static float distanceToLine(const QPointF &point, const QLineF &line);

    static QString HashPixmap(const QPixmap &pixmap);

    static cv::Mat ImageToMat(const QImage &image);
    static QImage MatToImage(const cv::Mat &mat);

    static cv::Mat PixmapToMat(const QPixmap &pixmap);
    static QPixmap MatToPixmap(const cv::Mat &mat);

    static cv::Rect masks_to_bboxes(const cv::Mat &mask);

    static cv::Mat img_data_to_arr(const QByteArray &img_data);
    static QByteArray img_arr_to_data(const cv::Mat &img_data);

    static cv::Mat img_b64_to_arr(const std::string &b64_string);

};
#endif // __INC_UTILS_H