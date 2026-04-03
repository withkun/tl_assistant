#ifndef __INC_AI_ASSIST_ANNOTATION_H
#define __INC_AI_ASSIST_ANNOTATION_H

#include <QWidget>
#include <QComboBox>

class AiAssistAnnotation : public QWidget {
    Q_OBJECT
public:
    explicit AiAssistAnnotation(const std::string &default_model, const std::function<void(const std::string &name)> &on_model_changed, QWidget *parent = nullptr);
    ~AiAssistAnnotation() override;

    void setEnabled(bool a0);
    bool eventFilter(QObject *watched, QEvent *event) override;

    void init_ui(const std::string &default_model, const std::function<void(const std::string &name)> &on_model_changed);

private:
    QWidget                *body_;
    QComboBox              *model_combo_;
};
#endif //__INC_AI_ASSIST_ANNOTATION_H