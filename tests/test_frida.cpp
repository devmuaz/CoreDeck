#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/frida/frida_abi.h"
#include "core/frida/frida_messages.h"
#include "core/frida/frida_prelude.h"
#include "core/frida_release.h"
#include "platform/archive.h"

using namespace CoreDeck::Core;

TEST_CASE("Android ABIs map onto Frida server assets", "[frida]") {
    REQUIRE(std::string_view(FridaAndroidAsset("arm64-v8a\n")) == "android-arm64");
    REQUIRE(std::string_view(FridaAndroidAsset("armeabi-v7a")) == "android-arm");
    REQUIRE(std::string_view(FridaAndroidAsset("armeabi")) == "android-arm");
    REQUIRE(std::string_view(FridaAndroidAsset("x86_64")) == "android-x86_64");
    REQUIRE(std::string_view(FridaAndroidAsset("x86")) == "android-x86");
    REQUIRE(FridaAndroidAsset("mips") == nullptr);
    REQUIRE(std::string_view(FirstFridaAndroidAsset("foo, arm64-v8a, x86")) == "android-arm64");
    REQUIRE(FirstFridaAndroidAsset("mips,riscv") == nullptr);
}

TEST_CASE("Frida server URLs and checksums stay pinned to the linked release", "[frida]") {
    const std::string url = FridaServerUrl("android-arm64");
    REQUIRE(url.find(FRIDA_VERSION) != std::string::npos);
    REQUIRE(url.find("frida-server-") != std::string::npos);
    REQUIRE(url.find("android-arm64.xz") != std::string::npos);
    REQUIRE(std::string(FridaServerSha256("android-arm64")).size() == 64);
    REQUIRE(FridaServerSha256("android-mips") == nullptr);
    REQUIRE(FridaServerArchiveName("android-x86") == std::string("frida-server-") + FRIDA_VERSION + "-android-x86.xz");
}

TEST_CASE("adb forward prints a single local port", "[frida]") {
    REQUIRE(ParseForwardedPort("27142\n") == 27142);
    REQUIRE(ParseForwardedPort("  5555\r\n") == 5555);
    REQUIRE_FALSE(ParseForwardedPort("error: unknown").has_value());
    REQUIRE_FALSE(ParseForwardedPort("0").has_value());
}

TEST_CASE("Managed Frida commands stay off the default server name and port", "[frida]") {
    const std::string daemon = FridaDaemonCommand(false);
    REQUIRE(daemon.find("coredeck-frida-server") != std::string::npos);
    REQUIRE(daemon.find("127.0.0.1:27099") != std::string::npos);
    REQUIRE(daemon.find("</dev/null &") != std::string::npos);
    REQUIRE(daemon.find("/frida-server") == std::string::npos);
    REQUIRE(FridaDaemonCommand(true).find("su -c '") == 0);
    REQUIRE(FridaKillCommand(false) == "pkill -f coredeck-frida-server");
    REQUIRE(FridaKillCommand(true).find("su -c '") == 0);
    REQUIRE(ShellIsRoot("uid=0(root) gid=0(root)\n"));
    REQUIRE_FALSE(ShellIsRoot("uid=2000(shell) gid=2000(shell)\n"));
}

TEST_CASE("A CoreDeck server is not mistaken for a foreign frida-server", "[frida]") {
    const char *coredeck = "root 12643 1 0 S coredeck-frida-server\n";
    const FridaServerPresence owned = FridaServersInProcessList(coredeck);
    REQUIRE(owned.CoreDeck);
    REQUIRE_FALSE(owned.Foreign);

    const FridaServerPresence truncated = FridaServersInProcessList("root 9 1 0 S coredeck-frida\n");
    REQUIRE(truncated.CoreDeck);
    REQUIRE_FALSE(truncated.Foreign);

    const FridaServerPresence foreign = FridaServersInProcessList("root 4 1 0 S frida-server\n");
    REQUIRE_FALSE(foreign.CoreDeck);
    REQUIRE(foreign.Foreign);

    const FridaServerPresence both = FridaServersInProcessList("root 4 1 0 S frida-server\nroot 9 1 0 S coredeck-frida-server\n");
    REQUIRE(both.CoreDeck);
    REQUIRE(both.Foreign);

    const FridaServerPresence none = FridaServersInProcessList("root 1 0 0 S init\n");
    REQUIRE_FALSE(none.CoreDeck);
    REQUIRE_FALSE(none.Foreign);
}

TEST_CASE("Snippet mode imports the Java bridge ahead of the buffer", "[frida]") {
    const std::string wrapped = WrapFridaSnippet("Java.perform(() => {\n  console.log('ok');\n});");
    REQUIRE(wrapped.find("import Java from 'frida-java-bridge';") == 0);
    REQUIRE(wrapped.find("globalThis.Java = Java;") != std::string::npos);
    REQUIRE(wrapped.find("Java.perform") != std::string::npos);
    REQUIRE(wrapped.back() == '\n');
}

TEST_CASE("Agent requests keep a stable JSON shape", "[frida]") {
    FridaAgentCall call;
    call.Id = 7;
    call.Method = "read";
    call.Address = "0x1000";
    call.Size = 16;
    const std::string json = FridaAgentRequest(call);
    REQUIRE(json.find("\"type\":\"coredeck\"") != std::string::npos);
    REQUIRE(json.find("\"id\":7") != std::string::npos);
    REQUIRE(json.find("\"method\":\"read\"") != std::string::npos);
    REQUIRE(json.find("\"address\":\"0x1000\"") != std::string::npos);
    REQUIRE(json.find("\"size\":16") != std::string::npos);

    call.Method = "quote";
    call.Address = "a\"b\\c";
    const std::string escaped = FridaAgentRequest(call);
    REQUIRE(escaped.find("a\\\"b\\\\c") != std::string::npos);
}

TEST_CASE("Frida console and rpc messages are split apart", "[frida]") {
    const FridaParsedMessage log = ParseFridaMessage(R"json({"type":"log","level":"info","payload":"hello"})json");
    REQUIRE(log.ConsoleKind == FridaConsoleKind::Log);
    REQUIRE(log.Console == "hello");
    REQUIRE_FALSE(log.Rpc);

    const FridaParsedMessage rpc = ParseFridaMessage(
        R"json({"type":"send","payload":{"id":3,"ok":true,"result":{"modules":[{"name":"libc.so","base":"0x10","size":4,"path":"/libc"}]}}})json"
    );
    REQUIRE(rpc.Rpc);
    REQUIRE(rpc.Ok);
    REQUIRE(rpc.Id == 3);
    const auto modules = ParseFridaModules(rpc.ResultJson);
    REQUIRE(modules.size() == 1);
    REQUIRE(modules.front().Name == "libc.so");
    REQUIRE(modules.front().Base == "0x10");
    REQUIRE(modules.front().Size == 4);

    const FridaParsedMessage hook = ParseFridaMessage(
        R"json({"type":"send","payload":{"kind":"hook","hookId":9,"text":"open()"}})json"
    );
    REQUIRE(hook.IsHook);
    REQUIRE(hook.HookId == 9);
    REQUIRE(hook.Console == "open()");

    const FridaParsedMessage error = ParseFridaMessage(
        R"json({"type":"error","description":"boom","stack":"at agent"})json"
    );
    REQUIRE(error.ConsoleKind == FridaConsoleKind::Error);
    REQUIRE(error.Console.find("boom") != std::string::npos);
}

TEST_CASE("Hex patterns accept bytes, wildcards, and text", "[frida]") {
    REQUIRE(FridaHexPattern("A", false) == "41");
    REQUIRE(FridaHexPattern("A", true) == "41 00");
    REQUIRE(FridaLooksLikeHex("41 ?? 42"));
    REQUIRE_FALSE(FridaLooksLikeHex("Activity"));
    REQUIRE(FridaNormalizeHex("4142") == std::optional<std::string>{"41 42"});
    REQUIRE(FridaNormalizeHex("41 ?? 42") == std::optional<std::string>{"41 ?? 42"});
    REQUIRE_FALSE(FridaNormalizeHex("4").has_value());
    REQUIRE_FALSE(FridaNormalizeHex("zz").has_value());
}

TEST_CASE("xz archives decompress to the original bytes", "[frida]") {
    const std::vector<std::uint8_t> archive = {
        253,
        55,
        122,
        88,
        90,
        0,
        0,
        4,
        230,
        214,
        180,
        70,
        4,
        192,
        10,
        6,
        33,
        1,
        28,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        99,
        160,
        172,
        177,
        1,
        0,
        5,
        104,
        101,
        108,
        108,
        111,
        10,
        0,
        0,
        0,
        165,
        96,
        151,
        241,
        148,
        246,
        253,
        224,
        0,
        1,
        38,
        6,
        58,
        147,
        59,
        10,
        31,
        182,
        243,
        125,
        1,
        0,
        0,
        0,
        0,
        4,
        89,
        90,
    };
    const auto directory = std::filesystem::temp_directory_path() / "coredeck-frida-xz";
    std::filesystem::create_directories(directory);
    const auto source = directory / "hello.xz";
    const auto dest = directory / "hello.txt";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(archive.data()), static_cast<std::streamsize>(archive.size()));
    }
    std::string error;
    REQUIRE(CoreDeck::Platform::DecompressXz(source.string(), dest.string(), error));
    std::string text;
    {
        std::ifstream input(dest, std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    REQUIRE(text == "hello\n");
    std::filesystem::remove_all(directory);
}
