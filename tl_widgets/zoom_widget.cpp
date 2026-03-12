#include "zoom_widget.h"


ZoomWidget::ZoomWidget(int32_t value, QWidget *parent) : QSpinBox(parent) {
    this->setButtonSymbols(NoButtons);
    this->setRange(1, 1000);
    this->setSuffix(" %");
    this->setValue(value);
    this->setToolTip("Zoom Level");
    this->setStatusTip(this->toolTip());
    this->setAlignment(Qt::AlignCenter);
}

QSize ZoomWidget::minimumSizeHint() const {
    const auto ht = QSpinBox::minimumSizeHint().height();
    const auto fm = QFontMetrics(this->font());
    const auto wd = fm.horizontalAdvance(QString::number(this->maximum()));
    return QSize(wd, ht);
}