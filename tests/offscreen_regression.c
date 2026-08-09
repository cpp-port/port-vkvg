#include "vkvg.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST_WIDTH 208
#define TEST_HEIGHT 64

static unsigned char *pixel_at(unsigned char *pixels, unsigned int x, unsigned int y) {
    return pixels + (y * TEST_WIDTH + x) * 4;
}

static int report_pixel_failure(const char *name, const unsigned char *pixel) {
    fprintf(stderr, "%s pixel was RGBA(%u, %u, %u, %u)\n", name, pixel[0], pixel[1], pixel[2], pixel[3]);
    return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
    vkvg_device_create_info_t info = {VK_SAMPLE_COUNT_1_BIT, false};
    VkvgDevice                dev  = vkvg_device_create(&info);
    vkvg_status_t             status = vkvg_device_status(dev);
    if (status != VKVG_STATUS_SUCCESS) {
        fprintf(stderr, "device creation failed: %s\n", vkvg_status_to_string(status));
        return EXIT_FAILURE;
    }

    VkvgSurface surf = vkvg_surface_create(dev, TEST_WIDTH, TEST_HEIGHT);
    status = vkvg_surface_status(surf);
    if (status != VKVG_STATUS_SUCCESS) {
        fprintf(stderr, "surface creation failed: %s\n", vkvg_status_to_string(status));
        vkvg_device_destroy(dev);
        return EXIT_FAILURE;
    }

    VkvgContext ctx = vkvg_create(surf);
    status = vkvg_status(ctx);
    if (status != VKVG_STATUS_SUCCESS) {
        fprintf(stderr, "context creation failed: %s\n", vkvg_status_to_string(status));
        vkvg_surface_destroy(surf);
        vkvg_device_destroy(dev);
        return EXIT_FAILURE;
    }

    vkvg_clear(ctx);

    vkvg_rectangle(ctx, 8, 8, 40, 48);
    vkvg_set_source_rgb(ctx, 1, 0, 0);
    vkvg_fill(ctx);

    VkvgPattern gradient = vkvg_pattern_create_linear(0, 8, 0, 56);
    status = vkvg_pattern_status(gradient);
    if (status != VKVG_STATUS_SUCCESS) {
        fprintf(stderr, "gradient creation failed: %s\n", vkvg_status_to_string(status));
        vkvg_destroy(ctx);
        vkvg_surface_destroy(surf);
        vkvg_device_destroy(dev);
        return EXIT_FAILURE;
    }
    vkvg_pattern_add_color_stop(gradient, 0, 0, 1, 0, 1);
    vkvg_pattern_add_color_stop(gradient, 1, 0, 0, 1, 1);

    vkvg_move_to(ctx, 120, 56);
    vkvg_line_to(ctx, 73, 56);
    vkvg_line_to(ctx, 68, 8);
    vkvg_line_to(ctx, 80, 8);
    vkvg_line_to(ctx, 120, 8);
    vkvg_line_to(ctx, 120, 56);
    vkvg_close_path(ctx);

    vkvg_move_to(ctx, 192, 56);
    vkvg_line_to(ctx, 192, 8);
    vkvg_line_to(ctx, 152, 8);
    vkvg_line_to(ctx, 140, 8);
    vkvg_line_to(ctx, 145, 56);
    vkvg_line_to(ctx, 192, 56);
    vkvg_close_path(ctx);
    vkvg_set_source(ctx, gradient);
    vkvg_pattern_destroy(gradient);
    vkvg_fill(ctx);

    vkvg_destroy(ctx);

    unsigned char *pixels = calloc(TEST_WIDTH * TEST_HEIGHT, 4);
    if (!pixels) {
        vkvg_surface_destroy(surf);
        vkvg_device_destroy(dev);
        return EXIT_FAILURE;
    }

    if (vkvg_surface_write_to_memory(surf, pixels) != VKVG_STATUS_SUCCESS) {
        free(pixels);
        vkvg_surface_destroy(surf);
        vkvg_device_destroy(dev);
        return EXIT_FAILURE;
    }

    if (argc > 1 && vkvg_surface_write_to_png(surf, argv[1]) != VKVG_STATUS_SUCCESS) {
        free(pixels);
        vkvg_surface_destroy(surf);
        vkvg_device_destroy(dev);
        return EXIT_FAILURE;
    }

    unsigned char *background = pixel_at(pixels, 2, 2);
    unsigned char *solid = pixel_at(pixels, 24, 32);
    unsigned char *clockwise_gradient_top = pixel_at(pixels, 100, 16);
    unsigned char *clockwise_gradient_bottom = pixel_at(pixels, 100, 48);
    unsigned char *counterclockwise_gradient_top = pixel_at(pixels, 172, 16);
    unsigned char *counterclockwise_gradient_bottom = pixel_at(pixels, 172, 48);

    int result = EXIT_SUCCESS;
    if (background[3] != 0)
        result = report_pixel_failure("background", background);
    if (solid[0] < 240 || solid[1] > 15 || solid[2] > 15 || solid[3] < 240)
        result = report_pixel_failure("solid fill", solid);
    if (clockwise_gradient_top[1] <= clockwise_gradient_top[2] || clockwise_gradient_top[3] < 240)
        result = report_pixel_failure("clockwise gradient top", clockwise_gradient_top);
    if (clockwise_gradient_bottom[2] <= clockwise_gradient_bottom[1] || clockwise_gradient_bottom[3] < 240)
        result = report_pixel_failure("clockwise gradient bottom", clockwise_gradient_bottom);
    if (counterclockwise_gradient_top[1] <= counterclockwise_gradient_top[2] ||
        counterclockwise_gradient_top[3] < 240)
        result = report_pixel_failure("counterclockwise gradient top", counterclockwise_gradient_top);
    if (counterclockwise_gradient_bottom[2] <= counterclockwise_gradient_bottom[1] ||
        counterclockwise_gradient_bottom[3] < 240)
        result = report_pixel_failure("counterclockwise gradient bottom", counterclockwise_gradient_bottom);

    free(pixels);
    vkvg_surface_destroy(surf);
    vkvg_device_destroy(dev);
    return result;
}
