cmake_minimum_required(VERSION 3.23)

include(cmake/Version.cmake)

project(CoreDeck VERSION ${PROJECT_VERSION})

string(TIMESTAMP CURRENT_YEAR "%Y")

set(APP_VENDOR "AbdulMuaz Aqeel")
set(APP_DESCRIPTION "A native control center for your Android development toolchain. Easily create virtual devices, manage system images, inspect APKs, and handle keystores in one fast, standalone desktop app.")
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

find_package(OpenGL REQUIRED)
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

add_executable(${PROJECT_NAME}
    ${IMGUI_SOURCES}
    src/main.cpp
    src/gui/windows/adb/adb.cpp
    src/gui/windows/adb/adb_session.cpp
    src/gui/windows/adb/adb_logcat.cpp
    src/gui/windows/adb/adb_shell.cpp
    src/gui/windows/adb/adb_packages.cpp
    src/gui/windows/adb/adb_connections.cpp
    src/gui/windows/adb/adb_devices.cpp
    src/gui/windows/adb/adb_wifi_help.cpp
    src/gui/windows/app/about.cpp
    src/gui/windows/app/health_check.cpp
    src/gui/windows/app/health_check_banner.cpp
    src/gui/windows/app/main_menu_bar.cpp
    src/gui/windows/app/onboarding.cpp
    src/gui/windows/app/update.cpp
    src/gui/windows/avd/accept_licenses.cpp
    src/gui/windows/avd/avd_info.cpp
    src/gui/windows/avd/avd_list.cpp
    src/gui/windows/avd/avd_logs.cpp
    src/gui/windows/avd/avd_options.cpp
    src/gui/windows/avd/create_avd.cpp
    src/gui/windows/avd/device_profile.cpp
    src/gui/windows/avd/install_image.cpp
    src/gui/windows/avd/skin.cpp
    src/gui/windows/avd/storage.cpp
    src/gui/windows/packages/apk_analyzer.cpp
    src/gui/windows/packages/apk_signer.cpp
    src/gui/windows/packages/jar_signer.cpp
    src/gui/windows/packages/keytool.cpp
    src/gui/windows/preferences/preferences.cpp
    src/gui/windows/preferences/preferences_android_sdk.cpp
    src/gui/windows/preferences/preferences_general.cpp
    src/gui/windows/preferences/preferences_jdk.cpp
    src/gui/application.cpp
    src/gui/theme.cpp
    src/gui/utilities.cpp
    src/gui/widgets/style.cpp
    src/gui/widgets/buttons.cpp
    src/gui/widgets/feedback.cpp
    src/gui/widgets/form.cpp
    src/gui/widgets/log_viewer.cpp
    src/gui/widgets/details.cpp
    src/gui/widgets/chrome.cpp
    src/gui/widgets/banner.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    coredeck_app
    coredeck_core
    glfw
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi)
endif()

if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE src/gui/theme_mac.mm)
    set_source_files_properties(src/gui/theme_mac.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
endif()

include(cmake/Locales.cmake)
include(cmake/Runtime.cmake)
include(cmake/Packaging.cmake)

option(COREDECK_BUILD_TESTS "Build CoreDeck unit tests" OFF)
if(COREDECK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
