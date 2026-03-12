#ifndef __INC_SAM_ASSIST_ANNOTATION_H
#define __INC_SAM_ASSIST_ANNOTATION_H

#include <QWidget>

namespace Ui {
class SamAssistAnnotation;
}

class SamAssistAnnotation : public QWidget {
    Q_OBJECT
public:
    explicit SamAssistAnnotation(const std::string &default_model, QWidget *parent = nullptr);
    ~SamAssistAnnotation() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_cbxModel_currentTextChanged(const QString &arg1);

Q_SIGNALS:
    void samModelChanged(const std::string &name);

private:
    Ui::SamAssistAnnotation    *ui_{nullptr};
};
#endif // __INC_SAM_ASSIST_ANNOTATION_H