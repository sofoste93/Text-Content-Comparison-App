cmake_minimum_required(VERSION 3.24)
project(TextOrbitCompare VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_library(text_orbit_core src/diff_engine.c)
target_include_directories(text_orbit_core PUBLIC src)
if(MSVC)
    target_compile_options(text_orbit_core PRIVATE /W4)
else()
    target_compile_options(text_orbit_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(APP_SOURCES
    src/main.c
    third_party/tinyfiledialogs/tinyfiledialogs.c
)
if(WIN32)
    list(APPEND APP_SOURCES packaging/windows/text-orbit.rc)
    add_executable(TextOrbitCompare WIN32 ${APP_SOURCES})
else()
    add_executable(TextOrbitCompare ${APP_SOURCES})
endif()
target_include_directories(TextOrbitCompare PRIVATE
    src
    third_party/raygui
    third_party/tinyfiledialogs
)
target_link_libraries(TextOrbitCompare PRIVATE text_orbit_core raylib)
if(MINGW)
    target_link_options(TextOrbitCompare PRIVATE -static-libgcc)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(diff_engine_test tests/diff_engine_test.c)
    target_link_libraries(diff_engine_test PRIVATE text_orbit_core)
    add_test(NAME diff_engine COMMAND diff_engine_test)
endif()

install(TARGETS TextOrbitCompare RUNTIME DESTINATION . BUNDLE DESTINATION .)
install(FILES README.md CHANGELOG.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)
