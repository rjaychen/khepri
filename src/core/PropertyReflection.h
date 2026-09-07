#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>
#include <glm/glm.hpp>

enum class PropertyType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Int,
    Bool,
    String,
    Color3,
    Color4
};

struct FloatRange {
    float min = 0.0f;
    float max = 1.0f;
};

struct IntRange {
    int min = 0;
    int max = 100;
};

using PropertyLimits = std::variant<std::monostate, FloatRange, IntRange>;

struct Property {
    std::string name;
    PropertyType type;
    std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, bool, std::string> value;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    PropertyLimits limits = std::monostate{};
    std::function<void()> onChangeCallback = nullptr;

    [[nodiscard]] bool HasNumericLimits() const noexcept {
        return !std::holds_alternative<std::monostate>(limits);
    }

    template<typename T>
    [[nodiscard]] std::optional<T> GetOptionalValue() const noexcept {
        if (const auto* val = std::get_if<T>(&value)) {
            return *val;
        }
        return std::nullopt;
    }

    template<typename T>
    [[nodiscard]] T GetValueOr(const T& fallback) const noexcept {
        if (const auto* val = std::get_if<T>(&value)) {
            return *val;
        }
        return fallback;
    }

    template<typename T>
    [[nodiscard]] T GetValue() const {
        if (const auto* val = std::get_if<T>(&value)) {
            return *val;
        }
        throw std::bad_variant_access();
    }

    template<typename T>
    void SetValue(const T& newValue) {
        value = newValue;
        if (onChangeCallback) {
            onChangeCallback();
        }
    }
};

class IReflectable {
public:
    virtual ~IReflectable() = default;
    virtual std::vector<Property>& GetProperties() = 0;
    virtual const std::vector<Property>& GetProperties() const = 0;
};

