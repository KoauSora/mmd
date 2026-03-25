add_library(render STATIC
    shader.cpp
    texture.cpp
    mesh.cpp
    skinned_mesh.cpp
    renderer.cpp
)

target_include_directories(render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(render PUBLIC
    core
    scene
    asset
    animation
    glad
    OpenGL::GL
    thirdparty_glm
    thirdparty_stb
)

target_compile_definitions(render PUBLIC
    GLFW_INCLUDE_NONE
)

if(MSVC)
    target_compile_options(render PRIVATE /W4)
else()
    target_compile_options(render PRIVATE -Wall -Wextra -Wpedantic)
endif()