//
// Created by njtl007 on 2024/10/25.
//

#ifndef CHARTYOLO_TL_UTILS_TL_FILE_WATCHER_H_
#define CHARTYOLO_TL_UTILS_TL_FILE_WATCHER_H_

#include <QFileSystemWatcher>

class TlFileWatcher : public QFileSystemWatcher {
    Q_OBJECT
  public:
    TlFileWatcher(const QString &path, QObject *parent = nullptr);
    ~TlFileWatcher() override;

  private slots:
    void slotFileChanged(const QString &path);
};


#endif // CHARTYOLO_TL_UTILS_TL_FILE_WATCHER_H_
