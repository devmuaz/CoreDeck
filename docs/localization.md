# Localization

English is the source language. User-visible strings stay in the C++ source, and a
translation catalog replaces them at runtime. A missing translation, or a missing
catalog, shows the English text.

Translators edit GNU gettext `.po` files. The build compiles each one with `msgfmt`
into `assets/locales/<tag>/LC_MESSAGES/coredeck.mo`. At runtime CoreDeck loads those
binary catalogs with libintl. `SetLanguage` calls `setlocale(LC_MESSAGES, "")` only,
so `LC_NUMERIC` stays untouched and decimal parsing keeps the dot.

## Adding a language

1. Copy `locales/coredeck.pot` to `locales/<tag>.po`. The tag is a BCP 47 language tag:
   `fr`, `pt-BR`, `zh-Hans`, `zh-Hant`.
2. Fill in the header. Set `Language` to the same tag, `charset=UTF-8`, and a real
   `Plural-Forms` line. Remove the `fuzzy` flag on the header.
3. Translate `msgstr` entries. Leave `msgid` unchanged. An empty `msgstr` falls back to
   English. Entries marked `#, fuzzy` are ignored until the flag is removed.

Chinese scripts are separate catalogs. `zh-CN` and `zh-SG` load `zh-Hans` when that file
exists. `zh-TW`, `zh-HK`, and `zh-MO` load `zh-Hant`. A Traditional catalog must not be
named `zh-CN` or `zh-Hans`.

The compiled catalog is copied next to the app
(`assets/locales/<tag>/LC_MESSAGES/coredeck.mo`) on build. Preferences → General lists
every language directory that contains that file. An empty language preference follows
the operating system and falls back to English. French (`locales/fr.po`), German
(`locales/de.po`), Simplified Chinese (`locales/zh-Hans.po`), and Traditional Chinese
(`locales/zh-Hant.po`) are included.

Endonyms in that list (English, 简体中文, 繁體中文, …) are not translated.

## Placeholders and plurals

Numbered placeholders keep their braces. Callers pass the values already converted to
text:

```
msgid "Found {0}, which is older than JDK {1}."
msgstr "JDK {1} より古い {0} が見つかりました。"
```

`{0}` may be reordered. A literal brace is written `{{` or `}}`. A broken placeholder
falls back to the English pattern.

Plural entries have `msgid` and `msgid_plural`. Fill every `msgstr[n]` the header's
`Plural-Forms` asks for. Chinese uses `nplurals=1; plural=0;`.

Do not add a bare `%`. Some widgets print with a format string, and a stray `%s` or `%d`
in a translation will not receive an argument.

## What stays in English

Logs, emulator and sdkmanager output, paths, AVD names, package ids, and internal error
text from the operating system stay in English. Product names (CoreDeck, Android, JDK)
stay as names inside a translated sentence. ImGui widget ids (`###Options`, `##Theme`)
are not messages.

Right-to-left layout is not supported. The bundled fonts cover Latin, Cyrillic, Greek,
and Chinese, Japanese, and Korean. Arabic and Hebrew can be translated, but those glyphs
are not in the current fonts, and the layout stays left to right.

## Refreshing the catalogs

After strings change, from the repository root:

```
tools/update_translations.sh
```

The script needs GNU gettext (`xgettext` and `msgmerge`). On macOS, Homebrew
installs them with `brew install gettext`. It rewrites `locales/coredeck.pot`
from `Tr`, `TrNoop`, `TrFormat`, and the other helpers in `src`, sets `charset=UTF-8` in
the template header, and merges that template into every `locales/*.po` file.
New strings arrive with an empty `msgstr`. A slightly changed English sentence
keeps its old translation and is marked `#, fuzzy` until that flag is removed.

Fill in the new translations, then rebuild. The build compiles each `.po` file
into a `.mo` catalog. This refresh is for developers. It is not part of the
release build.
