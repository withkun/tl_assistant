#include "ai_assist_annotation.h"

#include "info_button.h"
#include "spdlog/spdlog.h"
#include "base/format_qt.h"

#include <QLabel>
#include <QEvent>
#include <QVBoxLayout>
#include <QToolTip>


namespace {
std::vector<std::pair<std::string, std::string>> available_models_{
    {"efficientsam:10m", "EfficientSam (speed)"},
    {"efficientsam:latest", "EfficientSam (accuracy)"},
    {"sam:100m", "Sam (speed)"},
    {"sam:300m", "Sam (balanced)"},
    {"sam:latest", "Sam (accuracy)"},
    {"sam2:small", "Sam2 (speed)"},
    {"sam2:latest", "Sam2 (balanced)"},
    {"sam2:large", "Sam2 (accuracy)"},
};
}


AiAssistAnnotation::AiAssistAnnotation(const std::string &default_model, const std::function<void(const std::string &name)> &on_model_changed, QWidget *parent)
    : QWidget(parent) {
    init_ui(default_model, on_model_changed);
}

AiAssistAnnotation::~AiAssistAnnotation() {
}

void AiAssistAnnotation::init_ui(const std::string &default_model, const std::function<void(const std::string &name)> &on_model_changed) {
    auto *const layout = new QVBoxLayout();
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    this->setLayout(layout);

    auto *const header_layout = new QHBoxLayout();
    header_layout->addStretch();
    auto *const label = new QLabel(tr("AI-Assisted Annotation"));
    header_layout->addWidget(label);
    auto *const info_button = new InfoButton(
        tr(
            "AI suggests annotation in 'AI-Polygon' and 'AI-Mask' modes"
        )
    );
    header_layout->addWidget(info_button);
    header_layout->addStretch();
    layout->addLayout(header_layout);

    body_ = new QWidget();
    body_->installEventFilter(this);
    auto *const body_layout = new QVBoxLayout();
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);
    body_->setLayout(body_layout);

    model_combo_ = new QComboBox();
    for (auto &[model_id, model_display] : available_models_) {
        model_combo_->addItem(QString::fromStdString(model_display), QString::fromStdString(model_id));
    }
    body_layout->addWidget(model_combo_);

    layout->addWidget(body_);

    int32_t model_index = 0;
    QList<std::string> model_ui_names;
    std::ranges::transform(available_models_, std::back_inserter(model_ui_names), [](const auto &it){ return it.first; });
    if (model_ui_names.contains(default_model)) {
        model_index = model_ui_names.indexOf(default_model);
    } else {
        SPDLOG_WARN("Default AI model is not found: {}", default_model);
        model_index = 0;
    }

    QObject::connect(model_combo_, &QComboBox::currentIndexChanged, [=](int index) {
        QString model_name = model_combo_->itemData(index).toString();
        on_model_changed(model_name.toStdString());
    });
    model_combo_->setCurrentIndex(model_index);

    setMaximumWidth(200);
}

void AiAssistAnnotation::setEnabled(bool a0) {
    body_->setEnabled(a0);
}

bool AiAssistAnnotation::eventFilter(QObject *a0, QEvent *a1) {
    if (a0 == body_ && !body_->isEnabled()) {
        if (a1->type() == QEvent::Enter)
            QToolTip::showText(
                QCursor::pos(),
                tr(
                    "Select 'AI-Polygon' or 'AI-Mask' mode "
                    "to enable AI-Assisted Annotation"
                ),
                body_
            );
    }
    return QWidget::eventFilter(a0, a1);
}