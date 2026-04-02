#include "tl_utils.h"

#include "base64.h"
#include "spdlog/spdlog.h"
#include "base/format_qt.h"

#include <QMenu>
#include <QBuffer>
#include <QCryptographicHash>


QIcon utils::newIcon(const QString &icon) {
    const QString icons_dir(":/icons/" + icon + ".png");
    return QIcon(icons_dir);
}

QString utils::fmtShortcut(const QList<QString> &text) {
    if (text.empty()) { return ""; }
    auto slist = text[0].split("+");
    if (slist.size() < 2) { return text[0]; }
    QString mod = slist[0], key = slist[1];
    return QString("<b>%1</b>+<b>%2</b>").arg(mod, key);
}

QAction *utils::newAction(const QString &text, const QList<QString> &shortcut, const QString &file, const QString &tip, bool checkable, bool enabled, bool checked) {
    auto *a = new QAction(text);
    const QIcon icon(file);
    if (!icon.isNull()) {
        a->setIconText(text);
        a->setIcon(icon);
    }

    if (shortcut.size() == 1) {
        a->setShortcut(shortcut[0]);
    } else if (shortcut.size() > 1) {
        QList<QKeySequence> shortcuts;
        std::ranges::transform(shortcut, std::back_inserter(shortcuts), [](auto &s){ return QKeySequence(s); });
        if (!shortcuts.empty()) {
            a->setShortcuts(shortcuts);
        }
    }

    if (!tip.isEmpty()) {
        a->setToolTip(tip);
        a->setStatusTip(tip);
    }

    a->setCheckable(checkable);
    a->setEnabled(enabled);
    a->setChecked(checked);
    return a;
};

void utils::addActions(QMenu *menu, const std::list<QObject *> &actions) {
    for (auto *a : actions) {
        if (a == nullptr) {
            menu->addSeparator();
        } else if (qobject_cast<QMenu *>(a)) {
            menu->addMenu(qobject_cast<QMenu *>(a));
        } else {
            menu->addAction(qobject_cast<QAction *>(a));
        }
    }
}

void utils::addActions(QToolBar *tool, const std::list<QAction *> &actions) {
    for (const auto &a : actions) {
        if (a == nullptr) {
            tool->addSeparator();
        } else {
            tool->addAction(a);
        }
    }
}

QValidator *utils::labelValidator() {
    return new QRegularExpressionValidator(QRegularExpression("^[^ \t].+"));
}

// 向量点积
double dot(const std::vector<double> &a, const std::vector<double> &b) {
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// 向量模长
double mod(const std::vector<double> &v) {
    return dot(v, v);
}

double pnt_to_line(const QPointF &point, const QLineF &line) {
    std::vector<double> AP = {point.x() - line.x1(), point.y() - line.y1()};
    std::vector<double> BP = {point.x() - line.x2(), point.y() - line.y2()};

    double M_AP = mod(AP);
    double M_BP = mod(BP);
    double AP_BP = dot(AP, BP);
    return std::sqrt((M_AP * M_BP) / (line.y1() - line.x1() * AP_BP + 0.0001));
}

static float distancePtSeg(const std::vector<float> &P, const std::vector<float> &A, const std::vector<float> &B) {
    // https://blog.csdn.net/u012138730/article/details/79779996
    // 点到线段距离分三种情况:
    //                P               P                                                      P
    //               /|               |\                                                    /|
    //             /  |               |  \                                                /  |
    //           /    |d              |    \d                                          d/    |
    //         /      |               |      \                                        /      |
    //       /========+=======        +        \================    ================/        +
    //      A         C      B        C         A              B    A               B        C
    // 1.点到线段距离PC(PAB为锐角)       2.点到线段距离PA(PAB为钝角)       3.点到线段距离PB(PBA为锐角)
    // 无论P在何处都有:      AP * AB    AB         AP * AB                AB * AP                    PC if (0 < R < 1)
    //               AC = ------- * ----    = ----------- * AB,      ----------- = R   ===>   d = AP if (R <= 0)
    //                     |AB|     |AB|      |AB| * |AB|            |AB| * |AB|                  BP if (R >= 1)
    //                       标量      单位向量           标量
    // 特殊情况：
    // 1.当P在线段AB上：计算出来r仍然是 1>r>0, P点即C点, PC的距离d = 0；
    // 2.当P在线段AB端点或其延长线上：r仍然有是 r<=0 或者是 r >= 1, 仍然是计算 PA 或 PB 的距离；
    // 3.当AB是同一点：无法计算r, 所以需要对AB的长度进行一个判断, 如果是AB为零, 直接令r = 0, 直接计算AP或BP的距离都一样.

    // 先计算r的值, 看r的范围(p相当于A点, q相当于B点, pt相当于P点)
    // AB 向量
    float ABX = B[0] - A[0];
    float ABY = B[1] - A[1];
    // AP 向量
    float APX = P[0] - A[0];
    float APY = P[1] - A[1];

    // qp线段长度的平方=上面公式中的分母: AB向量的平方。
    float d = ABX * ABX + ABY * ABY;
    // (p pt向量) 点积 (pq 向量) = 公式中的分子: AP点积AB
    float t = ABX * APX + ABY * APY;

    // t 就是 公式中的r了
    if (d > 0) {   // 除数不能为0; 如果为零 t应该也为零。下面计算结果仍然成立。
        t /= d;    // 此时t 相当于 上述推导中的 r。
    }
    // 分类讨论
    if (t < 0) {
        t = 0;     // 当t(r) < 0时, 最短距离即为 pt点 和 p点(A点和P点)之间的距离。
    } else if (t > 1) {
        t = 1;     // 当t(r) > 1时, 最短距离即为 pt点 和 q点(B点和P点)之间的距离。
    }

    // t = 0, 计算 pt点 和 p点的距离; (A点和P点)
    // t = 1, 计算 pt点 和 q点 的距离; (B点和P点)
    // 否则计算 pt点 和 投影点 的距离。(C点和P点 , t*(ABX, ABY)就是向量AC)
    APX = A[0] + t * ABX - P[0];
    APY = A[1] + t * ABY - P[1];

    // 算出来是距离的平方
    return std::sqrt(APX * APX + APY * APY);
}

// 计算点到原点的距离
qreal utils::distance(const QPointF &p) {
    return std::sqrt(p.x() * p.x() + p.y() * p.y());
}

// 计算两点之间的距离
qreal utils::distance(const QPointF &p1, const QPointF &p2) {
    return distance(p1 - p2);
}

// 计算点到线段的距离
qreal utils::distanceToLine(const QPointF &point, const QLineF &line) {
    /* 点到线段距离:
     *
     *  方法一: 经典算法
     *    先判断点在线段端点、点在线上等等的特殊情况, 逐步的由特殊到一般, 当忽略点在线段上的特殊情况时, 判断点到线段方向的垂线是否落在线段上的方法是通过比较横纵坐标的方式来判断, 最后把不同的判断情况用不同的几何方式来进行处理计算得出结果。
     *
     *  方法二: 面积算法
     *    先判断投影点是否在线段上, 投影点在线段延长线上时, 最短距离长度为点到端点的线段长度；当投影点在线段上时, 先使用海伦公式计算三角形面积, 再计算出三角形的高, 即为最短距离。
     *
     *  方法三: 矢量算法
     *    矢量算法过程清晰, 如果具有一定的空间几何基础, 则是解决此类问题时应优先考虑的方法。当需要计算的数据量很大时, 这种方式优势明显。
     **/
    const auto dist_ab = distance(line.p1(), line.p2()); // 线段长度
    const auto dist_pa = distance(point, line.p1());     // 到起点的距离
    const auto dist_pb = distance(point, line.p2());     // 到终点的距离
    if (dist_pa <= 0.00001 || dist_pb <= 0.00001) {
        return 0;
    }
    if (dist_ab <= 0.00001) {
        return dist_pa;
    }

    if (dist_pb*dist_pb >= dist_ab*dist_ab + dist_pa*dist_pa) {
        return dist_pa;
    }
    if (dist_pa*dist_pa >= dist_ab*dist_ab + dist_pb*dist_pb) {
        return dist_pb;
    }

    const auto dist_hp = 0.5*(dist_ab+dist_pa+dist_pb);   // 半周长
    const auto tr_area = std::sqrt(dist_hp * (dist_hp - dist_ab) * (dist_hp - dist_pa) * (dist_hp - dist_pb)); // 海伦公式求面积
    return 2 * tr_area / dist_ab;    // 点到线的距离(利用三角形面积公式求高)

    //p1, p2 = line;
    //p1 = np.array([p1.x(), p1.y()]);
    //p2 = np.array([p2.x(), p2.y()]);
    //p3 = np.array([point.x(), point.y()]);
    //if np.dot((p3 - p1), (p2 - p1)) < 0:
    //    return np.linalg.norm(p3 - p1);
    //if np.dot((p3 - p2), (p1 - p2)) < 0:
    //    return np.linalg.norm(p3 - p2);
    //if np.linalg.norm(p2 - p1) == 0:
    //    return np.linalg.norm(p3 - p1);
    //return np.linalg.norm(np.cross(p2 - p1, p1 - p3)) / np.linalg.norm(p2 - p1);
}

QString utils::HashPixmap(const QPixmap &pixmap) {
    QImage image = pixmap.toImage();
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG"); // 使用合适的格式，例如 "PNG"
    buffer.close();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(byteArray);
    QByteArray hashResult = hash.result();

    QString hashString = QString(hashResult.toHex()); // 将结果转换为十六进制字符串形式
    return hashString;
}

cv::Mat utils::ImageToMat(const QImage &image) {
    switch (image.format()) {
        case QImage::Format_Grayscale8: {     // 灰度图, 每个像素点1个字节(8位)
            // Mat构造：行数, 列数, 存储结构, 数据, step每行多少字节
            return cv::Mat(image.height(), image.width(), CV_8UC1, (void *)image.bits(), image.bytesPerLine());
        }
        case QImage::Format_ARGB32:     // uint32存储0xAARRGGBB, pc一般小端存储低位在前, 所以字节顺序就成了BGRA
        case QImage::Format_RGB32:      // Alpha为FF
        case QImage::Format_ARGB32_Premultiplied: {
            return cv::Mat(image.height(), image.width(), CV_8UC4, (void *)image.bits(), image.bytesPerLine());
        }
        case QImage::Format_RGB888: {   // RR,GG,BB字节顺序存储
            return cv::Mat(image.height(), image.width(), CV_8UC3, (void *)image.bits(), image.bytesPerLine());
        }
        case QImage::Format_RGBA64: {   // uint64存储, 顺序和Format_ARGB32相反, RGBA
            return cv::Mat(image.height(), image.width(), CV_16UC4, (void *)image.bits(), image.bytesPerLine());
        }
        default: {
            throw std::runtime_error(std::format("Unknown image format: {}", static_cast<int32_t>(image.format())));
        }
    }
}

QImage utils::MatToImage(const cv::Mat &mat) {
    const auto cv_type = mat.type(); //防止警告
    switch (cv_type) {
        case CV_8UC1: {
            // QImage构造：数据, 宽度, 高度, 每行多少字节, 存储结构
            return QImage(static_cast<const uint8_t *>(mat.data), mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        }
        case CV_8UC3: {
            return QImage(static_cast<const uint8_t *>(mat.data), mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
        }
        case CV_8UC4: {
            return QImage(static_cast<const uint8_t *>(mat.data), mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        }
        case CV_16UC4: {
            return QImage(static_cast<const uint8_t *>(mat.data), mat.cols, mat.rows, mat.step, QImage::Format_RGBA64).rgbSwapped();
        }
        default: {
            return QImage();
        }
    }
}

cv::Mat utils::PixmapToMat(const QPixmap &pixmap) {
    return ImageToMat(pixmap.toImage());
}

QPixmap utils::MatToPixmap(const cv::Mat &mat) {
    return QPixmap::fromImage(MatToImage(mat));
}

cv::Rect utils::masks_to_bboxes(const cv::Mat &mask) {
    // 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        return {};
    }

    // 寻找最长的轮廓
    size_t max_length = 0;
    size_t max_length_index = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < contours.size(); ++i) {
        size_t length = contours[i].size();
        if (length > max_length) {
            max_length = length;
            max_length_index = i;
        }
    }

    //drop last point that is duplicate of first point
    return cv::boundingRect(contours[max_length_index]);
}

cv::Mat utils::img_data_to_arr(const QByteArray &img_data) {
    return cv::Mat();
}

QByteArray utils::img_arr_to_data(const cv::Mat &img_data) {
    std::vector<uint8_t> im_data;
    cv::imencode(".png", img_data, im_data);
    return QByteArray(im_data);
}

cv::Mat utils::img_b64_to_arr(const std::string &b64_string) {
    std::string im_data = base64::b64decode(b64_string);
    return cv::imdecode(std::vector<uint8_t>{im_data.begin(), im_data.end()}, cv::IMREAD_GRAYSCALE);
}

//def img_pil_to_data(img_pil):
//    f = io.BytesIO()
//    img_pil.save(f, format="PNG")
//    img_data = f.getvalue()
//    return img_data

//def img_data_to_pil(img_data):
//    f = io.BytesIO()
//    f.write(img_data)
//    img_pil = PIL.Image.open(f)
//    return img_pil

//def img_data_to_arr(img_data):
//    img_pil = img_data_to_pil(img_data)
//    img_arr = np.array(img_pil)
//    return img_arr

//def img_arr_to_data(img_arr):
//    img_pil = PIL.Image.fromarray(img_arr)
//    img_data = img_pil_to_data(img_pil)
//    return img_data