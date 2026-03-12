#include "tl_train_widget.h"
#include "ui_tl_train_widget.h"

TlTrainWidget::TlTrainWidget(QWidget *parent) : QWidget(parent), ui(new Ui::TlTrainWidget) {
    ui->setupUi(this);
    this->setWindowTitle("模型配置");
    this->setVisible(false);
}

TlTrainWidget::~TlTrainWidget() {
    delete ui;
}

void TlTrainWidget::on_pushButton1_clicked() {

}


void TlTrainWidget::on_pushButton2_clicked() {

}

