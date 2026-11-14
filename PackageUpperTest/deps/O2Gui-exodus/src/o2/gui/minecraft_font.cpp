#include <o2/gui/minecraft_font.h>
#include <assert.h>
#include <stdlib.h> // RL_CALLOC is calloc by default
#include <string.h>

namespace o2 {
namespace minecraft {

    static const int GLYPH_COUNT = 65536; // every codepoint from U+0000 to U+FFFF
    static const int CELL_SIZE = 16;
    static const int CELLS_PER_ROW = 256;

    // width byte for codepoints missing from the widths file: the full cell
    static const unsigned char FULL_WIDTH = 0x0F;

    Font loadUnicodeFont(const char* atlasPath, const char* widthsPath) {
        Font font;
        memset(&font, 0, sizeof(Font));
        font.baseSize = CELL_SIZE;
        font.glyphCount = GLYPH_COUNT;
        font.glyphPadding = 0;

        Image atlas = LoadImage(atlasPath);
        assert(IsImageValid(atlas));
        font.texture = LoadTextureFromImage(atlas);
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT); // keep pixels sharp when scaled
        UnloadImage(atlas);

        int widthsSize = 0;
        unsigned char* widths = LoadFileData(widthsPath, &widthsSize);
        assert(widths != NULL);

        // raylib's UnloadFont frees these with RL_FREE, so they must come from RL_CALLOC
        font.glyphs = (GlyphInfo*)RL_CALLOC(GLYPH_COUNT, sizeof(GlyphInfo));
        font.recs = (Rectangle*)RL_CALLOC(GLYPH_COUNT, sizeof(Rectangle));

        for (int i = 0; i < GLYPH_COUNT; i++) {
            unsigned char width = (i < widthsSize) ? widths[i] : FULL_WIDTH;
            int startX = (width >> 4) & 0x0F;
            int endX = width & 0x0F;

            // the source rect covers the whole cell; offsetX shifts the glyph
            // left so its first used column lands at the pen position
            font.glyphs[i].value = i;
            font.glyphs[i].offsetX = -startX;
            font.glyphs[i].offsetY = 0;
            font.glyphs[i].advanceX = endX - startX + 1;

            font.recs[i].x = (float)((i % CELLS_PER_ROW) * CELL_SIZE);
            font.recs[i].y = (float)((i / CELLS_PER_ROW) * CELL_SIZE);
            font.recs[i].width = (float)CELL_SIZE;
            font.recs[i].height = (float)CELL_SIZE;
        }

        UnloadFileData(widths);
        return font;
    }

}
}
