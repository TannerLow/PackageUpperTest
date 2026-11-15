#pragma once
#include <raylib.h>

// Loader for Minecraft's unicode bitmap font, used by the demo. Ported from
// o2craft's LoadMinecraftCustomFont; when o2gui is integrated into o2craft,
// o2craft keeps its own loader and font files.
//
// The font is a 4096x4096 atlas of 16x16 cells, one cell per codepoint from
// U+0000 to U+FFFF, row by row. The widths file has one byte per codepoint:
// the upper 4 bits are the first pixel column the glyph uses, the lower 4
// bits are the last.
//
// The returned font stores each glyph at the index of its codepoint, so
// o2::gui::raylib looks glyphs up directly. Its base size is 16, so it is
// only pixel-perfect at multiples of 16. Free it with raylib's UnloadFont.

namespace o2 {
namespace minecraft {

    Font loadUnicodeFont(const char* atlasPath, const char* widthsPath);

}
}
