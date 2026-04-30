#ifndef __INC_BLOB_H
#define __INC_BLOB_H

#include <string>

class Attach {
public:
    std::string url_;
    std::string hash_;
};

class Blob {
public:
    Blob(const std::string &url, const std::string &hash, const Attach &attach={});

    std::string filename() const;
    std::string path() const;

    void pull();

    std::string url_;
    std::string hash_;
    Attach attachments_;
};
#endif //__INC_BLOB_H