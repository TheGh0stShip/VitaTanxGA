#pragma once

#include <cstdlib>
#include <functional>
#include <utility>

#include <psp2/kernel/threadmgr.h>

namespace btga {

static_assert(sizeof(void*) == 4, "Vita thread bridge requires the ILP32 ABI");

// Preserve the runtime's C++ callable interface while owning a native Vita
// user thread. VitaSDK's std::thread constructor does not reach its callable
// in the current Vita3K environment.
class VitaThread {
public:
    VitaThread() = default;

    template <typename Function, typename... Args>
    explicit VitaThread(Function&& function, Args&&... args) {
        auto bound = std::bind(std::forward<Function>(function),
                               std::forward<Args>(args)...);
        auto* context = new StartContext{
            [callable = std::move(bound)]() mutable { callable(); }};
        uid_ = sceKernelCreateThread("VitaTanxGA runtime", thread_entry,
                                     0x10000100, 0x40000, 0, 0, nullptr);
        if (uid_ < 0) {
            delete context;
            std::abort();
        }
        StartContext* argument = context;
        if (sceKernelStartThread(uid_, sizeof(argument), &argument) < 0) {
            sceKernelDeleteThread(uid_);
            uid_ = -1;
            delete context;
            std::abort();
        }
    }

    VitaThread(const VitaThread&) = delete;
    VitaThread& operator=(const VitaThread&) = delete;

    VitaThread(VitaThread&& other) noexcept : uid_(other.uid_) {
        other.uid_ = -1;
    }

    VitaThread& operator=(VitaThread&& other) noexcept {
        if (joinable()) std::abort();
        uid_ = other.uid_;
        other.uid_ = -1;
        return *this;
    }

    ~VitaThread() {
        if (joinable()) std::abort();
    }

    bool joinable() const noexcept { return uid_ >= 0; }

    void join() {
        if (!joinable()) std::abort();
        int status = 0;
        if (sceKernelWaitThreadEnd(uid_, &status, nullptr) < 0) std::abort();
        sceKernelDeleteThread(uid_);
        uid_ = -1;
    }

    void detach() {
        if (!joinable()) std::abort();
        // The sole detached worker is the process-lifetime timer. Finite
        // workers remain owned and are joined explicitly.
        uid_ = -1;
    }

private:
    struct StartContext {
        std::function<void()> callable;
    };

    static int thread_entry(SceSize argument_size, void* arguments) {
        if (argument_size != sizeof(StartContext*) || arguments == nullptr) {
            return -1;
        }
        StartContext* context = *static_cast<StartContext**>(arguments);
        auto callable = std::move(context->callable);
        delete context;
        callable();
        return 0;
    }

    SceUID uid_ = -1;
};

} // namespace btga
