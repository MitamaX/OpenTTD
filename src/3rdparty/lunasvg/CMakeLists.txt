enable_language(C)

file(GLOB PLUTOVG_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/plutovg/source/*.c)
file(GLOB LUNASVG_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

add_library(plutovg STATIC ${PLUTOVG_SOURCES})
set_target_properties(plutovg PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_compile_options(plutovg PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-w>)
target_include_directories(plutovg SYSTEM PUBLIC plutovg/include)
target_compile_definitions(plutovg
    PUBLIC PLUTOVG_BUILD_STATIC
    PRIVATE PLUTOVG_BUILD PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD
)

add_library(lunasvg STATIC ${LUNASVG_SOURCES})
target_compile_features(lunasvg PUBLIC cxx_std_17)
target_compile_options(lunasvg PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)
target_include_directories(lunasvg SYSTEM PUBLIC include)
target_compile_definitions(lunasvg
    PUBLIC LUNASVG_BUILD_STATIC
    PRIVATE LUNASVG_BUILD LUNASVG_DISABLE_LOAD_SYSTEM_FONTS
)
target_link_libraries(lunasvg PRIVATE plutovg)
