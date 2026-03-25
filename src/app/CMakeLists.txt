add_executable(mmd_player
    main.cpp
    application.cpp
    free_camera_controller.cpp
    mmd_diagnostics.cpp
)

target_include_directories(mmd_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mmd_player PRIVATE
    core
    platform
    scene
    asset
    animation
    physics
    render
)

add_custom_command(TARGET mmd_player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:mmd_player>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/shaders
            $<TARGET_FILE_DIR:mmd_player>/shaders

    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:mmd_player>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:mmd_player>/assets
)

if(MSVC)
    target_compile_options(mmd_player PRIVATE /W4)
else()
    target_compile_options(mmd_player PRIVATE -Wall -Wextra -Wpedantic)
endif()