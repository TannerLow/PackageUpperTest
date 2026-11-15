#include <o2/gui/gui_raylib.h>
#include <math.h>

namespace o2 {
namespace gui {
namespace raylib {

    // Text drawing and measuring follow raylib's DrawTextEx / MeasureTextEx,
    // with two differences:
    // - glyphs are found with glyphIndex instead of always searching
    // - measuring uses the same advance rule as drawing, so measured widths
    //   match what is drawn and centered text is actually centered

    static int glyphIndex(const Font* font, int codepoint) {
        // fast path: the glyph for codepoint is stored at index codepoint
        if (codepoint >= 0 && codepoint < font->glyphCount && font->glyphs[codepoint].value == codepoint) {
            return codepoint;
        }
        // searches every glyph, falls back to '?' if the codepoint isn't in the font
        return GetGlyphIndex(*font, codepoint);
    }

    // how far the pen moves after drawing the glyph at index
    static float glyphAdvance(const TextFont* textFont, int index, float scale) {
        const Font* font = &textFont->font;
        float advance = (float)font->glyphs[index].advanceX;
        if (advance == 0.0f) {
            advance = font->recs[index].width;
        }
        return (advance + textFont->spacing) * scale;
    }

    // same as raylib's DrawTextCodepoint, minus its glyph search
    static void drawGlyph(const Font* font, int index, Vector2 position, float scale, Color color) {
        const GlyphInfo* glyph = &font->glyphs[index];
        Rectangle rec = font->recs[index];
        float padding = (float)font->glyphPadding;

        Rectangle source = { rec.x - padding, rec.y - padding, rec.width + 2.0f * padding, rec.height + 2.0f * padding };
        Rectangle dest = {
            position.x + ((float)glyph->offsetX - padding) * scale,
            position.y + ((float)glyph->offsetY - padding) * scale,
            source.width * scale,
            source.height * scale
        };
        DrawTexturePro(font->texture, source, dest, Vector2{ 0, 0 }, 0.0f, color);
    }

    static Vector2 textFontMeasure(void* user, const char* text, float fontSize) {
        const TextFont* textFont = (const TextFont*)user;
        float scale = fontSize / (float)textFont->font.baseSize;
        float trailingSpacing = textFont->spacing * scale; // no spacing after a line's last glyph

        float maxWidth = 0.0f;
        float lineWidth = 0.0f;
        int lineCount = 1;

        for (int i = 0; text[i] != '\0';) {
            int byteCount = 0;
            int codepoint = GetCodepointNext(&text[i], &byteCount);
            i += byteCount;

            if (codepoint == '\n') {
                if (lineWidth > 0.0f) lineWidth -= trailingSpacing;
                if (lineWidth > maxWidth) maxWidth = lineWidth;
                lineWidth = 0.0f;
                lineCount++;
                continue;
            }
            lineWidth += glyphAdvance(textFont, glyphIndex(&textFont->font, codepoint), scale);
        }
        if (lineWidth > 0.0f) lineWidth -= trailingSpacing;
        if (lineWidth > maxWidth) maxWidth = lineWidth;

        float lineGap = textFont->lineSpacing * scale;
        Vector2 size = { maxWidth, (float)lineCount * fontSize + (float)(lineCount - 1) * lineGap };
        return size;
    }

    // the glyph's width in the font's own pixels, unscaled and without spacing
    static float rawAdvance(const Font* font, int index) {
        float advance = (float)font->glyphs[index].advanceX;
        if (advance == 0.0f) advance = font->recs[index].width;
        return advance;
    }

    // characters obfuscation never picks: control characters, space, and DEL
    static bool isUnprintable(int codepoint) {
        return codepoint <= 32 || (codepoint >= 127 && codepoint < 160);
    }

    // a well-mixed number from two inputs, so the same seed and character
    // always pick the same replacement
    static uint32_t hash(uint32_t a, uint32_t b) {
        uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
        h ^= h >> 16;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13;
        h *= 0xC2B2AE35u;
        h ^= h >> 16;
        return h;
    }

    // Whether glyph candidate can stand in for original when obfuscating: a
    // different, printable glyph of exactly that width. For fonts stored by
    // codepoint, candidate and original are codepoints, which are also their
    // glyph indices; otherwise they're glyph indices.
    static bool isObfuscationCandidate(
        const Font* font,
        int candidate,
        int original,
        float width,
        bool storedByCodepoint
    ) {
        if (candidate == original || candidate < 0 || candidate >= font->glyphCount) return false;
        int codepoint = storedByCodepoint ? candidate : font->glyphs[candidate].value;
        if (isUnprintable(codepoint)) return false;
        if (storedByCodepoint && font->glyphs[candidate].value != candidate) return false;
        return rawAdvance(font, candidate) == width;
    }

    // A random glyph of the same width as the one at index, for obfuscation.
    // The pool: ASCII characters pick from printable ASCII; other characters
    // from the 256 codepoints of their own block (for fonts not stored by
    // codepoint, from all the font's glyphs). Picks uniformly: count the
    // candidates, choose one at random, then walk to it. Returns index if
    // the pool has no candidates.
    static int obfuscatedGlyph(
        const Font* font,
        int index,
        int codepoint,
        uint32_t seed,
        int characterIndex
    ) {
        float width = rawAdvance(font, index);
        bool storedByCodepoint = codepoint >= 0 && codepoint < font->glyphCount && font->glyphs[codepoint].value == codepoint;

        int first = 0;
        int last = font->glyphCount - 1;
        int original = index;
        if (storedByCodepoint) {
            original = codepoint;
            if (codepoint < 128) {
                first = '!';
                last = '~';
            }
            else {
                first = codepoint & ~0xFF;
                last = first + 255;
            }
        }

        int count = 0;
        for (int candidate = first; candidate <= last; candidate++) {
            if (isObfuscationCandidate(font, candidate, original, width, storedByCodepoint)) count++;
        }
        if (count == 0) return index;

        int pick = (int)(hash(seed, (uint32_t)characterIndex) % (uint32_t)count);
        for (int candidate = first; candidate <= last; candidate++) {
            if (!isObfuscationCandidate(font, candidate, original, width, storedByCodepoint)) continue;
            if (pick == 0) return candidate;
            pick--;
        }
        return index;
    }

    static Color darkenColor(Color color) {
        Color result = { (unsigned char)(color.r / 4), (unsigned char)(color.g / 4), (unsigned char)(color.b / 4), color.a };
        return result;
    }

    static void textFontDraw(
        void* user,
        const char* text,
        Vector2 position,
        float fontSize,
        Color color,
        const TextGlyphStyle* glyphs
    ) {
        const TextFont* textFont = (const TextFont*)user;
        const Font* font = &textFont->font;
        float scale = fontSize / (float)font->baseSize;

        float x = position.x;
        float y = position.y;
        int characterIndex = 0; // counts characters for per-character styling, not line breaks

        for (int i = 0; text[i] != '\0';) {
            int byteCount = 0;
            int codepoint = GetCodepointNext(&text[i], &byteCount);
            i += byteCount;

            if (codepoint == '\n') {
                x = position.x;
                y += fontSize + textFont->lineSpacing * scale;
                continue;
            }

            int index = glyphIndex(font, codepoint);
            if (codepoint != ' ' && codepoint != '\t') {
                // per-character styling: this character's color, and a random
                // same-width glyph if it's obfuscated
                Color glyphColor = color;
                int drawnIndex = index;
                if (glyphs != NULL) {
                    if (glyphs->colors != NULL && characterIndex < glyphs->colorCount) {
                        glyphColor = glyphs->colors[characterIndex];
                        if (glyphs->darkenColors) glyphColor = darkenColor(glyphColor);
                    }
                    if (glyphs->obfuscated != NULL && characterIndex < glyphs->obfuscatedCount && glyphs->obfuscated[characterIndex]) {
                        drawnIndex = obfuscatedGlyph(font, index, codepoint, glyphs->obfuscationSeed, characterIndex);
                    }
                }

                // whole pixels, so pixel fonts don't lose or double a column
                drawGlyph(font, drawnIndex, Vector2{ roundf(x), roundf(y) }, scale, glyphColor);
            }
            // always the original's advance, so obfuscation can't move the layout
            x += glyphAdvance(textFont, index, scale);
            characterIndex++;
        }
    }

    static bool rectsEqual(Rectangle a, Rectangle b) {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }

    // scissor works in whole pixels, so grow the rect outward to cover any partial pixels
    static void beginScissor(Rectangle clip) {
        float left = floorf(clip.x);
        float top = floorf(clip.y);
        float right = ceilf(clip.x + clip.width);
        float bottom = ceilf(clip.y + clip.height);
        BeginScissorMode((int)left, (int)top, (int)(right - left), (int)(bottom - top));
    }

    static void addKeyboardEvent(Input* input, KeyboardEvent event) {
        // past the limit, events are dropped
        if (input->keyboardEventCount < O2_GUI_MAX_KEYBOARD_EVENTS) {
            input->keyboardEvents[input->keyboardEventCount] = event;
            input->keyboardEventCount++;
        }
    }

    // appends this frame's typing: characters first, then editing keys, since
    // raylib doesn't say how they interleave within one frame
    static void collectKeyboard(Input* input) {
        KeyboardEvent event;
        event.type = KeyboardEventType::Character;
        event.key = Key::Backspace;
        event.ctrl = false;

        // drain the whole queue even when full, so old characters don't linger in raylib
        int codepoint = GetCharPressed();
        while (codepoint != 0) {
            event.codepoint = codepoint;
            addKeyboardEvent(input, event);
            codepoint = GetCharPressed();
        }

        struct KeyMapping {
            int raylibKey;
            Key key;
        };
        static const KeyMapping KEYS[] = {
            { KEY_BACKSPACE, Key::Backspace },
            { KEY_DELETE, Key::Delete },
            { KEY_LEFT, Key::Left },
            { KEY_RIGHT, Key::Right },
            { KEY_HOME, Key::Home },
            { KEY_END, Key::End },
            { KEY_ENTER, Key::Enter },
            { KEY_ESCAPE, Key::Escape },
        };

        event.type = KeyboardEventType::Key;
        event.codepoint = 0;
        event.ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        for (int i = 0; i < (int)(sizeof(KEYS) / sizeof(KEYS[0])); i++) {
            // IsKeyPressedRepeat is the operating system's key repeat while held
            if (IsKeyPressed(KEYS[i].raylibKey) || IsKeyPressedRepeat(KEYS[i].raylibKey)) {
                event.key = KEYS[i].key;
                addKeyboardEvent(input, event);
            }
        }
    }

    Texture2D loadTexture(const char* path, int filter) {
        Texture2D texture = LoadTexture(path);
        SetTextureFilter(texture, filter);
        return texture;
    }

    void readInput(Input* input) {
        input->mousePosition = GetMousePosition();
        input->mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        input->mouseWheel = GetMouseWheelMove();
        input->keyboardEventCount = 0;
        collectKeyboard(input);
    }

    void accumulateInput(Input* input) {
        input->mousePosition = GetMousePosition();
        input->mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        // raylib resets the wheel every frame, so add it up until the UI reads it
        input->mouseWheel += GetMouseWheelMove();
        collectKeyboard(input);
    }

    void clearInput(Input* input) {
        input->mouseWheel = 0.0f;
        input->keyboardEventCount = 0;
    }

    TextFont defaultTextFont() {
        TextFont textFont;
        textFont.font = GetFontDefault();
        // DrawText uses fontSize / 10 as spacing, and the default font's base size is 10
        textFont.spacing = 1.0f;
        textFont.lineSpacing = 2.0f;
        return textFont;
    }

    void useFont(Context* ctx, const TextFont* font) {
        ctx->text.user = (void*)font;
        ctx->text.measure = textFontMeasure;
        ctx->text.draw = textFontDraw;
    }

    void render(const Context* ctx) {
        // scissor state is only changed when the clip changes between commands
        bool scissorOn = false;
        Rectangle currentClip = { 0, 0, 0, 0 };

        for (int layer = 0; layer < (int)Layer::Count; layer++) {
            for (int i = 0; i < ctx->commandCount; i++) {
                const DrawCommand* command = &ctx->commands[i];
                if ((int)command->layer != layer) continue;

                bool clipChanged = command->clipped != scissorOn
                    || (command->clipped && !rectsEqual(command->clip, currentClip));
                if (clipChanged) {
                    if (scissorOn) EndScissorMode();
                    if (command->clipped) beginScissor(command->clip);
                    scissorOn = command->clipped;
                    currentClip = command->clip;
                }

                switch (command->type) {
                case DrawCommandType::Rect:
                    DrawRectangleRec(command->rect, command->color);
                    break;
                case DrawCommandType::RectLines:
                    DrawRectangleLinesEx(command->rect, command->thickness, command->color);
                    break;
                case DrawCommandType::RectGradientH:
                    DrawRectangleGradientH(
                        (int)command->rect.x,
                        (int)command->rect.y,
                        (int)command->rect.width,
                        (int)command->rect.height,
                        command->color,
                        command->colorEnd
                    );
                    break;
                case DrawCommandType::Texture:
                    DrawTexturePro(
                        command->texture->texture,
                        command->texture->source,
                        command->rect,
                        Vector2{ 0, 0 },
                        0.0f,
                        command->color
                    );
                    break;
                case DrawCommandType::NineSlice: {
                    const TextureDraw* texture = command->texture;
                    NPatchInfo nineSlice;
                    nineSlice.source = texture->source;
                    nineSlice.left = texture->left;
                    nineSlice.top = texture->top;
                    nineSlice.right = texture->right;
                    nineSlice.bottom = texture->bottom;
                    nineSlice.layout = NPATCH_NINE_PATCH;
                    DrawTextureNPatch(texture->texture, nineSlice, command->rect, Vector2{ 0, 0 }, 0.0f, command->color);
                    break;
                }
                case DrawCommandType::Text: {
                    Vector2 position = { command->rect.x, command->rect.y };
                    ctx->text.draw(ctx->text.user, command->text, position, command->fontSize, command->color, command->glyphs);
                    break;
                }
                }
            }
        }

        if (scissorOn) EndScissorMode();
    }

}
}
}
