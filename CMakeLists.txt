cmake_minimum_required(VERSION 3.23)

include(cmake/Version.cmake)

project(CoreDeck VERSION ${PROJECT_VERSION})

string(TIMESTAMP CURRENT_YEAR "%Y")

set(APP_VENDOR "AbdulMuaz Aqeel")
string(CONCAT APP_DESCRIPTION
    "Engineered for fast, low-overhead workflows, CoreDeck brings a sleek desktop "
    "interface to official Android SDK tools. Create and launch emulators, "
    "connect and control hardware devices, inspect and sign application builds, "
    "configure environment health, and manage system resources without ever "
    "touching Android Studio. Powered natively by C++20 and Dear ImGui."
)
set(APP_DESCRIPTION_SHORT "Native Desktop Interface for Official Android Command-Line Tools")
set(APP_BUNDLE_ID "com.devmuaz.coredeck")
set(APP_COPYRIGHT "Copyright © ${CURRENT_YEAR} ${PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
endif()

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
endif()

include(cmake/Dependencies.cmake)

find_package(Threads REQUIRED)

if(NOT WIN32)
    find_package(CURL REQUIRED)
endif()

if(APPLE)
    foreach(_gettext_prefix /opt/homebrew/opt/gettext /usr/local/opt/gettext)
        if(EXISTS "${_gettext_prefix}")
            list(APPEND CMAKE_PREFIX_PATH "${_gettext_prefix}")
        endif()
    endforeach()
endif()
find_package(Intl REQUIRED)
set(_coredeck_msgfmt_hints
    /opt/homebrew/bin
    /opt/homebrew/opt/gettext/bin
    /usr/local/bin
    /usr/local/opt/gettext/bin
)
if(DEFINED ENV{COREDECK_GETTEXT_TOOLS})
    list(APPEND _coredeck_msgfmt_hints "$ENV{COREDECK_GETTEXT_TOOLS}")
endif()
if(DEFINED ENV{VCPKG_INSTALLATION_ROOT})
    list(APPEND _coredeck_msgfmt_hints
        "$ENV{VCPKG_INSTALLATION_ROOT}/installed/x64-windows/tools/gettext/bin"
    )
endif()
foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
    list(APPEND _coredeck_msgfmt_hints "${_prefix}/tools/gettext/bin" "${_prefix}/bin")
endforeach()
find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt HINTS ${_coredeck_msgfmt_hints} REQUIRED)

add_subdirectory(src/platform)
add_subdirectory(src/app)
add_subdirectory(src/core)
add_subdirectory(src/gui)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE coredeck_gui)

include(cmake/Locales.cmake)
include(cmake/Runtime.cmake)
include(cmake/Packaging.cmake)

option(COREDECK_BUILD_TESTS "Build CoreDeck unit tests" OFF)
if(COREDECK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
