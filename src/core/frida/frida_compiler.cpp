//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "core/frida/frida_compiler.h"

#include <frida-core.h>

namespace CoreDeck::Core {
    namespace {
        std::string TakeError(GError *&error) {
            std::string message = "Frida compile failed.";
            if (error != nullptr) {
                if (error->message != nullptr) {
                    message = error->message;
                }
                g_error_free(error);
                error = nullptr;
            }
            return message;
        }
    }

    FridaCompileResult CompileFridaBundle(
        void **compiler,
        void *manager,
        const std::string &projectRoot,
        const std::string &entry
    ) {
        FridaCompileResult result;
        if (compiler == nullptr || projectRoot.empty() || entry.empty()) {
            result.Message = "The Frida compiler was not given a project.";
            return result;
        }
        auto **slot = reinterpret_cast<FridaCompiler **>(compiler);
        if (*slot == nullptr) {
            *slot = frida_compiler_new(static_cast<FridaDeviceManager *>(manager));
        }
        if (*slot == nullptr) {
            result.Message = "Frida could not create a compiler.";
            return result;
        }
        FridaBuildOptions *options = frida_build_options_new();
        frida_compiler_options_set_project_root(FRIDA_COMPILER_OPTIONS(options), projectRoot.c_str());
        frida_compiler_options_set_type_check(FRIDA_COMPILER_OPTIONS(options), FRIDA_TYPE_CHECK_MODE_NONE);
        frida_compiler_options_set_bundle_format(FRIDA_COMPILER_OPTIONS(options), FRIDA_BUNDLE_FORMAT_ESM);
        GError *error = nullptr;
        gchar *bundle = frida_compiler_build_sync(*slot, entry.c_str(), options, nullptr, &error);
        frida_unref(options);
        if (error != nullptr || bundle == nullptr) {
            result.Message = TakeError(error);
            if (bundle != nullptr) {
                g_free(bundle);
            }
            return result;
        }
        result.Ok = true;
        result.Bundle.assign(bundle);
        g_free(bundle);
        return result;
    }
}
