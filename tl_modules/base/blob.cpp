#include "blob.h"

#include <Shlobj.h>
#include <filesystem>
#include <QCoreApplication>
#include "spdlog/spdlog.h"


Blob::Blob(const std::string &url, const std::string &hash, const Attach &attach) {
    url_ = url;
    hash_ = hash;
    attachments_ = attach;
}

std::string Blob::filename() const {
    // 查找路径开始位置（跳过协议和域名部分）
    std::string::size_type pos = url_.find_last_of("/");
    if (pos != std::string::npos) {
        return url_.substr(pos+1);
    }
    return url_;
}

std::string Blob::path() const {
    // 先判断应用进程目录下是否存在models目录, 如果存在则从进程目录获取模型.
    const QString appDirPath = QCoreApplication::applicationDirPath();
    std::filesystem::path exe_directory{appDirPath.toLocal8Bit().constData()};
    exe_directory /= "models";
    if (exists(exe_directory)) {
        exe_directory /= filename();
        const std::string path = absolute(exe_directory).string();
        SPDLOG_INFO("===> path: {}", path);
        return path;
    }

    //const auto blobPath = QDir::homePath() + "/.cache/osam/models/blobs/";
    TCHAR szPath[1024]{};
    SHGetSpecialFolderPath(nullptr, szPath, CSIDL_PROFILE, 0);
    std::filesystem::path home_directory{szPath};
    home_directory /= ".cache/osam/models/blobs";
    home_directory /= filename();
    const std::string path = absolute(home_directory).string();
    SPDLOG_INFO("===> path: {}", path);
    return path;
}

void Blob::pull() {
    // 下载远程文件, 暂不实现.
}