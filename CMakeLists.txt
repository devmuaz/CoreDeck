cmake_minimum_required(VERSION 3.20)
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
add_subdirectory(extern/glfw)

set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/reflect-cpp)

find_package(OpenGL REQUIRED)

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
        $<TARGET_FILE_DIR:CoreDeck>/assets
)

if (WIN32)
    target_compile_definitions(CoreDeck PRIVATE COREDECK_PLATFORM_WINDOWS)
    target_link_libraries(CoreDeck PRIVATE shell32)
elseif (APPLE)
    target_compile_definitions(CoreDeck PRIVATE COREDECK_PLATFORM_MACOS)
else ()
    target_compile_definitions(CoreDeck PRIVATE COREDECK_PLATFORM_LINUX)
endif ()