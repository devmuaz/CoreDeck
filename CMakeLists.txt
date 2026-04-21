cmake_minimum_required(VERSION 3.23)

find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_TAG_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_COUNT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (GIT_TAG_VERSION)
        string(REGEX REPLACE "^v" "" PROJECT_VERSION ${GIT_TAG_VERSION})
    else ()
        set(PROJECT_VERSION "0.0.1")
    endif ()

    if (GIT_COMMIT_COUNT)
        set(PROJECT_BUILD_NUMBER ${GIT_COMMIT_COUNT})
    else ()
        set(PROJECT_BUILD_NUMBER "1")
    endif ()
else ()
    set(PROJECT_VERSION "0.0.1")
    set(PROJECT_BUILD_NUMBER "1")
endif ()

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

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
endif ()

if (WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/glfw)

set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/reflect-cpp)

set(TINYFD_DIR ${CMAKE_SOURCE_DIR}/extern/tinyfiledialogs)
add_library(tinyfiledialogs STATIC ${TINYFD_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs PUBLIC ${TINYFD_DIR})

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
        ${IMGUI_SOURCES}
        src/main.cpp
        # Core
        src/core/app_settings.cpp
        src/core/avd.cpp
        src/core/emulator.cpp
        src/core/file_dialog.cpp
        src/core/log_buffer.cpp
        src/core/options.cpp
        src/core/paths.cpp
        src/core/process.cpp
        src/core/sdk.cpp
        src/core/system_image.cpp
        src/core/utilities.cpp
        src/core/version_check.cpp
        # Gui
        src/gui/windows/about.cpp
        src/gui/windows/avd_info.cpp
        src/gui/windows/avd_list.cpp
        src/gui/windows/avd_logs.cpp
        src/gui/windows/avd_options.cpp
        src/gui/windows/delete_avd.cpp
        src/gui/windows/create_avd.cpp
        src/gui/windows/install_image.cpp
        src/gui/windows/main_menu_bar.cpp
        src/gui/windows/onboarding.cpp
        src/gui/windows/preferences.cpp
        src/gui/windows/sdk_banner.cpp
        src/gui/windows/storage.cpp
        src/gui/windows/update.cpp
        src/gui/application.cpp
        src/gui/theme.cpp
        src/gui/widgets.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        OpenGL::GL
        reflectcpp
        tinyfiledialogs
)

if (APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets/fonts
            $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/assets/fonts
    )
else ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets/fonts
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/fonts
    )
endif ()

if (UNIX AND NOT APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/assets/icons/icon.png
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/icons/icon.png
    )
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        COREDECK_VERSION="${CMAKE_PROJECT_VERSION}"
        COREDECK_BUILD_NUMBER="${PROJECT_BUILD_NUMBER}"
        COREDECK_VENDOR="${APP_VENDOR}"
        COREDECK_DESCRIPTION="${APP_DESCRIPTION_SHORT}"
        COREDECK_YEAR="${CURRENT_YEAR}"
        COREDECK_WEBSITE="https://coredeck.dev"
        COREDECK_AUTHOR_WEBSITE="https://devmuaz.com"
        COREDECK_GITHUB="https://github.com/devmuaz/CoreDeck"
        COREDECK_GITHUB_API="https://api.github.com/repos/devmuaz/${PROJECT_NAME}/releases/latest"
        COREDECK_COPYRIGHT="${APP_COPYRIGHT}"
)

if (WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/resources.rc.in ${CMAKE_BINARY_DIR}/resources.rc @ONLY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 comdlg32 ole32)
    target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/resources.rc)
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_BUNDLE_ID}"
            MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_BUILD_NUMBER}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
            MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}"
            MACOSX_BUNDLE_INFO_STRING "${APP_DESCRIPTION_SHORT}"
            MACOSX_BUNDLE_COPYRIGHT "${APP_COPYRIGHT}"
    )
    if (EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.icns")
        set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_SOURCE_DIR}/assets/icons/icon.icns"
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/icon.icns"
        )
    endif ()
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
endif ()

if (WIN32)
    set(CPACK_GENERATOR "WIX;ZIP")
elseif (APPLE)
    set(CPACK_GENERATOR "TGZ")
else ()
    set(CPACK_GENERATOR "TGZ")
endif ()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${APP_VENDOR}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_DESCRIPTION_SHORT}")
set(CPACK_PACKAGE_DESCRIPTION "${APP_DESCRIPTION}")

install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION .
)

if (NOT APPLE)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/fonts DESTINATION assets)
endif ()

if (UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/assets/icons/icon.png DESTINATION assets/icons)
endif ()

if (WIN32)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_NAME}")
    set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}")
    set(CPACK_WIX_UPGRADE_GUID "B8F3E5D2-4A7C-4D2B-9E1F-6C8A5B3D7E9F")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "${PROJECT_NAME}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/devmuaz")
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/LICENSE.rtf")
    if (EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
    endif ()
endif ()

include(CPack)