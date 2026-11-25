#pragma once
#include <chrono>

namespace pm {
inline double now_s() {
    using C = std::chrono::steady_clock;
    return std::chrono::duration<double>(C::now().time_since_epoch()).count();
}
}  // namespace pm
