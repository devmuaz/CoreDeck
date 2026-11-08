//
// Created by AbdulMuaz Aqeel on 04/11/2026.
//

#ifndef COREDECK_ADB_SESSION_H
#define COREDECK_ADB_SESSION_H

#include <algorithm>
#include <atomic>
#include <future>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <pthread.h>
#endif

#include "../../context/context.h"
#include "../../utilities.h"
#include "../../widgets/widgets.h"

namespace CoreDeck::Gui::AdbWindow {
    struct CheckItem {
        const char *Id;
        bool *Value;
        const char *Label;
    };

    bool DeviceUsable(const Core::AdbDevice *device);

    std::string DeviceTitle(const Core::AdbDevice &device);

    const Core::AdbDevice *SelectedDevice(
        const Core::AdbDeviceSnapshot &snapshot,
        const std::string &serial
    );

    void SetTabStatus(AdbDeviceSession &session, AdbWorkspaceTab tab, StatusBarTone tone, std::string message);

    AdbDeviceSession &SelectedSession(AdbWork &workspace);

    bool AnySessionBusy(const AdbWork &workspace);

    AdbWorkspaceTab TabForTask(AdbTask task);

    const char *BusyText(AdbTask task);

    bool IsPackageTask(AdbTask task);

    std::string FirstStatusLine(const std::string &text);

    void EnsureDeviceProfiles(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    void RememberWirelessDevice(Context &context, const std::string &endpoint);

    void ForgetWirelessDevice(Context &context, const std::string &endpoint);

    Core::AdbCommandResult RunPortCommand(
        const Core::SdkInfo &sdk,
        const std::string &serial,
        bool reverse,
        const std::vector<std::string> &args,
        const std::atomic<bool> *cancel
    );

    void QueueListForwards(Context &context, AdbDeviceSession &session);

    void LeaveAdb(Context &context);

    void PollTask(Context &context);

    void SyncPortRoutes(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    void EnsureSelection(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    float ActionWidth(const char *label);

    float ComboWidth(const char *const *labels, int count, float minimum, float maximum);

    void DisabledCombo(const char *id, const char *label, float width);

    void Checkboxes(const CheckItem *items, int count);

    void DevicePicker(Context &context, const Core::AdbDeviceSnapshot &snapshot, float room);

    void ShowTabStatus(
        const AdbWork &workspace,
        const AdbDeviceSession &session,
        const Core::AdbDeviceSnapshot &snapshot,
        const Core::AdbDevice *device,
        std::optional<std::size_t> capturedLines
    );

    std::string OutputPlaceholder(
        bool busy,
        bool hasOutput,
        const std::string &output,
        const char *busyText,
        const std::string &idleText
    );

    template<typename Task>
    void StartTask(Context &context, AdbDeviceSession &session, const AdbTask task, Task run) {
        if (session.Busy.load() || context.AdbWork.CloseRequested) {
            return;
        }
        session.Cancel.store(false);
        session.Busy.store(true);
        session.Task = task;
        if (task == AdbTask::Connect || task == AdbTask::ConnectOverWifi || task == AdbTask::Disconnect) {
            context.AdbWork.DevicesTone = StatusBarTone::Busy;
            context.AdbWork.DevicesMessage = BusyText(task);
        }
        if (IsPackageTask(task)) {
            session.PackageOutput.clear();
            session.HasPackageOutput = false;
        }
        SetTabStatus(session, TabForTask(task), StatusBarTone::Busy, BusyText(task));
        const Core::SdkInfo sdk = context.Host.Adb.Sdk();
        const std::string serial = session.Serial;
        const std::atomic<bool> *cancel = &session.Cancel;
        session.Future = std::async(
            std::launch::async,
            [sdk, serial, cancel, run = std::move(run)] {
                return run(sdk, serial, cancel);
            }
        );
    }

    template<typename Content>
    float FittedCardHeight(const char *id, const Content &content, const float width) {
        const ImGuiCursorState cursor(ImGui::GetCurrentWindow());
        StyleColor sc;
        StyleVar sv;
        PushDetailCardChrome(sc, sv);
        sv.Push(ImGuiStyleVar_Alpha, 0.0F);
        ImGui::PushID(id);
        ImGui::BeginDisabled();
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        ImGui::BeginChild(
            "##card",
            ImVec2(width, 100000.0F),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground
        );
        ImGuiWindow *child = ImGui::GetCurrentWindow();
        child->Hidden = false;
        child->SkipItems = false;
        content();
        const float height = ImGui::CalcWindowNextAutoFitSize(child).y;
        ImGui::EndChild();
        ImGui::PopItemFlag();
        ImGui::EndDisabled();
        ImGui::PopID();
        return std::max(1.0F, height);
    }

    template<typename Content>
    void FittedDetailCard(const char *id, const Content &content, const float width) {
        const float height = FittedCardHeight(id, content, width);
        DetailCard(id, height, content, width);
    }

    template<typename Left, typename Right>
    void FittedCardPair(
        const char *leftId,
        const Left &left,
        const char *rightId,
        const Right &right,
        const float width,
        const float spacing
    ) {
        const float height = std::max(FittedCardHeight(leftId, left, width), FittedCardHeight(rightId, right, width));
        DetailCard(leftId, height, left, width);
        ImGui::SameLine(0.0F, spacing);
        DetailCard(rightId, height, right, width);
    }

    template<typename Pick>
    std::optional<std::string> PickFromDialog(Context &context, Pick pick) {
#ifndef _WIN32
        sigset_t blocked{};
        sigset_t previous{};
        sigemptyset(&blocked);
        sigaddset(&blocked, SIGCHLD);
        pthread_sigmask(SIG_BLOCK, &blocked, &previous);
#endif
        context.Host.Adb.StopTracking();
        std::optional<std::string> picked = pick();
#ifndef _WIN32
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
#endif
        return picked;
    }

}

#endif // COREDECK_ADB_SESSION_H
