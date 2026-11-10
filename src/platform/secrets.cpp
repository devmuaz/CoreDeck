//
// Created by AbdulMuaz Aqeel on 06/04/2026.
//

#include "platform/secrets.h"

#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace CoreDeck::Platform {
    void WipeSecret(void *data, const std::size_t size) {
        if (data == nullptr || size == 0) {
            return;
        }
#ifdef _WIN32
        SecureZeroMemory(data, size);
#else
        auto *bytes = static_cast<volatile unsigned char *>(data);
        for (std::size_t index = 0; index < size; ++index) {
            bytes[index] = 0;
        }
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    void WipeSecret(std::string &secret) {
        const std::size_t bytes = secret.capacity();
        if (bytes > 0) {
            secret.resize(bytes);
            WipeSecret(static_cast<void *>(secret.data()), secret.size());
        }
        secret.clear();
    }
}
