//
// Created by AbdulMuaz Aqeel on 23/09/2026.
//

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <numbers>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "imgui.h"

#include "apk_analyzer.h"
#include "onboarding.h"
#include "../context.h"
#include "../widgets.h"
#include "../theme.h"
#include "../../core/apk_analyzer.h"
#include "../../core/file_dialog.h"
#include "../../core/paths.h"
#include "../../core/utilities.h"

namespace CoreDeck {
    namespace {
        constexpr const char *UNAVAILABLE = "\u2014";

        struct LoadWatch {
            std::atomic<float> *Progress = nullptr;
            std::atomic<int> *Stage = nullptr;
            std::atomic<bool> *Cancel = nullptr;
            std::atomic<std::uint32_t> *Epoch = nullptr;
            std::uint32_t Expected = 0;
        };

        bool ReportLoadProgress(void *user, const float progress, const int stage) {
            const auto *watch = static_cast<const LoadWatch *>(user);
            if (watch == nullptr || watch->Epoch == nullptr || watch->Epoch->load() != watch->Expected) {
                return false;
            }
            if (watch->Cancel != nullptr && watch->Cancel->load()) {
                return false;
            }
            if (watch->Progress != nullptr) {
                watch->Progress->store(progress);
            }
            if (watch->Stage != nullptr) {
                watch->Stage->store(stage);
            }
            return true;
        }

        const char *LoadStageText(const int stage) {
            if (stage == 1) {
                return "Reading APK archive headers...";
            }
            return "Building file tree structure...";
        }

        constexpr ImGuiWindowFlags APK_ANALYZER_WINDOW_FLAGS =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoNavFocus;

        struct FileNode {
            std::string Name;
            std::string FullPath;
            bool Directory = false;
            bool Visible = true;
            std::int64_t RawSize = 0;
            std::int64_t DownloadSize = 0;
            std::uint32_t Alignment = 0;
            bool Stored = false;
            std::vector<FileNode> Children;
        };


        struct FlatFile {
            FileNode *Node = nullptr;
            int Depth = 0;
        };

        struct FileCache {
            std::string Key;
            FileNode Root;
            std::unordered_map<std::string, FileNode *> Index;
            std::unordered_set<std::string> Open;
            int FileCount = 0;
        };


        struct RetiredJobs {
            std::vector<std::future<ApkReport>> Reports;
            std::vector<std::future<ApkAnalyzerQuery<std::vector<ApkCompareEntry>>>> Compares;
        };

        RetiredJobs &Graveyard() {
            static RetiredJobs jobs;
            return jobs;
        }

        FileCache &FilesCache() {
            static FileCache cache;
            return cache;
        }


        template<typename T>
        void Retire(std::future<T> &job, std::vector<std::future<T>> &retired) {
            if (job.valid()) {
                retired.push_back(std::move(job));
            }
        }

        template<typename T>
        void DropFinished(std::vector<std::future<T>> &retired) {
            std::erase_if(retired, [](std::future<T> &job) {
                return job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            });
        }

        void PollRetired() {
            auto &grave = Graveyard();
            DropFinished(grave.Reports);
            DropFinished(grave.Compares);
        }

        std::string FileNameOf(const std::string &path) {
            if (path.empty()) {
                return {};
            }
            return std::filesystem::path(path).filename().string();
        }

        std::string DirectoryOf(const std::string &path) {
            if (path.empty()) {
                return {};
            }
            return std::filesystem::path(path).parent_path().string();
        }

        bool EndsWithIgnoreCase(const std::string &text, const std::string &suffix) {
            if (text.size() < suffix.size()) {
                return false;
            }
            for (std::size_t i = 0; i < suffix.size(); ++i) {
                const auto left = static_cast<unsigned char>(text.at(text.size() - suffix.size() + i));
                const auto right = static_cast<unsigned char>(suffix.at(i));
                if (std::tolower(left) != std::tolower(right)) {
                    return false;
                }
            }
            return true;
        }

        bool NameLess(const std::string &left, const std::string &right) {
            return std::ranges::lexicographical_compare(left, right, [](const char a, const char b) {
                return std::tolower(static_cast<unsigned char>(a)) < std::tolower(static_cast<unsigned char>(b));
            });
        }

        std::string FirstOutputLine(const std::string &output) {
            std::istringstream stream(output);
            std::string line;
            while (std::getline(stream, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                    line.pop_back();
                }
                if (line.empty()) {
                    continue;
                }
                if (line.size() > 180) {
                    line.resize(177);
                    line += "...";
                }
                return line;
            }
            return {};
        }

        std::string FormatBytes(const std::int64_t bytes) {
            if (bytes < 0) {
                return StrConcat("-", FormatFileSize(static_cast<std::uintmax_t>(-bytes)));
            }
            return FormatFileSize(static_cast<std::uintmax_t>(bytes));
        }

        std::string FormatCount(const std::int64_t value) {
            const bool negative = value < 0;
            std::string digits = std::to_string(negative ? -value : value);
            std::string grouped;
            grouped.reserve(digits.size() + (digits.size() / 3));
            for (std::size_t i = 0; i < digits.size(); ++i) {
                if (i > 0 && (digits.size() - i) % 3 == 0) {
                    grouped.push_back(',');
                }
                grouped.push_back(digits.at(i));
            }
            if (negative) {
                grouped.insert(grouped.begin(), '-');
            }
            return grouped;
        }

        bool IsDexPath(const std::string &path) {
            return EndsWithIgnoreCase(path, ".dex");
        }

        bool IsManifestPath(const std::string &path) {
            return EndsWithIgnoreCase(FileNameOf(path), "AndroidManifest.xml");
        }


        std::optional<std::string> PickApk(const std::string &title, const std::string &defaultDirectory) {
            return FileDialog::PickFile(title, "Android packages", {"*.apk", "*.APK"}, defaultDirectory);
        }

        constexpr int RECENT_APK_LIMIT = 4;

        std::string RecentApkPath() {
            return Paths::GetAppConfigPath("apk-analyzer-recent.txt");
        }

        void LoadRecentApks(Context &context) {
            auto &work = context.ApkAnalyzerWork;
            if (work.RecentLoaded) {
                return;
            }
            work.RecentLoaded = true;
            const std::string path = RecentApkPath();
            if (path.empty()) {
                return;
            }
            std::ifstream file(path);
            std::string line;
            while (std::getline(file, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                    line.pop_back();
                }
                if (!line.empty()) {
                    work.RecentApks.push_back(std::move(line));
                }
            }
        }

        void SaveRecentApks(const Context &context) {
            const std::string path = RecentApkPath();
            if (path.empty()) {
                return;
            }
            std::error_code error;
            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
            std::ofstream file(path, std::ios::trunc);
            if (!file.is_open()) {
                return;
            }
            for (const auto &apk: context.ApkAnalyzerWork.RecentApks) {
                file << apk << '\n';
            }
        }

        void RememberApk(Context &context, const std::string &apkPath) {
            LoadRecentApks(context);
            auto &recent = context.ApkAnalyzerWork.RecentApks;
            std::erase(recent, apkPath);
            recent.insert(recent.begin(), apkPath);
            if (static_cast<int>(recent.size()) > RECENT_APK_LIMIT) {
                recent.resize(RECENT_APK_LIMIT);
            }
            SaveRecentApks(context);
        }

        void ForgetApk(Context &context, const std::string &apkPath) {
            LoadRecentApks(context);
            std::erase(context.ApkAnalyzerWork.RecentApks, apkPath);
            SaveRecentApks(context);
        }

        void DrawDashedRoundedRect(const ImVec2 min, const ImVec2 max, const float rounding, const ImU32 color, const float thickness) {
            ImDrawList *draw = ImGui::GetWindowDrawList();
            const float radius = std::min(rounding, std::min(max.x - min.x, max.y - min.y) * 0.5F);
            constexpr float PI = std::numbers::pi_v<float>;
            const float dash = 8.0F;
            const float gap = 6.0F;
            std::vector<ImVec2> points;
            points.reserve(64);
            const auto push = [&](const ImVec2 point) {
                if (points.empty() || points.back().x != point.x || points.back().y != point.y) {
                    points.push_back(point);
                }
            };
            const auto arc = [&](const ImVec2 center, const float start, const float end) {
                constexpr int STEPS = 8;
                for (int i = 0; i <= STEPS; ++i) {
                    const float t = start + ((end - start) * (static_cast<float>(i) / static_cast<float>(STEPS)));
                    push(ImVec2(center.x + (radius * std::sin(t)), center.y - (radius * std::cos(t))));
                }
            };
            arc(ImVec2(max.x - radius, min.y + radius), 0.0F, PI * 0.5F);
            arc(ImVec2(max.x - radius, max.y - radius), PI * 0.5F, PI);
            arc(ImVec2(min.x + radius, max.y - radius), PI, PI * 1.5F);
            arc(ImVec2(min.x + radius, min.y + radius), PI * 1.5F, PI * 2.0F);
            if (points.size() < 2) {
                return;
            }
            points.push_back(points.front());

            float phase = 0.0F;
            bool drawing = true;
            for (std::size_t i = 1; i < points.size(); ++i) {
                const ImVec2 from = points.at(i - 1);
                const ImVec2 to = points.at(i);
                const float length = std::sqrt(((to.x - from.x) * (to.x - from.x)) + ((to.y - from.y) * (to.y - from.y)));
                if (length <= 0.0F) {
                    continue;
                }
                const ImVec2 step((to.x - from.x) / length, (to.y - from.y) / length);
                float traveled = 0.0F;
                while (traveled < length) {
                    const float span = drawing ? dash : gap;
                    const float remain = span - phase;
                    const float stepLen = std::min(remain, length - traveled);
                    if (drawing) {
                        draw->AddLine(
                            ImVec2(from.x + (step.x * traveled), from.y + (step.y * traveled)),
                            ImVec2(from.x + (step.x * (traveled + stepLen)), from.y + (step.y * (traveled + stepLen))),
                            color,
                            thickness
                        );
                    }
                    traveled += stepLen;
                    phase += stepLen;
                    if (phase >= span - 0.01F) {
                        phase = 0.0F;
                        drawing = !drawing;
                    }
                }
            }
        }

        void DrawUploadMark(const ImVec2 center, const ImU32 color) {
            ImDrawList *draw = ImGui::GetWindowDrawList();
            const float s = ImGui::GetTextLineHeight();
            draw->AddLine(ImVec2(center.x, center.y - s), ImVec2(center.x, center.y + (s * 0.15F)), color, 2.0F);
            draw->AddLine(ImVec2(center.x, center.y - s), ImVec2(center.x - (s * 0.45F), center.y - (s * 0.4F)), color, 2.0F);
            draw->AddLine(ImVec2(center.x, center.y - s), ImVec2(center.x + (s * 0.45F), center.y - (s * 0.4F)), color, 2.0F);
            draw->AddLine(ImVec2(center.x - (s * 0.7F), center.y + (s * 0.35F)), ImVec2(center.x - (s * 0.7F), center.y + s), color, 2.0F);
            draw->AddLine(ImVec2(center.x - (s * 0.7F), center.y + s), ImVec2(center.x + (s * 0.7F), center.y + s), color, 2.0F);
            draw->AddLine(ImVec2(center.x + (s * 0.7F), center.y + s), ImVec2(center.x + (s * 0.7F), center.y + (s * 0.35F)), color, 2.0F);
        }

        void ClearInspection(Context &context) {
            auto &work = context.ApkAnalyzerWork;
            work.SelectedPath.clear();
            work.TreeFilter[0] = '\0';
        }

        void LeaveApkAnalyzer(Context &context) {
            auto &work = context.ApkAnalyzerWork;
            work.Cancel.store(true);
            work.LoadEpoch.fetch_add(1);
            Retire(work.Future, Graveyard().Reports);
            Retire(work.CompareFuture, Graveyard().Compares);
            work.Busy = false;
            work.CompareBusy = false;
            work.HasReport = false;
            work.HasCompare = false;
            work.PendingPath.clear();
            work.ComparePath.clear();
            work.Report = {};
            work.Compare = {};
            work.Progress.store(0.0F);
            work.Stage.store(1);
            ClearInspection(context);
            context.UI.ShowApkAnalyzerWindow = false;
        }

        void StartAnalysis(Context &context, std::string apkPath) {
            auto &work = context.ApkAnalyzerWork;
            auto &grave = Graveyard();
            Retire(work.Future, grave.Reports);
            Retire(work.CompareFuture, grave.Compares);
            work.Busy = false;
            work.CompareBusy = false;
            ClearInspection(context);

            work.PendingPath = apkPath;
            work.HasReport = false;
            work.HasCompare = false;
            work.ComparePath.clear();
            work.Compare = {};
            work.ContentEpoch += 1;
            work.Progress.store(0.0F);
            work.Stage.store(1);
            work.Cancel.store(false);
            const std::uint32_t epoch = work.LoadEpoch.fetch_add(1) + 1;
            work.Busy = true;
            RememberApk(context, apkPath);

            const SdkInfo sdk = context.Host.Sdk;
            work.Future = std::async(std::launch::async, [&work, epoch, sdk, apkPath = std::move(apkPath)] {
                LoadWatch watch{
                    .Progress = &work.Progress,
                    .Stage = &work.Stage,
                    .Cancel = &work.Cancel,
                    .Epoch = &work.LoadEpoch,
                    .Expected = epoch,
                };
                return LoadApkReport(sdk, apkPath, ReportLoadProgress, &watch);
            });
        }

        void StartCompare(Context &context, std::string otherPath) {
            auto &work = context.ApkAnalyzerWork;
            if (work.Busy.load() || work.Report.ApkPath.empty()) {
                return;
            }

            Retire(work.CompareFuture, Graveyard().Compares);
            work.ComparePath = otherPath;
            work.HasCompare = false;
            work.Compare = {};
            work.CompareBusy = true;

            const SdkInfo sdk = context.Host.Sdk;
            const std::string baseline = work.Report.ApkPath;
            work.CompareFuture = std::async(std::launch::async, [sdk, baseline, otherPath = std::move(otherPath)] {
                return LoadApkCompare(sdk, baseline, otherPath);
            });
        }


        void PollApkAnalyzer(Context &context) {
            PollRetired();
            auto &work = context.ApkAnalyzerWork;
            if (work.Busy.load() && work.Future.valid() &&
                work.Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                work.Report = work.Future.get();
                work.Busy = false;
                if (work.Report.Cancelled) {
                    work.HasReport = false;
                    work.Report = {};
                    work.PendingPath.clear();
                    work.Progress.store(0.0F);
                } else {
                    work.HasReport = !work.Report.ToolMissing;
                    work.Progress.store(1.0F);
                    if (work.HasReport) {
                        work.SelectedPath = "/";
                        work.ContentEpoch += 1;
                    }
                }
            }

            if (work.CompareBusy.load() && work.CompareFuture.valid() &&
                work.CompareFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                work.Compare = work.CompareFuture.get();
                work.HasCompare = true;
                work.CompareBusy = false;
            }
        }

        void SelectFile(Context &context, const std::string &path) {
            context.ApkAnalyzerWork.SelectedPath = path;
        }

        FileNode *FindOrAddChild(FileNode &parent, const std::string &name, const std::string &fullPath, const bool directory) {
            for (auto &child: parent.Children) {
                if (child.Name == name) {
                    return &child;
                }
            }
            FileNode created;
            created.Name = name;
            created.FullPath = fullPath;
            created.Directory = directory;
            parent.Children.push_back(std::move(created));
            return &parent.Children.back();
        }

        void AddFileEntry(FileNode &root, const ApkFileEntry &entry) {
            if (entry.Path.empty() || entry.Path == "/") {
                root.FullPath = "/";
                root.Directory = true;
                root.RawSize = entry.RawSize;
                root.DownloadSize = entry.DownloadSize;
                return;
            }

            const bool directory = entry.Path.ends_with('/');
            std::string relative = entry.Path;
            if (relative.starts_with('/')) {
                relative.erase(relative.begin());
            }
            if (relative.ends_with('/')) {
                relative.pop_back();
            }

            FileNode *node = &root;
            std::string walked = "/";
            std::size_t start = 0;
            while (start <= relative.size()) {
                const auto slash = relative.find('/', start);
                const bool last = slash == std::string::npos;
                const std::string name = relative.substr(start, last ? std::string::npos : slash - start);
                if (!name.empty()) {
                    const bool childDirectory = !last || directory;
                    const std::string childPath = childDirectory ? walked + name + "/" : walked + name;
                    FileNode *child = FindOrAddChild(*node, name, last ? entry.Path : childPath, childDirectory);
                    if (last) {
                        child->Directory = directory;
                        child->FullPath = entry.Path;
                        child->RawSize = entry.RawSize;
                        child->DownloadSize = entry.DownloadSize;
                        child->Alignment = entry.Alignment;
                        child->Stored = entry.Stored;
                    } else if (!child->Directory) {
                        child->Directory = true;
                        child->FullPath = childPath;
                    }
                    node = child;
                    if (child->Directory && !child->FullPath.empty()) {
                        walked = child->FullPath;
                        if (!walked.ends_with('/')) {
                            walked.push_back('/');
                        }
                    } else {
                        walked = childPath;
                    }
                }
                if (last) {
                    break;
                }
                start = slash + 1;
            }
        }

        void SortFileNode(FileNode &node) {
            std::ranges::sort(node.Children, [](const FileNode &left, const FileNode &right) {
                if (left.DownloadSize != right.DownloadSize) {
                    return left.DownloadSize > right.DownloadSize;
                }
                return NameLess(left.Name, right.Name);
            });
            for (auto &child: node.Children) {
                SortFileNode(child);
            }
        }

        int CountFiles(const FileNode &node) {
            int count = node.Directory ? 0 : 1;
            for (const auto &child: node.Children) {
                count += CountFiles(child);
            }
            return count;
        }

        void IndexFiles(FileNode &node, std::unordered_map<std::string, FileNode *> &index) {
            index.emplace(node.FullPath, &node);
            for (auto &child: node.Children) {
                IndexFiles(child, index);
            }
        }

        bool MarkFile(FileNode &node, const char *filter, const bool ancestorMatched) {
            const bool filtering = filter != nullptr && filter[0] != '\0';
            const bool self = !filtering || ContainsIgnoreCase(node.Name, filter) || ContainsIgnoreCase(node.FullPath, filter);
            bool descendant = false;
            for (auto &child: node.Children) {
                if (MarkFile(child, filter, ancestorMatched || self)) {
                    descendant = true;
                }
            }
            node.Visible = !filtering || ancestorMatched || self || descendant;
            return self || descendant;
        }

        void FlattenFiles(
            FileNode &node,
            const int depth,
            const std::unordered_set<std::string> &open,
            const bool forceOpen,
            std::vector<FlatFile> &rows
        ) {
            if (!node.Visible) {
                return;
            }
            rows.push_back({.Node = &node, .Depth = depth});
            if (!forceOpen && !open.contains(node.FullPath)) {
                return;
            }
            for (auto &child: node.Children) {
                FlattenFiles(child, depth + 1, open, forceOpen, rows);
            }
        }

        FileCache &CachedFiles(const ApkReport &report, const std::uint32_t epoch) {
            auto &cache = FilesCache();
            const std::string key = StrConcat(std::to_string(epoch), ":", report.ApkPath, ":", std::to_string(report.Files.Value.size()));
            if (cache.Key == key) {
                return cache;
            }

            FileNode root;
            root.Name = FileNameOf(report.ApkPath);
            if (root.Name.empty()) {
                root.Name = "APK";
            }
            root.FullPath = "/";
            root.Directory = true;
            for (const auto &entry: report.Files.Value) {
                AddFileEntry(root, entry);
            }
            if (root.RawSize == 0 && report.FileSize.Ok) {
                root.RawSize = report.FileSize.Value;
            }
            if (root.DownloadSize == 0 && report.DownloadSize.Ok) {
                root.DownloadSize = report.DownloadSize.Value;
            }
            SortFileNode(root);

            cache = {};
            cache.Key = key;
            cache.Root = std::move(root);
            cache.FileCount = CountFiles(cache.Root);
            cache.Open.insert("/");
            IndexFiles(cache.Root, cache.Index);
            return cache;
        }


        void DrawDisclosure(const ImVec2 pos, const float size, const bool open) {
            ImDrawList *draw = ImGui::GetWindowDrawList();
            const ImU32 color = ImGui::ColorConvertFloat4ToU32(HexColor(Colors::TEXT_SUBTLE));
            const float midY = pos.y + (size * 0.5F);
            if (open) {
                draw->AddTriangleFilled(
                    ImVec2(pos.x + (size * 0.18F), pos.y + (size * 0.32F)),
                    ImVec2(pos.x + (size * 0.82F), pos.y + (size * 0.32F)),
                    ImVec2(pos.x + (size * 0.50F), pos.y + (size * 0.74F)),
                    color
                );
            } else {
                draw->AddTriangleFilled(
                    ImVec2(pos.x + (size * 0.34F), pos.y + (size * 0.18F)),
                    ImVec2(pos.x + (size * 0.34F), midY + (size * 0.32F)),
                    ImVec2(pos.x + (size * 0.74F), midY),
                    color
                );
            }
        }

        struct LabelClick {
            bool Activated = false;
            bool Toggle = false;
        };

        LabelClick DrawTreeLabel(
            const char *id,
            const char *label,
            const char *tooltip,
            const int depth,
            const bool hasChildren,
            const bool isOpen,
            const bool selected,
            const char *textColor
        ) {
            LabelClick click;
            const float line = ImGui::GetTextLineHeight();
            if (depth > 0) {
                ImGui::Dummy(ImVec2(line * static_cast<float>(depth), line));
                ImGui::SameLine(0.0F, 0.0F);
            }

            ImGui::PushID(id);
            if (hasChildren) {
                const ImVec2 arrowPos = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##arrow", ImVec2(line, line))) {
                    click.Toggle = true;
                }
                DrawDisclosure(arrowPos, line, isOpen);
                ImGui::SameLine(0.0F, 4.0F);
            } else {
                ImGui::Dummy(ImVec2(line, line));
                ImGui::SameLine(0.0F, 4.0F);
            }

            const bool colored = textColor != nullptr;
            if (colored) {
                ImGui::PushStyleColor(ImGuiCol_Text, HexColor(textColor));
            }
            if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns)) {
                click.Activated = true;
                if (hasChildren && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    click.Toggle = true;
                }
            }
            if (colored) {
                ImGui::PopStyleColor();
            }
            if (tooltip != nullptr && tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", tooltip);
            }
            ImGui::PopID();
            return click;
        }

        void DrawAlignedText(const std::string &text, const char *color = nullptr) {
            const float width = ImGui::CalcTextSize(text.c_str()).x;
            const float start = ImGui::GetCursorPosX();
            const float avail = ImGui::GetContentRegionAvail().x;
            if (avail > width) {
                ImGui::SetCursorPosX(start + avail - width);
            }
            if (color != nullptr) {
                ImGui::TextColored(HexColor(color), "%s", text.c_str());
            } else {
                ImGui::TextUnformatted(text.c_str());
            }
        }

        void ToggleOpen(std::unordered_set<std::string> &open, const std::string &id) {
            if (const auto found = open.find(id); found != open.end()) {
                open.erase(found);
            } else {
                open.insert(id);
            }
        }

        void DrawQueryMiss(const char *sentence, const std::string &output, const bool mayNeedAapt) {
            ImGui::TextColored(HexColor(Colors::WARNING), "%s", sentence);
            const std::string line = FirstOutputLine(output);
            const bool exception = line.find("Exception in thread") != std::string::npos;
            const bool mentionsAapt = line.find("aapt") != std::string::npos;
            if (mayNeedAapt && (exception || mentionsAapt || line.empty())) {
                ImGui::TextWrapped(
                    "This command needs aapt from Android SDK build-tools. The rest of this APK is still shown."
                );
                return;
            }
            if (line.find("not a binary XML") != std::string::npos) {
                ImGui::TextWrapped("This file is not a compiled Android XML resource.");
                return;
            }
            if (!line.empty() && !exception) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x);
                ImGui::TextDisabled("%s", line.c_str());
                ImGui::PopTextWrapPos();
            } else if (exception) {
                ImGui::TextWrapped("apkanalyzer did not return a result for this section.");
            }
        }

        constexpr const char *SHARE_BAR = "#F57C00";

        const char *FileAccent(const FileNode &node) {
            if (node.Directory) {
                return nullptr;
            }
            if (IsDexPath(node.FullPath)) {
                return Colors::ACCENT_INFO;
            }
            if (IsManifestPath(node.FullPath)) {
                return Colors::WARNING;
            }
            if (EndsWithIgnoreCase(node.FullPath, ".so")) {
                return Colors::POSITIVE;
            }
            return nullptr;
        }

        std::string AlignmentLabel(const FileNode &node) {
            if (node.Directory || node.Alignment >= 16384U) {
                return node.Directory ? "N/A" : "16 KB";
            }
            if (node.Alignment >= 4096U) {
                return "4 KB";
            }
            return "N/A";
        }

        void DrawCenteredText(const std::string &text) {
            const float width = ImGui::CalcTextSize(text.c_str()).x;
            const float start = ImGui::GetCursorPosX();
            const float avail = ImGui::GetContentRegionAvail().x;
            if (avail > width) {
                ImGui::SetCursorPosX(start + ((avail - width) * 0.5F));
            }
            ImGui::TextUnformatted(text.c_str());
        }

        void DrawShareBar(const std::int64_t part, const std::int64_t total) {
            const float fraction = total <= 0 || part <= 0 ? 0.0F : std::min(1.0F, static_cast<float>(part) / static_cast<float>(total));
            char label[32];
            if (total <= 0 || part < 0) {
                (void) std::snprintf(label, sizeof(label), "%s", UNAVAILABLE);
            } else {
                const double percent = (100.0 * static_cast<double>(part)) / static_cast<double>(total);
                (void) std::snprintf(label, sizeof(label), "%.1f%%", percent);
            }
            const float barH = ImGui::GetTextLineHeight();
            const float frameH = std::max(barH, ImGui::GetFrameHeight());
            const float rounding = ImGui::GetStyle().FrameRounding * (barH / frameH);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, HexColor(SHARE_BAR));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, HexColor(Colors::SURFACE3));
            ImGui::PushStyleColor(ImGuiCol_Text, HexColor(Colors::WHITE));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0F));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
            ImGui::ProgressBar(fraction, ImVec2(-1.0F, barH), label);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        void DrawInfoChip(const char *label, const bool first) {
            if (!first) {
                ImGui::SameLine();
            }
            CategoryChip(label, false);
        }

        void DrawApkHeader(Context &context) {
            auto &work = context.ApkAnalyzerWork;
            const ApkReport &report = work.Report;
            bool first = true;
            if (!report.Summary.Ok || report.Summary.Value.ApplicationId.empty()) {
                const std::string name = FileNameOf(report.ApkPath);
                DrawInfoChip(name.empty() ? "APK" : name.c_str(), first);
                first = false;
            } else {
                const ApkSummary &summary = report.Summary.Value;
                const std::string versionName = summary.VersionName.empty() ? UNAVAILABLE : summary.VersionName;
                const std::string versionCode = summary.VersionCode.empty() ? UNAVAILABLE : summary.VersionCode;
                const std::string packageLabel = StrConcat("Package Name: ", summary.ApplicationId);
                const std::string versionNameLabel = StrConcat("Version Name: ", versionName);
                const std::string versionCodeLabel = StrConcat("Version Code: ", versionCode);
                DrawInfoChip(packageLabel.c_str(), first);
                DrawInfoChip(versionNameLabel.c_str(), false);
                DrawInfoChip(versionCodeLabel.c_str(), false);
                first = false;
            }

            const std::string apkSize = report.FileSize.Ok ? FormatBytes(report.FileSize.Value) : UNAVAILABLE;
            const std::string downloadSize = report.DownloadSize.Ok ? FormatBytes(report.DownloadSize.Value) : UNAVAILABLE;
            const std::string apkSizeLabel = StrConcat("APK Size: ", apkSize);
            const std::string downloadSizeLabel = StrConcat("Download Size: ", downloadSize);
            const char *debuggableValue = UNAVAILABLE;
            if (report.Debuggable.Ok) {
                debuggableValue = report.Debuggable.Value ? "Yes" : "No";
            }
            const std::string debuggableLabel = StrConcat("Debuggable: ", debuggableValue);
            DrawInfoChip(apkSizeLabel.c_str(), first);
            DrawInfoChip(downloadSizeLabel.c_str(), false);
            DrawInfoChip(debuggableLabel.c_str(), false);
            ImGui::Spacing();
        }

        void DrawFileTree(Context &context, FileCache &cache) {
            auto &work = context.ApkAnalyzerWork;
            const bool filtering = work.TreeFilter[0] != '\0';
            MarkFile(cache.Root, work.TreeFilter, false);

            std::vector<FlatFile> rows;
            rows.reserve(static_cast<std::size_t>(std::max(cache.FileCount, 16)));
            FlattenFiles(cache.Root, 0, cache.Open, filtering, rows);

            const std::int64_t downloadTotal = cache.Root.DownloadSize;
            if (ImGui::BeginTable("##ApkFileTree", 6, PICKER_TABLE_FLAGS, ImVec2(-1.0F, -1.0F))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(" File", ImGuiTableColumnFlags_WidthStretch, 3.2F);
                ImGui::TableSetupColumn("Raw Size", ImGuiTableColumnFlags_WidthFixed, Em(11.0F));
                ImGui::TableSetupColumn("Download Size", ImGuiTableColumnFlags_WidthFixed, Em(13.0F));
                ImGui::TableSetupColumn("% of Download Size", ImGuiTableColumnFlags_WidthFixed, Em(16.0F));
                ImGui::TableSetupColumn("Compressed", ImGuiTableColumnFlags_WidthFixed, Em(11.0F));
                ImGui::TableSetupColumn("Alignment", ImGuiTableColumnFlags_WidthFixed, Em(10.0F));
                ImGui::TableHeadersRow();

                if (rows.empty()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("No files match.");
                } else {
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(rows.size()));
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                            const FlatFile &file = rows.at(static_cast<std::size_t>(row));
                            FileNode *node = file.Node;
                            const int depth = file.Depth;
                            const bool hasChildren = !node->Children.empty();
                            const bool isOpen = filtering || cache.Open.contains(node->FullPath);
                            const bool selected = work.SelectedPath == node->FullPath;

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            const LabelClick click = DrawTreeLabel(
                                node->FullPath.c_str(),
                                node->Name.c_str(),
                                node->FullPath.c_str(),
                                depth,
                                hasChildren,
                                isOpen,
                                selected,
                                FileAccent(*node)
                            );
                            if (click.Toggle) {
                                ToggleOpen(cache.Open, node->FullPath);
                            }
                            if (click.Activated) {
                                SelectFile(context, node->FullPath);
                            }
                            ImGui::TableNextColumn();
                            DrawAlignedText(FormatBytes(node->RawSize));
                            ImGui::TableNextColumn();
                            DrawAlignedText(FormatBytes(node->DownloadSize));
                            ImGui::TableNextColumn();
                            DrawShareBar(node->DownloadSize, downloadTotal);
                            ImGui::TableNextColumn();
                            const char *compressed = "Yes";
                            if (node->Directory) {
                                compressed = "N/A";
                            } else if (node->Stored) {
                                compressed = "No";
                            }
                            DrawCenteredText(compressed);
                            ImGui::TableNextColumn();
                            DrawCenteredText(AlignmentLabel(*node));
                        }
                    }
                }
                ImGui::EndTable();
            }
        }

        void DrawBrowser(Context &context) {
            auto &work = context.ApkAnalyzerWork;
            const ApkReport &report = work.Report;
            DrawApkHeader(context);

            if (!report.Files.Ok) {
                DrawQueryMiss("The file list could not be read.", report.Files.Output, false);
                return;
            }

            FileCache &cache = CachedFiles(report, work.ContentEpoch);
            const float tableHeight = std::max(Eh(8.0F), ImGui::GetContentRegionAvail().y);
            PickerTableStyle tableStyle;
            ImGui::BeginChild("##ApkTreeScroll", ImVec2(-1.0F, tableHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            DrawFileTree(context, cache);
            ImGui::EndChild();
        }

        std::string EllipsizeMiddle(const std::string &text, const float maxWidth) {
            if (maxWidth <= 0.0F || ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
                return text;
            }
            std::string gap = "...";
            if (ImGui::CalcTextSize(gap.c_str()).x >= maxWidth || text.size() < 2) {
                return gap;
            }
            std::size_t head = text.size() / 2;
            std::size_t tail = text.size() - head;
            while (head + tail > 0) {
                const std::string candidate = text.substr(0, head) + gap + text.substr(text.size() - tail);
                if (ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth) {
                    return candidate;
                }
                if (head >= tail && head > 0) {
                    head -= 1;
                } else if (tail > 0) {
                    tail -= 1;
                } else {
                    break;
                }
            }
            return gap;
        }

        std::string FitFileName(const std::string &name, const float maxWidth) {
            if (name.empty() || ImGui::CalcTextSize(name.c_str()).x <= maxWidth) {
                return name;
            }
            std::string stem = name;
            std::string extension;
            if (const auto dot = name.rfind('.'); dot != std::string::npos && dot > 0 && name.size() - dot <= 8) {
                stem = name.substr(0, dot);
                extension = name.substr(dot);
            }
            const std::string gap = "...";
            const std::size_t tailKeep = std::min<std::size_t>(8, stem.size());
            const std::string tail = stem.substr(stem.size() - tailKeep) + extension;
            if (ImGui::CalcTextSize((gap + tail).c_str()).x > maxWidth) {
                return EllipsizeMiddle(name, maxWidth);
            }
            std::size_t head = stem.size() > tailKeep ? stem.size() - tailKeep : 0;
            while (true) {
                std::string candidate = stem.substr(0, head);
                candidate.append(gap);
                candidate.append(tail);
                if (head == 0 || ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth) {
                    return candidate;
                }
                head -= 1;
            }
        }

        void DrawReadingState(Context &context) {
            auto &work = context.ApkAnalyzerWork;
            const std::string name = FileNameOf(work.PendingPath);
            const float titleBudget = std::max(1.0F, Em(66.0F) - ImGui::CalcTextSize("Analyzing ").x);
            const std::string shownName = FitFileName(name.empty() ? "APK" : name, titleBudget);
            const std::string title = StrConcat("Analyzing ", shownName);
            const bool cancelling = work.Cancel.load();
            const TaskProgress task{
                .Title = title.c_str(),
                .TitleTooltip = work.PendingPath.c_str(),
                .Subtitle = cancelling ? "Cancelling..." : "In Progress... (This may take a while)",
                .Fraction = work.Progress.load(),
                .Status = LoadStageText(work.Stage.load()),
                .CancelLabel = cancelling ? "Cancelling..." : "Cancel",
                .CancelSizingLabel = "Cancelling...",
                .CancelEnabled = !cancelling,
                .CenterVertically = true,
            };
            if (TaskProgressPanel(task)) {
                work.Cancel.store(true);
            }
        }

        void DrawEmptyState(Context &context) {
            LoadRecentApks(context);
            auto &work = context.ApkAnalyzerWork;

            std::vector<std::string> recent;
            recent.reserve(4);
            for (const auto &path: work.RecentApks) {
                if (recent.size() >= 4 || !std::filesystem::exists(path)) {
                    continue;
                }
                recent.push_back(path);
            }

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float height = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float rowH = RecentFileItemHeight();
            const float rowGap = 4.0F * GetDpiScale();
            const float recentGap = Eh(1.1F);
            const float recentHeader = Eh(1.4F);
            const float recentList = recent.empty() ? ImGui::GetTextLineHeight() : ((rowH + rowGap) * static_cast<float>(recent.size())) - rowGap;
            const float recentBlock = recentGap + recentHeader + recentList;
            const float lineH = ImGui::GetTextLineHeight();
            const float contentH = Eh(2.4F) + lineH + spacing + lineH + spacing + ImGui::GetFrameHeight();
            const float dropH = contentH + Eh(2.6F);
            const float maxW = std::max(1.0F, width - Eh(1.0F));
            const float preferredW = std::max(dropH * 2.1F, ImGui::CalcTextSize("Drag & Drop .apk file here").x + Eh(6.0F));
            const float dropW = std::min(maxW, std::clamp(preferredW, std::min(Em(28.0F), maxW), std::min(Em(40.0F), maxW)));
            const float stackH = dropH + recentBlock;
            const float stackY = origin.y + std::max(Eh(0.6F), (height - stackH) * 0.5F);
            const ImVec2 dropMin(origin.x + std::max(0.0F, (width - dropW) * 0.5F), stackY);
            const ImVec2 dropMax(dropMin.x + dropW, dropMin.y + dropH);
            const bool hovered = ImGui::IsMouseHoveringRect(dropMin, dropMax);

            ImDrawList *draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(
                dropMin,
                dropMax,
                ImGui::ColorConvertFloat4ToU32(HexColor(hovered ? Colors::SURFACE2 : Colors::SURFACE1)),
                ImGui::GetStyle().FrameRounding
            );

            const char *title = "Drag & Drop .apk file here";
            const char *orLabel = "Or";
            const float titleW = ImGui::CalcTextSize(title).x;
            const float orW = ImGui::CalcTextSize(orLabel).x;
            const float browseW = std::min(Em(16.0F), dropW - 24.0F);
            float cursorY = dropMin.y + std::max(12.0F, (dropH - contentH) * 0.5F);
            DrawUploadMark(
                ImVec2(dropMin.x + (dropW * 0.5F), cursorY + (lineH * 0.35F)),
                ImGui::ColorConvertFloat4ToU32(HexColor(hovered ? Colors::ACCENT_INFO_SOFT : Colors::ACCENT_INFO))
            );
            cursorY += Eh(2.4F);
            ImGui::SetCursorScreenPos(ImVec2(dropMin.x + ((dropW - titleW) * 0.5F), cursorY));
            ImGui::TextUnformatted(title);
            cursorY += lineH + spacing;
            ImGui::SetCursorScreenPos(ImVec2(dropMin.x + ((dropW - orW) * 0.5F), cursorY));
            ImGui::TextDisabled("%s", orLabel);
            cursorY += lineH + spacing;
            ImGui::SetCursorScreenPos(ImVec2(dropMin.x + ((dropW - browseW) * 0.5F), cursorY));
            if (PositiveButton("Browse File...", !work.Busy.load(), ImVec2(browseW, 0.0F))) {
                if (const auto picked = PickApk("Open APK", DirectoryOf(work.PendingPath))) {
                    StartAnalysis(context, *picked);
                }
            }

            const ImU32 border = ImGui::ColorConvertFloat4ToU32(HexColor(hovered ? Colors::ACCENT_INFO : Colors::BORDER_HOVER));
            const float inset = 1.5F;
            DrawDashedRoundedRect(
                ImVec2(dropMin.x + inset, dropMin.y + inset),
                ImVec2(dropMax.x - inset, dropMax.y - inset),
                ImGui::GetStyle().FrameRounding,
                border,
                1.5F
            );

            const float centerX = origin.x + (width * 0.5F);
            float rowY = dropMax.y + Eh(1.1F);
            const char *recentTitle = "Recent APKs";
            ImGui::SetCursorScreenPos(ImVec2(centerX - (ImGui::CalcTextSize(recentTitle).x * 0.5F), rowY));
            ImGui::TextDisabled("%s", recentTitle);
            rowY += Eh(1.4F);
            if (recent.empty()) {
                const char *emptyRecent = "No recent APKs";
                ImGui::SetCursorScreenPos(ImVec2(centerX - (ImGui::CalcTextSize(emptyRecent).x * 0.5F), rowY));
                ImGui::TextDisabled("%s", emptyRecent);
            }
            for (const auto &path: recent) {
                const std::string name = FileNameOf(path);
                const float itemW = RecentFileItemWidth(name.c_str());
                ImGui::SetCursorScreenPos(ImVec2(centerX - (itemW * 0.5F), rowY));
                const RecentFileAction action = RecentFileItem(path.c_str(), name.c_str());
                if (action == RecentFileAction::Activated) {
                    StartAnalysis(context, path);
                } else if (action == RecentFileAction::Removed) {
                    ForgetApk(context, path);
                }
                rowY += rowH + rowGap;
            }
        }

        const char *AdvanceWrappedLine(const char *cursor, const char *end, const float wrapW, const char *&lineEnd) {
            const char *wrap = ImGui::GetFont()->CalcWordWrapPositionA(1.0F, cursor, end, wrapW);
            if (wrap == cursor && cursor + 1 < end) {
                wrap += 1;
            }
            lineEnd = wrap;
            while (lineEnd > cursor && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\n')) {
                lineEnd -= 1;
            }
            while (wrap < end && *wrap == ' ') {
                wrap += 1;
            }
            return wrap;
        }

        int CountWrappedLines(const char *text, const char *end, const float wrapW) {
            int lines = 0;
            for (const char *cursor = text; cursor < end;) {
                lines += 1;
                const char *lineEnd = nullptr;
                cursor = AdvanceWrappedLine(cursor, end, wrapW, lineEnd);
            }
            return lines;
        }

        void DrawCenteredWrappedText(const char *text, const char *end, const float columnX, const float wrapW, float &cursorY) {
            const float textH = ImGui::GetTextLineHeight();
            for (const char *cursor = text; cursor < end;) {
                const char *lineEnd = nullptr;
                const char *next = AdvanceWrappedLine(cursor, end, wrapW, lineEnd);
                const float lineW = ImGui::CalcTextSize(cursor, lineEnd).x;
                ImGui::SetCursorPos(ImVec2(columnX + std::max(0.0F, (wrapW - lineW) * 0.5F), cursorY));
                ImGui::TextUnformatted(cursor, lineEnd);
                cursorY += textH;
                cursor = next;
            }
        }

        void DrawMissingAnalyzer(Context &context) {
            const char *title = "apkanalyzer is not installed";
            const char *body =
                "APK Analyzer uses apkanalyzer from the Android command-line tools. Install those tools to add it. "
                "AVDs already in this SDK stay where they are.";
            const char *bodyEnd = body + std::char_traits<char>::length(body);
            const float availW = std::max(1.0F, ImGui::GetContentRegionAvail().x);
            const float availH = std::max(1.0F, ImGui::GetContentRegionAvail().y);
            const float wrapW = std::min(Em(46.0F), availW);
            const float gap = Eh(0.85F);
            const float textH = ImGui::GetTextLineHeight();
            const float bodyH = textH * static_cast<float>(CountWrappedLines(body, bodyEnd, wrapW));
            const float buttonW = std::min(Em(28.0F), wrapW);
            const float blockH = textH + gap + bodyH + gap + ImGui::GetFrameHeight();
            const float originX = ImGui::GetCursorPosX();
            const float originY = ImGui::GetCursorPosY();
            ImGui::SetCursorPos(ImVec2(originX + std::max(0.0F, (availW - wrapW) * 0.5F), originY + std::max(0.0F, (availH - blockH) * 0.5F)));
            const float columnX = ImGui::GetCursorPosX();
            float cursorY = ImGui::GetCursorPosY();
            const float titleW = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPos(ImVec2(columnX + std::max(0.0F, (wrapW - titleW) * 0.5F), cursorY));
            ImGui::TextColored(HexColor(Colors::WARNING), "%s", title);
            cursorY += textH + gap;
            DrawCenteredWrappedText(body, bodyEnd, columnX, wrapW, cursorY);
            cursorY += gap;
            ImGui::SetCursorPos(ImVec2(columnX + std::max(0.0F, (wrapW - buttonW) * 0.5F), cursorY));
            if (PositiveButton("Install command-line tools...", true, ImVec2(buttonW, 0.0F))) {
                LeaveApkAnalyzer(context);
                OpenCmdlineToolsInstall(context);
            }
        }

        void DrawAnalyzerToolbar(Context &context, const bool reportBusy, const bool toolMissing, const bool showReport) {
            auto &work = context.ApkAnalyzerWork;
            if (PrimaryButton(Icons::CHEVRON_LEFT)) {
                LeaveApkAnalyzer(context);
            }
            if (!toolMissing && (reportBusy || showReport)) {
                const std::string defaultDirectory = DirectoryOf(work.HasReport ? work.Report.ApkPath : work.PendingPath);
                ImGui::SameLine();
                if (PositiveButton(reportBusy ? "Reading..." : "Open APK...", !reportBusy, ImVec2(Em(16.0F), 0.0F))) {
                    if (const auto picked = PickApk("Open APK", defaultDirectory)) {
                        StartAnalysis(context, *picked);
                    }
                }
            }
            if (!showReport) {
                return;
            }

            const char *idleLabel = "Compare with previous APK...";
            const char *busyLabel = "Comparing...";
            const char *compareLabel = work.CompareBusy.load() ? busyLabel : idleLabel;
            const float compareW = std::max(ImGui::CalcTextSize(idleLabel).x, ImGui::CalcTextSize(busyLabel).x) + (ImGui::GetStyle().FramePadding.x * 2.0F);
            ImGui::SameLine();
            if (PrimaryButton(compareLabel, !work.CompareBusy.load(), ImVec2(compareW, 0.0F))) {
                if (const auto picked = PickApk("Compare with previous APK", DirectoryOf(work.Report.ApkPath))) {
                    StartCompare(context, *picked);
                }
            }
            if (!work.Report.Files.Ok) {
                return;
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(Em(28.0F));
            const std::string hint = IconWithLabel(Icons::SEARCH, "Filter files...");
            ImGui::InputTextWithHint("##ApkTreeFilter", hint.c_str(), work.TreeFilter, IM_ARRAYSIZE(work.TreeFilter));
            const FileCache &cache = CachedFiles(work.Report, work.ContentEpoch);
            const std::string fileCount = StrConcat(FormatCount(cache.FileCount), " files");
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", fileCount.c_str());
            if (work.CompareBusy.load()) {
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(HexColor(Colors::ACCENT_INFO), "Comparing with %s...", FileNameOf(work.ComparePath).c_str());
            } else if (work.HasCompare && !work.Compare.Ok) {
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(HexColor(Colors::WARNING), "The comparison could not be read.");
            }
        }

        void AcceptPendingApkDrop(Context &context) {
            if (context.ApkAnalyzerWork.PendingDrops.empty() || context.ApkAnalyzerWork.Busy.load()) {
                return;
            }
            const std::string dropped = context.ApkAnalyzerWork.PendingDrops.front();
            context.ApkAnalyzerWork.PendingDrops.clear();
            context.UI.ShowApkAnalyzerWindow = true;
            StartAnalysis(context, dropped);
        }
    }

    void BuildApkAnalyzerWindow(Context &context) {
        PollApkAnalyzer(context);
        LoadRecentApks(context);
        AcceptPendingApkDrop(context);
        if (!context.UI.ShowApkAnalyzerWindow) {
            return;
        }
        SetNativeWindowPage(context, "APK Analyzer");

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::Begin("##ApkAnalyzer", nullptr, APK_ANALYZER_WINDOW_FLAGS);
        ImGui::PopStyleVar(2);

        auto &work = context.ApkAnalyzerWork;
        bool reportBusy = work.Busy.load();
        if (!reportBusy && !context.Host.Sdk.ApkAnalyzerPath.empty() && work.Report.ToolMissing &&
            !work.PendingPath.empty()) {
            StartAnalysis(context, work.PendingPath);
            reportBusy = work.Busy.load();
        }
        const bool toolMissing =
            context.Host.Sdk.ApkAnalyzerPath.empty() || (work.Report.ToolMissing && !work.PendingPath.empty());
        const bool showReport = work.HasReport && !reportBusy && !work.Report.ToolMissing;
        DrawAnalyzerToolbar(context, reportBusy, toolMissing, showReport);

        ImGui::Spacing();
        const float bodyHeight = std::max(Eh(12.0F), ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("##ApkAnalyzerBody", ImVec2(0.0F, bodyHeight), 0, ImGuiWindowFlags_NoScrollbar);
        if (reportBusy) {
            DrawReadingState(context);
        } else if (toolMissing) {
            DrawMissingAnalyzer(context);
        } else if (!showReport) {
            DrawEmptyState(context);
        } else {
            DrawBrowser(context);
        }
        ImGui::EndChild();
        ImGui::End();
    }
}
