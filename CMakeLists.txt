cmake_minimum_required(VERSION 4.2)
project(Emu_Launcher)

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

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(Emu_Launcher
        ${IMGUI_SOURCES}
        src/main.cpp
        src/sdk.cpp
        src/process.cpp
        src/emulator.cpp
        src/options.cpp
        src/application.cpp
        src/components.cpp
        src/log_buffer.cpp
        src/avd_info.cpp
        src/utilities.cpp
)

target_include_directories(Emu_Launcher PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(Emu_Launcher PRIVATE
        glfw
        OpenGL::GL
)