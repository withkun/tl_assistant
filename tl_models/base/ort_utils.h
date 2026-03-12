#ifndef __INC_ORT_UTILS_H
#define __INC_ORT_UTILS_H

#include "onnxruntime_cxx_api.h"

size_t GetOrtValueByteSize(const Ort::Value &value);

#endif // __INC_ORT_UTILS_H