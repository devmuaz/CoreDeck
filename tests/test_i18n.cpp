#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/i18n.h"

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
            SetLocalesDirectory(m_Directory.string());
        }

        ~CatalogFixture() {
            SetLanguage("en");
            SetLocalesDirectory({});
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
            REQUIRE(std::system(command.c_str()) == 0);
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
    CatalogFixture fixture;
    SetLanguage("en");
    REQUIRE(std::string(Tr("Stop")) == "Stop");
    REQUIRE(TrFormat("Hello {0}", "Ada") == "Hello Ada");
    REQUIRE(TrFormatN("{0} system image installed.", "{0} system images installed.", 2, 2) == "2 system images installed.");
}

TEST_CASE("Catalog lookup keeps context, escapes, and multiline strings", "[i18n]") {
    CatalogFixture fixture;
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

    SetLanguage("fr");
    REQUIRE(ActiveLanguage() == "fr");
    REQUIRE(std::string(Tr("Stop")) == "Arrêter");
    REQUIRE(std::string(Tr("Open")) == "Ouvert");
    REQUIRE(std::string(TrC("menu", "Open")) == "Ouvrir");
    REQUIRE(std::string(Tr("Line\nbreak")) == "Saut\nde ligne");
    REQUIRE(std::string(Tr("Skip")) == "Skip");
}

TEST_CASE("Plural formulas select the translated form", "[i18n]") {
    CatalogFixture fixture;
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

    SetLanguage("fr");
    REQUIRE(TrFormatN("{0} file", "{0} files", 0, 0) == "0 fichier");
    REQUIRE(TrFormatN("{0} file", "{0} files", 1, 1) == "1 fichier");
    REQUIRE(TrFormatN("{0} file", "{0} files", 2, 2) == "2 fichiers");

    SetLanguage("pl");
    REQUIRE(TrFormatN("{0} file", "{0} files", 1, 1) == "one");
    REQUIRE(TrFormatN("{0} file", "{0} files", 2, 2) == "few");
    REQUIRE(TrFormatN("{0} file", "{0} files", 5, 5) == "many");
    REQUIRE(TrFormatN("{0} file", "{0} files", 12, 12) == "many");
    REQUIRE(TrFormatN("{0} file", "{0} files", 22, 22) == "few");

    SetLanguage("zh-CN");
    REQUIRE(TrFormatN("{0} file", "{0} files", 1, 1) == "1 个文件");
    REQUIRE(TrFormatN("{0} file", "{0} files", 8, 8) == "8 个文件");
}

TEST_CASE("Broken placeholders fall back to the English pattern", "[i18n]") {
    CatalogFixture fixture;
    fixture.Write(
        "fr",
        Header("nplurals=2; plural=(n > 1);") +
            "msgid \"Save {0}\"\n"
            "msgstr \"{1} / {0}\"\n\n"
            "msgid \"File {0}\"\n"
            "msgstr \"Fichier {\"\n"
    );

    SetLanguage("fr");
    REQUIRE(TrFormat("Save {0}", "now") == "Save now");
    REQUIRE(TrFormat("File {0}", "a.txt") == "File a.txt");
}

TEST_CASE("Placeholders can be reordered", "[i18n]") {
    CatalogFixture fixture;
    fixture.Write(
        "fr",
        Header("nplurals=2; plural=(n > 1);") +
            "msgid \"{0} then {1}\"\n"
            "msgstr \"{1} puis {0}\"\n"
    );
    SetLanguage("fr");
    REQUIRE(TrFormat("{0} then {1}", "one", "two") == "two puis one");
    REQUIRE(TrFormat("Literal {{0}}") == "Literal {0}");
}

TEST_CASE("Region tags do not cross Chinese scripts", "[i18n]") {
    CatalogFixture fixture;
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

    SetLanguage("zh-CN");
    REQUIRE(ActiveLanguage() == "zh-Hans");
    REQUIRE(std::string(Tr("Stop")) == "停止");

    SetLanguage("zh-TW");
    REQUIRE(ActiveLanguage() == "zh");
    REQUIRE(std::string(Tr("Stop")) == "通用");

    fixture.Write(
        "zh-TW",
        Header("nplurals=1; plural=0;") +
            "msgid \"Stop\"\n"
            "msgstr \"停止\"\n"
    );
    SetLanguage("zh-TW");
    REQUIRE(ActiveLanguage() == "zh-TW");
}

TEST_CASE("Shipped French and German catalogs translate the interface", "[i18n]") {
    CatalogFixture fixture;
    SetLocalesDirectory(COREDECK_LOCALES_DIR);

    SetLanguage("fr");
    REQUIRE(ActiveLanguage() == "fr");
    REQUIRE(std::string(Tr("Cancel")) == "Annuler");
    REQUIRE(std::string(Tr("Stop")) == "Arrêter");
    REQUIRE(TrFormat("Found {0}.", "17.0.1") == "17.0.1 a été trouvé.");
    REQUIRE(TrFormat("Run the \"{0}\" AVD to view logs", "Pixel") == "Lancez l'AVD « Pixel » pour afficher les journaux");
    REQUIRE(std::string(Tr("Failed to create window.\nYour system may not support OpenGL 3.3.")) == "Impossible de créer la fenêtre.\nVotre système ne prend peut-être pas en charge OpenGL 3.3.");
    REQUIRE(TrFormatN("{0} file", "{0} files", 0, "0") == "0 fichier");
    REQUIRE(TrFormatN("{0} file", "{0} files", 1, "1") == "1 fichier");
    REQUIRE(TrFormatN("{0} file", "{0} files", 2, "2") == "2 fichiers");

    SetLanguage("de");
    REQUIRE(ActiveLanguage() == "de");
    REQUIRE(std::string(Tr("Cancel")) == "Abbrechen");
    REQUIRE(std::string(Tr("Quit")) == "Beenden");
    REQUIRE(std::string(Tr("Stop")) == "Stoppen");
    REQUIRE(TrFormatN("{0} system image installed.", "{0} system images installed.", 1, "1") == "1 System-Image installiert.");
    REQUIRE(TrFormatN("{0} system image installed.", "{0} system images installed.", 0, "0") == "0 System-Images installiert.");
    REQUIRE(TrFormatN("{0} system image installed.", "{0} system images installed.", 2, "2") == "2 System-Images installiert.");
    REQUIRE(LanguageEndonym("fr") == "Français");
    REQUIRE(LanguageEndonym("de") == "Deutsch");

    SetLanguage("zh-CN");
    REQUIRE(ActiveLanguage() == "zh-Hans");
    REQUIRE(std::string(Tr("Cancel")) == "取消");
    REQUIRE(std::string(Tr("File")) == "文件");
    REQUIRE(std::string(Tr("Delete")) == "删除");
    REQUIRE(std::string(Tr("Quit")) == "退出");
    REQUIRE(TrFormat("Found {0}.", "17.0.1") == "已找到 17.0.1。");
    REQUIRE(TrFormat("Run the \"{0}\" AVD to view logs", "Pixel") == "运行“Pixel”AVD 以查看日志");
    REQUIRE(std::string(Tr("Failed to create window.\nYour system may not support OpenGL 3.3.")) == "创建窗口失败。\n您的系统可能不支持 OpenGL 3.3。");
    REQUIRE(TrFormatN("{0} file", "{0} files", 0, "0") == "0 个文件");
    REQUIRE(TrFormatN("{0} file", "{0} files", 1, "1") == "1 个文件");
    REQUIRE(TrFormatN("{0} file", "{0} files", 2, "2") == "2 个文件");
    REQUIRE(TrFormatN("{0} system image installed.", "{0} system images installed.", 1, "1") == "已安装 1 个系统映像。");
    REQUIRE(LanguageEndonym("zh-Hans") == "简体中文");

    SetLanguage("zh-TW");
    REQUIRE(ActiveLanguage() == "zh-Hant");
    REQUIRE(std::string(Tr("File")) == "檔案");
    REQUIRE(std::string(Tr("Delete")) == "刪除");
    REQUIRE(std::string(Tr("Quit")) == "結束");
    REQUIRE(TrFormat("Run the \"{0}\" AVD to view logs", "Pixel") == "執行「Pixel」AVD 以查看記錄");
    REQUIRE(TrFormatN("{0} file", "{0} files", 2, "2") == "2 個檔案");
    REQUIRE(TrFormatN("{0} system image installed.", "{0} system images installed.", 0, "0") == "已安裝 0 個系統映像。");
    REQUIRE(LanguageEndonym("zh-Hant") == "繁體中文");

    SetLanguage("zh-HK");
    REQUIRE(ActiveLanguage() == "zh-Hant");
    REQUIRE(std::string(Tr("Delete")) == "刪除");
}

TEST_CASE("Available languages lists catalog files", "[i18n]") {
    CatalogFixture fixture;
    fixture.Write("fr", Header("nplurals=2; plural=(n > 1);"));
    fixture.Write("zh-CN", Header("nplurals=1; plural=0;"));
    const std::vector<std::string> tags = AvailableLanguages();
    REQUIRE(tags.size() == 2);
    REQUIRE(tags[0] == "fr");
    REQUIRE(tags[1] == "zh-CN");
    REQUIRE(LanguageEndonym("zh-CN") == "简体中文");
    REQUIRE(LanguageEndonym("zh-TW") == "繁體中文");
    REQUIRE_FALSE(SystemLanguage().empty());
}
