#include "info_button.h"

#include <QToolTip>


InfoButton::InfoButton(const QString &tooltip, QWidget *parent)
    : QToolButton(parent) {
    this->setIcon(QIcon(":/icons/info.svg"));
    this->setIconSize(QSize(16, 16));
    this->setStyleSheet(
        "QToolButton {"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 0px;"
        "}"
        "QToolButton:hover {"
        "    background-color: rgba(0, 0, 0, 0.1);"
        "}"
    );
    this->setCursor(QCursor(Qt::PointingHandCursor));
    this->setToolTip(tooltip);
}

InfoButton::~InfoButton() {
}

void InfoButton::enterEvent(QEnterEvent *event) {
    QToolButton::enterEvent(event);
    QToolTip::showText(QCursor::pos(), this->toolTip());
}