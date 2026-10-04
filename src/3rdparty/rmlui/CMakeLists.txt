if(NOT raylib_FOUND)
    return()
endif()

file(GLOB_RECURSE RMLUI_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.cpp)
file(GLOB RMLUI_SVG_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/SVG/*.cpp)

add_library(rmlui STATIC
    ${RMLUI_CORE_SOURCES}
    ${RMLUI_SVG_SOURCES}
    Backends/RmlUi_Renderer_GL3.cpp
)

target_compile_features(rmlui PUBLIC cxx_std_17)
target_compile_options(rmlui PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)
target_include_directories(rmlui SYSTEM PUBLIC Include Backends)
target_compile_definitions(rmlui
    PUBLIC RMLUI_STATIC_LIB
    PRIVATE RMLUI_FONT_ENGINE_FREETYPE RMLUI_SVG_PLUGIN
)
target_link_libraries(rmlui PRIVATE lunasvg)

# MinGW links everything statically, and FindFreetype leaves out FreeType's own dependencies.
if(MINGW)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE2 REQUIRED freetype2)
    target_include_directories(rmlui PRIVATE ${FREETYPE2_STATIC_INCLUDE_DIRS})
    target_link_libraries(rmlui PRIVATE ${FREETYPE2_STATIC_LDFLAGS})
else()
    find_package(Freetype REQUIRED)
    target_link_libraries(rmlui PRIVATE Freetype::Freetype)
endif()

target_link_libraries(openttd_lib rmlui)
