add_library(physics STATIC
    pbd_world.cpp
    pmx_physics_builder.cpp
)

target_include_directories(physics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(physics PUBLIC
    core
    thirdparty_glm
    asset
    animation
)

if(MSVC)
    target_compile_options(physics PRIVATE /W4)
else()
    target_compile_options(physics PRIVATE -Wall -Wextra -Wpedantic)
endif()

