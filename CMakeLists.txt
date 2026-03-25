cmake_minimum_required(VERSION 3.20)

project(mmd_player LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

find_package(OpenGL REQUIRED)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/glad)

add_library(thirdparty_glm INTERFACE)
target_include_directories(thirdparty_glm INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/glm
)

add_library(thirdparty_stb INTERFACE)
target_include_directories(thirdparty_stb INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/scene)
add_subdirectory(src/asset)
add_subdirectory(src/animation)
add_subdirectory(src/physics)
add_subdirectory(src/render)
add_subdirectory(src/app)