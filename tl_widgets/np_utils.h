#ifndef __INC_NP_UTILS_H
#define __INC_NP_UTILS_H

#include <vector>

namespace np {
// 返回元素的符号，即1（正数），0（零），或-1（负数）
template<typename T>
int32_t sign(const T &x) {
    return x > 0 ? 1 : x < 0 ? -1 : 0;
}

template<typename T>
T clip(T v, T lower, T upper) {
    return std::min(std::max(v, lower), upper);
}

template<typename T>
std::vector<T> clip(const std::vector<T> &v, T lower, T upper) {
    std::vector<T> clipped;
    clipped.reserve(v.size());
    std::ranges::transform(v, std::back_inserter(clipped), [lower, upper](const auto &x) { return clip(x, lower, upper); });
    return clipped;
}

}
#endif // __INC_NP_UTILS_H