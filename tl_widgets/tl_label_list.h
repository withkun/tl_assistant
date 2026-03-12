#ifndef __INC_LABEL_LIST_H
#define __INC_LABEL_LIST_H

#include <QListWidget>

class EscapableQListWidget : public QListWidget {
    Q_OBJECT
public:
    void keyPressEvent(QKeyEvent *keyEvent) override;
};

class TlLabelList : public EscapableQListWidget {
    Q_OBJECT
public:
    explicit TlLabelList();

    void mousePressEvent(QMouseEvent *mouseEvent) override;

    QListWidgetItem *find_label_item(const QString &label);
    void add_label_item(const QString &label, const std::vector<int32_t> &color);
};
#endif // __INC_LABEL_LIST_H