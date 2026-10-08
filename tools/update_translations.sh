#!/usr/bin/env bash
# Extract user-visible strings into locales/coredeck.pot and merge that template
# into every locales/*.po catalog. Translations are not filled in here.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOCALES_DIR="${REPO_ROOT}/locales"
POT_FILE="${LOCALES_DIR}/coredeck.pot"

find_gettext_tool() {
    local name="$1"
    local candidate
    if command -v "${name}" >/dev/null 2>&1; then
        command -v "${name}"
        return
    fi
    for candidate in \
        "/opt/homebrew/bin/${name}" \
        "/opt/homebrew/opt/gettext/bin/${name}" \
        "/usr/local/bin/${name}" \
        "/usr/local/opt/gettext/bin/${name}"
    do
        if [ -x "${candidate}" ]; then
            echo "${candidate}"
            return
        fi
    done
    echo "error: ${name} not found. Install gettext (brew install gettext, or apt install gettext)." >&2
    exit 1
}

XGETTEXT="$(find_gettext_tool xgettext)"
MSGMERGE="$(find_gettext_tool msgmerge)"

PACKAGE_NAME="CoreDeck"
COPYRIGHT_HOLDER="CoreDeck contributors"
BUGS_ADDRESS="https://github.com/devmuaz/CoreDeck/issues/new?template=translation.md"
POT_AUTHOR="AbdulMuaz Aqeel <info@devmuaz.com>"
YEAR="$(date +%Y)"

SOURCES="$(mktemp)"
trap 'rm -f "${SOURCES}"' EXIT

find "${REPO_ROOT}/src" \( -name '*.cpp' -o -name '*.h' \) | sort > "${SOURCES}"

"${XGETTEXT}" \
    --from-code=UTF-8 \
    --language=C++ \
    --keyword=Tr:1 \
    --keyword=TrNoop:1 \
    --keyword=TrC:1c,2 \
    --keyword=TrN:1,2 \
    --keyword=TrFormat:1 \
    --keyword=TrFormatN:1,2 \
    --keyword=TrWindow:1 \
    --package-name="${PACKAGE_NAME}" \
    --msgid-bugs-address="${BUGS_ADDRESS}" \
    --copyright-holder="${COPYRIGHT_HOLDER}" \
    -o "${POT_FILE}" \
    -f "${SOURCES}"

FIXED="$(mktemp)"
sed \
    -e "s/^# Copyright (C) YEAR /# Copyright (C) ${YEAR} /" \
    -e "s/^# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR\\./# ${POT_AUTHOR}, ${YEAR}./" \
    -e "s/Last-Translator: FULL NAME <EMAIL@ADDRESS>/Last-Translator: ${POT_AUTHOR}/" \
    -e 's/\(Content-Type: text\/plain; charset=\)CHARSET/\1UTF-8/' \
    "${POT_FILE}" > "${FIXED}"
mv "${FIXED}" "${POT_FILE}"

shopt -s nullglob
PO_FILES=("${LOCALES_DIR}"/*.po)
if [ "${#PO_FILES[@]}" -eq 0 ]; then
    echo "updated ${POT_FILE}; no .po catalogs to merge"
    exit 0
fi

for po in "${PO_FILES[@]}"; do
    "${MSGMERGE}" --update --backup=none "${po}" "${POT_FILE}"
    echo "merged $(basename "${po}")"
done

echo "updated ${POT_FILE}"
echo "Fill in empty msgstr entries, then rebuild so msgfmt compiles the .mo files."
