#include "vkvg_source_image.h"
#include <vkh.h>
#include <cstdio>
#include <cstdlib>

static void require(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main() {
    vkvg_device_create_info_t info{};
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.threadAware = true;
    auto device = vkvg_device_create(&info);
    require(device && !vkvg_device_status(device), "device failed");
    for (int iteration = 0; iteration < 8; ++iteration) {
        auto original = vkvg_surface_create(device, 32, 32);
        auto producer = vkvg_create(original);
        vkvg_set_source_rgb(producer, 1, 0, 0);
        vkvg_paint(producer);
        vkvg_destroy(producer);
        // Match an image graphics lease: draw into an externally owned texture,
        // retaining its existing pixels instead of the graphics' private surface.
        auto targetImport = vkh_image_import(reinterpret_cast<VkhDevice>(&info.vkdev),
            vkvg_surface_get_vk_image(original), vkvg_surface_get_vk_format(original), 32, 32);
        vkh_image_create_view(targetImport, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
        auto targetView = vkvg_surface_create_target_for_VkhImage(device, targetImport);
        require(targetView && !vkvg_surface_status(targetView), "target import failed");
        producer = vkvg_create(targetView);
        vkvg_set_source_rgb(producer, 0, 1, 0);
        vkvg_rectangle(producer, 0, 16, 32, 16);
        vkvg_fill(producer);
        vkvg_destroy(producer);
        vkvg_surface_destroy(targetView);
        vkh_image_destroy(targetImport);
        auto imported = vkh_image_import(reinterpret_cast<VkhDevice>(&info.vkdev),
            vkvg_surface_get_vk_image(original), vkvg_surface_get_vk_format(original), 32, 32);
        vkh_image_create_view(imported, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
        auto source = vkvg_surface_create_source_for_VkhImage(device, imported);
        require(source && !vkvg_surface_status(source), "source import failed");
        auto target = vkvg_surface_create(device, 96, 96);
        auto ctx = vkvg_create(target);
        vkvg_translate(ctx, 10, 7); // Parent origin must remain effective.
        vkvg_save(ctx);
        vkvg_translate(ctx, 24, 28);
        vkvg_scale(ctx, 2, 2);
        vkvg_set_opacity(ctx, 0.5f);
        vkvg_matrix_t deviceToSource;
        vkvg_get_matrix(ctx, &deviceToSource);
        vkvg_matrix_translate(&deviceToSource, -8, -16);
        require(!vkvg_matrix_invert(&deviceToSource), "inverse failed");
        vkvg_set_source_surface_transform(ctx, source, &deviceToSource);
        vkvg_new_path(ctx);
        vkvg_rectangle(ctx, 0, 0, 8, 8);
        vkvg_fill(ctx);
        vkvg_set_source_rgb(ctx, 0, 0, 0); // Synchronously flush and unbind.
        vkvg_restore(ctx);
        require(!vkvg_status(ctx), "source drawing failed");
        vkvg_surface_destroy(source);
        vkh_image_destroy(imported);
        vkvg_destroy(ctx);
        unsigned char pixels[96 * 96 * 4]{};
        require(!vkvg_surface_write_to_memory(target, pixels), "readback failed");
        const auto inside = pixels + (40 * 96 + 40) * 4;
        unsigned count = 0, minX = 96, minY = 96, maxX = 0, maxY = 0;
        for (unsigned y=0; y<96; ++y) for(unsigned x=0; x<96; ++x) if(pixels[(y*96+x)*4+3]) {
            ++count; if(x<minX) minX=x; if(y<minY) minY=y; if(x>maxX) maxX=x; if(y>maxY) maxY=y;
        }
        require(count == 256 && minX == 34 && minY == 35 && maxX == 49 && maxY == 50,
                "image draw bounds are incorrect");
        require(inside[0] < 2 && inside[1] > 240 && inside[2] < 2 &&
                inside[3] >= 126 && inside[3] <= 129, "crop, scale, opacity, or orientation incorrect");
        require(pixels[(10 * 96 + 10) * 4 + 3] == 0, "draw lost parent translation");
        vkvg_surface_destroy(target);
        vkvg_surface_destroy(original);
    }
    vkvg_device_destroy(device);
    std::puts("Imported source pixels, crop, parent translation, scale, opacity, and repeated destruction passed.");
}
