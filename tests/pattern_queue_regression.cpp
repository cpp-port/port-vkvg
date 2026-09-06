#include "vkvg_queue_host_sync.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace {
struct queue_sync {
    std::recursive_mutex mutex;
    std::atomic<unsigned> locks{0};
    std::atomic<unsigned> unlocks{0};
};

void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void lock_queue(void *data) noexcept {
    auto &sync = *static_cast<queue_sync *>(data);
    sync.mutex.lock();
    ++sync.locks;
}

void unlock_queue(void *data) noexcept {
    auto &sync = *static_cast<queue_sync *>(data);
    ++sync.unlocks;
    sync.mutex.unlock();
}

void render(VkvgDevice device) {
    // Each thread creates, uses, and destroys its own context and surfaces.
    VkvgSurface source = vkvg_surface_create(device, 32, 32);
    VkvgSurface target = vkvg_surface_create(device, 32, 32);
    require(vkvg_surface_status(source) == VKVG_STATUS_SUCCESS, "source creation failed");
    require(vkvg_surface_status(target) == VKVG_STATUS_SUCCESS, "target creation failed");
    VkvgContext sourceContext = vkvg_create(source);
    vkvg_set_source_rgb(sourceContext, 1, 0, 0);
    vkvg_paint(sourceContext);
    vkvg_destroy(sourceContext);

    VkvgContext ctx = vkvg_create(target);
    VkvgPattern pattern = vkvg_pattern_create_for_surface(source);
    vkvg_set_source(ctx, pattern);
    require(vkvg_pattern_get_reference_count(pattern) == 2, "initial pattern ownership");

    for (unsigned i = 0; i < 32; ++i) {
        // Unchanged and replaced patterns take different restore branches.
        vkvg_save(ctx);
        vkvg_restore(ctx);
        require(vkvg_pattern_get_reference_count(pattern) == 2, "unchanged restore released context ownership");
        vkvg_save(ctx);
        vkvg_set_source_rgb(ctx, 0, 1, 0);
        vkvg_restore(ctx);
        require(vkvg_pattern_get_reference_count(pattern) == 2, "changed restore released transferred ownership");
        vkvg_paint(ctx);
        vkvg_flush(ctx);
        require(vkvg_status(ctx) == VKVG_STATUS_SUCCESS, "render failed");
    }

    vkvg_pattern_destroy(pattern);
    vkvg_destroy(ctx);
    unsigned char pixels[32 * 32 * 4]{};
    require(vkvg_surface_write_to_memory(target, pixels) == VKVG_STATUS_SUCCESS, "readback failed");
    // Surface storage can be RGBA or BGRA; both must retain solid red.
    require(pixels[1] == 0 && pixels[3] == 255 &&
            ((pixels[0] == 255 && pixels[2] == 0) || (pixels[0] == 0 && pixels[2] == 255)),
            "restored surface pattern produced incorrect pixels");
    vkvg_surface_destroy(target);
    vkvg_surface_destroy(source);
}
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("Creating thread-aware VKVG test device...");
    queue_sync sync;
    vkvg_device_create_info_t info{};
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.threadAware = true;
    VkvgDevice device = vkvg_device_create(&info);
    std::puts("Device created; installing queue callbacks...");
    require(device && vkvg_device_status(device) == VKVG_STATUS_SUCCESS, "device creation failed");
    vkvg_device_set_queue_host_synchronization(device, &sync, lock_queue, unlock_queue);

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(info.vkdev, info.qFamIdx, info.qIndex, &queue);
    std::atomic<unsigned> completed{0};
    std::puts("Starting concurrent rendering...");
    std::thread first([&] { render(device); ++completed; });
    std::thread second([&] { render(device); ++completed; });
    unsigned externalCalls = 0;
    while (completed.load() != 2) {
        {
            // Emulate ca2 using the very same raw queue outside VKVG.
            std::lock_guard<std::recursive_mutex> lock(sync.mutex);
            require(vkQueueWaitIdle(queue) == VK_SUCCESS, "external queue wait failed");
            ++externalCalls;
        }
        std::this_thread::yield();
    }
    first.join();
    second.join();
    vkvg_device_destroy(device);
    require(sync.locks.load() > 0, "VKVG did not invoke external synchronization");
    require(sync.locks.load() == sync.unlocks.load(), "unbalanced queue callbacks");
    std::printf("Pattern ownership, pixel readback, and concurrent queue access passed (%u VKVG calls, %u external calls).\n",
                sync.locks.load(), externalCalls);
}
