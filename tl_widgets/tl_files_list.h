#ifndef __INC_FILES_LIST_H
#define __INC_FILES_LIST_H

#include <QListWidget>

class TlFilesList : public QListWidget {
    Q_OBJECT
public:
    explicit TlFilesList(QWidget *parent = nullptr);
    ~TlFilesList() override;

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};
#endif // __INC_FILES_LIST_H