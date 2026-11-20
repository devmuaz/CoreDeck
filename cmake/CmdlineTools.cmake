file(READ "${CMAKE_SOURCE_DIR}/pins/cmdline-tools.json" _coredeck_cmdline_tools_pins)
string(JSON COREDECK_CMDLINE_TOOLS_VERSION GET "${_coredeck_cmdline_tools_pins}" version)
string(JSON COREDECK_CMDLINE_TOOLS_URL_WINDOWS GET "${_coredeck_cmdline_tools_pins}" archives windows url)
string(JSON COREDECK_CMDLINE_TOOLS_SHA256_WINDOWS GET "${_coredeck_cmdline_tools_pins}" archives windows sha256)
string(JSON COREDECK_CMDLINE_TOOLS_SIZE_WINDOWS GET "${_coredeck_cmdline_tools_pins}" archives windows size)
string(JSON COREDECK_CMDLINE_TOOLS_URL_MAC_ARM64 GET "${_coredeck_cmdline_tools_pins}" archives "mac-arm64" url)
string(JSON COREDECK_CMDLINE_TOOLS_SHA256_MAC_ARM64 GET "${_coredeck_cmdline_tools_pins}" archives "mac-arm64" sha256)
string(JSON COREDECK_CMDLINE_TOOLS_SIZE_MAC_ARM64 GET "${_coredeck_cmdline_tools_pins}" archives "mac-arm64" size)
string(JSON COREDECK_CMDLINE_TOOLS_URL_MAC_X86_64 GET "${_coredeck_cmdline_tools_pins}" archives "mac-x86_64" url)
string(JSON COREDECK_CMDLINE_TOOLS_SHA256_MAC_X86_64 GET "${_coredeck_cmdline_tools_pins}" archives "mac-x86_64" sha256)
string(JSON COREDECK_CMDLINE_TOOLS_SIZE_MAC_X86_64 GET "${_coredeck_cmdline_tools_pins}" archives "mac-x86_64" size)
string(JSON COREDECK_CMDLINE_TOOLS_URL_LINUX GET "${_coredeck_cmdline_tools_pins}" archives linux url)
string(JSON COREDECK_CMDLINE_TOOLS_SHA256_LINUX GET "${_coredeck_cmdline_tools_pins}" archives linux sha256)
string(JSON COREDECK_CMDLINE_TOOLS_SIZE_LINUX GET "${_coredeck_cmdline_tools_pins}" archives linux size)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/core/sdk/cmdline_tools_release.h.in"
    "${CMAKE_BINARY_DIR}/generated/core/cmdline_tools_release.h"
    @ONLY
)
