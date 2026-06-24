# Spry — a retained-mode, animation-first UI toolkit for Cleat. Epic #205.
#
# Decoupling contract: depends ONLY on SDL3 (FreeType+HarfBuzz for text later).
# NEVER on Cleat. Develops in-repo under libs/ now; splits to its own public repo
# later via git subtree (#224). A CI guard enforcing "no Cleat includes" lands
# with #206.
cmake_minimum_required(VERSION 3.20)
project(spry CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SPRY_BUILD_DEMO "Build the spry demo/gallery" ON)

# The GL backend (#208) calls GL 1.1 directly; modern entry points are loaded at
# runtime via SDL_GL_GetProcAddress.
find_package(OpenGL REQUIRED)

# SDL3: fetched when building standalone; reused when a parent already provides it.
if(NOT TARGET SDL3::SDL3)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# FreeType for real text rasterization (#212). HarfBuzz (shaping) comes later.
if(NOT TARGET freetype)
    include(FetchContent)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG VER-2-14-1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(freetype)
endif()

# HarfBuzz for text shaping (#212): ligatures, complex scripts, proportional
# advances. Uses FreeType (which we built with HB disabled to break the cycle).
if(NOT TARGET harfbuzz)
    include(FetchContent)
    set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
    set(HB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(harfbuzz
        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
        GIT_TAG 10.4.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(harfbuzz)
    set(_spry_built_harfbuzz TRUE)
endif()

# HarfBuzz's heavy templates leave undefined refs to inlinable iterator/lambda
# methods when compiled at -O0 (Debug) on MinGW. Force optimization on just these
# targets so the link resolves — but only when WE built harfbuzz (standalone). When
# a parent (Cleat) provides it, leave its build settings alone.
if(_spry_built_harfbuzz)
    foreach(_hb harfbuzz harfbuzz-subset)
        if(TARGET ${_hb})
            target_compile_options(${_hb} PRIVATE -O2)
        endif()
    endforeach()
endif()

add_library(spry STATIC
    src/geometry.cpp
    src/sdl_renderer.cpp
    src/gl_renderer.cpp
    src/widget.cpp
    src/box.cpp
    src/theme.cpp
    src/clipboard.cpp
    src/text_edit.cpp
    src/textfield.cpp)
target_include_directories(spry PUBLIC include)
# harfbuzz-subset: in a Debug build the core harfbuzz lib leaves undefined refs to
# subset symbols that live in this companion lib, so link both.
target_link_libraries(spry PUBLIC SDL3::SDL3 freetype harfbuzz harfbuzz-subset OpenGL::GL)

if(SPRY_BUILD_DEMO)
    # When consumed as a subproject (Cleat), the demos are configured but kept out
    # of the default `all` build via EXCLUDE_FROM_ALL — an IDE still lists/runs
    # them, and `cmake --build ... --target spry_gl_demo` builds them on demand.
    set(_spry_demo_excl "")
    if(SPRY_DEMO_EXCLUDE_FROM_ALL)
        set(_spry_demo_excl EXCLUDE_FROM_ALL)
    endif()

    add_executable(spry_demo ${_spry_demo_excl} examples/demo.cpp)
    target_link_libraries(spry_demo PRIVATE spry)
    if(MINGW)
        # Self-contained exe (see #204 — avoids system libstdc++-6.dll mismatch).
        target_link_options(spry_demo PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    # Theme files load relative to the exe (SDL_GetBasePath) — copy them next to it.
    add_custom_command(TARGET spry_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/themes
            $<TARGET_FILE_DIR:spry_demo>/themes)

    # Same scene, rendered through the OpenGL backend (#208) — the path to Cleat.
    add_executable(spry_gl_demo ${_spry_demo_excl} examples/gl_demo.cpp)
    target_link_libraries(spry_gl_demo PRIVATE spry)
    if(MINGW)
        target_link_options(spry_gl_demo PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_custom_command(TARGET spry_gl_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/themes
            $<TARGET_FILE_DIR:spry_gl_demo>/themes)
endif()

# Headless unit tests for the input/focus/keyboard logic (#216). Defaults on when
# Spry is the top-level project; Cleat drives it via CLEAT_BUILD_TESTS so the same
# ctest run covers Spry. Reuses a Catch2 target if the parent already fetched one.
option(SPRY_BUILD_TESTS "Build Spry unit tests" ${PROJECT_IS_TOP_LEVEL})
if(SPRY_BUILD_TESTS)
    if(NOT TARGET Catch2::Catch2WithMain)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.7.1
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(Catch2)
    endif()
    enable_testing()
    add_executable(spry_tests tests/test_input.cpp tests/test_text_edit.cpp tests/test_widgets.cpp)
    target_link_libraries(spry_tests PRIVATE spry Catch2::Catch2WithMain)
    if(MINGW)
        target_link_options(spry_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME spry_tests COMMAND spry_tests)
endif()
