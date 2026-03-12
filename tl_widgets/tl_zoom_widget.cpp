#include "tl_zoom_widget.h"
#include "ui_tl_zoom_widget.h"

TlZoomWidget::TlZoomWidget(QWidget *parent) : QWidget(parent), ui_(new Ui::TlZoomWidget) {
    ui_->setupUi(this);
}

TlZoomWidget::~TlZoomWidget() {
    delete ui_;
}

QSize TlZoomWidget::minimumSizeHint() const {
    const auto ht = ui_->spinBox->minimumSizeHint().height();
    const auto fm = QFontMetrics(this->font());
    const auto wd = fm.horizontalAdvance(QString::number(ui_->spinBox->maximum()));
    return QSize(wd, ht);
}

void TlZoomWidget::setValue(int32_t value) {
    ui_->spinBox->setValue(value);
}

int32_t TlZoomWidget::value() const {
    return ui_->spinBox->value();
}

void TlZoomWidget::on_spinBox_valueChanged(int value) {
    emit valueChanged(value);
}

