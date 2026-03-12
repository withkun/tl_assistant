#ifndef TL_ZOOM_WIDGET_H
#define TL_ZOOM_WIDGET_H

#include <QWidget>

namespace Ui {
class TlZoomWidget;
}

class TlZoomWidget : public QWidget {
    Q_OBJECT
public:
    explicit TlZoomWidget(QWidget *parent = nullptr);
    ~TlZoomWidget() override;

    QSize minimumSizeHint() const;

    void setValue(int32_t value);
    int32_t value() const;

Q_SIGNALS:
    void valueChanged(int value);

private slots:
  void on_spinBox_valueChanged(int value);

private:
    Ui::TlZoomWidget *ui_;
};
#endif// TL_ZOOM_WIDGET_H