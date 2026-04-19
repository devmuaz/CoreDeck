//
// Created by AbdulMuaz Aqeel on 18/04/2026.
//

#include <chrono>
#include "imgui.h"

#include "install_image.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    void BuildInstallImageWindow(Context &context) {
        if (context.UI.ShowInstallImageDialog) {
            constexpr auto title = "Install System Image###InstallImageDialog";
            if (!ImGui::IsPopupOpen(title)) {
                ImGui::OpenPopup(title);
            }

            const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);

            constexpr ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoDocking;

            const bool installing = context.ImageInstallationWork.Installing.load();
            bool *pOpen = installing ? nullptr : &context.UI.ShowInstallImageDialog;

            if (ImGui::BeginPopupModal(title, pOpen, flags)) {
                auto &work = context.ImageInstallationWork;
                const bool isLoading = work.Prefetch.Loading.load();
                const bool isInstalling = installing;


                if (!isInstalling && work.InstallFuture.valid()) {
                    if (work.InstallFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        if (work.InstallFuture.get()) {
                            context.AvdCreationWork.SystemImages = ListSystemImages(context.Host.Sdk);
                            work.RemoteImages = ListRemoteSystemImages(
                                context.Host.Sdk,
                                context.AvdCreationWork.SystemImages
                            );
                        }
                    }
                }

                ImGui::Text("Available System Images");
                ImGui::Spacing();

                if (isLoading) {
                    ImGui::TextDisabled("Fetching available images from SDK manager...");
                } else if (work.RemoteImages.empty()) {
                    ImGui::TextColored(
                        HexColor("#E64D40"),
                        "No remote system images found. Check your SDK and internet connection."
                    );
                } else {
                    bool hasAvailable = false;
                    for (const auto &img: work.RemoteImages) {
                        if (!img.IsInstalled) {
                            hasAvailable = true;
                            break;
                        }
                    }

                    if (!hasAvailable) {
                        ImGui::TextColored(HexColor("#33CC47"), "All available system images are already installed.");
                    } else {
                        if (isInstalling) ImGui::BeginDisabled();
                        ImGui::SetNextItemWidth(-1.0f);
                        auto preview = "Select an image...";
                        if (work.SelectedImage >= 0 &&
                            work.SelectedImage < static_cast<int>(work.RemoteImages.size()) &&
                            !work.RemoteImages[work.SelectedImage].IsInstalled) {
                            preview = work.RemoteImages[work.SelectedImage].DisplayName.c_str();
                        }

                        if (ImGui::BeginCombo("##RemoteImagePicker", preview)) {
                            for (int i = 0; i < static_cast<int>(work.RemoteImages.size()); i++) {
                                const auto &img = work.RemoteImages[i];
                                if (img.IsInstalled) continue;

                                const bool isSelected = work.SelectedImage == i;
                                if (ImGui::Selectable(img.DisplayName.c_str(), isSelected)) {
                                    work.SelectedImage = i;
                                }
                                if (isSelected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        if (isInstalling) ImGui::EndDisabled();
                    }
                }

                if (isInstalling && work.Progress) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    float fraction;
                    std::string statusText; {
                        std::lock_guard lock(work.Progress->Mutex);
                        fraction = work.Progress->Percent;
                        statusText = work.Progress->StatusText;
                    }

                    ImGui::Text("%s", statusText.c_str());
                    ImGui::Spacing();

                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, HexColor("#33CC47"));
                    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f));
                    ImGui::PopStyleColor();
                }

                if (!isInstalling && work.Progress) {
                    bool finished;
                    bool succeeded;
                    std::string statusText; {
                        std::lock_guard lock(work.Progress->Mutex);
                        finished = work.Progress->Finished;
                        succeeded = work.Progress->Succeeded;
                        statusText = work.Progress->StatusText;
                    }

                    if (finished) {
                        ImGui::Spacing();
                        const float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
                        ImGui::SetCursorPosX(
                            (ImGui::GetContentRegionAvail().x - textWidth) * 0.5f + ImGui::GetCursorStartPos().x);
                        if (succeeded) ImGui::TextColored(HexColor("#33CC47"), "%s", statusText.c_str());
                        else ImGui::TextColored(HexColor("#E64D40"), "%s", statusText.c_str());
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const bool canInstall =
                        !isLoading && !isInstalling &&
                        work.SelectedImage >= 0 &&
                        work.SelectedImage < static_cast<int>(work.RemoteImages.size()) &&
                        !work.RemoteImages[work.SelectedImage].IsInstalled;

                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float halfWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

                if (isInstalling) {
                    ImGui::BeginDisabled();
                    PositiveButton("Installing...", false, ImVec2(halfWidth, 0));
                    ImGui::EndDisabled();
                } else {
                    if (PositiveButton("Install", canInstall, ImVec2(halfWidth, 0))) {
                        const auto &img = work.RemoteImages[work.SelectedImage];
                        work.Progress = std::make_shared<InstallProgressData>();
                        work.Installing = true;
                        const std::string pkgPath = img.PackagePath;
                        auto progress = work.Progress;
                        work.InstallFuture = std::async(
                            std::launch::async,
                            [&context, pkgPath, progress] {
                                const bool ok = InstallSystemImage(context.Host.Sdk, pkgPath, progress);
                                context.ImageInstallationWork.Installing = false;
                                return ok;
                            }
                        );
                    }
                }

                ImGui::SameLine();
                if (PrimaryButton("Close", !isInstalling, ImVec2(halfWidth, 0))) {
                    work.Progress.reset();
                    context.UI.ShowInstallImageDialog = false;
                    if (context.UI.ReopenCreateAvdOnInstallClose) {
                        context.UI.ReopenCreateAvdOnInstallClose = false;
                        context.UI.ShowCreateAvdDialog = true;
                    }
                }

                ImGui::EndPopup();
            }
        }

        if (!context.UI.ShowInstallImageDialog && context.UI.ReopenCreateAvdOnInstallClose) {
            context.UI.ReopenCreateAvdOnInstallClose = false;
            context.UI.ShowCreateAvdDialog = true;
        }
    }
}
