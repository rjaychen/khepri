cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(VulkanGeometryEngine VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure FetchContent
include(FetchContent)

# 1. GLFW
message(STATUS "Fetching GLFW...")
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.3.8.zip
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. GLM
message(STATUS "Fetching GLM...")
FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.zip
)
FetchContent_MakeAvailable(glm)

# 3. Vulkan Headers
message(STATUS "Fetching Vulkan-Headers...")
FetchContent_Declare(
    vulkan_headers
    URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.3.268.zip
)
FetchContent_MakeAvailable(vulkan_headers)

# 4. Volk (Meta-loader for Vulkan)
message(STATUS "Fetching volk...")
FetchContent_Declare(
    volk
    URL https://github.com/zeux/volk/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(volk)

# 5. VulkanMemoryAllocator (VMA)
message(STATUS "Fetching VulkanMemoryAllocator...")
FetchContent_Declare(
    vma
    URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(vma)

# 6. Dear ImGui
message(STATUS "Fetching Dear ImGui...")
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/heads/docking.zip
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(imgui PUBLIC 
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw volk Vulkan-Headers)
    target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)
endif()

# Source files setup
file(GLOB_RECURSE ENGINE_SOURCES 
    "src/*.cpp"
    "src/*.h"
    "src/*.hpp"
)

add_executable(VulkanGeometryEngine ${ENGINE_SOURCES})

target_include_directories(VulkanGeometryEngine PRIVATE 
    src
    ${vulkan_headers_SOURCE_DIR}/include
    ${vma_SOURCE_DIR}/include
)

target_compile_definitions(VulkanGeometryEngine PRIVATE VK_NO_PROTOTYPES GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(VulkanGeometryEngine PRIVATE 
    glfw 
    glm::glm 
    volk 
    imgui
    Vulkan-Headers
)

if(MSVC)
    target_compile_options(VulkanGeometryEngine PRIVATE /W4 /utf-8)
endif()
