add_subdirectory(${CMAKE_SOURCE_DIR}/extern/catch2 ${CMAKE_BINARY_DIR}/extern/catch2)

add_executable(coredeck_tests
        test_utilities.cpp
        test_paths.cpp
        test_log_buffer.cpp
        test_version_check.cpp
)

target_link_libraries(coredeck_tests PRIVATE
        coredeck_core
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/extern/catch2/extras)
include(Catch)
catch_discover_tests(coredeck_tests)