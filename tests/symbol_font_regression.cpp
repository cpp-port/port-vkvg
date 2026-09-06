#include <vkvg.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

static void require(bool ok, const char *message)
{
    if (!ok) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

static void check_font(VkvgDevice device, FT_Library library, const char *path,
                      const char *name, bool symbolOnly)
{
    FT_Face face = nullptr;
    require(FT_New_Face(library, path, 0, &face) == 0, "FreeType font open failed");
    const auto unicodeError = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    require(symbolOnly ? unicodeError != 0 : unicodeError == 0, "unexpected test font charmap");
    if (unicodeError)
        require(FT_Select_Charmap(face, FT_ENCODING_MS_SYMBOL) == 0, "symbol map missing");

    // Symbol fonts need not contain the Latin alphabet. Pick three real
    // byte-encoded glyphs, then compare their advances against FreeType.
    std::string sample;
    for (unsigned ch = 33; ch < 127 && sample.size() < 3; ++ch) {
        auto index = FT_Get_Char_Index(face, ch);
        if (!index && symbolOnly) index = FT_Get_Char_Index(face, ch + 0xf000);
        if (index) sample += static_cast<char>(ch);
    }
    require(sample.size() == 3, "test font needs three symbol glyphs");

    auto surface = vkvg_surface_create(device, 256, 128);
    auto ctx = vkvg_create(surface);
    require(vkvg_status(ctx) == VKVG_STATUS_SUCCESS, "context creation failed");
    vkvg_load_font_from_path(ctx, path, name);
    int dpiX = 0, dpiY = 0;
    vkvg_device_get_dpy(device, &dpiX, &dpiY);
    for (int round = 0; round < 2; ++round) {
        for (int size : {16, 24, 32}) {
            vkvg_select_font_face(ctx, name);
            vkvg_set_font_size(ctx, static_cast<float>(size));
            require(FT_Set_Char_Size(face, 0, size << 6, dpiX, dpiY) == 0, "FreeType size failed");
            unsigned expectedWidth = 0;
            for (unsigned char ch : sample) {
                auto index = FT_Get_Char_Index(face, ch);
                if (!index && symbolOnly) index = FT_Get_Char_Index(face, ch + 0xf000);
                require(index != 0, "test sample must contain real glyphs");
                require(FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) == 0, "FreeType glyph load failed");
                expectedWidth += face->glyph->advance.x >> 6;
            }
            vkvg_text_extents_t extents{};
            vkvg_text_extents(ctx, sample.c_str(), &extents);
            require(vkvg_status(ctx) == VKVG_STATUS_SUCCESS, "VKVG measurement failed");
            require(std::fabs(extents.x_advance - expectedWidth) < 0.01f,
                    "VKVG symbol glyph advances differ from FreeType");
            require(extents.height > 0, "missing font height");
            vkvg_clear(ctx);
            vkvg_set_source_rgb(ctx, 1, 1, 1);
            vkvg_move_to(ctx, 8, 72);
            vkvg_show_text(ctx, sample.c_str());
            vkvg_flush(ctx);
            require(vkvg_status(ctx) == VKVG_STATUS_SUCCESS, "VKVG symbol drawing failed");
            unsigned char pixels[256 * 128 * 4]{};
            require(vkvg_surface_write_to_memory(surface, pixels) == VKVG_STATUS_SUCCESS,
                    "symbol readback failed");
            unsigned painted = 0;
            for (unsigned i = 3; i < sizeof(pixels); i += 4) painted += pixels[i] != 0;
            require(painted > 20, "font preview has no visible glyphs");
        }
    }
    vkvg_destroy(ctx);
    vkvg_surface_destroy(surface);
    FT_Done_Face(face);
    std::printf("%s: measurement, repeated sizes, and glyph readback passed.\n", name);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "Usage: symbol_font_regression marlett.ttf wingding.ttf arial.ttf\n");
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    FT_Library library = nullptr;
    require(FT_Init_FreeType(&library) == 0, "FreeType init failed");
    vkvg_device_create_info_t info{};
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.threadAware = true;
    auto device = vkvg_device_create(&info);
    require(device && vkvg_device_status(device) == VKVG_STATUS_SUCCESS, "VKVG device creation failed");
    check_font(device, library, argv[1], "Marlett", true);
    check_font(device, library, argv[2], "Wingdings", true);
    check_font(device, library, argv[3], "Arial", false);
    vkvg_device_destroy(device);
    FT_Done_FreeType(library);
}
