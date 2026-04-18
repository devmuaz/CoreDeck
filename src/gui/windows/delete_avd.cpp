//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "delete_avd.h"
#include "../application.h"
#include "../widgets.h"
#include "../../core/avd.h"

namespace CoreDeck {
    void StartDeleteAvdAsync(Context &context, const std::string &avdName) {
        if (context.Jobs.AvdDeletion.Busy.load()) return;

        context.Jobs.AvdDeletion.Busy = true;
        context.Jobs.AvdDeletion.Future = std::async(std::launch::async, [&context, avdName]() {
            DeleteAvd(context.Host.Sdk, avdName);
            context.Jobs.AvdDeletion.Busy = false;
        });
    }

    void BuildDeleteAvdWindow(Context &context) {
        if (!context.Jobs.AvdDeletion.Busy.load() && context.Jobs.AvdDeletion.Future.valid()) {
            context.Jobs.AvdDeletion.Future.get();
            RefreshAvds(context);
            context.UI.ShowDeleteAvdDialog = false;
            return;
        }

        if (context.Catalog.SelectedAvd < 0 || context.Catalog.SelectedAvd >= static_cast<int>(context.Catalog.Avds.size())) return;
        if (!context.UI.ShowDeleteAvdDialog) return;

        const auto &avdName = context.Catalog.Avds[context.Catalog.SelectedAvd].Name;
        const std::string title = "Delete \"" + avdName + "\"?";
        const bool isDeleting = context.Jobs.AvdDeletion.Busy.load();
        const DialogResult result = SimpleDialog(
            {
                .Id = "Delete###DeleteAvdDialog",
                .isOpen = context.UI.ShowDeleteAvdDialog,
                .title = title.c_str(),
                .message = "This will permanently remove the AVD and all its data. This action cannot be undone.",
                .confirmButtonTitle = "Delete",
                .cancelButtonTitle = "Cancel",
                .busyButtonTitle = "Deleting...",
                .type = DialogType::Negative,
                .isBusy = isDeleting
            }
        );

        if (result == DialogResult::Confirmed) {
            StartDeleteAvdAsync(context, avdName);
        }
    }
}
