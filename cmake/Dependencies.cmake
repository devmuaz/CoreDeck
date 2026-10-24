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
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/glfw ${CMAKE_BINARY_DIR}/extern/glfw)

set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/reflect-cpp ${CMAKE_BINARY_DIR}/extern/reflect-cpp)
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
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/miniz ${CMAKE_BINARY_DIR}/extern/miniz)

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
    add_subdirectory(${CMAKE_SOURCE_DIR}/extern/sentry-native ${CMAKE_BINARY_DIR}/extern/sentry-native)
    message(STATUS "Sentry: Enabled (Release only)")
elseif(COREDECK_SENTRY_DSN)
    message(STATUS "Sentry: Disabled (non-Release build)")
else()
    message(STATUS "Sentry: Disabled")
endif()
