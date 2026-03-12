//
// Created by njtl007 on 2024/10/25.
//

#include "tl_file_watcher.h"

TlFileWatcher::TlFileWatcher(const QString &path, QObject *parent) : QFileSystemWatcher(parent) {
    this->addPath(path);
}

TlFileWatcher::~TlFileWatcher() {
    this->removePaths(this->files());
    this->removePaths(this->directories());
}

void TlFileWatcher::slotFileChanged(const QString &path) {
    //qDebug() << "File changed:" << path;
}
