#pragma once

#include <string_view>
#include <cstdint>

namespace khepri {

enum class ImportError : uint32_t {
    None = 0,
    FileNotFound,
    CorruptedFile,
    UnsupportedFormat,
    ParsingFailed,
    VulkanAllocationFailed,
    Unknown
};

[[nodiscard]] constexpr std::string_view ToString(ImportError err) noexcept {
    switch (err) {
        case ImportError::None: return "None";
        case ImportError::FileNotFound: return "File not found";
        case ImportError::CorruptedFile: return "Corrupted file contents";
        case ImportError::UnsupportedFormat: return "Unsupported file format or extension";
        case ImportError::ParsingFailed: return "Parsing failed";
        case ImportError::VulkanAllocationFailed: return "Vulkan buffer/resource allocation failed";
        case ImportError::Unknown: return "Unknown import error";
    }
    return "Unrecognized error";
}

enum class VulkanError : uint32_t {
    None = 0,
    InitializationFailed,
    DeviceLost,
    OutOfMemory,
    PipelineCreationFailed,
    ShaderCompilationFailed,
    DescriptorAllocationFailed
};

[[nodiscard]] constexpr std::string_view ToString(VulkanError err) noexcept {
    switch (err) {
        case VulkanError::None: return "None";
        case VulkanError::InitializationFailed: return "Vulkan initialization failed";
        case VulkanError::DeviceLost: return "Vulkan device lost";
        case VulkanError::OutOfMemory: return "Vulkan out of memory";
        case VulkanError::PipelineCreationFailed: return "Pipeline creation failed";
        case VulkanError::ShaderCompilationFailed: return "Shader module compilation failed";
        case VulkanError::DescriptorAllocationFailed: return "Descriptor allocation failed";
    }
    return "Unrecognized Vulkan error";
}

} // namespace khepri
