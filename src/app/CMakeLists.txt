add_library(coredeck_app STATIC
    app_paths.cpp
    frida_paths.cpp
    app_config.cpp
    crash_reporter.cpp
    file_dialog.cpp
    version_check.cpp
)

target_include_directories(coredeck_app PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_precompile_headers(coredeck_app PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(coredeck_app PUBLIC coredeck_platform reflectcpp nfd)

if(COREDECK_SENTRY_ENABLED)
    target_compile_definitions(coredeck_app PUBLIC
        $<$<CONFIG:Release>:COREDECK_SENTRY_DSN="${COREDECK_SENTRY_DSN}">
    )
    target_link_libraries(coredeck_app PUBLIC $<$<CONFIG:Release>:sentry>)
endif()
