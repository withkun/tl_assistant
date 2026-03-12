#include "sam_prompt_annotation.h"
#include "ui_sam_prompt_annotation.h"

#include "config/app_config.h"


SamPromptAnnotation::SamPromptAnnotation(const std::string &default_model, QWidget *parent)
    : QWidget(parent), ui_(new Ui::SamPromptAnnotation) {
    ui_->setupUi(this);

    AppConfig &appConfig = AppConfig::instance();
    ui_->sbxIoU->setValue(appConfig.iou_threshold_);
    ui_->sbxScore->setValue(appConfig.score_threshold_);
    ui_->cbxModel->setCurrentText(QString::fromStdString(default_model));
}

SamPromptAnnotation::~SamPromptAnnotation() {
    delete ui_;
}

std::string SamPromptAnnotation::get_model_name() const {
    return ui_->cbxModel->currentText().toStdString();
}

std::vector<std::string> SamPromptAnnotation::get_prompt_texts() const {
    const auto items = ui_->edtInput->text().split(",");
    std::vector<std::string> prompt_texts;
    std::ranges::transform(items, std::back_inserter(prompt_texts), [](auto &s) { return s.toStdString(); });
    return prompt_texts;
}

void SamPromptAnnotation::on_btnRun_clicked() {
    emit submitAiPrompt();
}

void SamPromptAnnotation::on_sbxIoU_valueChanged(double arg1) {
    AppConfig::instance().iou_threshold_ = arg1;
}

void SamPromptAnnotation::on_sbxScore_valueChanged(double arg1) {
    AppConfig::instance().score_threshold_ = arg1;
}

void SamPromptAnnotation::on_cbxModel_currentIndexChanged(int index) {
    const auto item = ui_->cbxModel->itemData(index).toString();
    AppConfig::instance().ai_prompt_name_ = item.toStdString();
}