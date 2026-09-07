#pragma once

#include <cstdint>
#include <cstddef>

namespace khepri::ui {

// Font metadata and embedded binary definitions.
// In production builds, full TTF byte arrays can be bundled here or loaded from assets/fonts/.
struct EmbeddedFontData {
    static const unsigned char* GetInterFontData(size_t& outSize) {
        outSize = 0;
        return nullptr;
    }

    static const unsigned char* GetIconFontData(size_t& outSize) {
        outSize = 0;
        return nullptr;
    }
};

} // namespace khepri::ui
