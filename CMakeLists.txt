cmake_minimum_required(VERSION 3.23)

include(cmake/Version.cmake)

project(CoreDeck VERSION ${PROJECT_VERSION})

string(TIMESTAMP CURRENT_YEAR "%Y")

set(APP_VENDOR "AbdulMuaz Aqeel")
set(APP_DESCRIPTION "A native desktop application around your Android SDK’s official emulator, avdmanager, and sdkmanager binaries. Use it for everyday work without opening Android Studio. Built with C++20 and Dear ImGui.")
set(APP_DESCRIPTION_SHORT "Official tools, polished workflow — in a friendly GUI.")
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
    src/gui/windows/about.cpp
    src/gui/windows/apk_analyzer.cpp
    src/gui/windows/apk_signer.cpp
    src/gui/windows/jar_signer.cpp
    src/gui/windows/keytool.cpp
    src/gui/windows/avd_info.cpp
    src/gui/windows/avd_list.cpp
    src/gui/windows/avd_logs.cpp
    src/gui/windows/avd_options.cpp
    src/gui/windows/delete_avd.cpp
    src/gui/windows/create_avd.cpp
    src/gui/windows/device_profile.cpp
    src/gui/windows/accept_licenses.cpp
    src/gui/windows/health_check.cpp
    src/gui/windows/install_image.cpp
    src/gui/windows/main_menu_bar.cpp
    src/gui/windows/onboarding.cpp
    src/gui/windows/preferences.cpp
    src/gui/windows/preferences_android_sdk.cpp
    src/gui/windows/preferences_general.cpp
    src/gui/windows/preferences_jdk.cpp
    src/gui/windows/health_check_banner.cpp
    src/gui/windows/skin.cpp
    src/gui/windows/storage.cpp
    src/gui/windows/update.cpp
    src/gui/application.cpp
    src/gui/theme.cpp
    src/gui/utilities.cpp
    src/gui/widgets/style.cpp
    src/gui/widgets/buttons.cpp
    src/gui/widgets/feedback.cpp
    src/gui/widgets/form.cpp
    src/gui/widgets/report.cpp
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
