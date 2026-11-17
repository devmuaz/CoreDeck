//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_memory_tab.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/frida/frida_abi.h"
#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_widgets.h"
#include "gui/widgets/widgets.h"
#include "imgui_memory_editor.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        MemoryEditor &EditorFor(const std::string &serial) {
            static std::unordered_map<std::string, MemoryEditor> editors;
            MemoryEditor &editor = editors[serial];
            editor.OptShowOptions = false;
            editor.OptShowDataPreview = false;
            return editor;
        }

        void StepMemorySize(int &size, const int delta) {
            const int limit = static_cast<int>(Core::FRIDA_READ_LIMIT);
            size = std::clamp(size + delta, 1, limit);
        }

        void MemoryToolbar(Context &context, FridaDeviceSession &session) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float stepW = ImGui::GetFrameHeight();
            const std::string refreshLabel = IconWithLabel(Icons::REFRESH, Platform::Tr("Refresh"));
            const std::string writeLabel = Platform::Tr("Write");
            const std::string revertLabel = Platform::Tr("Revert");
            const std::string findLabel = IconWithLabel(Icons::SEARCH, Platform::Tr("Find"));
            const float refreshW = ActionWidth(refreshLabel.c_str());
            const float writeW = ActionWidth(writeLabel.c_str());
            const float revertW = ActionWidth(revertLabel.c_str());
            const float findW = ActionWidth(findLabel.c_str());
            const float sizeW = Em(7.0F);
            const char *encodings[] = {Platform::Tr("ASCII"), Platform::Tr("UTF-16")};
            const float encodingW = ComboWidth(encodings, IM_ARRAYSIZE(encodings), Em(14.0F), Em(22.0F));
            const float hitsW = session.Hits.empty() ? 0.0F : Em(14.0F);
            const int gaps = session.Hits.empty() ? 9 : 10;
            const float fixed = stepW + sizeW + stepW + refreshW + writeW + revertW + findW + encodingW + hitsW + (spacing * static_cast<float>(gaps));
            const float fields = std::max(2.0F, ImGui::GetContentRegionAvail().x - fixed);
            const float addressW = std::max(1.0F, fields * 0.42F);
            const float patternW = std::max(1.0F, fields - addressW);

            ImGui::SetNextItemWidth(addressW);
            ImGui::InputTextWithHint("##FridaMemoryAddress", Platform::Tr("Address"), session.MemoryAddress, sizeof(session.MemoryAddress));
            HoverTooltip(Platform::Tr("Address to read"));
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(Icons::MINUS, session.MemorySize > 1, ImVec2(stepW, 0.0F))) {
                StepMemorySize(session.MemorySize, -1);
            }
            HoverTooltip(Platform::Tr("Read fewer bytes"));
            ImGui::SameLine(0.0F, spacing);
            ImGui::SetNextItemWidth(sizeW);
            ImGui::InputInt("##FridaMemorySize", &session.MemorySize, 0, 0);
            HoverTooltip(Platform::Tr("Bytes to read"));
            session.MemorySize = std::clamp(session.MemorySize, 1, static_cast<int>(Core::FRIDA_READ_LIMIT));
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(Icons::PLUS, std::cmp_less(session.MemorySize, Core::FRIDA_READ_LIMIT), ImVec2(stepW, 0.0F))) {
                StepMemorySize(session.MemorySize, 1);
            }
            HoverTooltip(Platform::Tr("Read more bytes"));
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(refreshLabel.c_str(), session.Attached && session.MemoryAddress[0] != '\0', ImVec2(refreshW, 0.0F))) {
                RequestFridaRead(context, session);
            }
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(writeLabel.c_str(), session.Attached && session.MemoryDirty, ImVec2(writeW, 0.0F))) {
                RequestFridaWrite(context, session);
            }
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(revertLabel.c_str(), session.MemoryDirty, ImVec2(revertW, 0.0F))) {
                session.Memory = session.MemoryClean;
                session.MemoryDirty = false;
            }
            ImGui::SameLine(0.0F, spacing);
            SearchField("##FridaMemoryFind", Platform::Tr("Hex pattern or string"), session.MemoryFind, sizeof(session.MemoryFind), patternW);
            ImGui::SameLine(0.0F, spacing);
            int encoding = session.MemoryFindUtf16 ? 1 : 0;
            if (RoundedCombo("##FridaMemoryEncoding", encodings[encoding], encodings, IM_ARRAYSIZE(encodings), encoding, encodingW, Platform::Tr("Search text encoding"))) {
                session.MemoryFindUtf16 = encoding == 1;
            }
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(findLabel.c_str(), session.Attached && session.MemoryFind[0] != '\0', ImVec2(findW, 0.0F))) {
                RequestFridaScan(context, session);
            }
            if (session.Hits.empty()) {
                return;
            }
            std::vector<const char *> labels;
            labels.reserve(session.Hits.size());
            for (const FridaHitRow &hit: session.Hits) {
                labels.push_back(hit.Address.c_str());
            }
            int selected = -1;
            ImGui::SameLine(0.0F, spacing);
            if (RoundedCombo("##FridaMemoryHits", Platform::Tr("Hits"), labels.data(), static_cast<int>(labels.size()), selected, hitsW, Platform::Tr("Scan hits")) && selected >= 0) {
                CopyToBuffer(session.MemoryAddress, sizeof(session.MemoryAddress), session.Hits.at(static_cast<std::size_t>(selected)).Address);
                RequestFridaRead(context, session);
            }
        }

        constexpr std::size_t NO_BYTE = static_cast<std::size_t>(-1);

        void PreviewValue(
            const MemoryEditor &editor,
            const std::uint8_t *bytes,
            const std::size_t size,
            const std::size_t address,
            const MemoryEditor::DataFormat format,
            const char *label
        ) {
            char text[128] = {};
            if (address != NO_BYTE && address < size) {
                editor.DrawPreviewData(address, bytes, size, editor.PreviewDataType, format, text, sizeof(text));
            }
            PropertyText(label, text[0] == '\0' ? "—" : text);
        }

        void PreviewFormats(MemoryEditor &editor) {
            const char *types[] = {"Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64", "Float", "Double"};
            const char *endian[] = {"LE", "BE"};
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float endianW = ComboWidth(endian, IM_ARRAYSIZE(endian), Em(6.0F), Em(10.0F));
            const float typeW = std::max(1.0F, ImGui::GetContentRegionAvail().x - endianW - spacing);
            int type = static_cast<int>(editor.PreviewDataType);
            if (type < 0 || std::cmp_greater_equal(type, IM_ARRAYSIZE(types))) {
                type = static_cast<int>(ImGuiDataType_S32);
            }
            if (RoundedCombo("##FridaPreviewType", types[type], types, IM_ARRAYSIZE(types), type, typeW, Platform::Tr("Preview type"))) {
                editor.PreviewDataType = static_cast<ImGuiDataType>(type);
            }
            ImGui::SameLine(0.0F, spacing);
            int order = editor.PreviewEndianness == 0 ? 0 : 1;
            if (RoundedCombo("##FridaPreviewEndian", endian[order], endian, IM_ARRAYSIZE(endian), order, endianW, Platform::Tr("Endianness"))) {
                editor.PreviewEndianness = order;
            }
        }

        void GoToAddress(MemoryEditor &editor, const std::size_t base, const std::size_t size) {
            ImGui::SetNextItemWidth(-1.0F);
            const bool submitted = ImGui::InputTextWithHint(
                "##FridaGoto",
                Platform::Tr("Go to address"),
                editor.AddrInputBuf,
                sizeof(editor.AddrInputBuf),
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue
            );
            HoverTooltip(Platform::Tr("Scroll the hex view to this address"));
            if (!submitted && !ImGui::IsItemDeactivatedAfterEdit()) {
                return;
            }
            const char *begin = editor.AddrInputBuf;
            if (begin[0] == '0' && (begin[1] == 'x' || begin[1] == 'X')) {
                begin += 2;
            }
            std::uint64_t value = 0;
            const auto parsed = std::from_chars(begin, begin + std::strlen(begin), value, 16);
            if (parsed.ec != std::errc{} || parsed.ptr == begin) {
                return;
            }
            std::size_t target = NO_BYTE;
            if (value >= base && value - base < size) {
                target = static_cast<std::size_t>(value - base);
            } else if (value < size) {
                target = static_cast<std::size_t>(value);
            }
            if (target == NO_BYTE) {
                return;
            }
            editor.GotoAddr = target;
            editor.HighlightMin = NO_BYTE;
            editor.HighlightMax = NO_BYTE;
        }

        void ViewerColumns(MemoryEditor &editor) {
            constexpr int MIN_COLS = 4;
            constexpr int MAX_COLS = 32;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float step = ImGui::GetFrameHeight();
            if (PrimaryButton(Icons::MINUS, editor.Cols > MIN_COLS, ImVec2(step, 0.0F))) {
                editor.Cols = std::max(MIN_COLS, editor.Cols - 1);
                editor.ContentsWidthChanged = true;
            }
            HoverTooltip(Platform::Tr("Fewer columns"));
            ImGui::SameLine(0.0F, spacing);
            ImGui::SetNextItemWidth(std::max(1.0F, ImGui::GetContentRegionAvail().x - step - spacing));
            if (ImGui::InputInt("##FridaCols", &editor.Cols, 0, 0)) {
                editor.Cols = std::clamp(editor.Cols, MIN_COLS, MAX_COLS);
                editor.ContentsWidthChanged = true;
            }
            HoverTooltip(Platform::Tr("Columns per row"));
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(Icons::PLUS, editor.Cols < MAX_COLS, ImVec2(step, 0.0F))) {
                editor.Cols = std::min(MAX_COLS, editor.Cols + 1);
                editor.ContentsWidthChanged = true;
            }
            HoverTooltip(Platform::Tr("More columns"));
        }

        void ViewerOptions(MemoryEditor &editor) {
            const bool ascii = editor.OptShowAscii;
            bool groups = editor.OptMidColsCount > 0;
            const CheckboxRowItem options[] = {
                {.Id = "##FridaAscii", .Value = &editor.OptShowAscii, .Label = Platform::Tr("ASCII")},
                {.Id = "##FridaHexII", .Value = &editor.OptShowHexII, .Label = Platform::Tr("HexII")},
                {.Id = "##FridaZeroes", .Value = &editor.OptGreyOutZeroes, .Label = Platform::Tr("Grey Out Zeroes")},
                {.Id = "##FridaUpper", .Value = &editor.OptUpperCaseHex, .Label = Platform::Tr("Uppercase")},
                {.Id = "##FridaGroups", .Value = &groups, .Label = Platform::Tr("Group Every 8 Bytes")},
            };
            CheckboxRow(options);
            if (ascii != editor.OptShowAscii || groups != (editor.OptMidColsCount > 0)) {
                editor.OptMidColsCount = groups ? 8 : 0;
                editor.ContentsWidthChanged = true;
            }
        }

        void PreviewPane(MemoryEditor &editor, const std::uint8_t *bytes, const std::size_t size, const std::size_t base, const bool loaded) {
            SectionHeading(Platform::Tr("Preview"));
            PreviewFormats(editor);
            const std::size_t selected = editor.MouseHovered ? editor.MouseHoveredAddr : editor.DataPreviewAddr;
            const bool hasValue = loaded && selected != NO_BYTE && selected < size;
            char address[32] = {};
            if (hasValue) {
                (void) std::snprintf(address, sizeof(address), "0x%zX", base + selected);
            }
            PropertyText(Platform::Tr("Address"), hasValue ? address : "—");
            PreviewValue(editor, bytes, size, hasValue ? selected : NO_BYTE, MemoryEditor::DataFormat_Dec, Platform::Tr("Decimal"));
            PreviewValue(editor, bytes, size, hasValue ? selected : NO_BYTE, MemoryEditor::DataFormat_Hex, Platform::Tr("Hex"));
            PreviewValue(editor, bytes, size, hasValue ? selected : NO_BYTE, MemoryEditor::DataFormat_Bin, Platform::Tr("Binary"));

            SectionHeading(Platform::Tr("Viewer"));
            ViewerColumns(editor);
            GoToAddress(editor, base, size);
            char range[80] = {};
            if (size > 0) {
                (void) std::snprintf(range, sizeof(range), "0x%zX – 0x%zX", base, base + size - 1);
            }
            PropertyText(Platform::Tr("Range"), size > 0 ? range : "—");
            ViewerOptions(editor);
        }

        void MemoryView(FridaDeviceSession &session) {
            const PickerTableStyle chrome;
            const bool loaded = !session.Memory.empty();
            std::vector<std::uint8_t> blank;
            void *data = session.Memory.data();
            std::size_t size = session.Memory.size();
            auto base = static_cast<std::size_t>(session.MemoryBase);
            if (!loaded) {
                const int count = std::clamp(session.MemorySize, 16, static_cast<int>(Core::FRIDA_PAGE_SIZE));
                blank.assign(static_cast<std::size_t>(count), 0);
                data = blank.data();
                size = blank.size();
                base = 0;
            }

            const ImGuiStyle &style = ImGui::GetStyle();
            const float height = std::max(Em(10.0F), ImGui::GetContentRegionAvail().y);
            const float avail = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            MemoryEditor &editor = EditorFor(session.Serial);
            MemoryEditor::Sizes sizes;
            editor.CalcSizes(sizes, size, base);
            const float minPreview = Em(18.0F);
            float hexW = sizes.WindowWidth + (style.ChildBorderSize * 2.0F);
            hexW = std::min(hexW, std::max(Em(12.0F), avail - minPreview - style.ItemSpacing.x));
            const float previewW = std::max(1.0F, avail - hexW - style.ItemSpacing.x);
            const ImVec2 origin = ImGui::GetCursorPos();

            ImGui::SetCursorPos(ImVec2(origin.x + previewW + style.ItemSpacing.x, origin.y));
            ImGui::BeginChild("##FridaHex", ImVec2(hexW, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            if (!loaded) {
                ImGui::BeginDisabled();
            }
            editor.DrawContents(data, size, base);
            if (loaded) {
                session.MemoryDirty = session.Memory != session.MemoryClean;
            } else {
                ImGui::EndDisabled();
            }
            ImGui::EndChild();

            ImGui::SetCursorPos(origin);
            {
                StyleVar sv;
                sv.Push(ImGuiStyleVar_WindowPadding, style.FramePadding);
                ImGui::BeginChild("##FridaPreview", ImVec2(previewW, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
            }
            if (!loaded) {
                ImGui::BeginDisabled();
            }
            PreviewPane(editor, static_cast<const std::uint8_t *>(data), size, base, loaded);
            if (!loaded) {
                ImGui::EndDisabled();
            }
            ImGui::EndChild();
            ImGui::SetCursorPos(ImVec2(origin.x, origin.y + height));
        }
    }

    void BuildFridaMemoryTab(Context &context, FridaDeviceSession &session) {
        MemoryToolbar(context, session);
        SpaceBelowToolbar();
        MemoryView(session);
    }
}
