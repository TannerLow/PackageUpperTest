#pragma once
#include <o2/gui/gui.h>

// raylib backend for o2::gui: fills Input from raylib, renders draw commands
// with raylib, and lets any raylib Font be used for text.

namespace o2 {
namespace gui {
namespace raylib {

    // A raylib Font plus the spacing to draw it with. Spacing is in pixels at
    // font.baseSize and scales with the font size, so it looks the same at
    // every size.
    struct TextFont {
        Font font;
        float spacing;     // extra space after each glyph
        float lineSpacing; // extra space between lines, for '\n'
    };

    // raylib's built-in font, spaced the same as raylib's DrawText
    TextFont defaultTextFont();

    // Sets the font widgets measure and draw text with. font is used by
    // pointer, so it must stay alive and unmoved while ctx uses it.
    //
    // Fonts that store each glyph at the index of its codepoint, like the
    // Minecraft unicode font, get a direct lookup. Other fonts fall back to
    // raylib's GetGlyphIndex, which searches the font's glyph list from the
    // start, so it's slow for fonts with many glyphs.
    void useFont(Context* ctx, const TextFont* font);

    // Loads a texture and sets its filter, a raylib TextureFilter such as
    // TEXTURE_FILTER_POINT (sharp pixels) or TEXTURE_FILTER_BILINEAR
    // (smooth). Every region of the texture, e.g. in an atlas, shares the
    // filter. Free it with raylib's UnloadTexture.
    Texture2D loadTexture(const char* path, int filter);

    // Fills input from raylib's current state, including this frame's typing.
    // Use this when the UI updates every time raylib polls input (every frame).
    void readInput(Input* input);

    // For a UI that updates less often than the game loop: call every game
    // frame to keep the latest mouse position and button state, add this
    // frame's wheel movement to the total, and add this frame's typed
    // characters and editing key presses to the keyboard events. Pass input
    // to beginFrame on UI updates, then clearInput.
    void accumulateInput(Input* input);

    // Zeroes the wheel total and empties the keyboard events after a UI
    // update. Position and button state are kept.
    void clearInput(Input* input);

    void render(const Context* ctx); // call between BeginDrawing and EndDrawing

}
}
}
