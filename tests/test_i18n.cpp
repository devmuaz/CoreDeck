#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "platform/i18n.h"

using namespace CoreDeck::Platform;
using namespace CoreDeck;

namespace {
    class CatalogFixture {
    public:
        CatalogFixture() {
            // libintl caches a domain by directory path and does not reread a replaced catalog.
            static unsigned sequence = 0;
            m_Directory = std::filesystem::temp_directory_path() /
                          ("coredeck-i18n-" + std::to_string(++sequence));
            std::filesystem::remove_all(m_Directory);
            std::filesystem::create_directories(m_Directory);
            Platform::SetLocalesDirectory(m_Directory.string());
        }

        CatalogFixture(const CatalogFixture &) = delete;
        CatalogFixture &operator=(const CatalogFixture &) = delete;
        CatalogFixture(CatalogFixture &&) = delete;
        CatalogFixture &operator=(CatalogFixture &&) = delete;

        ~CatalogFixture() {
            Platform::SetLanguage("en");
            Platform::SetLocalesDirectory({});
            std::filesystem::remove_all(m_Directory);
        }

        void Write(const std::string &tag, const std::string &body) const {
            const std::filesystem::path po = m_Directory / (tag + ".po");
            {
                std::ofstream file(po);
                file << body;
            }
            const std::filesystem::path messages = m_Directory / tag / "LC_MESSAGES";
            std::filesystem::create_directories(messages);
            const std::filesystem::path mo = messages / "coredeck.mo";
            const std::string command = Quote(COREDECK_MSGFMT) + " -c -o " + Quote(mo.string()) + " " + Quote(po.string());
#ifdef _WIN32
            // cmd.exe /c strips the first and last quote when the line has more than one quoted path.
            const std::string invocation = "\"" + command + "\"";
#else
            const std::string &invocation = command;
#endif
            REQUIRE(std::system(invocation.c_str()) == 0);
        }

        static std::string Quote(const std::string &value) {
            return "\"" + value + "\"";
        }

    private:
        std::filesystem::path m_Directory;
    };

    std::string Header(const std::string &plural) {
        return "msgid \"\"\n"
               "msgstr \"\"\n"
               "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
               "\"Plural-Forms: " +
               plural + "\\n\"\n\n";
    }
}

TEST_CASE("Missing translations fall back to the source text", "[i18n]") {
    const CatalogFixture fixture;
    Platform::SetLanguage("en");
    REQUIRE(std::string(Platform::Tr("Stop")) == "Stop");
    REQUIRE(Platform::TrFormat("Hello {0}", "Ada") == "Hello Ada");
    REQUIRE(Platform::TrFormatN("{0} system image installed.", "{0} system images installed.", 2, 2) == "2 system images installed.");
}

TEST_CASE("Catalog lookup keeps context, escapes, and multiline strings", "[i18n]") {
    const CatalogFixture fixture;
    fixture.Write(
        "fr",
        Header("nplurals=2; plural=(n > 1);") +
            "msgid \"Stop\"\n"
            "msgstr \"Arrêter\"\n\n"
            "msgctxt \"menu\"\n"
            "msgid \"Open\"\n"
            "msgstr \"Ouvrir\"\n\n"
            "msgid \"Open\"\n"
            "msgstr \"Ouvert\"\n\n"
            "msgid \"Line\\n\" \"break\"\n"
            "msgstr \"Saut\\n\" \"de ligne\"\n\n"
            "#, fuzzy\n"
            "msgid \"Skip\"\n"
            "msgstr \"Ignorer\"\n"
    );

    Platform::SetLanguage("fr");
    REQUIRE(Platform::ActiveLanguage() == "fr");
    REQUIRE(std::string(Platform::Tr("Stop")) == "Arrêter");
    REQUIRE(std::string(Platform::Tr("Open")) == "Ouvert");
    REQUIRE(std::string(Platform::TrC("menu", "Open")) == "Ouvrir");
    REQUIRE(std::string(Platform::Tr("Line\nbreak")) == "Saut\nde ligne");
    REQUIRE(std::string(Platform::Tr("Skip")) == "Skip");
}

TEST_CASE("Plural formulas select the translated form", "[i18n]") {
    const CatalogFixture fixture;
    fixture.Write(
        "fr",
        Header("nplurals=2; plural=(n > 1);") +
            "msgid \"{0} file\"\n"
            "msgid_plural \"{0} files\"\n"
            "msgstr[0] \"{0} fichier\"\n"
            "msgstr[1] \"{0} fichiers\"\n"
    );
    fixture.Write(
        "pl",
        Header("nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);") +
            "msgid \"{0} file\"\n"
            "msgid_plural \"{0} files\"\n"
            "msgstr[0] \"one\"\n"
            "msgstr[1] \"few\"\n"
            "msgstr[2] \"many\"\n"
    );
    fixture.Write(
        "zh-CN",
        Header("nplurals=1; plural=0;") +
            "msgid \"{0} file\"\n"
            "msgid_plural \"{0} files\"\n"
            "msgstr[0] \"{0} 个文件\"\n"
    );

    Platform::SetLanguage("fr");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 0, 0) == "0 fichier");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 1, 1) == "1 fichier");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 2, 2) == "2 fichiers");

    Platform::SetLanguage("pl");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 1, 1) == "one");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 2, 2) == "few");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 5, 5) == "many");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 12, 12) == "many");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 22, 22) == "few");

    Platform::SetLanguage("zh-CN");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 1, 1) == "1 个文件");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 8, 8) == "8 个文件");
}

TEST_CASE("Broken placeholders fall back to the English pattern", "[i18n]") {
    const CatalogFixture fixture;
    fixture.Write(
        "fr",
        Header("nplurals=2; plural=(n > 1);") +
            "msgid \"Save {0}\"\n"
            "msgstr \"{1} / {0}\"\n\n"
            "msgid \"File {0}\"\n"
            "msgstr \"Fichier {\"\n"
    );

    Platform::SetLanguage("fr");
    REQUIRE(Platform::TrFormat("Save {0}", "now") == "Save now");
    REQUIRE(Platform::TrFormat("File {0}", "a.txt") == "File a.txt");
}

TEST_CASE("Placeholders can be reordered", "[i18n]") {
    const CatalogFixture fixture;
    fixture.Write(
        "fr",
        Header("nplurals=2; plural=(n > 1);") +
            "msgid \"{0} then {1}\"\n"
            "msgstr \"{1} puis {0}\"\n"
    );
    Platform::SetLanguage("fr");
    REQUIRE(Platform::TrFormat("{0} then {1}", "one", "two") == "two puis one");
    REQUIRE(Platform::TrFormat("Literal {{0}}") == "Literal {0}");
}

TEST_CASE("Region tags do not cross Chinese scripts", "[i18n]") {
    const CatalogFixture fixture;
    fixture.Write(
        "zh-Hans",
        Header("nplurals=1; plural=0;") +
            "msgid \"Stop\"\n"
            "msgstr \"停止\"\n"
    );
    fixture.Write(
        "zh",
        Header("nplurals=1; plural=0;") +
            "msgid \"Stop\"\n"
            "msgstr \"通用\"\n"
    );

    Platform::SetLanguage("zh-CN");
    REQUIRE(Platform::ActiveLanguage() == "zh-Hans");
    REQUIRE(std::string(Platform::Tr("Stop")) == "停止");

    Platform::SetLanguage("zh-TW");
    REQUIRE(Platform::ActiveLanguage() == "zh");
    REQUIRE(std::string(Platform::Tr("Stop")) == "通用");

    fixture.Write(
        "zh-TW",
        Header("nplurals=1; plural=0;") +
            "msgid \"Stop\"\n"
            "msgstr \"停止\"\n"
    );
    Platform::SetLanguage("zh-TW");
    REQUIRE(Platform::ActiveLanguage() == "zh-TW");
}

TEST_CASE("Shipped French and German catalogs translate the interface", "[i18n]") {
    const CatalogFixture fixture;
    Platform::SetLocalesDirectory(COREDECK_LOCALES_DIR);

    Platform::SetLanguage("fr");
    REQUIRE(Platform::ActiveLanguage() == "fr");
    REQUIRE(std::string(Platform::Tr("Cancel")) == "Annuler");
    REQUIRE(std::string(Platform::Tr("Stop")) == "Arrêter");
    REQUIRE(Platform::TrFormat("Found {0}.", "17.0.1") == "17.0.1 a été trouvé.");
    REQUIRE(Platform::TrFormat("Run the \"{0}\" AVD to view logs", "Pixel") == "Lancez l'AVD « Pixel » pour afficher les journaux");
    REQUIRE(std::string(Platform::Tr("Failed to create window.\nYour system may not support OpenGL 3.3.")) == "Impossible de créer la fenêtre.\nVotre système ne prend peut-être pas en charge OpenGL 3.3.");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 0, "0") == "0 fichier");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 1, "1") == "1 fichier");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 2, "2") == "2 fichiers");

    Platform::SetLanguage("de");
    REQUIRE(Platform::ActiveLanguage() == "de");
    REQUIRE(std::string(Platform::Tr("Cancel")) == "Abbrechen");
    REQUIRE(std::string(Platform::Tr("Quit")) == "Beenden");
    REQUIRE(std::string(Platform::Tr("Stop")) == "Stoppen");
    REQUIRE(Platform::TrFormatN("{0} system image installed.", "{0} system images installed.", 1, "1") == "1 System-Image installiert.");
    REQUIRE(Platform::TrFormatN("{0} system image installed.", "{0} system images installed.", 0, "0") == "0 System-Images installiert.");
    REQUIRE(Platform::TrFormatN("{0} system image installed.", "{0} system images installed.", 2, "2") == "2 System-Images installiert.");
    REQUIRE(Platform::LanguageEndonym("fr") == "Français");
    REQUIRE(Platform::LanguageEndonym("de") == "Deutsch");

    Platform::SetLanguage("zh-CN");
    REQUIRE(Platform::ActiveLanguage() == "zh-Hans");
    REQUIRE(std::string(Platform::Tr("Cancel")) == "取消");
    REQUIRE(std::string(Platform::Tr("File")) == "文件");
    REQUIRE(std::string(Platform::Tr("Delete")) == "删除");
    REQUIRE(std::string(Platform::Tr("Quit")) == "退出");
    REQUIRE(Platform::TrFormat("Found {0}.", "17.0.1") == "已找到 17.0.1。");
    REQUIRE(Platform::TrFormat("Run the \"{0}\" AVD to view logs", "Pixel") == "运行“Pixel”AVD 以查看日志");
    REQUIRE(std::string(Platform::Tr("Failed to create window.\nYour system may not support OpenGL 3.3.")) == "创建窗口失败。\n您的系统可能不支持 OpenGL 3.3。");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 0, "0") == "0 个文件");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 1, "1") == "1 个文件");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 2, "2") == "2 个文件");
    REQUIRE(Platform::TrFormatN("{0} system image installed.", "{0} system images installed.", 1, "1") == "已安装 1 个系统映像。");
    REQUIRE(Platform::LanguageEndonym("zh-Hans") == "简体中文");

    Platform::SetLanguage("zh-TW");
    REQUIRE(Platform::ActiveLanguage() == "zh-Hant");
    REQUIRE(std::string(Platform::Tr("File")) == "檔案");
    REQUIRE(std::string(Platform::Tr("Delete")) == "刪除");
    REQUIRE(std::string(Platform::Tr("Quit")) == "結束");
    REQUIRE(Platform::TrFormat("Run the \"{0}\" AVD to view logs", "Pixel") == "執行「Pixel」AVD 以查看記錄");
    REQUIRE(Platform::TrFormatN("{0} file", "{0} files", 2, "2") == "2 個檔案");
    REQUIRE(Platform::TrFormatN("{0} system image installed.", "{0} system images installed.", 0, "0") == "已安裝 0 個系統映像。");
    REQUIRE(Platform::LanguageEndonym("zh-Hant") == "繁體中文");

    Platform::SetLanguage("zh-HK");
    REQUIRE(Platform::ActiveLanguage() == "zh-Hant");
    REQUIRE(std::string(Platform::Tr("Delete")) == "刪除");
}

TEST_CASE("Available languages lists catalog files", "[i18n]") {
    const CatalogFixture fixture;
    fixture.Write("fr", Header("nplurals=2; plural=(n > 1);"));
    fixture.Write("zh-CN", Header("nplurals=1; plural=0;"));
    const std::vector<std::string> tags = Platform::AvailableLanguages();
    REQUIRE(tags.size() == 2);
    REQUIRE(tags.at(0) == "fr");
    REQUIRE(tags.at(1) == "zh-CN");
    REQUIRE(Platform::LanguageEndonym("zh-CN") == "简体中文");
    REQUIRE(Platform::LanguageEndonym("zh-TW") == "繁體中文");
    REQUIRE_FALSE(Platform::SystemLanguage().empty());
}
