#ifndef TL_TRAIN_WIDGET_H
#define TL_TRAIN_WIDGET_H

#include <QWidget>

namespace Ui {
class TlTrainWidget;
}

class TlTrainWidget : public QWidget {
    Q_OBJECT

  public:
    explicit TlTrainWidget(QWidget *parent = nullptr);
    ~TlTrainWidget();

  private slots:
    void on_pushButton1_clicked();
    void on_pushButton2_clicked();

  private:
    Ui::TlTrainWidget *ui;
};

#endif// TL_TRAIN_WIDGET_H
