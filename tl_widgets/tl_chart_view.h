//
// Created by njtl007 on 2024/10/23.
//

#ifndef YOLOCHART_TL_UTILS_TL_CHART_VIEW_H_
#define YOLOCHART_TL_UTILS_TL_CHART_VIEW_H_

#include <QtCharts>

class TlChartView : public QChartView {
    Q_OBJECT
  public:
    TlChartView(QChart *chart, QWidget *parent = nullptr);

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override ;

    void mousePressEvent(QMouseEvent *event) override;

  signals:
    void sgl_recoverRange(QChartView *);

  private:
    bool is_Pressed_{true};
};

#endif // YOLOCHART_TL_UTILS_TL_CHART_VIEW_H_
