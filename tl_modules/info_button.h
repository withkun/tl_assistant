#ifndef INFO_BUTTON_H
#define INFO_BUTTON_H

#include <QToolButton>


class InfoButton : public QToolButton {
    Q_OBJECT
public:
    explicit InfoButton(const QString &tooltip, QWidget *parent = nullptr);
    ~InfoButton() override;

    void enterEvent(QEnterEvent *event) override;

private:
};
#endif // INFO_BUTTON_H