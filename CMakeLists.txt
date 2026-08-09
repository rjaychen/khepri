cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(KhepriEngine VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
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

# Explicitly list GLTFImporter to prevent CMake globbing cache issues
list(APPEND ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mesh/GLTFImporter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mesh/GLTFImporter.h"
)
list(REMOVE_DUPLICATES ENGINE_SOURCES)

add_executable(KhepriEngine ${ENGINE_SOURCES})

# 7. stb (Image Loader)
message(STATUS "Fetching stb...")
FetchContent_Declare(
    stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(stb)

target_include_directories(KhepriEngine PRIVATE 
    src
    ${vulkan_headers_SOURCE_DIR}/include
    ${vma_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_compile_definitions(KhepriEngine PRIVATE VK_NO_PROTOTYPES GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(KhepriEngine PRIVATE 
    glfw 
    glm::glm 
    volk 
    imgui
    Vulkan-Headers
)

if(MSVC)
    target_compile_options(KhepriEngine PRIVATE /W4 /utf-8 /FS)
endif()

# -------------------------------------------------------
# Shader compilation (GLSL -> SPIR-V via glslc from SDK)
# -------------------------------------------------------
find_program(GLSLC glslc HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "C:/VulkanSDK/1.4.357.0/Bin"
    "C:/VulkanSDK/1.4.321.0/Bin"
    "C:/VulkanSDK/1.3.283.0/Bin"
)

if(GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")
    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/shaders/compiled")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    set(SHADER_SOURCES
        ${SHADER_SOURCE_DIR}/mesh.vert
        ${SHADER_SOURCE_DIR}/mesh.frag
    )

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
        add_custom_command(
            OUTPUT  ${SPV_OUTPUT}
            COMMAND ${GLSLC} --target-env=vulkan1.3 -O ${SHADER} -o ${SPV_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
        )
        list(APPEND SPV_OUTPUTS ${SPV_OUTPUT})
    endforeach()

    add_custom_target(Shaders ALL DEPENDS ${SPV_OUTPUTS})
    add_dependencies(KhepriEngine Shaders)

    add_custom_command(TARGET KhepriEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:KhepriEngine>/shaders/compiled"
        COMMENT "Copying compiled shaders to output executable directory"
    )
    message(STATUS "Shader auto-compilation and post-build copying enabled")
else()
    message(WARNING "glslc not found - shaders will NOT be compiled automatically. Install Vulkan SDK.")
endif()

# -------------------------------------------------------
# Unit Testing Framework (GoogleTest)
# -------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    message(STATUS "Fetching GoogleTest...")
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    add_executable(KhepriEngineTests
        tests/MainTests.cpp
        tests/HalfEdgeTests.cpp
        src/mesh/HalfEdgeMesh.cpp
        src/mesh/CDT.cpp
        src/mesh/ExactPredicates.cpp
        src/mesh/MeshBoolean.cpp
        src/scene/SceneNode.cpp
        src/scene/MeshComponent.cpp
        src/animation/Timeline.cpp
        src/animation/Rigging.cpp
        src/vulkan/Buffer.cpp
        src/vulkan/Descriptors.cpp
        src/vulkan/Texture.cpp
        src/vulkan/VulkanContext.cpp
    )

    target_include_directories(KhepriEngineTests PRIVATE
        src
        ${glfw_SOURCE_DIR}/include
        ${vulkan_headers_SOURCE_DIR}/include
        ${vma_SOURCE_DIR}/include
        ${stb_SOURCE_DIR}
    )

    target_compile_definitions(KhepriEngineTests PRIVATE VK_NO_PROTOTYPES GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE)

    target_link_libraries(KhepriEngineTests PRIVATE
        gmock
        gtest_main
        glfw
        volk
        Vulkan-Headers
        glm::glm
    )

    if(MSVC)
        target_compile_options(KhepriEngineTests PRIVATE /W4 /utf-8 /FS)
    endif()

    include(GoogleTest)
    gtest_discover_tests(KhepriEngineTests)
endif()