add_library(coredeck_xz STATIC
    ${CMAKE_SOURCE_DIR}/extern/xz-embedded/xz_crc32.c
    ${CMAKE_SOURCE_DIR}/extern/xz-embedded/xz_crc64.c
    ${CMAKE_SOURCE_DIR}/extern/xz-embedded/xz_dec_bcj.c
    ${CMAKE_SOURCE_DIR}/extern/xz-embedded/xz_dec_lzma2.c
    ${CMAKE_SOURCE_DIR}/extern/xz-embedded/xz_dec_stream.c
)
target_include_directories(coredeck_xz PUBLIC ${CMAKE_SOURCE_DIR}/extern/xz-embedded)
target_compile_options(coredeck_xz PRIVATE
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>:-w>
    $<$<C_COMPILER_ID:MSVC>:/w>
)

add_library(coredeck_core STATIC
    adb/adb_client.cpp
    adb/adb_devices.cpp
    adb/adb_logcat.cpp
    adb/adb_service.cpp
    frida/frida_abi.cpp
    frida/frida_messages.cpp
    frida/frida_prelude.cpp
    frida/frida_server.cpp
    apk/apk_analyzer.cpp
    apk/apk_signer.cpp
    apk/jar_signer.cpp
    apk/keytool.cpp
    avd/avd_manager.cpp
    avd/emulator.cpp
    avd/emulator_console.cpp
    avd/emulator_options.cpp
    avd/skin.cpp
    avd/system_image.cpp
    sdk/health_check.cpp
    sdk/jdk.cpp
    sdk/sdk.cpp
    sdk/sdk_bootstrap.cpp
    sdk/sdk_manager.cpp
)

target_include_directories(coredeck_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)
target_precompile_headers(coredeck_core PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(coredeck_core PUBLIC coredeck_platform reflectcpp)
target_link_libraries(coredeck_core PRIVATE coredeck_xz)

add_library(coredeck_frida STATIC
    frida/frida_compiler.cpp
    frida/frida_host.cpp
)
target_include_directories(coredeck_frida PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(coredeck_frida SYSTEM PRIVATE "${COREDECK_FRIDA_DEVKIT_INCLUDE}")
target_link_libraries(coredeck_frida PUBLIC coredeck_core Frida::Core)
set_source_files_properties(
    frida/frida_compiler.cpp
    frida/frida_host.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)
