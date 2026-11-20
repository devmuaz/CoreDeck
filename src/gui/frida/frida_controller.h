//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#ifndef COREDECK_GUI_FRIDA_FRIDA_CONTROLLER_H
#define COREDECK_GUI_FRIDA_FRIDA_CONTROLLER_H

#include <cstdint>
#include <string>

#include "gui/shell/context.h"

namespace CoreDeck::Gui::Frida {
    FridaDeviceSession &SelectedSession(FridaState &workspace);

    void SetTabStatus(FridaDeviceSession &session, FridaWorkspaceTab tab, Tone tone, std::string message);

    FridaTabStatus VisibleTabStatus(
        const FridaState &workspace,
        const FridaDeviceSession &session,
        const Core::AdbDevice *device
    );

    bool AnySessionWorking(const FridaState &workspace);

    void LoadFridaSettings(FridaState &workspace);

    void SaveFridaSettings(const FridaState &workspace);

    void EnsureSelection(Context &context, const Core::AdbDeviceSnapshot &snapshot);

    void PollFrida(Context &context);

    void StartManagedFrida(Context &context, FridaDeviceSession &session);

    void UseExistingFrida(Context &context, FridaDeviceSession &session);

    void StopFridaSession(const Context &context, FridaDeviceSession &session);

    void RefreshFridaProcesses(Context &context, const FridaDeviceSession &session);

    void AttachSelectedFridaProcess(Context &context, FridaDeviceSession &session);

    void SpawnFridaPackage(Context &context, FridaDeviceSession &session);

    void ResumeFridaSpawn(Context &context, const FridaDeviceSession &session);

    void DetachFrida(Context &context, const FridaDeviceSession &session);

    void RunFridaScript(Context &context, FridaDeviceSession &session);

    void StopFridaScript(Context &context, const FridaDeviceSession &session);

    void RequestFridaModules(Context &context, FridaDeviceSession &session);

    void RequestFridaExports(Context &context, FridaDeviceSession &session, const std::string &moduleName);

    void RequestFridaRead(Context &context, FridaDeviceSession &session);

    void RequestFridaWrite(Context &context, FridaDeviceSession &session);

    void RequestFridaScan(Context &context, FridaDeviceSession &session);

    void RequestJavaClasses(Context &context, FridaDeviceSession &session);

    void RequestJavaMethods(Context &context, FridaDeviceSession &session, const std::string &className);

    void RequestJavaHook(Context &context, FridaDeviceSession &session);

    void RequestNativeHook(Context &context, FridaDeviceSession &session);

    void RequestUnhook(Context &context, FridaDeviceSession &session, std::uint64_t hookId);

    std::string FridaScriptsDirectory(const FridaState &workspace);
}

#endif // COREDECK_GUI_FRIDA_FRIDA_CONTROLLER_H
