#include "include/vkvg_source_image.h"
#include "vkvg_surface_internal.h"
#include "vkvg_context_internal.h"
#include "vkh_image.h"

VkvgSurface vkvg_surface_create_source_for_VkhImage(VkvgDevice dev, void *image) {
    VkhImage img = (VkhImage)image;
    if (!img || !img->imported || !img->view)
        return NULL;
    VkvgSurface surf = _create_surface(dev, img->infos.format);
    if (vkvg_surface_status(surf))
        return surf;
    surf->img = img;
    surf->width = img->infos.extent.width;
    surf->height = img->infos.extent.height;
    // vkh barriers read image->layout (not their old-layout argument). The
    // consumer already transitioned this image; UNDEFINED would discard it.
    img->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // No framebuffer, stencil, clear, or layout transition for a source view.
    return surf;
}

void vkvg_set_source_surface_transform(VkvgContext ctx, VkvgSurface surface,
                                       const vkvg_matrix_t *deviceToSource) {
    if (vkvg_status(ctx) || vkvg_surface_status(surface) || !deviceToSource)
        return;
    VkvgPattern pattern = vkvg_pattern_create_for_surface(surface);
    vkvg_pattern_set_matrix(pattern, deviceToSource);
    // Surface offsets are subtracted in device space by VKVG's shader. The
    // explicit matrix contains all translation, including the source crop.
    ctx->pushConsts.source.x = 0;
    ctx->pushConsts.source.y = 0;
    vkvg_set_source(ctx, pattern);
    vkvg_pattern_destroy(pattern);
}

VkvgSurface vkvg_surface_create_target_for_VkhImage(VkvgDevice dev, void *image) {
    VkhImage img = (VkhImage)image;
    if (!img || !img->imported || !img->view)
        return NULL;
    img->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return vkvg_surface_create_for_VkhImage(dev, image);
}
