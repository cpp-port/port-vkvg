#pragma once
#include <vkvg.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Source-only wrapper: does not clear, transition, or own the supplied VkhImage.
 * Caller supplies a view and keeps the image alive until sampling completes.
 * VKVG expects COLOR_ATTACHMENT_OPTIMAL on binding and leaves SHADER_READ_ONLY_OPTIMAL.
 * Never use this surface as a render target or for readback.
 */
vkvg_public VkvgSurface vkvg_surface_create_source_for_VkhImage(VkvgDevice dev, void *image);
/* Render-target import preserving pixels; caller first transitions to COLOR_ATTACHMENT_OPTIMAL. */
vkvg_public VkvgSurface vkvg_surface_create_target_for_VkhImage(VkvgDevice dev, void *image);
/* Bind with an explicit device-coordinate-to-source-pixel transform. */
vkvg_public void vkvg_set_source_surface_transform(VkvgContext ctx, VkvgSurface surface,
                                                  const vkvg_matrix_t *deviceToSource);
#ifdef __cplusplus
}
#endif
