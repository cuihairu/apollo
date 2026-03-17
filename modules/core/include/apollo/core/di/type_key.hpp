#pragma once

#include <cstddef>

namespace apollo::core::di {

using TypeKey = const void*;

template <typename T>
inline TypeKey type_key_of() noexcept {
    static int type_key;
    return &type_key;
}

} // namespace apollo::core::di
