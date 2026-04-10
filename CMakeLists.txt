cmake_minimum_required(VERSION 3.23)
project(CoreDeck)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
add_subdirectory(extern/glfw)

set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/reflect-cpp)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(CoreDeck
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

target_include_directories(CoreDeck PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(CoreDeck PRIVATE
        glfw
        OpenGL::GL
        reflectcpp
)

add_custom_command(TARGET CoreDeck POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<IF:$<PLATFORM_ID:Darwin>,$<TARGET_BUNDLE_CONTENT_DIR:CoreDeck>/Resources/assets,$<TARGET_FILE_DIR:CoreDeck>/assets>
)

if (WIN32)
    target_compile_definitions(CoreDeck PRIVATE COREDECK_PLATFORM_WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(CoreDeck PRIVATE shell32)
elseif (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
    target_compile_definitions(CoreDeck PRIVATE COREDECK_PLATFORM_MACOS GL_SILENCE_DEPRECATION)
    target_link_libraries(CoreDeck PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    set_target_properties(CoreDeck PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER com.devmuaz.coredeck
            MACOSX_BUNDLE_BUNDLE_NAME "CoreDeck"
            MACOSX_BUNDLE_BUNDLE_VERSION "2"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.0.8"
            MACOSX_BUNDLE_EXECUTABLE_NAME "CoreDeck"
            MACOSX_BUNDLE_INFO_STRING "Android Emulator and AVD Manager"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 CoreDeck"
    )
else ()
    target_compile_definitions(CoreDeck PRIVATE COREDECK_PLATFORM_LINUX)
    target_link_libraries(CoreDeck PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
endif ()