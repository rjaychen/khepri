#pragma once

#include <string>
#include <variant>
#include <vector>
#include <functional>
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

struct Property {
    std::string name;
    PropertyType type;
    std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, bool, std::string> value;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    std::function<void()> onChangeCallback = nullptr;

    template<typename T>
    T GetValue() const {
        return std::get<T>(value);
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
