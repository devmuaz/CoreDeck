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
    # MSVC rejects std::invoke_result_t<decltype(fn), ...> when fn is declared
    # with a deduced return type: the function type still contains auto, and a
    # template argument may not. Use the return type of a call instead, from a
    # copy of the headers so the reflect-cpp submodule stays untouched.
    set(_coredeck_rfl_overlay "${CMAKE_BINARY_DIR}/generated/reflect-cpp")
    file(REMOVE_RECURSE "${_coredeck_rfl_overlay}")
    file(COPY "${CMAKE_SOURCE_DIR}/extern/reflect-cpp/include" DESTINATION "${_coredeck_rfl_overlay}")
    function(coredeck_patch_reflect_header relative old new)
        set(path "${_coredeck_rfl_overlay}/include/${relative}")
        file(READ "${path}" contents)
        string(FIND "${contents}" "${old}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "reflect-cpp overlay no longer contains the expected text in ${relative}")
        endif()
        string(REPLACE "${old}" "${new}" contents "${contents}")
        file(WRITE "${path}" "${contents}")
    endfunction()
    coredeck_patch_reflect_header("rfl/internal/ptr_named_tuple_t.hpp"
        "#include <type_traits>"
        "#include <type_traits>\n#include <utility>")
    coredeck_patch_reflect_header("rfl/internal/ptr_named_tuple_t.hpp"
        "using ptr_named_tuple_t =\n    std::invoke_result_t<decltype(to_ptr_named_tuple<T>), T>;"
        "using ptr_named_tuple_t = decltype(to_ptr_named_tuple(std::declval<T>()));")
    coredeck_patch_reflect_header("rfl/view_t.hpp"
        "#include <type_traits>"
        "#include <type_traits>\n#include <utility>")
    coredeck_patch_reflect_header("rfl/view_t.hpp"
        "using view_t =\n    std::invoke_result_t<decltype(Processors<Ps...>::template process<\n                                  T, internal::ptr_named_tuple_t<T>>),\n                         internal::ptr_named_tuple_t<T>>;"
        "using view_t = decltype(Processors<Ps...>::template process<T>(\n    std::declval<internal::ptr_named_tuple_t<T>>()));")
    coredeck_patch_reflect_header("rfl/internal/processed_t.hpp"
        "#include <type_traits>"
        "#include <type_traits>\n#include <utility>")
    coredeck_patch_reflect_header("rfl/internal/processed_t.hpp"
        "using type = typename remove_ptrs_nt<\n      std::invoke_result_t<decltype(Processors<Ps...>::template process<\n                                    StructType, PtrNamedTupleType>),\n                           PtrNamedTupleType>>::NamedTupleType;"
        "using type = typename remove_ptrs_nt<\n      decltype(Processors<Ps...>::template process<StructType>(\n          std::declval<PtrNamedTupleType>()))>::NamedTupleType;")
    coredeck_patch_reflect_header("rfl/internal/tag_t.hpp"
        "#include <type_traits>"
        "#include <type_traits>\n#include <utility>")
    coredeck_patch_reflect_header("rfl/internal/tag_t.hpp"
        "using tag_t = std::invoke_result_t<decltype(make_tag<_discriminator, T>), T>;"
        "using tag_t = decltype(make_tag<_discriminator, T>(std::declval<const T&>()));")
    coredeck_patch_reflect_header("rfl/internal/nth_element_t.hpp"
        "using nth_element_t =\n    typename std::invoke_result_t<decltype(nth_element<N, TypeWrapper<Ts>...>),\n                                  TypeWrapper<Ts>...>::Type;"
        "using nth_element_t =\n    typename decltype(nth_element<N>(std::declval<TypeWrapper<Ts>>()...))::Type;")
    target_include_directories(reflectcpp BEFORE PUBLIC "${_coredeck_rfl_overlay}/include")
    unset(_coredeck_rfl_overlay)
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

# libarchive reads .zip and .xz. zlib is the deflate codec (zip entries and PNG),
# and liblzma is the .xz codec. Both are linked only through libarchive, plus zlib
# for PNG IDAT, which is a zlib stream rather than an archive.
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/zlib ${CMAKE_BINARY_DIR}/extern/zlib)

set(XZ_NLS OFF CACHE BOOL "" FORCE)
set(XZ_DOC OFF CACHE BOOL "" FORCE)
set(XZ_DOXYGEN OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_XZ OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_XZDEC OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_LZMADEC OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_LZMAINFO OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_SCRIPTS OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_SYMLINKS OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_SYMLINKS_LZMA OFF CACHE BOOL "" FORCE)
if(DEFINED CACHE{BUILD_TESTING})
    set(_coredeck_build_testing "${BUILD_TESTING}")
else()
    unset(_coredeck_build_testing)
endif()
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
if(DEFINED CACHE{BUILD_SHARED_LIBS})
    set(_coredeck_shared_libs "${BUILD_SHARED_LIBS}")
else()
    unset(_coredeck_shared_libs)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/xz ${CMAKE_BINARY_DIR}/extern/xz)
if(NOT WIN32)
    target_link_libraries(liblzma INTERFACE Threads::Threads)
endif()
if(DEFINED _coredeck_shared_libs)
    set(BUILD_SHARED_LIBS "${_coredeck_shared_libs}" CACHE BOOL "" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()
unset(_coredeck_shared_libs)
if(DEFINED _coredeck_build_testing)
    set(BUILD_TESTING "${_coredeck_build_testing}" CACHE BOOL "" FORCE)
else()
    unset(BUILD_TESTING CACHE)
endif()
unset(_coredeck_build_testing)

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()
if(NOT TARGET LibLZMA::LibLZMA)
    add_library(LibLZMA::LibLZMA ALIAS liblzma)
endif()
set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/zlib" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE FILEPATH "" FORCE)
unset(ZLIB_LIBRARIES CACHE)
set(LIBLZMA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/xz/src/liblzma/api" CACHE PATH "" FORCE)
set(LIBLZMA_LIBRARY liblzma CACHE FILEPATH "" FORCE)
set(LIBLZMA_HAS_AUTO_DECODER TRUE CACHE INTERNAL "" FORCE)
set(LIBLZMA_HAS_EASY_ENCODER TRUE CACHE INTERNAL "" FORCE)
set(LIBLZMA_HAS_LZMA_PRESET TRUE CACHE INTERNAL "" FORCE)

set(ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(ENABLE_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_TAR OFF CACHE BOOL "" FORCE)
set(ENABLE_CPIO OFF CACHE BOOL "" FORCE)
set(ENABLE_CAT OFF CACHE BOOL "" FORCE)
set(ENABLE_UNZIP OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
set(ENABLE_NETTLE OFF CACHE BOOL "" FORCE)
set(ENABLE_LIBB2 OFF CACHE BOOL "" FORCE)
set(ENABLE_LZ4 OFF CACHE BOOL "" FORCE)
set(ENABLE_LZO OFF CACHE BOOL "" FORCE)
set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(ENABLE_BZip2 OFF CACHE BOOL "" FORCE)
set(ENABLE_LIBXML2 OFF CACHE BOOL "" FORCE)
set(ENABLE_BSDXML OFF CACHE BOOL "" FORCE)
set(ENABLE_EXPAT OFF CACHE BOOL "" FORCE)
set(ENABLE_PCREPOSIX OFF CACHE BOOL "" FORCE)
set(ENABLE_PCRE2POSIX OFF CACHE BOOL "" FORCE)
set(ENABLE_CNG OFF CACHE BOOL "" FORCE)
set(ENABLE_ZLIB ON CACHE BOOL "" FORCE)
set(ENABLE_LZMA ON CACHE BOOL "" FORCE)
if(DEFINED CACHE{BUILD_SHARED_LIBS})
    set(_coredeck_shared_libs "${BUILD_SHARED_LIBS}")
else()
    unset(_coredeck_shared_libs)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
coredeck_add_extern(${CMAKE_SOURCE_DIR}/extern/libarchive ${CMAKE_BINARY_DIR}/extern/libarchive)
if(DEFINED _coredeck_shared_libs)
    set(BUILD_SHARED_LIBS "${_coredeck_shared_libs}" CACHE BOOL "" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()
unset(_coredeck_shared_libs)
target_compile_definitions(archive_static PUBLIC LIBARCHIVE_STATIC)

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
    # Non-MSVC crashpad links its exported interface library crashpad_zlib to
    # ZLIB::ZLIB. That alias is the in-tree zlibstatic, which is not installed,
    # so install(EXPORT crashpad_export) rejects the dependency. Keep the build
    # link and omit it from the exported interface.
    if(TARGET crashpad_zlib)
        get_target_property(_coredeck_crashpad_zlib_type crashpad_zlib TYPE)
        if(_coredeck_crashpad_zlib_type STREQUAL "INTERFACE_LIBRARY")
            set_property(TARGET crashpad_zlib PROPERTY INTERFACE_LINK_LIBRARIES
                "$<BUILD_INTERFACE:zlibstatic>")
        endif()
        unset(_coredeck_crashpad_zlib_type)
    endif()
    message(STATUS "Sentry: Enabled (Release only)")
elseif(COREDECK_SENTRY_DSN)
    message(STATUS "Sentry: Disabled (non-Release build)")
else()
    message(STATUS "Sentry: Disabled")
endif()
