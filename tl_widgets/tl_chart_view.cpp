#include "tl_chart_view.h"


TlChartView::TlChartView(QChart *chart, QWidget *parent) : QChartView(chart, parent) {
    is_Pressed_ = false;
}

void TlChartView::mouseMoveEvent(QMouseEvent *event) {
    is_Pressed_ = false;
    QChartView::mouseMoveEvent(event);
}

void TlChartView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (is_Pressed_) {
            is_Pressed_ = false;
            emit sgl_recoverRange(this);   // 单击鼠标恢复缩放
        }
    }
    QChartView::mouseReleaseEvent(event);
}

void TlChartView::mousePressEvent(QMouseEvent *event) {
    is_Pressed_ = true;
    QChartView::mousePressEvent(event);
}