add_subdirectory(${CMAKE_SOURCE_DIR}/extern/catch2 ${CMAKE_BINARY_DIR}/extern/catch2)

add_executable(coredeck_tests
        test_utilities.cpp
        test_paths.cpp
        test_log_buffer.cpp
        test_log_filter.cpp
        test_skin.cpp
        test_version_check.cpp
        test_sha256.cpp
        test_archive.cpp
        test_apk_analyzer.cpp
        test_avd_manager.cpp
        test_health_check.cpp
        test_i18n.cpp
        test_jdk.cpp
        test_sdk_bootstrap.cpp
        test_sdk_manager.cpp
        test_system_image.cpp
        test_sdk_bootstrap_network.cpp
)

target_link_libraries(coredeck_tests PRIVATE
        coredeck_core
        Catch2::Catch2WithMain
)

file(TO_CMAKE_PATH "${COREDECK_LOCALE_DIR}" _coredeck_locales_dir)
file(TO_CMAKE_PATH "${GETTEXT_MSGFMT_EXECUTABLE}" _coredeck_msgfmt)
target_compile_definitions(coredeck_tests PRIVATE
        "COREDECK_LOCALES_DIR=\"${_coredeck_locales_dir}\""
        "COREDECK_MSGFMT=\"${_coredeck_msgfmt}\""
)
add_dependencies(coredeck_tests coredeck_locales)
if(WIN32 AND COREDECK_INTL_RUNTIME_DLLS)
    foreach(_dll IN LISTS COREDECK_INTL_RUNTIME_DLLS)
        add_custom_command(TARGET coredeck_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:coredeck_tests>"
            VERBATIM
        )
    endforeach()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/extern/catch2/extras)
include(Catch)
catch_discover_tests(coredeck_tests)

add_test(NAME sdk_bootstrap_network COMMAND coredeck_tests "[.network]")
set_tests_properties(sdk_bootstrap_network PROPERTIES
        LABELS "network"
        TIMEOUT 900
        SKIP_RETURN_CODE 4
)
