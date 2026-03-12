//
// Created by njtl007 on 2024/10/11.
//

#ifndef TL_ASSISTANT_TL_UTILS_TL_FILE_DIALOG_H_
#define TL_ASSISTANT_TL_UTILS_TL_FILE_DIALOG_H_

#include <QFileDialog>
#include <QScrollArea>

class ScrollAreaPreview : public QScrollArea {

};

class FileDialogPreview : public QFileDialog {
  public:
    FileDialogPreview(QWidget *parent);
    
};

#endif// TL_ASSISTANT_TL_UTILS_TL_FILE_DIALOG_H_
