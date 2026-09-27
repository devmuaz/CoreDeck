#include <catch2/catch_test_macros.hpp>

#include "core/jdk.h"

using namespace CoreDeck;

namespace {
    const EnvVar *FindEnv(const EnvVars &env, const std::string &name) {
        for (const auto &var: env) {
            if (var.Name == name) {
                return &var;
            }
        }
        return nullptr;
    }
}

TEST_CASE("JavaToolEnvironment skips the cmdline-tools version check for JDK 17+", "[jdk]") {
    JdkInfo jdk;
    jdk.JavaHome = "C:/Program Files/Java/jdk-27";
    jdk.MajorVersion = 27;
    jdk.IsFound = true;
    jdk.IsValid = true;

    const EnvVars env = JavaToolEnvironment(jdk);

    const EnvVar *skip = FindEnv(env, "SKIP_JDK_VERSION_CHECK");
    REQUIRE(skip != nullptr);
    REQUIRE(skip->Value == "1");
    REQUIRE(FindEnv(env, "JAVA_HOME") != nullptr);
    REQUIRE(FindEnv(env, "JAVA_HOME")->Value == jdk.JavaHome);
}

TEST_CASE("JavaToolEnvironment leaves the cmdline-tools version check in place for older JDKs", "[jdk]") {
    JdkInfo jdk;
    jdk.JavaHome = "/usr/lib/jvm/java-11";
    jdk.MajorVersion = 11;
    jdk.IsFound = true;
    jdk.IsValid = false;

    const EnvVars env = JavaToolEnvironment(jdk);

    REQUIRE(FindEnv(env, "SKIP_JDK_VERSION_CHECK") == nullptr);
    REQUIRE(FindEnv(env, "JAVA_HOME") != nullptr);
}

TEST_CASE("JavaToolEnvironment is empty without a JDK home", "[jdk]") {
    REQUIRE(JavaToolEnvironment({}).empty());
}
