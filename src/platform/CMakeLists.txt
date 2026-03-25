add_library(platform STATIC
    window.cpp
    input.cpp
)

target_include_directories(platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform PUBLIC
    core
    glfw
)

target_compile_definitions(platform PUBLIC
    GLFW_INCLUDE_NONE
)

if(MSVC)
    target_compile_options(platform PRIVATE /W4)
else()
    target_compile_options(platform PRIVATE -Wall -Wextra -Wpedantic)
endif()