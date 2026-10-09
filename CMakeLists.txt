cmake_minimum_required(VERSION 3.23)

set(COREDECK_VERSION_OVERRIDE "" CACHE STRING "Explicit version override (e.g., set by CI from a release tag)")

find_package(Git QUIET)
if(GIT_FOUND)
    if(COREDECK_VERSION_OVERRIDE)
        set(GIT_TAG_VERSION "${COREDECK_VERSION_OVERRIDE}")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_TAG_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_COUNT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(GIT_TAG_VERSION)
        string(REGEX REPLACE "^v" "" GIT_TAG_STRIPPED ${GIT_TAG_VERSION})
        string(REGEX REPLACE "-.*$" "" PROJECT_VERSION ${GIT_TAG_STRIPPED})
        set(PROJECT_VERSION_FULL ${GIT_TAG_STRIPPED})
    else()
        set(PROJECT_VERSION "0.0.1")
        set(PROJECT_VERSION_FULL "0.0.1")
    endif()

    if(GIT_COMMIT_COUNT)
        set(PROJECT_BUILD_NUMBER ${GIT_COMMIT_COUNT})
    else()
        set(PROJECT_BUILD_NUMBER "1")
    endif()
else()
    if(COREDECK_VERSION_OVERRIDE)
        string(REGEX REPLACE "^v" "" GIT_TAG_STRIPPED "${COREDECK_VERSION_OVERRIDE}")
        string(REGEX REPLACE "-.*$" "" PROJECT_VERSION "${GIT_TAG_STRIPPED}")
        set(PROJECT_VERSION_FULL "${GIT_TAG_STRIPPED}")
    else()
        set(PROJECT_VERSION "0.0.1")
        set(PROJECT_VERSION_FULL "0.0.1")
    endif()
    set(PROJECT_BUILD_NUMBER "1")
endif()

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
if(MINGW)
    target_compile_options(reflectcpp PRIVATE
        -Wa,-mbig-obj
        $<$<CONFIG:Debug>:-O1>
        $<$<CONFIG:Debug>:-g0>
    )
endif()

set(TINYFD_DIR ${CMAKE_SOURCE_DIR}/extern/tinyfiledialogs)
add_library(tinyfiledialogs STATIC ${TINYFD_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs PUBLIC ${TINYFD_DIR})

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_HEADER_ONLY OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/miniz)

set(COREDECK_SENTRY_DSN "$ENV{COREDECK_SENTRY_DSN}")
set(COREDECK_SENTRY_ENABLED OFF)
if(COREDECK_SENTRY_DSN)
    if(CMAKE_CONFIGURATION_TYPES)
        set(COREDECK_SENTRY_ENABLED ON)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(COREDECK_SENTRY_ENABLED ON)
    endif()
endif()

if(COREDECK_SENTRY_ENABLED)
    set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SENTRY_BACKEND "crashpad" CACHE STRING "" FORCE)
    add_subdirectory(extern/sentry-native)
    message(STATUS "Sentry: Enabled (Release only)")
elseif(COREDECK_SENTRY_DSN)
    message(STATUS "Sentry: Disabled (non-Release build)")
else()
    message(STATUS "Sentry: Disabled")
endif()

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

add_library(coredeck_core STATIC
    src/core/app_settings.cpp
    src/core/apk_analyzer.cpp
    src/core/archive.cpp
    src/core/avd_manager.cpp
    src/core/crash_reporter.cpp
    src/core/emulator.cpp
    src/core/emulator_console.cpp
    src/core/file_dialog.cpp
    src/core/health_check.cpp
    src/core/http_download.cpp
    src/core/i18n.cpp
    src/core/jdk.cpp
    src/core/log_buffer.cpp
    src/core/log_filter.cpp
    src/core/options.cpp
    src/core/paths.cpp
    src/core/process.cpp
    src/core/process_stats.cpp
    src/core/sdk.cpp
    src/core/sdk_bootstrap.cpp
    src/core/sdk_manager.cpp
    src/core/sha256.cpp
    src/core/skin.cpp
    src/core/system_image.cpp
    src/core/utilities.cpp
    src/core/version_check.cpp
)

target_include_directories(coredeck_core PUBLIC src)
target_precompile_headers(coredeck_core PRIVATE src/pch.h)
target_link_libraries(coredeck_core PUBLIC reflectcpp tinyfiledialogs miniz Intl::Intl)

target_compile_definitions(coredeck_core PUBLIC
    COREDECK_TITLE="CoreDeck"
    COREDECK_VERSION="${PROJECT_VERSION_FULL}"
    COREDECK_BUILD_NUMBER="${PROJECT_BUILD_NUMBER}"
    COREDECK_VENDOR="${APP_VENDOR}"
    COREDECK_DESCRIPTION="${APP_DESCRIPTION_SHORT}"
    COREDECK_YEAR="${CURRENT_YEAR}"
    COREDECK_COPYRIGHT="${APP_COPYRIGHT}"
)

if(COREDECK_SENTRY_ENABLED)
    target_compile_definitions(coredeck_core PUBLIC
        $<$<CONFIG:Release>:COREDECK_SENTRY_DSN="${COREDECK_SENTRY_DSN}">
    )
    target_link_libraries(coredeck_core PUBLIC $<$<CONFIG:Release>:sentry>)
endif()

if(WIN32)
    target_compile_definitions(coredeck_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(coredeck_core PUBLIC shell32 comdlg32 ole32 winhttp ws2_32)
elseif(APPLE)
    target_link_libraries(coredeck_core PUBLIC CURL::libcurl "-framework CoreFoundation")
elseif(UNIX)
    target_link_libraries(coredeck_core PUBLIC ${CMAKE_DL_LIBS} Threads::Threads CURL::libcurl)
else()
    target_link_libraries(coredeck_core PUBLIC CURL::libcurl)
endif()

add_executable(${PROJECT_NAME}
    ${IMGUI_SOURCES}
    src/main.cpp
    src/gui/windows/about.cpp
    src/gui/windows/apk_analyzer.cpp
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
    src/gui/widgets.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
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

set(COREDECK_LOCALE_DIR "${CMAKE_BINARY_DIR}/locales")
file(MAKE_DIRECTORY "${COREDECK_LOCALE_DIR}")
file(GLOB COREDECK_PO_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/locales/*.po")
set(COREDECK_MO_FILES)
foreach(_po_file IN LISTS COREDECK_PO_FILES)
    get_filename_component(_tag "${_po_file}" NAME_WE)
    set(_mo_file "${COREDECK_LOCALE_DIR}/${_tag}/LC_MESSAGES/coredeck.mo")
    add_custom_command(
        OUTPUT "${_mo_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${COREDECK_LOCALE_DIR}/${_tag}/LC_MESSAGES"
        COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" -c -o "${_mo_file}" "${_po_file}"
        DEPENDS "${_po_file}"
        VERBATIM
    )
    list(APPEND COREDECK_MO_FILES "${_mo_file}")
endforeach()
add_custom_target(coredeck_locales ALL DEPENDS ${COREDECK_MO_FILES})
add_dependencies(${PROJECT_NAME} coredeck_locales)

if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/fonts
        $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/assets/fonts
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${COREDECK_LOCALE_DIR}
        $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/assets/locales
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/fonts
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/fonts
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${COREDECK_LOCALE_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/locales
    )
endif()

if(APPLE AND NOT Intl_IS_BUILT_IN)
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY INSTALL_RPATH "@executable_path/../Frameworks")
    get_filename_component(_intl_real "${Intl_LIBRARY}" REALPATH)
    execute_process(
        COMMAND otool -D "${_intl_real}"
        OUTPUT_VARIABLE _intl_otool
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX REPLACE ".*\n" "" COREDECK_INTL_INSTALL_NAME "${_intl_otool}")
    string(STRIP "${COREDECK_INTL_INSTALL_NAME}" COREDECK_INTL_INSTALL_NAME)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy "${_intl_real}" "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks/libintl.8.dylib"
        COMMAND install_name_tool -id "@rpath/libintl.8.dylib" "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks/libintl.8.dylib"
        COMMAND install_name_tool -change "${COREDECK_INTL_INSTALL_NAME}" "@rpath/libintl.8.dylib" "$<TARGET_FILE:${PROJECT_NAME}>"
        VERBATIM
    )
endif()

if(WIN32 AND NOT Intl_IS_BUILT_IN AND Intl_LIBRARY)
    # FindIntl may return a single path or an optimized/debug list. The runtime
    # DLLs live in the sibling bin directory of the import library, not in lib/.
    set(_intl_search_libs)
    foreach(_intl_item IN ITEMS ${Intl_LIBRARY} ${Intl_LIBRARY_RELEASE} ${Intl_LIBRARY_DEBUG})
        if(_intl_item STREQUAL "optimized" OR _intl_item STREQUAL "debug" OR _intl_item STREQUAL "general")
            continue()
        endif()
        if(EXISTS "${_intl_item}")
            list(APPEND _intl_search_libs "${_intl_item}")
        endif()
    endforeach()

    set(_intl_release_bins)
    set(_intl_debug_bins)
    foreach(_intl_lib IN LISTS _intl_search_libs)
        cmake_path(GET _intl_lib PARENT_PATH _intl_lib_dir)
        cmake_path(GET _intl_lib_dir PARENT_PATH _intl_prefix)
        set(_intl_bin "${_intl_prefix}/bin")
        if(NOT EXISTS "${_intl_bin}")
            continue()
        endif()
        cmake_path(NORMAL_PATH _intl_bin)
        if(_intl_bin MATCHES "[/\\\\]debug[/\\\\]bin$")
            list(APPEND _intl_debug_bins "${_intl_bin}")
        else()
            list(APPEND _intl_release_bins "${_intl_bin}")
        endif()
    endforeach()

    set(_intl_bins "${_intl_release_bins}")
    if(NOT _intl_bins)
        set(_intl_bins "${_intl_debug_bins}")
    endif()
    if(_intl_bins)
        list(REMOVE_DUPLICATES _intl_bins)
    endif()

    set(_intl_runtime_dlls)
    foreach(_intl_bin IN LISTS _intl_bins)
        file(GLOB _intl_found
            "${_intl_bin}/*intl*.dll"
            "${_intl_bin}/*iconv*.dll"
            "${_intl_bin}/*charset*.dll"
        )
        list(APPEND _intl_runtime_dlls ${_intl_found})
    endforeach()
    if(_intl_runtime_dlls)
        list(REMOVE_DUPLICATES _intl_runtime_dlls)
    endif()

    set(_intl_has_runtime FALSE)
    foreach(_dll IN LISTS _intl_runtime_dlls)
        cmake_path(GET _dll FILENAME _intl_dll_name)
        if(_intl_dll_name MATCHES "[Ii]ntl")
            set(_intl_has_runtime TRUE)
        endif()
    endforeach()
    if(NOT _intl_has_runtime)
        message(FATAL_ERROR
            "CoreDeck links shared libintl (${Intl_LIBRARY}) but no intl runtime DLL "
            "was found beside it. The Windows package would fail at startup because "
            "intl-8.dll was not found. Install shared gettext and libiconv "
            "(vcpkg triplet x64-windows).")
    endif()
    message(STATUS "Windows libintl runtime: ${_intl_runtime_dlls}")

    foreach(_dll IN LISTS _intl_runtime_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            VERBATIM
        )
    endforeach()
    set(COREDECK_INTL_RUNTIME_DLLS ${_intl_runtime_dlls})
endif()

if(UNIX AND NOT APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/assets/icons/icon.png
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/icons/icon.png
    )
endif()

if(COREDECK_SENTRY_ENABLED)
    if(APPLE)
        target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-no_warn_duplicate_libraries")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:crashpad_handler>
            $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/MacOS/crashpad_handler
        )
    else()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:crashpad_handler>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:crashpad_handler>
        )
    endif()
endif()

if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/resources.rc.in ${CMAKE_BINARY_DIR}/resources.rc @ONLY)
    if(MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
    elseif(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE "-mwindows")
    endif()
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/resources.rc)
elseif(APPLE)
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
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.icns")
        set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/assets/icons/icon.icns"
            "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/icon.icns"
        )
    endif()
endif()

if(WIN32)
    set(CPACK_GENERATOR "WIX;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/dmg-setup.applescript")

    # DMG background image
    set(_dmg_bg_1x "${CMAKE_SOURCE_DIR}/media/branding/dmg-background.png")
    set(_dmg_bg_2x "${CMAKE_SOURCE_DIR}/media/branding/dmg-background@2x.png")
    if(EXISTS "${_dmg_bg_1x}" AND EXISTS "${_dmg_bg_2x}")
        set(_dmg_bg_tiff "${CMAKE_BINARY_DIR}/dmg-background.tiff")
        execute_process(
            COMMAND tiffutil -cathidpicheck "${_dmg_bg_1x}" "${_dmg_bg_2x}" -out "${_dmg_bg_tiff}"
            RESULT_VARIABLE _tiffutil_rc
            OUTPUT_QUIET ERROR_QUIET
        )
        if(_tiffutil_rc EQUAL 0 AND EXISTS "${_dmg_bg_tiff}")
            set(CPACK_DMG_BACKGROUND_IMAGE "${_dmg_bg_tiff}")
            message(STATUS "DMG background: multi-resolution TIFF (1x + @2x)")
        else()
            set(CPACK_DMG_BACKGROUND_IMAGE "${_dmg_bg_1x}")
            message(WARNING "tiffutil failed; falling back to 1x PNG only")
        endif()
    elseif(EXISTS "${_dmg_bg_1x}")
        set(CPACK_DMG_BACKGROUND_IMAGE "${_dmg_bg_1x}")
        message(STATUS "DMG background: 1x PNG only (add dmg-background@2x.png for Retina)")
    endif()

    # The DMG should only contain CoreDeck.app + an Applications symlink.
    # Sentry-native's bundled crashpad has unconditional install rules that
    # drop crashpad_handler (and headers/libs) at the install root. The
    # handler is already embedded inside the bundle via the POST_BUILD copy
    # above; the stray top-level copy is unsigned and breaks notarization.
    # Strip the unwanted directories from the CPack staging dir.
    file(WRITE "${CMAKE_BINARY_DIR}/cpack_strip_macos_extras.cmake" [=[
        foreach(_dir bin include lib lib64 share)
            if (EXISTS "${CPACK_TEMPORARY_DIRECTORY}/${_dir}")
                file(REMOVE_RECURSE "${CPACK_TEMPORARY_DIRECTORY}/${_dir}")
            endif ()
        endforeach()
    ]=])
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/cpack_strip_macos_extras.cmake")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${APP_VENDOR}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_DESCRIPTION_SHORT}")
set(CPACK_PACKAGE_DESCRIPTION "${APP_DESCRIPTION}")

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

if(COREDECK_SENTRY_ENABLED AND NOT APPLE)
    install(PROGRAMS $<TARGET_FILE:crashpad_handler> DESTINATION .)
endif()

# CPack packages install() rules only. Copying these DLLs next to the build
# output is not enough: Windows loads intl-8.dll (and the iconv/charset DLLs
# it imports) from the executable directory.
if(WIN32 AND COREDECK_INTL_RUNTIME_DLLS)
    install(FILES ${COREDECK_INTL_RUNTIME_DLLS} DESTINATION .)
endif()

if(NOT APPLE)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/fonts DESTINATION assets)
    install(DIRECTORY ${COREDECK_LOCALE_DIR}/ DESTINATION assets/locales)
endif()

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/assets/icons/icon.png DESTINATION assets/icons)
endif()

if(WIN32)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_NAME}")
    set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}")
    set(CPACK_WIX_UPGRADE_GUID "B8F3E5D2-4A7C-4D2B-9E1F-6C8A5B3D7E9F")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "${PROJECT_NAME}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/devmuaz")
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/LICENSE.rtf")
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
    endif()
endif()

include(CPack)

option(COREDECK_BUILD_TESTS "Build CoreDeck unit tests" OFF)
if(COREDECK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
