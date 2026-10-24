add_library(coredeck_platform STATIC
    archive.cpp
    http_download.cpp
    i18n.cpp
    log_buffer.cpp
    log_filter.cpp
    paths.cpp
    process.cpp
    process_stats.cpp
    sha256.cpp
    utilities.cpp
)

target_include_directories(coredeck_platform PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_precompile_headers(coredeck_platform PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(coredeck_platform PUBLIC miniz)
target_compile_definitions(coredeck_platform PUBLIC
    COREDECK_TITLE="CoreDeck"
    COREDECK_VERSION="${PROJECT_VERSION_FULL}"
    COREDECK_BUILD_NUMBER="${PROJECT_BUILD_NUMBER}"
    COREDECK_VENDOR="${APP_VENDOR}"
    COREDECK_DESCRIPTION="${APP_DESCRIPTION_SHORT}"
    COREDECK_YEAR="${CURRENT_YEAR}"
    COREDECK_COPYRIGHT="${APP_COPYRIGHT}"
)

if(WIN32)
    target_compile_definitions(coredeck_platform PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(coredeck_platform PUBLIC shell32 ole32 winhttp ws2_32)
elseif(APPLE)
    target_link_libraries(coredeck_platform PUBLIC CURL::libcurl "-framework CoreFoundation")
elseif(UNIX)
    target_link_libraries(coredeck_platform PUBLIC ${CMAKE_DL_LIBS} Threads::Threads CURL::libcurl)
else()
    target_link_libraries(coredeck_platform PUBLIC CURL::libcurl)
endif()
target_link_libraries(coredeck_platform PUBLIC Intl::Intl)
