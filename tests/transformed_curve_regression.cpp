#include <vkvg.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void require(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

// Compare transformed ellipses with an independent sampled conic, and cubics
// with explicitly transformed control points under the identity CTM.
static std::vector<unsigned char> render(VkvgDevice device, bool reference,
                                        float angle, bool marker, bool ellipse) {
    constexpr unsigned side = 320;
    auto surface = vkvg_surface_create(device, side, side);
    auto ctx = vkvg_create(surface);
    require(!vkvg_status(ctx), "context creation failed");
    vkvg_translate(ctx, 180, 160);
    vkvg_rotate(ctx, angle);
    vkvg_scale(ctx, 1.25f, 0.8f);
    vkvg_matrix_t matrix;
    vkvg_get_matrix(ctx, &matrix);
    if (reference) vkvg_identity_matrix(ctx);
    const float cx = marker ? 17.0f : 0.0f;
    const float cy = marker ? -12.0f : 0.0f;
    const float rx = marker ? 4.0f : 48.0f;
    const float ry = marker ? 4.0f : 40.0f;
    auto move = [&](float x, float y) {
        if (reference) vkvg_matrix_transform_point(&matrix, &x, &y);
        vkvg_move_to(ctx, x, y);
    };
    auto curve = [&](float x1, float y1, float x2, float y2, float x3, float y3) {
        if (reference) {
            vkvg_matrix_transform_point(&matrix, &x1, &y1);
            vkvg_matrix_transform_point(&matrix, &x2, &y2);
            vkvg_matrix_transform_point(&matrix, &x3, &y3);
        }
        vkvg_curve_to(ctx, x1, y1, x2, y2, x3, y3);
    };
    if (ellipse && !reference) {
        vkvg_ellipse(ctx, rx, ry, cx, cy, 0.31f);
    } else if (ellipse) {
        // Do not repeat vkvg_ellipse's approximation in its oracle.
        for (int i = 0; i < 512; ++i) {
            const double a = -1.5707963267948966 + i * 6.283185307179586 / 512;
            const double ex = rx * std::cos(a), ey = ry * std::sin(a);
            float px = cx + float(ex * std::cos(0.31f) - ey * std::sin(0.31f));
            float py = cy + float(ex * std::sin(0.31f) + ey * std::cos(0.31f));
            vkvg_matrix_transform_point(&matrix, &px, &py);
            if (i == 0) vkvg_move_to(ctx, px, py);
            else vkvg_line_to(ctx, px, py);
        }
        vkvg_close_path(ctx);
    } else {
        move(-40, -25);
        curve(-20, 45, 25, -45, 40, 25);
    }
    vkvg_set_source_rgb(ctx, 1, 1, 1);
    vkvg_set_line_width(ctx, 2);
    vkvg_stroke(ctx);
    require(!vkvg_status(ctx), "transformed stroke failed");
    vkvg_destroy(ctx);
    std::vector<unsigned char> pixels(side * side * 4);
    require(!vkvg_surface_write_to_memory(surface, pixels.data()), "readback failed");
    vkvg_surface_destroy(surface);
    return pixels;
}

static void check_wheel_radius(VkvgDevice device, float angle, float radius) {
    constexpr int side = 448;
    auto surface = vkvg_surface_create(device, side, side);
    auto ctx = vkvg_create(surface);
    require(!vkvg_status(ctx), "wheel context creation failed");
    vkvg_translate(ctx, 224, 224);
    vkvg_rotate(ctx, angle);
    vkvg_ellipse(ctx, radius, radius, 0, 0, 0);
    vkvg_set_line_width(ctx, 1);
    vkvg_set_source_rgb(ctx, 1, 1, 1);
    vkvg_stroke(ctx);
    require(!vkvg_status(ctx), "wheel stroke failed");
    vkvg_destroy(ctx);
    std::vector<unsigned char> pixels(side * side * 4);
    require(!vkvg_surface_write_to_memory(surface, pixels.data()), "wheel readback failed");
    unsigned count = 0;
    double maxError = 0;
    for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x) {
        if (!pixels[(y * side + x) * 4 + 3]) continue;
        ++count;
        const double error = std::abs(std::hypot(x + 0.5 - 224, y + 0.5 - 224) - radius);
        if (error > maxError) maxError = error;
    }
    std::printf("wheel angle=%.3f radius=%.1f pixels=%u max radial error=%.3f px\n",
                angle, radius, count, maxError);
    // Includes half the stroke width, pixel coverage and tessellation tolerance.
    // The old two-semicircle approximation exceeds this by several pixels.
    require(count > 4 * radius && maxError < 1.5, "wheel outline bulges away from the raster circle");
    vkvg_surface_destroy(surface);
}

int main() {
    vkvg_device_create_info_t info{};
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.threadAware = true;
    auto device = vkvg_device_create(&info);
    require(device && !vkvg_device_status(device), "device creation failed");
    for (float angle : {0.0f, 0.4f, 0.8f, 1.6f, 3.14159265f}) {
        check_wheel_radius(device, angle, 175.0f);
        check_wheel_radius(device, angle, 131.25f);
    }
    for (float angle : {0.0f, 0.7f, 3.14159265f}) {
        for (int shape = 0; shape < 3; ++shape) {
            const auto actual = render(device, false, angle, shape == 1, shape != 2);
            const auto reference = render(device, true, angle, shape == 1, shape != 2);
            unsigned actualCount = 0, referenceCount = 0, unmatched = 0;
            // Allow a one-pixel tessellation/rasterization difference. A displaced
            // curve must never pass just because it retains its original endpoint.
            for (int y = 0; y < 320; ++y) for (int x = 0; x < 320; ++x) {
                const bool a = actual[(y * 320 + x) * 4 + 3] != 0;
                const bool r = reference[(y * 320 + x) * 4 + 3] != 0;
                actualCount += a;
                referenceCount += r;
                if (!a) continue;
                bool nearby = false;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                    if (x + dx >= 0 && x + dx < 320 && y + dy >= 0 && y + dy < 320)
                        nearby |= reference[((y + dy) * 320 + x + dx) * 4 + 3] != 0;
                unmatched += !nearby;
            }
            std::printf("angle=%.3f shape=%d pixels=%u reference=%u misplaced=%u\n",
                        angle, shape, actualCount, referenceCount, unmatched);
            require(referenceCount > 10 && actualCount > referenceCount * 0.8f &&
                    actualCount < referenceCount * 1.2f && unmatched == 0,
                    "transformed curve differs from device-coordinate reference");
        }
    }
    vkvg_device_destroy(device);
    std::puts("Translated, rotated and scaled ellipse, marker and cubic strokes passed.");
}
