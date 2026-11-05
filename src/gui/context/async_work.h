//
// Created by AbdulMuaz Aqeel on 03/11/2026.
//

#ifndef COREDECK_ASYNC_WORK_H
#define COREDECK_ASYNC_WORK_H

#include <atomic>
#include <future>

namespace CoreDeck::Gui {
    struct AsyncLoad {
        std::atomic<bool> Loading{false};
        std::atomic<bool> Ready{false};
        std::future<void> Future;
    };

    template<typename Result>
    struct AsyncWork {
        std::atomic<bool> Busy{false};
        std::future<Result> Future;
    };
}

#endif // COREDECK_ASYNC_WORK_H
