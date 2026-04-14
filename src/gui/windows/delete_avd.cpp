//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "delete_avd.h"
#include "../application.h"
#include "../widgets.h"

namespace CoreDeck {
    void BuildDeleteAvdWindow(Context &context) {
        if (context.SelectedAvd < 0 || context.SelectedAvd >= static_cast<int>(context.Avds.size())) return;

        if (context.ShowDeleteDialog && !context.AsyncBusy && context.AsyncFuture.valid()) {
            context.AsyncFuture.get();
            context.ShowDeleteDialog = false;
            RefreshAvds(context);
            return;
        }

        const auto &avdName = context.Avds[context.SelectedAvd].Name;
        const std::string title = "Delete \"" + avdName + "\"?";
        const bool isDeleting = context.AsyncBusy.load();
        const DialogResult result = SimpleDialog(
            {
                .Id = "Delete###DeleteAvdDialog",
                .isOpen = context.ShowDeleteDialog,
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
            context.AsyncBusy = true;
            const std::string deletableAvdName = avdName;
            context.AsyncFuture = std::async(std::launch::async, [&context, deletableAvdName]() {
                DeleteAvd(context.Sdk, deletableAvdName);
                context.AsyncBusy = false;
            });
        }
    }
}
