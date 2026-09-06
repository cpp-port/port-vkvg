#pragma once

#include <vkvg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wrapper extension. Install before publishing the device to other threads.
 * Callbacks must not throw and userData must outlive the VkvgDevice.
 * Both callbacks must be supplied together (or both NULL to disable).
 * The caller must also guard vkvg_device_create, which can submit work before
 * this hook is installed. Contexts still require exclusive caller ownership.
 */
typedef void (*vkvg_queue_host_sync_callback_t)(void *userData);
vkvg_public void vkvg_device_set_queue_host_synchronization(
    VkvgDevice dev, void *userData, vkvg_queue_host_sync_callback_t lockCallback,
    vkvg_queue_host_sync_callback_t unlockCallback);

#ifdef __cplusplus
}
#endif
