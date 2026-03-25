add_library(scene STATIC
    camera.cpp
    scene.cpp
)

target_include_directories(scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(scene PUBLIC
    core
    thirdparty_glm
)

if(MSVC)
    target_compile_options(scene PRIVATE /W4)
else()
    target_compile_options(scene PRIVATE -Wall -Wextra -Wpedantic)
endif()