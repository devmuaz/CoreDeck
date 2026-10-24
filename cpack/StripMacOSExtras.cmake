# Included by CPack before the macOS package is built.
#
# Sentry-native's install rules drop crashpad_handler, headers, and libs at the
# staging root. The handler already lives inside CoreDeck.app; these copies are
# unsigned and break notarization during distribution (release.yml) and
# build (build.yml).

foreach(_dir bin include lib lib64 share)
    if(EXISTS "${CPACK_TEMPORARY_DIRECTORY}/${_dir}")
        file(REMOVE_RECURSE "${CPACK_TEMPORARY_DIRECTORY}/${_dir}")
    endif()
endforeach()
