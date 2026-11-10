add_library(coredeck_core STATIC
    adb/adb_client.cpp
    adb/adb_devices.cpp
    adb/adb_logcat.cpp
    adb/adb_service.cpp
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

target_include_directories(coredeck_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_precompile_headers(coredeck_core PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(coredeck_core PUBLIC coredeck_platform reflectcpp)
