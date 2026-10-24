if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/resources.rc.in ${CMAKE_BINARY_DIR}/resources.rc @ONLY)
    if(MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
    elseif(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE "-mwindows")
    endif()
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/resources.rc)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_BUILD_NUMBER}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
        MACOSX_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_INFO_STRING "${APP_DESCRIPTION_SHORT}"
        MACOSX_BUNDLE_COPYRIGHT "${APP_COPYRIGHT}"
    )
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.icns")
        set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/assets/icons/icon.icns"
            "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/icon.icns"
        )
    endif()
endif()

if(WIN32)
    set(CPACK_GENERATOR "WIX;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/cpack/dmg-setup.applescript")

    # DMG background image
    set(_dmg_bg_1x "${CMAKE_SOURCE_DIR}/cpack/dmg-background.png")
    set(_dmg_bg_2x "${CMAKE_SOURCE_DIR}/cpack/dmg-background@2x.png")
    if(EXISTS "${_dmg_bg_1x}" AND EXISTS "${_dmg_bg_2x}")
        set(_dmg_bg_tiff "${CMAKE_BINARY_DIR}/dmg-background.tiff")
        execute_process(
            COMMAND tiffutil -cathidpicheck "${_dmg_bg_1x}" "${_dmg_bg_2x}" -out "${_dmg_bg_tiff}"
            RESULT_VARIABLE _tiffutil_rc
            OUTPUT_QUIET ERROR_QUIET
        )
        if(_tiffutil_rc EQUAL 0 AND EXISTS "${_dmg_bg_tiff}")
            set(CPACK_DMG_BACKGROUND_IMAGE "${_dmg_bg_tiff}")
            message(STATUS "DMG background: multi-resolution TIFF (1x + @2x)")
        else()
            set(CPACK_DMG_BACKGROUND_IMAGE "${_dmg_bg_1x}")
            message(WARNING "tiffutil failed; falling back to 1x PNG only")
        endif()
    elseif(EXISTS "${_dmg_bg_1x}")
        set(CPACK_DMG_BACKGROUND_IMAGE "${_dmg_bg_1x}")
        message(STATUS "DMG background: 1x PNG only (add dmg-background@2x.png for Retina)")
    endif()

    # The DMG should only contain CoreDeck.app + an Applications symlink.
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/cpack/StripMacOSExtras.cmake")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${APP_VENDOR}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_DESCRIPTION_SHORT}")
set(CPACK_PACKAGE_DESCRIPTION "${APP_DESCRIPTION}")

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

if(COREDECK_SENTRY_ENABLED AND NOT APPLE)
    install(PROGRAMS $<TARGET_FILE:crashpad_handler> DESTINATION .)
endif()

# CPack packages install() rules only. Copying these DLLs next to the build
# output is not enough: Windows loads intl-8.dll (and the iconv/charset DLLs
# it imports) from the executable directory.
if(WIN32 AND COREDECK_INTL_RUNTIME_DLLS)
    install(FILES ${COREDECK_INTL_RUNTIME_DLLS} DESTINATION .)
endif()

if(NOT APPLE)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/fonts DESTINATION assets)
    install(DIRECTORY ${COREDECK_LOCALE_DIR}/ DESTINATION assets/locales)
endif()

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/assets/icons/icon.png DESTINATION assets/icons)
endif()

if(WIN32)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_NAME}")
    set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}")
    set(CPACK_WIX_UPGRADE_GUID "B8F3E5D2-4A7C-4D2B-9E1F-6C8A5B3D7E9F")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "${PROJECT_NAME}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/devmuaz")
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/LICENSE.rtf")
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icons/icon.ico")
    endif()
endif()

include(CPack)
