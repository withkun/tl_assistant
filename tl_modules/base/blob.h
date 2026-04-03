#ifndef __INC_BLOB_H
#define __INC_BLOB_H

#include <string>

class Blob {
public:
    Blob(const std::string &url, const std::string &hash);

    std::string filename() const;
    std::string path() const;

    void pull();

    std::string url_;
    std::string hash_;
};
#endif //__INC_BLOB_H