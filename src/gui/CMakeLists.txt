add_library(coredeck_gui STATIC
    adb/adb_connections_tab.cpp
    adb/adb_controller.cpp
    adb/adb_devices_dialog.cpp
    adb/adb_logcat_tab.cpp
    adb/adb_package_list_dialog.cpp
    adb/adb_packages_files_tab.cpp
    adb/adb_shell_tab.cpp
    adb/adb_widgets.cpp
    adb/adb_wifi_help_dialog.cpp
    adb/adb_workspace.cpp
    apk/apk_analyzer_workspace.cpp
    apk/apk_signer_workspace.cpp
    apk/jar_signer_workspace.cpp
    apk/keytool_workspace.cpp
    home/avd_details_panel.cpp
    home/avd_list_panel.cpp
    home/avd_logs_panel.cpp
    home/avd_options_panel.cpp
    home/create_avd_dialog.cpp
    home/device_profile_dialog.cpp
    home/install_image_dialog.cpp
    home/skin_dialog.cpp
    home/storage_dialog.cpp
    preferences/preferences_android_sdk_section.cpp
    preferences/preferences_dialog.cpp
    preferences/preferences_general_section.cpp
    preferences/preferences_java_section.cpp
    sdk/accept_licenses_dialog.cpp
    sdk/health_check_banner.cpp
    sdk/health_check_dialog.cpp
    sdk/onboarding_screen.cpp
    shell/about_dialog.cpp
    shell/application.cpp
    shell/main_menu_bar.cpp
    shell/update_dialog.cpp
    theme/theme.cpp
    widgets/banner.cpp
    widgets/buttons.cpp
    widgets/chrome.cpp
    widgets/details.cpp
    widgets/feedback.cpp
    widgets/form.cpp
    widgets/log_viewer.cpp
    widgets/style.cpp
)

target_precompile_headers(coredeck_gui PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_sources(coredeck_gui PRIVATE
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
if(WIN32)
    target_compile_definitions(coredeck_gui PRIVATE "IMGUI_IMPL_API=")
endif()
target_link_libraries(coredeck_gui PUBLIC coredeck_app coredeck_core imgui)
target_link_libraries(coredeck_gui PRIVATE glfw OpenGL::GL)

if(WIN32)
    target_link_libraries(coredeck_gui PRIVATE dwmapi)
elseif(APPLE)
    target_sources(coredeck_gui PRIVATE theme/theme_mac.mm)
    set_source_files_properties(theme/theme_mac.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_compile_definitions(coredeck_gui PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(coredeck_gui PRIVATE "-framework CoreFoundation")
endif()
