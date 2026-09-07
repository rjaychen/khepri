cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(KhepriEngine VERSION 0.1.9 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Visual Studio Solution Explorer configuration
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT KhepriEngine)

# Configure FetchContent
include(FetchContent)

# -------------------------------------------------------
# Vulkan SDK Configuration (Path Variable & Includes/Libs)
# -------------------------------------------------------
set(VULKAN_SDK_PATH "$ENV{VULKAN_SDK}" CACHE PATH "Path to Vulkan SDK installation directory (e.g., C:/VulkanSDK/1.4.357.0)")
if(VULKAN_SDK_PATH)
    set(Vulkan_ROOT "${VULKAN_SDK_PATH}")
    set(ENV{VULKAN_SDK} "${VULKAN_SDK_PATH}")
endif()

find_package(Vulkan REQUIRED)
message(STATUS "Found Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Found Vulkan Library: ${Vulkan_LIBRARIES}")

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

# 3. Volk (Meta-loader for Vulkan)
message(STATUS "Fetching volk...")
FetchContent_Declare(
    volk
    URL https://github.com/zeux/volk/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(volk)

# 4. VulkanMemoryAllocator (VMA)
message(STATUS "Fetching VulkanMemoryAllocator...")
FetchContent_Declare(
    vma
    URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.1.0.zip
)
FetchContent_MakeAvailable(vma)

# 5. Dear ImGui
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
    target_link_libraries(imgui PUBLIC glfw volk Vulkan::Vulkan)
    target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_USE_VOLK)
endif()

# 6. stb (Image Loader)
message(STATUS "Fetching stb...")
FetchContent_Declare(
    stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(stb)

# -------------------------------------------------------
# Shader Definitions & Compilation
# -------------------------------------------------------
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_SOURCES
    ${SHADER_SOURCE_DIR}/mesh.vert
    ${SHADER_SOURCE_DIR}/mesh.frag
    ${SHADER_SOURCE_DIR}/deform_twist.comp
    ${SHADER_SOURCE_DIR}/deform_wave.comp
)

find_program(GLSLC glslc HINTS
    "${VULKAN_SDK_PATH}/Bin"
    "$ENV{VULKAN_SDK}/Bin"
    "${Vulkan_GLSLC_EXECUTABLE}"
)

if(GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

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

    add_custom_target(Shaders ALL DEPENDS ${SPV_OUTPUTS} SOURCES ${SHADER_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SHADER_SOURCES})
    message(STATUS "Shader auto-compilation and post-build copying enabled")
else()
    message(WARNING "glslc not found - shaders will NOT be compiled automatically. Install Vulkan SDK.")
endif()

# -------------------------------------------------------
# 1. KhepriVulkan (Modular Static Library - Vulkan Backend)
# -------------------------------------------------------
file(GLOB_RECURSE VULKAN_SOURCES
    "src/vulkan/*.cpp"
    "src/vulkan/*.h"
    "src/vulkan/*.hpp"
)

add_library(KhepriVulkan STATIC ${VULKAN_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${VULKAN_SOURCES})

target_include_directories(KhepriVulkan PUBLIC 
    src
    ${vma_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_compile_definitions(KhepriVulkan PUBLIC 
    VK_NO_PROTOTYPES 
    GLM_FORCE_RADIANS 
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_link_libraries(KhepriVulkan PUBLIC 
    glfw 
    volk 
    Vulkan::Vulkan 
    glm::glm
)

if(MSVC)
    target_compile_options(KhepriVulkan PRIVATE /W4 /utf-8 /FS)
endif()

# -------------------------------------------------------
# 2. KhepriCore (Modular Static Library - Mesh, Graph, Scene, Assets)
# -------------------------------------------------------
file(GLOB_RECURSE CORE_SOURCES
    "src/core/*.cpp"
    "src/core/*.h"
    "src/core/*.hpp"
    "src/mesh/*.cpp"
    "src/mesh/*.h"
    "src/mesh/*.hpp"
    "src/graph/*.cpp"
    "src/graph/*.h"
    "src/graph/*.hpp"
    "src/scene/*.cpp"
    "src/scene/*.h"
    "src/scene/*.hpp"
    "src/animation/*.cpp"
    "src/animation/*.h"
    "src/animation/*.hpp"
    "src/assets/*.cpp"
    "src/assets/*.h"
    "src/assets/*.hpp"
    "src/material/*.cpp"
    "src/material/*.h"
    "src/material/*.hpp"
)

list(APPEND CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mesh/GLTFImporter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mesh/GLTFImporter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/UndoStack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/UndoStack.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Command.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scene/TransformCommand.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scene/SceneHierarchyCommand.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graph/NodeGraphCommand.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/animation/KeyframeCommand.h"
)
list(REMOVE_DUPLICATES CORE_SOURCES)

add_library(KhepriCore STATIC ${CORE_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CORE_SOURCES})

target_include_directories(KhepriCore PUBLIC 
    src
    ${stb_SOURCE_DIR}
)

target_compile_definitions(KhepriCore PUBLIC 
    VK_NO_PROTOTYPES 
    GLM_FORCE_RADIANS 
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_link_libraries(KhepriCore PUBLIC 
    KhepriVulkan
    glfw 
    glm::glm
)

if(MSVC)
    target_compile_options(KhepriCore PRIVATE /W4 /utf-8 /FS)
endif()

# -------------------------------------------------------
# 3. KhepriEngine (Executable - Main App & Editor)
# -------------------------------------------------------
file(GLOB_RECURSE EDITOR_SOURCES
    "src/editor/*.cpp"
    "src/editor/*.h"
    "src/editor/*.hpp"
)

list(APPEND EDITOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AssetManagerPanel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AssetManagerPanel.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/OracleBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/OracleBridge.h"
)
list(REMOVE_DUPLICATES EDITOR_SOURCES)

add_executable(KhepriEngine ${EDITOR_SOURCES} ${SHADER_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${EDITOR_SOURCES} ${SHADER_SOURCES})

target_include_directories(KhepriEngine PRIVATE src)

target_link_libraries(KhepriEngine PRIVATE 
    KhepriCore
    KhepriVulkan
    imgui
    glfw
    volk
    Vulkan::Vulkan
    glm::glm
)

if(MSVC)
    target_compile_options(KhepriEngine PRIVATE /W4 /utf-8 /FS)
endif()

if(TARGET Shaders)
    add_dependencies(KhepriEngine Shaders)
    add_custom_command(TARGET KhepriEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SHADER_OUTPUT_DIR}"
            "$<TARGET_FILE_DIR:KhepriEngine>/shaders/compiled"
        COMMENT "Copying compiled shaders to output executable directory"
    )
endif()

add_custom_command(TARGET KhepriEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:KhepriEngine>/assets"
    COMMENT "Copying assets to output executable directory"
)

# -------------------------------------------------------
# 4. Unit Testing Framework (GoogleTest)
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

    file(GLOB_RECURSE TEST_FILES
        "tests/*.cpp"
        "tests/*.h"
        "tests/*.hpp"
    )

    # Editor sources needed for UI and Gizmo/ViewCube tests
    set(TEST_ADDITIONAL_SOURCES
        src/editor/Theme.cpp
        src/editor/Theme.h
        src/editor/UIWidgets.cpp
        src/editor/UIWidgets.h
        src/editor/ThumbnailCache.cpp
        src/editor/ThumbnailCache.h
        src/editor/NodeGraphEditorPanel.cpp
        src/editor/NodeGraphEditorPanel.h
        src/editor/TransformGizmo.cpp
        src/editor/TransformGizmo.h
        src/editor/ViewCube.cpp
        src/editor/ViewCube.h
        src/editor/LightVisualizer.cpp
        src/editor/LightVisualizer.h
        src/editor/VectorIcons.cpp
        src/editor/VectorIcons.h
        src/editor/SceneTreePanel.cpp
        src/editor/SceneTreePanel.h
        src/editor/AssetManagerPanel.cpp
        src/editor/AssetManagerPanel.h
    )

    add_executable(KhepriEngineTests
        ${TEST_FILES}
        ${TEST_ADDITIONAL_SOURCES}
    )

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TEST_FILES} ${TEST_ADDITIONAL_SOURCES})

    target_include_directories(KhepriEngineTests PRIVATE
        src
        ${glfw_SOURCE_DIR}/include
    )

    target_link_libraries(KhepriEngineTests PRIVATE
        KhepriCore
        KhepriVulkan
        imgui
        gmock
        gtest_main
        glfw
        volk
        Vulkan::Vulkan
        glm::glm
    )

    if(MSVC)
        target_compile_options(KhepriEngineTests PRIVATE /W4 /utf-8 /FS)
    endif()

    add_custom_command(TARGET KhepriEngineTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:KhepriEngineTests>/assets"
        COMMENT "Copying assets to test executable directory"
    )

    include(GoogleTest)
    gtest_discover_tests(KhepriEngineTests)
endif()

# -------------------------------------------------------
# Solution Folders & Visual Studio Organization
# -------------------------------------------------------
set_target_properties(KhepriEngine PROPERTIES FOLDER "Engine")
set_target_properties(KhepriCore PROPERTIES FOLDER "Engine")
set_target_properties(KhepriVulkan PROPERTIES FOLDER "Engine")

if(TARGET Shaders)
    set_target_properties(Shaders PROPERTIES FOLDER "Engine")
endif()
if(TARGET KhepriEngineTests)
    set_target_properties(KhepriEngineTests PROPERTIES FOLDER "Tests")
endif()

foreach(DEP_TARGET glfw glm imgui volk gtest gtest_main gmock gmock_main)
    if(TARGET ${DEP_TARGET})
        set_target_properties(${DEP_TARGET} PROPERTIES FOLDER "Dependencies")
    endif()
endforeach()