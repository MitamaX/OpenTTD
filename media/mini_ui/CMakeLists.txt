set(MINI_UI_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/hud.rcss
        ${CMAKE_CURRENT_SOURCE_DIR}/hud.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/close.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/fast.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/pause.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/play.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/ledger.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/panel.rcss
        ${CMAKE_CURRENT_SOURCE_DIR}/panel.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/theme.rcss
)

foreach(MINI_UI_SOURCE_FILE IN LISTS MINI_UI_SOURCE_FILES)
    file(RELATIVE_PATH MINI_UI_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}" "${MINI_UI_SOURCE_FILE}")
    set(MINI_UI_BINARY_FILE "${CMAKE_BINARY_DIR}/mini_ui/${MINI_UI_FILE_NAME}")
    get_filename_component(MINI_UI_BINARY_DIR "${MINI_UI_BINARY_FILE}" DIRECTORY)

    add_custom_command_timestamp(OUTPUT ${MINI_UI_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MINI_UI_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${MINI_UI_SOURCE_FILE}
                    ${MINI_UI_BINARY_FILE}
            MAIN_DEPENDENCY ${MINI_UI_SOURCE_FILE}
            COMMENT "Copying ${MINI_UI_FILE_NAME} mini UI file"
    )

    list(APPEND MINI_UI_BINARY_FILES ${MINI_UI_BINARY_FILE})
endforeach()

add_custom_target_timestamp(mini_ui_files
        DEPENDS
        ${MINI_UI_BINARY_FILES}
)
