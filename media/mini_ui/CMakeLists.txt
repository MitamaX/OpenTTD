set(MINI_UI_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/floats.rcss
        ${CMAKE_CURRENT_SOURCE_DIR}/floats.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/hud.rcss
        ${CMAKE_CURRENT_SOURCE_DIR}/hud.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/abandon.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/aircraft.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/airport.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/arrow.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/axis.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/buoy.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/bus-stop.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/buy-land.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/canal.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/close.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/company.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/demolish.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/dock.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/fast.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/finances.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/goals.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/graph.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/groups.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/headquarters.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/industry.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/league.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/load.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/lock.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/map.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/music.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/news.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/options.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/pause.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/play.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/quit.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/rail-bridge.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/rail-convert.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/rail-tunnel.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/rail-waypoint.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/rail.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road-bridge.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road-convert.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road-depot.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road-tunnel.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road-vehicles.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road-waypoint.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/road.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/save.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/ship-depot.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/ships.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/sign.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/signal.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/station.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/subsidies.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/terraform.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/towns.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/train-depot.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/trains.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/trees.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/truck-stop.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/vehicle-shop.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/ledger.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/map.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/panel.rcss
        ${CMAKE_CURRENT_SOURCE_DIR}/panel.rml
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/composite.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/network.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/screen.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.vert
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
