#include <catch2/catch_test_macros.hpp>

#include "core/keytool.h"

using namespace CoreDeck;

TEST_CASE("ParseKeystoreAliases keeps PrivateKeyEntry aliases", "[keytool]") {
    const auto aliases = ParseKeystoreAliases(
        "Keystore type: PKCS12\n"
        "Keystore provider: SUN\n"
        "\n"
        "Your keystore contains 3 entries\n"
        "\n"
        "myalias, 1 Jan 2024, PrivateKeyEntry,\n"
        "Certificate fingerprint (SHA-256): C0:F4:9F:70:62:09:75:8C:E2:FD:34:7A:2B:E9:4D:77:E0:CD:4D:7F:8A:F1:EB:A8:C9:5F:4C:39:9F:54:EC:0B\n"
        "my-release, 14 Oct 2026, PrivateKeyEntry, \n"
        "Certificate fingerprint (SHA-256): AA:BB:CC:DD\n"
        "platform, 1 Jan 2024, trustedCertEntry,\n"
        "Certificate fingerprint (SHA-256): 11:22:33:44\n"
    );

    REQUIRE(aliases.size() == 2);
    REQUIRE(aliases.at(0) == "myalias");
    REQUIRE(aliases.at(1) == "my-release");
}
