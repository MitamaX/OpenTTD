if(NOT raylib_FOUND)
    return()
endif()

add_executable(mini_ui_preview
    json_model.cpp
    json_model.h
    preview_host.cpp
    preview_host.h
    preview_main.cpp
    preview_scene.cpp
    preview_scene.h
    scene_watch.cpp
    scene_watch.h
)

target_link_libraries(mini_ui_preview PRIVATE rmlui raylib)
target_compile_definitions(mini_ui_preview PRIVATE
    MEDIA_DIR="${CMAKE_SOURCE_DIR}/media"
    PREVIEW_SCENE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenes"
)
set_target_properties(mini_ui_preview PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
