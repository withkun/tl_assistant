#ifndef __INC_SAM_PROMPT_ANNOTATION_H
#define __INC_SAM_PROMPT_ANNOTATION_H

#include <QWidget>

namespace Ui {
class SamPromptAnnotation;
}

class SamPromptAnnotation : public QWidget {
    Q_OBJECT
public:
    explicit SamPromptAnnotation(const std::string &default_model, QWidget *parent = nullptr);
    ~SamPromptAnnotation() override;

    std::string get_model_name() const;
    std::vector<std::string> get_prompt_texts() const;

private slots:
    void on_btnRun_clicked();
    void on_sbxIoU_valueChanged(double arg1);
    void on_sbxScore_valueChanged(double arg1);
    void on_cbxModel_currentIndexChanged(int index);

Q_SIGNALS:
    void submitAiPrompt();

private:
    Ui::SamPromptAnnotation    *ui_{nullptr};
};
#endif // __INC_SAM_PROMPT_ANNOTATION_H