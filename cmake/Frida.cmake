if(MINGW)
    message(FATAL_ERROR
        "The Frida core devkit is an MSVC static library and cannot be linked with MinGW. "
        "Build CoreDeck with the Visual Studio toolchain on Windows.")
endif()

set(COREDECK_FRIDA_DEVKIT_DIR "" CACHE PATH "Pre-extracted frida-core devkit. Empty downloads the pinned release.")

file(READ "${CMAKE_SOURCE_DIR}/pins/frida.json" _coredeck_frida_pins)
string(JSON COREDECK_FRIDA_VERSION GET "${_coredeck_frida_pins}" version)
string(JSON COREDECK_FRIDA_SERVER_URL_ANDROID_ARM GET "${_coredeck_frida_pins}" server "android-arm" url)
string(JSON COREDECK_FRIDA_SERVER_SHA256_ANDROID_ARM GET "${_coredeck_frida_pins}" server "android-arm" sha256)
string(JSON COREDECK_FRIDA_SERVER_URL_ANDROID_ARM64 GET "${_coredeck_frida_pins}" server "android-arm64" url)
string(JSON COREDECK_FRIDA_SERVER_SHA256_ANDROID_ARM64 GET "${_coredeck_frida_pins}" server "android-arm64" sha256)
string(JSON COREDECK_FRIDA_SERVER_URL_ANDROID_X86 GET "${_coredeck_frida_pins}" server "android-x86" url)
string(JSON COREDECK_FRIDA_SERVER_SHA256_ANDROID_X86 GET "${_coredeck_frida_pins}" server "android-x86" sha256)
string(JSON COREDECK_FRIDA_SERVER_URL_ANDROID_X86_64 GET "${_coredeck_frida_pins}" server "android-x86_64" url)
string(JSON COREDECK_FRIDA_SERVER_SHA256_ANDROID_X86_64 GET "${_coredeck_frida_pins}" server "android-x86_64" sha256)

set(_coredeck_frida_arch "${CMAKE_OSX_ARCHITECTURES}")
if(NOT _coredeck_frida_arch)
    set(_coredeck_frida_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(APPLE)
    if(_coredeck_frida_arch STREQUAL "arm64" OR _coredeck_frida_arch STREQUAL "aarch64")
        set(COREDECK_FRIDA_HOST "macos-arm64")
    elseif(_coredeck_frida_arch STREQUAL "x86_64" OR _coredeck_frida_arch STREQUAL "AMD64")
        set(COREDECK_FRIDA_HOST "macos-x86_64")
    else()
        message(FATAL_ERROR
            "Frida supports one macOS architecture at a time (arm64 or x86_64). "
            "CMAKE_OSX_ARCHITECTURES='${CMAKE_OSX_ARCHITECTURES}'.")
    endif()
elseif(WIN32)
    if(_coredeck_frida_arch STREQUAL "AMD64" OR _coredeck_frida_arch STREQUAL "x86_64")
        set(COREDECK_FRIDA_HOST "windows-x86_64")
    else()
        message(FATAL_ERROR "Frida is linked for windows-x86_64. This build is '${_coredeck_frida_arch}'.")
    endif()
elseif(UNIX)
    if(_coredeck_frida_arch STREQUAL "x86_64" OR _coredeck_frida_arch STREQUAL "amd64")
        set(COREDECK_FRIDA_HOST "linux-x86_64")
    elseif(_coredeck_frida_arch STREQUAL "aarch64" OR _coredeck_frida_arch STREQUAL "arm64")
        set(COREDECK_FRIDA_HOST "linux-arm64")
    else()
        message(FATAL_ERROR "Frida is linked for linux-x86_64 and linux-arm64. This build is '${_coredeck_frida_arch}'.")
    endif()
else()
    message(FATAL_ERROR "Frida is not supported on this platform.")
endif()

string(JSON _coredeck_frida_url GET "${_coredeck_frida_pins}" devkit "${COREDECK_FRIDA_HOST}" url)
string(JSON _coredeck_frida_devkit_sha GET "${_coredeck_frida_pins}" devkit "${COREDECK_FRIDA_HOST}" sha256)

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
    get_filename_component(_coredeck_frida_archive_name "${_coredeck_frida_url}" NAME)
    set(_coredeck_frida_archive
        "${_coredeck_frida_cache}/${COREDECK_FRIDA_VERSION}/${_coredeck_frida_archive_name}")
    set(_coredeck_frida_root "${_coredeck_frida_cache}/${COREDECK_FRIDA_VERSION}/${COREDECK_FRIDA_HOST}")
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

# Windows records link libraries as #pragma comment(lib) in frida-core.h.
# The other devkits put one "Compile with:" line in frida-core-example.c.
function(coredeck_frida_extra_libs root out)
    set(extra "")
    set(found FALSE)
    if(WIN32)
        file(READ "${root}/frida-core.h" header LIMIT 4096)
        string(REGEX MATCHALL "#pragma comment\\(lib, \"[^\"]+\"\\)" pragmas "${header}")
        foreach(pragma IN LISTS pragmas)
            string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" name "${pragma}")
            if(name STREQUAL "frida-core.lib")
                set(found TRUE)
            else()
                list(APPEND extra "${name}")
            endif()
        endforeach()
    endif()
    if(NOT found)
        file(STRINGS "${root}/frida-core-example.c" lines)
        set(grab FALSE)
        set(command "")
        foreach(line IN LISTS lines)
            if(line MATCHES "Compile with:")
                set(grab TRUE)
            elseif(grab AND line MATCHES "^[ \t]*\\*[ \t]*(.+)")
                set(command "${CMAKE_MATCH_1}")
                break()
            endif()
        endforeach()
        if(command STREQUAL "")
            message(FATAL_ERROR "Could not read the Frida devkit link line from frida-core-example.c")
        endif()
        separate_arguments(tokens UNIX_COMMAND "${command}")
        foreach(token IN LISTS tokens)
            if(token STREQUAL "-lfrida-core" OR token MATCHES "frida-core\\.lib$")
                set(found TRUE)
            elseif(found)
                list(APPEND extra "${token}")
            endif()
        endforeach()
        if(NOT found)
            message(FATAL_ERROR "The Frida devkit link line does not mention libfrida-core: ${command}")
        endif()
    endif()
    set("${out}" "${extra}" PARENT_SCOPE)
endfunction()

coredeck_frida_extra_libs("${_coredeck_frida_root}" _coredeck_frida_extra)

add_library(coredeck_frida_devkit INTERFACE)
add_library(Frida::Core ALIAS coredeck_frida_devkit)
if(WIN32)
    # The header links frida-core.lib by name, and whole-archive pulls the USB
    # monitor, which also needs SetupAPI.
    target_link_directories(coredeck_frida_devkit INTERFACE "${_coredeck_frida_root}")
    target_link_libraries(coredeck_frida_devkit INTERFACE "frida-core.lib" ${_coredeck_frida_extra} setupapi)
    target_link_options(coredeck_frida_devkit INTERFACE "/WHOLEARCHIVE:frida-core.lib")
elseif(APPLE)
    target_link_options(coredeck_frida_devkit INTERFACE
        "SHELL:-Wl,-load_hidden,${_coredeck_frida_lib}"
        ${_coredeck_frida_extra}
    )
else()
    if(NOT TARGET coredeck_frida_resolv)
        add_library(coredeck_frida_resolv STATIC "${CMAKE_SOURCE_DIR}/src/core/frida/frida_resolv.c")
        target_link_libraries(coredeck_frida_resolv PUBLIC resolv)
    endif()
    target_link_directories(coredeck_frida_devkit INTERFACE "${_coredeck_frida_root}")
    target_link_libraries(coredeck_frida_devkit INTERFACE
        "-Wl,--whole-archive" "-lfrida-core" "-Wl,--no-whole-archive"
        "-Wl,--exclude-libs,libfrida-core.a"
        ${_coredeck_frida_extra}
        coredeck_frida_resolv
    )
endif()

set(COREDECK_FRIDA_DEVKIT_INCLUDE "${_coredeck_frida_root}" CACHE INTERNAL "Extracted frida-core devkit")
message(STATUS "Frida ${COREDECK_FRIDA_VERSION} (${COREDECK_FRIDA_HOST}): ${_coredeck_frida_lib}")

configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/frida/frida_release.h.in"
    "${CMAKE_BINARY_DIR}/generated/core/frida_release.h"
    @ONLY
)

function(coredeck_frida_finalize target)
    if(APPLE)
        target_link_options(${target} PRIVATE "-Wl,-no_warn_duplicate_libraries")
    elseif(MSVC)
        target_link_options(${target} PRIVATE "/OPT:REF")
    endif()
endfunction()
