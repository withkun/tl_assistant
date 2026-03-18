#include "sam_assist_annotation.h"
#include "ui_sam_assist_annotation.h"
#include <QToolTip>
#include <QCursor>


SamAssistAnnotation::SamAssistAnnotation(const std::string &default_model, QWidget *parent)
    : QWidget(parent), ui_(new Ui::SamAssistAnnotation) {
    ui_->setupUi(this);
    ui_->cbxModel->setCurrentText(QString::fromStdString(default_model));
    this->setMaximumWidth(200);
}

SamAssistAnnotation::~SamAssistAnnotation() {
    delete ui_;
}

bool SamAssistAnnotation::eventFilter(QObject *watched, QEvent *event) {
    if (watched == this && !this->isEnabled()) {
        if (event->type() == QEvent::Enter) {
            const QString text = tr("Select 'AI-Polygon' or 'AI-Mask' mode to enable AI-Assisted Annotation");
            QToolTip::showText(QCursor::pos(), text, this);
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SamAssistAnnotation::on_cbxModel_currentTextChanged(const QString &arg1) {
    emit samModelChanged(arg1.toStdString());
}