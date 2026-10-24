set(COREDECK_VERSION_OVERRIDE "" CACHE STRING "Explicit version override (e.g., set by CI from a release tag)")

find_package(Git QUIET)
if(GIT_FOUND)
    if(COREDECK_VERSION_OVERRIDE)
        set(GIT_TAG_VERSION "${COREDECK_VERSION_OVERRIDE}")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_TAG_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_COUNT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(GIT_TAG_VERSION)
        string(REGEX REPLACE "^v" "" GIT_TAG_STRIPPED ${GIT_TAG_VERSION})
        string(REGEX REPLACE "-.*$" "" PROJECT_VERSION ${GIT_TAG_STRIPPED})
        set(PROJECT_VERSION_FULL ${GIT_TAG_STRIPPED})
    else()
        set(PROJECT_VERSION "0.0.1")
        set(PROJECT_VERSION_FULL "0.0.1")
    endif()

    if(GIT_COMMIT_COUNT)
        set(PROJECT_BUILD_NUMBER ${GIT_COMMIT_COUNT})
    else()
        set(PROJECT_BUILD_NUMBER "1")
    endif()
else()
    if(COREDECK_VERSION_OVERRIDE)
        string(REGEX REPLACE "^v" "" GIT_TAG_STRIPPED "${COREDECK_VERSION_OVERRIDE}")
        string(REGEX REPLACE "-.*$" "" PROJECT_VERSION "${GIT_TAG_STRIPPED}")
        set(PROJECT_VERSION_FULL "${GIT_TAG_STRIPPED}")
    else()
        set(PROJECT_VERSION "0.0.1")
        set(PROJECT_VERSION_FULL "0.0.1")
    endif()
    set(PROJECT_BUILD_NUMBER "1")
endif()
