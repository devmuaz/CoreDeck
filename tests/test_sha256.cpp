#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/sha256.h"

using namespace CoreDeck;

TEST_CASE("Sha256Hex matches the NIST test vectors", "[sha256]") {
    REQUIRE(Sha256Hex(std::string("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    REQUIRE(Sha256Hex(std::string("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(
        Sha256Hex(std::string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
    );
}

TEST_CASE("Sha256Hex handles input spanning many blocks", "[sha256]") {
    const std::string million(1000000, 'a');
    REQUIRE(Sha256Hex(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("Sha256File hashes file contents", "[sha256][file]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "coredeck_sha256_test.bin";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "abc";
    }

    REQUIRE(Sha256File(path.string()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::filesystem::remove(path);
}

TEST_CASE("Sha256File returns empty for a missing file", "[sha256][file]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "coredeck_sha256_missing.bin";
    std::filesystem::remove(path);

    REQUIRE(Sha256File(path.string()).empty());
}

TEST_CASE("EqualsIgnoreCaseHex compares case-insensitively", "[sha256]") {
    REQUIRE(EqualsIgnoreCaseHex("ABCDEF", "abcdef"));
    REQUIRE_FALSE(EqualsIgnoreCaseHex("abcdef", "abcde"));
    REQUIRE_FALSE(EqualsIgnoreCaseHex("abcdef", "abcdee"));
}
