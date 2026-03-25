add_library(animation STATIC
    skeleton.cpp
    animator.cpp
    ik_solver.cpp
    pose_physics.cpp
    skinning.cpp
)

target_include_directories(animation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(animation PUBLIC
    core
    thirdparty_glm
    asset
)

if(MSVC)
    target_compile_options(animation PRIVATE /W4)
else()
    target_compile_options(animation PRIVATE -Wall -Wextra -Wpedantic)
endif()