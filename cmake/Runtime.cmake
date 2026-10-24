if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/fonts
        $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/assets/fonts
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${COREDECK_LOCALE_DIR}
        $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/assets/locales
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/fonts
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/fonts
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${COREDECK_LOCALE_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/locales
    )
endif()

if(APPLE AND NOT Intl_IS_BUILT_IN)
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY INSTALL_RPATH "@executable_path/../Frameworks")
    get_filename_component(_intl_real "${Intl_LIBRARY}" REALPATH)
    execute_process(
        COMMAND otool -D "${_intl_real}"
        OUTPUT_VARIABLE _intl_otool
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX REPLACE ".*\n" "" COREDECK_INTL_INSTALL_NAME "${_intl_otool}")
    string(STRIP "${COREDECK_INTL_INSTALL_NAME}" COREDECK_INTL_INSTALL_NAME)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy "${_intl_real}" "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks/libintl.8.dylib"
        COMMAND install_name_tool -id "@rpath/libintl.8.dylib" "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks/libintl.8.dylib"
        COMMAND install_name_tool -change "${COREDECK_INTL_INSTALL_NAME}" "@rpath/libintl.8.dylib" "$<TARGET_FILE:${PROJECT_NAME}>"
        VERBATIM
    )
endif()

if(WIN32 AND NOT Intl_IS_BUILT_IN AND Intl_LIBRARY)
    # FindIntl may return a single path or an optimized/debug list. The runtime
    # DLLs live in the sibling bin directory of the import library, not in lib/.
    set(_intl_search_libs)
    foreach(_intl_item IN ITEMS ${Intl_LIBRARY} ${Intl_LIBRARY_RELEASE} ${Intl_LIBRARY_DEBUG})
        if(_intl_item STREQUAL "optimized" OR _intl_item STREQUAL "debug" OR _intl_item STREQUAL "general")
            continue()
        endif()
        if(EXISTS "${_intl_item}")
            list(APPEND _intl_search_libs "${_intl_item}")
        endif()
    endforeach()

    set(_intl_release_bins)
    set(_intl_debug_bins)
    foreach(_intl_lib IN LISTS _intl_search_libs)
        cmake_path(GET _intl_lib PARENT_PATH _intl_lib_dir)
        cmake_path(GET _intl_lib_dir PARENT_PATH _intl_prefix)
        set(_intl_bin "${_intl_prefix}/bin")
        if(NOT EXISTS "${_intl_bin}")
            continue()
        endif()
        cmake_path(NORMAL_PATH _intl_bin)
        if(_intl_bin MATCHES "[/\\\\]debug[/\\\\]bin$")
            list(APPEND _intl_debug_bins "${_intl_bin}")
        else()
            list(APPEND _intl_release_bins "${_intl_bin}")
        endif()
    endforeach()

    set(_intl_bins "${_intl_release_bins}")
    if(NOT _intl_bins)
        set(_intl_bins "${_intl_debug_bins}")
    endif()
    if(_intl_bins)
        list(REMOVE_DUPLICATES _intl_bins)
    endif()

    set(_intl_runtime_dlls)
    foreach(_intl_bin IN LISTS _intl_bins)
        file(GLOB _intl_found
            "${_intl_bin}/*intl*.dll"
            "${_intl_bin}/*iconv*.dll"
            "${_intl_bin}/*charset*.dll"
        )
        list(APPEND _intl_runtime_dlls ${_intl_found})
    endforeach()
    if(_intl_runtime_dlls)
        list(REMOVE_DUPLICATES _intl_runtime_dlls)
    endif()

    set(_intl_has_runtime FALSE)
    foreach(_dll IN LISTS _intl_runtime_dlls)
        cmake_path(GET _dll FILENAME _intl_dll_name)
        if(_intl_dll_name MATCHES "[Ii]ntl")
            set(_intl_has_runtime TRUE)
        endif()
    endforeach()
    if(NOT _intl_has_runtime)
        message(FATAL_ERROR
            "CoreDeck links shared libintl (${Intl_LIBRARY}) but no intl runtime DLL "
            "was found beside it. The Windows package would fail at startup because "
            "intl-8.dll was not found. Install shared gettext and libiconv "
            "(vcpkg triplet x64-windows).")
    endif()
    message(STATUS "Windows libintl runtime: ${_intl_runtime_dlls}")

    foreach(_dll IN LISTS _intl_runtime_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            VERBATIM
        )
    endforeach()
    set(COREDECK_INTL_RUNTIME_DLLS ${_intl_runtime_dlls})
endif()

if(UNIX AND NOT APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/assets/icons/icon.png
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/icons/icon.png
    )
endif()

if(COREDECK_SENTRY_ENABLED)
    if(APPLE)
        target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-no_warn_duplicate_libraries")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:crashpad_handler>
            $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/MacOS/crashpad_handler
        )
    else()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:crashpad_handler>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_NAME:crashpad_handler>
        )
    endif()
endif()
