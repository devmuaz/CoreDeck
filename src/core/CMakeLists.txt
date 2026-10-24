add_library(coredeck_core STATIC
    apk_analyzer.cpp
    apk_signer.cpp
    avd_manager.cpp
    emulator.cpp
    emulator_console.cpp
    health_check.cpp
    jdk.cpp
    jar_signer.cpp
    keytool.cpp
    options.cpp
    sdk.cpp
    sdk_bootstrap.cpp
    sdk_manager.cpp
    skin.cpp
    system_image.cpp
)

target_include_directories(coredeck_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(coredeck_core PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(coredeck_core PUBLIC coredeck_platform reflectcpp)
