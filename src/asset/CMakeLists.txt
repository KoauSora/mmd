add_library(asset STATIC
    pmx_loader.cpp
    vmd_loader.cpp
)

target_include_directories(asset PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(asset PUBLIC
    core
    thirdparty_glm
)

if(MSVC)
    target_compile_options(asset PRIVATE /W4)
else()
    target_compile_options(asset PRIVATE -Wall -Wextra -Wpedantic)
endif()