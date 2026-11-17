if(MINGW)
    message(FATAL_ERROR
        "The Frida core devkit is an MSVC static library and cannot be linked with MinGW. "
        "Build CoreDeck with the Visual Studio toolchain on Windows.")
endif()

set(COREDECK_FRIDA_VERSION "17.9.2" CACHE STRING "Frida release statically linked into CoreDeck")
set(COREDECK_FRIDA_DEVKIT_DIR "" CACHE PATH "Pre-extracted frida-core devkit. Empty downloads the pinned release.")

set(COREDECK_FRIDA_SERVER_SHA256_ANDROID_ARM "b49895b2bd806e81cf2a1c0daf27cc45a41cd81b6863c37a33625567ec8b2d59")
set(COREDECK_FRIDA_SERVER_SHA256_ANDROID_ARM64 "1c6107a51d53f364e273373a34640e28d017d79bd7e72a30e93a53b28de06e4f")
set(COREDECK_FRIDA_SERVER_SHA256_ANDROID_X86 "ea4643ddfc9899c24042edd38ddf0315cfe6fedf7e4b73511bbfd84d77916cb9")
set(COREDECK_FRIDA_SERVER_SHA256_ANDROID_X86_64 "bc071af88daae6d424f9160de0330331ac3b88577e2f5a0b559cfb85fefcb0f5")

set(_coredeck_frida_devkit_macos_arm64 "7560f21a5e25df541454137b43e36995bb4f2f487617e2f4e358d95cee41ae1c")
set(_coredeck_frida_devkit_macos_x86_64 "10f534345f1a58aa344f66d8de848d8bbe54e22f6f79de3546a4cef3d0272ad6")
set(_coredeck_frida_devkit_linux_x86_64 "1fd9d2b3cd3fb56a6cb6ebd7931b0abda8304cbb176c0ff34698d1b2a1a31587")
set(_coredeck_frida_devkit_linux_arm64 "016579301ad2ef71c40496b49aa9d701e935758402cc3fcda3a9d72ae11a5f7c")
set(_coredeck_frida_devkit_windows_x86_64 "394d47d38ef5895dfe2d88db2ce4cfe54a1aeb3f3a1d4d79230d2d0698fb349a")

set(_coredeck_frida_arch "${CMAKE_OSX_ARCHITECTURES}")
if(NOT _coredeck_frida_arch)
    set(_coredeck_frida_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(APPLE)
    if(_coredeck_frida_arch STREQUAL "arm64" OR _coredeck_frida_arch STREQUAL "aarch64")
        set(COREDECK_FRIDA_HOST "macos-arm64")
        set(_coredeck_frida_devkit_sha "${_coredeck_frida_devkit_macos_arm64}")
    elseif(_coredeck_frida_arch STREQUAL "x86_64" OR _coredeck_frida_arch STREQUAL "AMD64")
        set(COREDECK_FRIDA_HOST "macos-x86_64")
        set(_coredeck_frida_devkit_sha "${_coredeck_frida_devkit_macos_x86_64}")
    else()
        message(FATAL_ERROR
            "Frida supports one macOS architecture at a time (arm64 or x86_64). "
            "CMAKE_OSX_ARCHITECTURES='${CMAKE_OSX_ARCHITECTURES}'.")
    endif()
elseif(WIN32)
    if(_coredeck_frida_arch STREQUAL "AMD64" OR _coredeck_frida_arch STREQUAL "x86_64")
        set(COREDECK_FRIDA_HOST "windows-x86_64")
        set(_coredeck_frida_devkit_sha "${_coredeck_frida_devkit_windows_x86_64}")
    else()
        message(FATAL_ERROR "Frida is linked for windows-x86_64. This build is '${_coredeck_frida_arch}'.")
    endif()
elseif(UNIX)
    if(_coredeck_frida_arch STREQUAL "x86_64" OR _coredeck_frida_arch STREQUAL "amd64")
        set(COREDECK_FRIDA_HOST "linux-x86_64")
        set(_coredeck_frida_devkit_sha "${_coredeck_frida_devkit_linux_x86_64}")
    elseif(_coredeck_frida_arch STREQUAL "aarch64" OR _coredeck_frida_arch STREQUAL "arm64")
        set(COREDECK_FRIDA_HOST "linux-arm64")
        set(_coredeck_frida_devkit_sha "${_coredeck_frida_devkit_linux_arm64}")
    else()
        message(FATAL_ERROR "Frida is linked for linux-x86_64 and linux-arm64. This build is '${_coredeck_frida_arch}'.")
    endif()
else()
    message(FATAL_ERROR "Frida is not supported on this platform.")
endif()

if(COREDECK_FRIDA_DEVKIT_DIR)
    set(_coredeck_frida_root "${COREDECK_FRIDA_DEVKIT_DIR}")
else()
    if(DEFINED ENV{COREDECK_FRIDA_CACHE} AND NOT "$ENV{COREDECK_FRIDA_CACHE}" STREQUAL "")
        set(_coredeck_frida_cache "$ENV{COREDECK_FRIDA_CACHE}")
    elseif(WIN32 AND DEFINED ENV{LOCALAPPDATA} AND NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
        set(_coredeck_frida_cache "$ENV{LOCALAPPDATA}/coredeck/frida")
    elseif(DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
        set(_coredeck_frida_cache "$ENV{XDG_CACHE_HOME}/coredeck/frida")
    else()
        set(_coredeck_frida_cache "$ENV{HOME}/.cache/coredeck/frida")
    endif()
    if(_coredeck_frida_cache MATCHES " ")
        message(FATAL_ERROR "The Frida devkit cache path must not contain spaces: ${_coredeck_frida_cache}")
    endif()
    set(_coredeck_frida_archive
        "${_coredeck_frida_cache}/${COREDECK_FRIDA_VERSION}/frida-core-devkit-${COREDECK_FRIDA_VERSION}-${COREDECK_FRIDA_HOST}.tar.xz")
    set(_coredeck_frida_root "${_coredeck_frida_cache}/${COREDECK_FRIDA_VERSION}/${COREDECK_FRIDA_HOST}")
    set(_coredeck_frida_url
        "https://github.com/frida/frida/releases/download/${COREDECK_FRIDA_VERSION}/frida-core-devkit-${COREDECK_FRIDA_VERSION}-${COREDECK_FRIDA_HOST}.tar.xz")
    get_filename_component(_coredeck_frida_archive_dir "${_coredeck_frida_archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${_coredeck_frida_archive_dir}")

    set(_coredeck_frida_have_archive FALSE)
    if(EXISTS "${_coredeck_frida_archive}")
        file(SHA256 "${_coredeck_frida_archive}" _coredeck_frida_actual)
        if(_coredeck_frida_actual STREQUAL _coredeck_frida_devkit_sha)
            set(_coredeck_frida_have_archive TRUE)
        else()
            file(REMOVE "${_coredeck_frida_archive}")
        endif()
    endif()
    if(NOT _coredeck_frida_have_archive)
        message(STATUS "Downloading Frida ${COREDECK_FRIDA_VERSION} devkit for ${COREDECK_FRIDA_HOST}")
        file(DOWNLOAD "${_coredeck_frida_url}" "${_coredeck_frida_archive}"
            SHOW_PROGRESS
            STATUS _coredeck_frida_download
            TIMEOUT 600
        )
        list(GET _coredeck_frida_download 0 _coredeck_frida_download_code)
        if(NOT _coredeck_frida_download_code EQUAL 0)
            list(GET _coredeck_frida_download 1 _coredeck_frida_download_message)
            message(FATAL_ERROR "Frida devkit download failed: ${_coredeck_frida_download_message}")
        endif()
        file(SHA256 "${_coredeck_frida_archive}" _coredeck_frida_actual)
        if(NOT _coredeck_frida_actual STREQUAL _coredeck_frida_devkit_sha)
            file(REMOVE "${_coredeck_frida_archive}")
            message(FATAL_ERROR
                "Frida devkit hash mismatch for ${COREDECK_FRIDA_HOST}. "
                "Expected ${_coredeck_frida_devkit_sha}, got ${_coredeck_frida_actual}.")
        endif()
    endif()
    if(NOT EXISTS "${_coredeck_frida_root}/frida-core.h")
        file(MAKE_DIRECTORY "${_coredeck_frida_root}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xf "${_coredeck_frida_archive}"
            WORKING_DIRECTORY "${_coredeck_frida_root}"
            RESULT_VARIABLE _coredeck_frida_tar
        )
        if(NOT _coredeck_frida_tar EQUAL 0 OR NOT EXISTS "${_coredeck_frida_root}/frida-core.h")
            message(FATAL_ERROR "Failed to extract the Frida devkit into ${_coredeck_frida_root}")
        endif()
    endif()
endif()

if(NOT EXISTS "${_coredeck_frida_root}/frida-core.h")
    message(FATAL_ERROR "Frida devkit is missing frida-core.h in ${_coredeck_frida_root}")
endif()
if(WIN32)
    set(_coredeck_frida_lib "${_coredeck_frida_root}/frida-core.lib")
else()
    set(_coredeck_frida_lib "${_coredeck_frida_root}/libfrida-core.a")
endif()
if(NOT EXISTS "${_coredeck_frida_lib}")
    message(FATAL_ERROR "Frida devkit is missing ${_coredeck_frida_lib}")
endif()

set(_coredeck_frida_command "")
file(STRINGS "${_coredeck_frida_root}/frida-core-example.c" _coredeck_frida_lines)
set(_coredeck_frida_grab OFF)
foreach(_coredeck_frida_line IN LISTS _coredeck_frida_lines)
    if(_coredeck_frida_line MATCHES "Compile with:")
        set(_coredeck_frida_grab ON)
        continue()
    endif()
    if(_coredeck_frida_grab AND _coredeck_frida_line MATCHES "^ \\* (.+)")
        set(_coredeck_frida_command "${CMAKE_MATCH_1}")
        break()
    endif()
endforeach()
if(_coredeck_frida_command STREQUAL "")
    message(FATAL_ERROR "Could not read the Frida devkit link line from frida-core-example.c")
endif()
separate_arguments(_coredeck_frida_tokens UNIX_COMMAND "${_coredeck_frida_command}")
set(_coredeck_frida_extra "")
set(_coredeck_frida_seen OFF)
foreach(_coredeck_frida_token IN LISTS _coredeck_frida_tokens)
    if(NOT _coredeck_frida_seen)
        if(_coredeck_frida_token STREQUAL "-lfrida-core" OR _coredeck_frida_token MATCHES "frida-core\\.lib$")
            set(_coredeck_frida_seen ON)
        endif()
        continue()
    endif()
    list(APPEND _coredeck_frida_extra "${_coredeck_frida_token}")
endforeach()
if(NOT _coredeck_frida_seen)
    message(FATAL_ERROR "The Frida devkit link line does not mention libfrida-core: ${_coredeck_frida_command}")
endif()

add_library(coredeck_frida_devkit INTERFACE)
add_library(Frida::Core ALIAS coredeck_frida_devkit)
if(WIN32)
    target_link_libraries(coredeck_frida_devkit INTERFACE "${_coredeck_frida_lib}" ${_coredeck_frida_extra})
    target_link_options(coredeck_frida_devkit INTERFACE "/WHOLEARCHIVE:${_coredeck_frida_lib}")
elseif(APPLE)
    target_link_options(coredeck_frida_devkit INTERFACE
        "SHELL:-Wl,-load_hidden,${_coredeck_frida_lib}"
        ${_coredeck_frida_extra}
    )
else()
    target_link_directories(coredeck_frida_devkit INTERFACE "${_coredeck_frida_root}")
    target_link_options(coredeck_frida_devkit INTERFACE
        "-Wl,--whole-archive"
        "-lfrida-core"
        "-Wl,--no-whole-archive"
        "-Wl,--exclude-libs,libfrida-core.a"
        ${_coredeck_frida_extra}
    )
endif()

set(COREDECK_FRIDA_DEVKIT_INCLUDE "${_coredeck_frida_root}" CACHE INTERNAL "Extracted frida-core devkit")
message(STATUS "Frida ${COREDECK_FRIDA_VERSION} (${COREDECK_FRIDA_HOST}): ${_coredeck_frida_lib}")

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/frida_release.h.in"
    "${CMAKE_BINARY_DIR}/generated/frida_release.h"
    @ONLY
)

function(coredeck_frida_finalize target)
    if(APPLE)
        target_link_options(${target} PRIVATE "-Wl,-no_warn_duplicate_libraries")
    elseif(MSVC)
        target_link_options(${target} PRIVATE "/OPT:REF")
    endif()
endfunction()
