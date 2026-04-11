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
set(APP_DESCRIPTION "Professional Android emulator and AVD management tool with an intuitive GUI interface.")
set(APP_DESCRIPTION_SHORT "Android Emulator and AVD Manager")
set(APP_BUNDLE_ID "com.devmuaz.coredeck")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
        ${IMGUI_SOURCES}
        src/main.cpp
        src/sdk.cpp
        src/process.cpp
        src/emulator.cpp
        src/options.cpp
        src/application.cpp
        src/components.cpp
        src/log_buffer.cpp
        src/avd.cpp
        src/utilities.cpp
        src/paths.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        OpenGL::GL
        reflectcpp
)

if (APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/assets
    )
else ()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif ()

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE COREDECK_PLATFORM_WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32)
    target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
    if (EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/resources.rc)
    endif ()
elseif (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    target_compile_definitions(${PROJECT_NAME} PRIVATE COREDECK_PLATFORM_MACOS GL_SILENCE_DEPRECATION)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_BUNDLE_ID}"
            MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_BUILD_NUMBER}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
            MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}"
            MACOSX_BUNDLE_INFO_STRING "${APP_DESCRIPTION_SHORT}"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © ${CURRENT_YEAR} ${PROJECT_NAME}"
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
    target_compile_definitions(${PROJECT_NAME} PRIVATE COREDECK_PLATFORM_LINUX)
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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)

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