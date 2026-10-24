set(COREDECK_LOCALE_DIR "${CMAKE_BINARY_DIR}/locales")
file(MAKE_DIRECTORY "${COREDECK_LOCALE_DIR}")
file(GLOB COREDECK_PO_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/locales/*.po")
set(COREDECK_MO_FILES)
foreach(_po_file IN LISTS COREDECK_PO_FILES)
    get_filename_component(_tag "${_po_file}" NAME_WE)
    set(_mo_file "${COREDECK_LOCALE_DIR}/${_tag}/LC_MESSAGES/coredeck.mo")
    add_custom_command(
        OUTPUT "${_mo_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${COREDECK_LOCALE_DIR}/${_tag}/LC_MESSAGES"
        COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" -c -o "${_mo_file}" "${_po_file}"
        DEPENDS "${_po_file}"
        VERBATIM
    )
    list(APPEND COREDECK_MO_FILES "${_mo_file}")
endforeach()
add_custom_target(coredeck_locales ALL DEPENDS ${COREDECK_MO_FILES})
add_dependencies(${PROJECT_NAME} coredeck_locales)
