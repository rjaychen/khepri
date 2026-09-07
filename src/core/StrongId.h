#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <ostream>
#include <compare>

namespace khepri {

template <typename Tag, typename UnderlyingType = uint32_t>
struct StrongId {
    using Underlying = UnderlyingType;
    Underlying value = InvalidValue();

    constexpr StrongId() noexcept = default;
    constexpr explicit StrongId(Underlying val) noexcept : value(val) {}

    [[nodiscard]] constexpr static Underlying InvalidValue() noexcept {
        return std::numeric_limits<Underlying>::max();
    }

    [[nodiscard]] constexpr static StrongId Invalid() noexcept {
        return StrongId{InvalidValue()};
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return value != InvalidValue();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return IsValid();
    }

    [[nodiscard]] constexpr Underlying Get() const noexcept {
        return value;
    }

    [[nodiscard]] constexpr explicit operator Underlying() const noexcept {
        return value;
    }

    constexpr auto operator<=>(const StrongId&) const noexcept = default;
    constexpr bool operator==(const StrongId&) const noexcept = default;
};

template <typename Tag, typename UnderlyingType>
inline std::ostream& operator<<(std::ostream& os, const StrongId<Tag, UnderlyingType>& id) {
    if (id.IsValid()) {
        os << id.value;
    } else {
        os << "<InvalidId>";
    }
    return os;
}

} // namespace khepri

namespace std {
template <typename Tag, typename UnderlyingType>
struct hash<khepri::StrongId<Tag, UnderlyingType>> {
    size_t operator()(const khepri::StrongId<Tag, UnderlyingType>& id) const noexcept {
        return std::hash<UnderlyingType>{}(id.value);
    }
};
} // namespace std
