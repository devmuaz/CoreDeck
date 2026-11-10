# Vendored projects still call cmake_minimum_required() with versions CMake
# 3.31+ deprecates. Ignore those diagnostics under extern/ only.
macro(coredeck_add_extern source_dir binary_dir)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
        block(SCOPE_FOR DIAGNOSTICS)
            cmake_diagnostic(SET CMD_DEPRECATED IGNORE)
            add_subdirectory("${source_dir}" "${binary_dir}")
        endblock()
    else()
        set(_coredeck_warn_deprecated "${CMAKE_WARN_DEPRECATED}")
        set(CMAKE_WARN_DEPRECATED OFF)
        add_subdirectory("${source_dir}" "${binary_dir}")
        if(_coredeck_warn_deprecated STREQUAL "")
            unset(CMAKE_WARN_DEPRECATED)
        else()
            set(CMAKE_WARN_DEPRECATED "${_coredeck_warn_deprecated}")
        endif()
        unset(_coredeck_warn_deprecated)
    endif()
endmacro()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/glfw ${CMAKE_BINARY_DIR}/extern/glfw)

find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
add_library(imgui SHARED
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
if(WIN32)
    target_compile_definitions(imgui PRIVATE "IMGUI_API=__declspec(dllexport)")
    target_compile_definitions(imgui INTERFACE "IMGUI_API=__declspec(dllimport)")
endif()

set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/reflect-cpp ${CMAKE_BINARY_DIR}/extern/reflect-cpp)
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
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/miniz ${CMAKE_BINARY_DIR}/extern/miniz)

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
    coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/sentry-native ${CMAKE_BINARY_DIR}/extern/sentry-native)
    message(STATUS "Sentry: Enabled (Release only)")
elseif(COREDECK_SENTRY_DSN)
    message(STATUS "Sentry: Disabled (non-Release build)")
else()
    message(STATUS "Sentry: Disabled")
endif()
