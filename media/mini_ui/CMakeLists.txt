set(MINI_UI_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/ledger.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/panel.rcss
        ${CMAKE_CURRENT_SOURCE_DIR}/panel.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/theme.rcss
)

foreach(MINI_UI_SOURCE_FILE IN LISTS MINI_UI_SOURCE_FILES)
    get_filename_component(MINI_UI_FILE_NAME "${MINI_UI_SOURCE_FILE}" NAME)
    set(MINI_UI_BINARY_FILE "${CMAKE_BINARY_DIR}/mini_ui/${MINI_UI_FILE_NAME}")

    add_custom_command_timestamp(OUTPUT ${MINI_UI_BINARY_FILE}
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
