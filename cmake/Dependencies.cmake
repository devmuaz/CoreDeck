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
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

set(REFLECTCPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/reflect-cpp ${CMAKE_BINARY_DIR}/extern/reflect-cpp)
if(MSVC)
    # MSVC rejects invoke_result_t<decltype(fn)> when fn's return type is auto.
    # Patch a build-tree copy so the reflect-cpp checkout stays untouched.
    set(_coredeck_rfl_root "${CMAKE_BINARY_DIR}/generated/reflect-cpp/include")
    file(COPY "${CMAKE_SOURCE_DIR}/extern/reflect-cpp/include/" DESTINATION "${_coredeck_rfl_root}")
    function(coredeck_patch_reflect relative old new)
        set(path "${_coredeck_rfl_root}/${relative}")
        file(READ "${path}" contents)
        string(FIND "${contents}" "${new}" done)
        if(NOT done EQUAL -1)
            return()
        endif()
        string(FIND "${contents}" "${old}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "reflect-cpp no longer contains the expected text in ${relative}")
        endif()
        string(REPLACE "${old}" "${new}" contents "${contents}")
        file(WRITE "${path}" "${contents}")
    endfunction()
    set(_coredeck_rfl_utility "#include <type_traits>\n#include <utility>")
    coredeck_patch_reflect("rfl/internal/ptr_named_tuple_t.hpp" "#include <type_traits>" "${_coredeck_rfl_utility}")
    coredeck_patch_reflect("rfl/internal/ptr_named_tuple_t.hpp"
        "std::invoke_result_t<decltype(to_ptr_named_tuple<T>), T>"
        "decltype(to_ptr_named_tuple(std::declval<T>()))")
    coredeck_patch_reflect("rfl/view_t.hpp" "#include <type_traits>" "${_coredeck_rfl_utility}")
    coredeck_patch_reflect("rfl/view_t.hpp"
        "std::invoke_result_t<decltype(Processors<Ps...>::template process<\n                                  T, internal::ptr_named_tuple_t<T>>),\n                         internal::ptr_named_tuple_t<T>>"
        "decltype(Processors<Ps...>::template process<T>(std::declval<internal::ptr_named_tuple_t<T>>()))")
    coredeck_patch_reflect("rfl/internal/processed_t.hpp" "#include <type_traits>" "${_coredeck_rfl_utility}")
    coredeck_patch_reflect("rfl/internal/processed_t.hpp"
        "std::invoke_result_t<decltype(Processors<Ps...>::template process<\n                                    StructType, PtrNamedTupleType>),\n                           PtrNamedTupleType>"
        "decltype(Processors<Ps...>::template process<StructType>(std::declval<PtrNamedTupleType>()))")
    coredeck_patch_reflect("rfl/internal/tag_t.hpp" "#include <type_traits>" "${_coredeck_rfl_utility}")
    coredeck_patch_reflect("rfl/internal/tag_t.hpp"
        "std::invoke_result_t<decltype(make_tag<_discriminator, T>), T>"
        "decltype(make_tag<_discriminator, T>(std::declval<const T&>()))")
    coredeck_patch_reflect("rfl/internal/nth_element_t.hpp"
        "std::invoke_result_t<decltype(nth_element<N, TypeWrapper<Ts>...>),\n                                  TypeWrapper<Ts>...>"
        "decltype(nth_element<N>(std::declval<TypeWrapper<Ts>>()...))")
    target_include_directories(reflectcpp BEFORE PUBLIC "${_coredeck_rfl_root}")
    unset(_coredeck_rfl_root)
    unset(_coredeck_rfl_utility)
endif()
if(MINGW)
    target_compile_options(reflectcpp PRIVATE
        -Wa,-mbig-obj
        $<$<CONFIG:Debug>:-O1>
        $<$<CONFIG:Debug>:-g0>
    )
endif()

set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
set(NFD_WAYLAND OFF CACHE BOOL "" FORCE)
if(APPLE)
    enable_language(OBJC)
endif()
set(_coredeck_cxx_flags "${CMAKE_CXX_FLAGS}")
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/nativefiledialog-extended ${CMAKE_BINARY_DIR}/extern/nativefiledialog-extended)
set(CMAKE_CXX_FLAGS "${_coredeck_cxx_flags}")
unset(_coredeck_cxx_flags)
if(WIN32)
    target_link_libraries(nfd PUBLIC ole32 uuid shell32)
endif()

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
    set(SENTRY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(CRASHPAD_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(CRASHPAD_ENABLE_INSTALL_DEV OFF CACHE BOOL "" FORCE)
    # Crashpad still installs its handler with an unconditional install() call.
    set(CMAKE_SKIP_INSTALL_RULES TRUE)
    coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/sentry-native ${CMAKE_BINARY_DIR}/extern/sentry-native)
    set(CMAKE_SKIP_INSTALL_RULES FALSE)
    message(STATUS "Sentry: Enabled (Release only)")
elseif(COREDECK_SENTRY_DSN)
    message(STATUS "Sentry: Disabled (non-Release build)")
else()
    message(STATUS "Sentry: Disabled")
endif()
