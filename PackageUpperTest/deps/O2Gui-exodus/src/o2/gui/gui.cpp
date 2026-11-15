#include <o2/gui/gui.h>
#include <raymath.h>
#include <assert.h>
#include <math.h>
#include <string.h>

namespace o2 {
namespace gui {

    // Widget ids
    //
    // A widget's id is a hash of its label, so the label doubles as the id and
    // callers never have to invent or keep track of id numbers. The hash is
    // seeded with the id of the enclosing scope (pushId), so two "Delete"
    // buttons in different rows of a list get different ids.
    //
    // The hash is 32-bit FNV-1a, a standard tiny string hash: start from the
    // offset basis, then for each byte xor it in and multiply by the prime.
    // The two constants are the values the FNV spec defines for 32 bits.
    // raylib has ComputeCRC32, but it can't be seeded, which scoping needs.
    static const Id FNV_OFFSET_BASIS = 2166136261u;
    static const Id FNV_PRIME = 16777619u;

    // Moves current toward target by at most step, without overshooting.
    static float approach(float current, float target, float step) {
        return current + Clamp(target - current, -step, step);
    }

    static Id hashBytes(Id seed, const void* data, size_t size) {
        const uint8_t* bytes = (const uint8_t*)data;
        Id hash = seed;
        for (size_t i = 0; i < size; i++) {
            hash ^= bytes[i];
            hash *= FNV_PRIME;
        }
        // 0 is reserved for "no widget"
        if (hash == 0) hash = 1;
        return hash;
    }

    static Id currentSeed(Context* ctx) {
        if (ctx->idStackCount == 0) return FNV_OFFSET_BASIS;
        return ctx->idStack[ctx->idStackCount - 1];
    }

    static DrawCommand* pushCommand(Context* ctx, DrawCommandType type) {
        assert(ctx->commandCount < ctx->commandCapacity);

        DrawCommand* command = &ctx->commands[ctx->commandCount];
        ctx->commandCount++;

        memset(command, 0, sizeof(DrawCommand));
        command->type = type;
        command->layer = ctx->layer;
        if (ctx->clipStackCount > 0) {
            command->clipped = true;
            command->clip = ctx->clipStack[ctx->clipStackCount - 1];
        }
        return command;
    }

    static const char* copyString(Context* ctx, const char* text) {
        size_t size = strlen(text) + 1;
        char* copy = (char*)utils::arena::push(&ctx->frameArena, size);
        assert(copy != NULL); // frame memory ran out
        if (copy == NULL) return "";
        memcpy(copy, text, size);
        return copy;
    }

    Style defaultStyle() {
        Style style;
        memset(&style, 0, sizeof(Style)); // text effects start disabled
        style.text = Color{ 230, 230, 235, 255 };
        style.fontSize = 20.0f;

        style.button.background = stateSkins(skinColor(Color{ 60, 60, 72, 255 }), skinColor(Color{ 85, 85, 102, 255 }), skinColor(Color{ 40, 40, 50, 255 }));
        style.button.border = Color{ 20, 20, 26, 255 };
        style.button.text = Color{ 230, 230, 235, 255 };
        style.button.borderThickness = 2.0f;
        style.button.animSpeed = 10.0f;

        style.panel.background = skinColor(Color{ 32, 32, 40, 255 });
        style.panel.border = Color{ 20, 20, 26, 255 };
        style.panel.borderThickness = 2.0f;
        style.panel.scrollbarWidth = 12.0f;
        style.panel.minThumbHeight = 24.0f;
        style.panel.track = skinColor(Color{ 26, 26, 32, 255 });
        style.panel.thumb = stateSkins(skinColor(Color{ 70, 70, 84, 255 }), skinColor(Color{ 95, 95, 112, 255 }), skinColor(Color{ 120, 120, 140, 255 }));
        style.panel.thumbAnimSpeed = 10.0f;
        style.panel.wheelStep = 60.0f;
        style.panel.scrollSpeed = 15.0f;

        style.checkbox.box = stateSkins(skinColor(Color{ 60, 60, 72, 255 }), skinColor(Color{ 85, 85, 102, 255 }), skinColor(Color{ 40, 40, 50, 255 }));
        style.checkbox.border = Color{ 20, 20, 26, 255 };
        style.checkbox.check = skinColor(Color{ 230, 230, 235, 255 });
        style.checkbox.text = Color{ 230, 230, 235, 255 };
        style.checkbox.borderThickness = 2.0f;
        style.checkbox.checkInset = 5.0f;
        style.checkbox.labelGap = 8.0f;
        style.checkbox.animSpeed = 10.0f;

        style.slider.track = skinColor(Color{ 26, 26, 32, 255 });
        style.slider.fill = skinColor(Color{ 85, 85, 102, 255 });
        style.slider.thumb = stateSkins(skinColor(Color{ 120, 120, 140, 255 }), skinColor(Color{ 150, 150, 170, 255 }), skinColor(Color{ 180, 180, 200, 255 }));
        style.slider.trackHeight = 6.0f;
        style.slider.thumbWidth = 12.0f;
        style.slider.animSpeed = 10.0f;

        style.dropdown.box = stateSkins(skinColor(Color{ 60, 60, 72, 255 }), skinColor(Color{ 85, 85, 102, 255 }), skinColor(Color{ 40, 40, 50, 255 }));
        style.dropdown.border = Color{ 20, 20, 26, 255 };
        style.dropdown.text = Color{ 230, 230, 235, 255 };
        style.dropdown.arrow = skinNone(); // no arrow until it's given a texture
        style.dropdown.listBackground = skinColor(Color{ 32, 32, 40, 255 });
        style.dropdown.itemHover = skinColor(Color{ 60, 60, 72, 255 });
        style.dropdown.itemSelected = skinColor(Color{ 85, 85, 102, 255 });
        style.dropdown.borderThickness = 2.0f;
        style.dropdown.padding = 8.0f;
        style.dropdown.arrowSize = 10.0f;
        style.dropdown.maxVisibleItems = 8;
        style.dropdown.animSpeed = 10.0f;

        style.textInput.background = skinColor(Color{ 26, 26, 32, 255 });
        style.textInput.backgroundHover = skinColor(Color{ 32, 32, 40, 255 });
        style.textInput.backgroundFocused = skinColor(Color{ 20, 20, 26, 255 });
        style.textInput.border = Color{ 60, 60, 72, 255 };
        style.textInput.borderFocused = Color{ 120, 120, 140, 255 };
        style.textInput.text = Color{ 230, 230, 235, 255 };
        style.textInput.placeholder = Color{ 120, 120, 130, 255 };
        style.textInput.cursor = Color{ 230, 230, 235, 255 };
        style.textInput.borderThickness = 2.0f;
        style.textInput.padding = 8.0f;
        style.textInput.cursorWidth = 2.0f;
        style.textInput.cursorBlinkTime = 0.5f;
        style.textInput.animSpeed = 10.0f;
        return style;
    }

    void init(Context* ctx, void* frameMemory, size_t frameMemorySize, int maxCommands) {
        memset(ctx, 0, sizeof(Context));
        ctx->frameArena = utils::arena::make(frameMemory, frameMemorySize);
        ctx->commandCapacity = maxCommands;
        ctx->style = defaultStyle();
    }

    void beginFrame(Context* ctx, const Input* input, float dt) {
        ctx->frame++;

        // presses and releases come from comparing the button with last update's
        bool wasDown = ctx->input.mouseDown;
        ctx->mousePressed = input->mouseDown && !wasDown;
        ctx->mouseReleased = !input->mouseDown && wasDown;

        ctx->input = *input;
        ctx->dt = dt;
        ctx->time += dt;

        utils::arena::reset(&ctx->frameArena);
        ctx->commands = (DrawCommand*)utils::arena::pushArrayAligned(&ctx->frameArena, sizeof(DrawCommand), ctx->commandCapacity, alignof(DrawCommand));
        assert(ctx->commands != NULL); // frame memory is too small for maxCommands
        ctx->commandCount = 0;

        ctx->idStackCount = 0;
        ctx->clipStackCount = 0;
        ctx->nextHotId = 0;
        ctx->nextScrollHotId = 0;
        ctx->popupCalled = false;
        ctx->focusCalled = false;
        ctx->textColorCount = 0;
        ctx->textShadowCount = 0;
        ctx->underlineCount = 0;
        ctx->strikethroughCount = 0;
        ctx->characterColorCount = 0;
        ctx->obfuscationCount = 0;
        ctx->nextBlockingActive = false;
        ctx->layer = Layer::Base;
    }

    void endFrame(Context* ctx) {
        assert(ctx->idStackCount == 0);   // every pushId needs a popId
        assert(ctx->clipStackCount == 0); // every pushClip needs a popClip
        // every text style push needs its pop
        assert(ctx->textColorCount == 0);
        assert(ctx->textShadowCount == 0);
        assert(ctx->underlineCount == 0);
        assert(ctx->strikethroughCount == 0);
        assert(ctx->characterColorCount == 0);
        assert(ctx->obfuscationCount == 0);

        ctx->hotId = ctx->nextHotId;
        ctx->scrollHotId = ctx->nextScrollHotId;

        // likewise, a focused widget that wasn't called loses focus, so it
        // can't swallow typing while hidden
        if (ctx->focusId != 0 && !ctx->focusCalled) {
            ctx->focusId = 0;
        }
        ctx->previousFocusId = ctx->focusId;

        // a popup whose widget wasn't called (its screen was hidden, say) closes,
        // so it can't keep blocking the mouse while invisible
        if (ctx->openPopupId != 0 && !ctx->popupCalled) {
            ctx->openPopupId = 0;
            ctx->nextBlockingActive = false;
        }
        ctx->blockingActive = ctx->nextBlockingActive;
        ctx->blockingRect = ctx->nextBlockingRect;

        // if the active widget wasn't called this frame, it can never release itself
        if (ctx->mouseReleased) {
            ctx->activeId = 0;
        }
    }

    Id getId(Context* ctx, const char* label) {
        return hashBytes(currentSeed(ctx), label, strlen(label));
    }

    void pushId(Context* ctx, const char* label) {
        assert(ctx->idStackCount < O2_GUI_MAX_ID_STACK);
        ctx->idStack[ctx->idStackCount] = getId(ctx, label);
        ctx->idStackCount++;
    }

    void pushId(Context* ctx, int index) {
        assert(ctx->idStackCount < O2_GUI_MAX_ID_STACK);
        ctx->idStack[ctx->idStackCount] = hashBytes(currentSeed(ctx), &index, sizeof(index));
        ctx->idStackCount++;
    }

    void popId(Context* ctx) {
        assert(ctx->idStackCount > 0);
        ctx->idStackCount--;
    }

    WidgetState* getWidgetState(Context* ctx, Id id) {
        WidgetState* freeSlot = NULL;

        for (int i = 0; i < O2_GUI_MAX_WIDGET_STATES; i++) {
            WidgetState* state = &ctx->widgets[i];
            if (state->id == id) {
                state->lastFrame = ctx->frame;
                return state;
            }
            // a slot is free if it was never used, or its widget wasn't called last frame
            bool unused = state->id == 0 || state->lastFrame + 1 < ctx->frame;
            if (freeSlot == NULL && unused) {
                freeSlot = state;
            }
        }

        if (freeSlot == NULL) {
            assert(!"o2::gui: out of widget states, raise O2_GUI_MAX_WIDGET_STATES");
            freeSlot = &ctx->overflowWidget;
        }

        memset(freeSlot, 0, sizeof(WidgetState));
        freeSlot->id = id;
        freeSlot->lastFrame = ctx->frame;
        return freeSlot;
    }

    void setFocus(Context* ctx, Id id) {
        ctx->focusId = id;
    }

    void setScreen(Context* ctx, Rectangle screen) {
        ctx->screen = screen;
    }

    void setLayer(Context* ctx, Layer layer) {
        assert(layer != Layer::Count);
        ctx->layer = layer;
    }

    void pushClip(Context* ctx, Rectangle rect) {
        assert(ctx->clipStackCount < O2_GUI_MAX_CLIP_STACK);

        if (ctx->clipStackCount > 0) {
            rect = GetCollisionRec(rect, ctx->clipStack[ctx->clipStackCount - 1]);
        }
        ctx->clipStack[ctx->clipStackCount] = rect;
        ctx->clipStackCount++;
    }

    void pushRootClip(Context* ctx, Rectangle rect) {
        assert(ctx->clipStackCount < O2_GUI_MAX_CLIP_STACK);

        // unlike pushClip, not intersected with the current clip
        ctx->clipStack[ctx->clipStackCount] = rect;
        ctx->clipStackCount++;
    }

    void popClip(Context* ctx) {
        assert(ctx->clipStackCount > 0);
        ctx->clipStackCount--;
    }

    bool isMouseOver(Context* ctx, Rectangle rect) {
        Vector2 mouse = ctx->input.mousePosition;
        if (!CheckCollisionPointRec(mouse, rect)) return false;

        // an open popup covers widgets on the base layer; its own contents are on the overlay
        if (ctx->layer == Layer::Base && ctx->blockingActive && CheckCollisionPointRec(mouse, ctx->blockingRect)) {
            return false;
        }

        if (ctx->clipStackCount == 0) return true;
        return CheckCollisionPointRec(mouse, ctx->clipStack[ctx->clipStackCount - 1]);
    }

    void drawRect(Context* ctx, Rectangle rect, Color color) {
        DrawCommand* command = pushCommand(ctx, DrawCommandType::Rect);
        command->rect = rect;
        command->color = color;
    }

    void drawRectLines(Context* ctx, Rectangle rect, float thickness, Color color) {
        DrawCommand* command = pushCommand(ctx, DrawCommandType::RectLines);
        command->rect = rect;
        command->thickness = thickness;
        command->color = color;
    }

    static void drawTextWithGlyphs(
        Context* ctx,
        const char* text,
        Vector2 position,
        float fontSize,
        Color color,
        const TextGlyphStyle* glyphs
    ) {
        DrawCommand* command = pushCommand(ctx, DrawCommandType::Text);
        command->glyphs = glyphs;
        // snap to whole pixels: font textures are pixel art, so a glyph drawn
        // at a half pixel loses or doubles a column of pixels, e.g. text
        // centered in an odd-width button
        command->rect.x = roundf(position.x);
        command->rect.y = roundf(position.y);
        command->text = copyString(ctx, text);
        command->fontSize = fontSize;
        command->color = color;
    }

    void drawText(Context* ctx, const char* text, Vector2 position, float fontSize, Color color) {
        drawTextWithGlyphs(ctx, text, position, fontSize, color, NULL);
    }

    void drawRectGradientH(Context* ctx, Rectangle rect, Color left, Color right) {
        DrawCommand* command = pushCommand(ctx, DrawCommandType::RectGradientH);
        command->rect = rect;
        command->color = left;
        command->colorEnd = right;
    }

    static void drawTextureCommand(
        Context* ctx,
        DrawCommandType type,
        Rectangle rect,
        TextureRegion region,
        NineSlice nineSlice,
        Color tint
    ) {
        TextureDraw* texture = (TextureDraw*)utils::arena::pushAligned(&ctx->frameArena, sizeof(TextureDraw), alignof(TextureDraw));
        assert(texture != NULL); // frame memory ran out
        if (texture == NULL) return;
        texture->texture = region.texture;
        texture->source = region.source;
        texture->left = nineSlice.left;
        texture->top = nineSlice.top;
        texture->right = nineSlice.right;
        texture->bottom = nineSlice.bottom;

        DrawCommand* command = pushCommand(ctx, type);
        command->rect = rect;
        command->color = tint;
        command->texture = texture;
    }

    void drawTexture(Context* ctx, Rectangle rect, TextureRegion region, Color tint) {
        NineSlice none = { 0, 0, 0, 0 };
        drawTextureCommand(ctx, DrawCommandType::Texture, rect, region, none, tint);
    }

    void drawTextureNineSlice(
        Context* ctx,
        Rectangle rect,
        TextureRegion region,
        NineSlice nineSlice,
        Color tint
    ) {
        drawTextureCommand(ctx, DrawCommandType::NineSlice, rect, region, nineSlice, tint);
    }

    static bool hasNineSlice(NineSlice nineSlice) {
        return nineSlice.left != 0 || nineSlice.top != 0 || nineSlice.right != 0 || nineSlice.bottom != 0;
    }

    // the frame an animation shows now
    static int animationFrame(Context* ctx, const TextureAnimation* animation, double animationStart) {
        if (animation->frameCount <= 0 || animation->framesPerSecond <= 0.0f) return 0;

        bool looping = animation->timing == AnimationTiming::Looping;
        double elapsed = looping ? ctx->time : ctx->time - animationStart;
        if (elapsed < 0.0) elapsed = 0.0;

        int frame = (int)floor(elapsed * animation->framesPerSecond);
        if (looping || animation->loop) {
            return frame % animation->frameCount;
        }
        // played once: hold the last frame
        return (frame < animation->frameCount) ? frame : animation->frameCount - 1;
    }

    void drawSkin(Context* ctx, Rectangle rect, const Skin* skin, double animationStart) {
        TextureRegion region;
        switch (skin->kind) {
        case SkinKind::None:
            return;
        case SkinKind::Color:
            drawRect(ctx, rect, skin->color);
            return;
        case SkinKind::Texture:
            region = skin->region;
            break;
        case SkinKind::Animation:
            if (skin->animation.frameCount <= 0) return;
            region = skin->animation.frames[animationFrame(ctx, &skin->animation, animationStart)];
            break;
        default:
            return;
        }

        if (hasNineSlice(skin->nineSlice)) {
            drawTextureNineSlice(ctx, rect, region, skin->nineSlice, skin->color);
        }
        else {
            drawTexture(ctx, rect, region, skin->color);
        }
    }

    // Records which state a widget's skins show, restarting triggered
    // animations when it changes. A brand new widget state starts its
    // timing from now.
    static void updateSkinState(Context* ctx, WidgetState* state, int skinState) {
        if (state->skinStateTime == 0.0) state->skinStateTime = ctx->time;
        if (state->skinState != skinState) {
            state->skinState = skinState;
            state->skinStateTime = ctx->time;
        }
    }

    void drawStateSkins(
        Context* ctx,
        Rectangle rect,
        const StateSkins* skins,
        WidgetState* state,
        bool hot,
        bool held
    ) {
        int current = held ? 2 : (hot ? 1 : 0);
        updateSkinState(ctx, state, current);

        // three colors fade into each other; anything else switches
        bool allColors = skins->normal.kind == SkinKind::Color
            && skins->hover.kind == SkinKind::Color
            && skins->pressed.kind == SkinKind::Color;
        if (allColors) {
            Color fill = ColorLerp(skins->normal.color, skins->hover.color, state->hoverT);
            fill = ColorLerp(fill, skins->pressed.color, state->pressT);
            drawRect(ctx, rect, fill);
            return;
        }

        const Skin* skin = &skins->normal;
        if (current == 1) skin = &skins->hover;
        if (current == 2) skin = &skins->pressed;
        drawSkin(ctx, rect, skin, state->skinStateTime);
    }

    // skin with its fill or tint made more transparent, for fading parts in and out
    static Skin fadeSkin(Skin skin, float t) {
        skin.color.a = (unsigned char)((float)skin.color.a * t);
        return skin;
    }

    void image(Context* ctx, Rectangle rect, const Skin* skin) {
        // no state, so triggered animations count from the start of the program
        drawSkin(ctx, rect, skin, 0.0);
    }

    TextureRegion makeTextureRegion(Texture2D texture) {
        Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
        return makeTextureRegion(texture, source);
    }

    TextureRegion makeTextureRegion(Texture2D texture, Rectangle source) {
        TextureRegion region;
        region.texture = texture;
        region.source = source;
        return region;
    }

    void fillFrameStrip(TextureRegion* frames, int count, Texture2D texture, Rectangle first) {
        fillFrameStrip(frames, count, texture, first, FrameOrder::Horizontal, count, 0.0f);
    }

    void fillFrameStrip(
        TextureRegion* frames,
        int count,
        Texture2D texture,
        Rectangle first,
        FrameOrder order,
        float spacing
    ) {
        fillFrameStrip(frames, count, texture, first, order, count, spacing);
    }

    void fillFrameStrip(
        TextureRegion* frames,
        int count,
        Texture2D texture,
        Rectangle first,
        FrameOrder order,
        int framesPerLine,
        float spacing
    ) {
        if (framesPerLine <= 0) framesPerLine = (count > 0) ? count : 1;
        float stepX = first.width + spacing;
        float stepY = first.height + spacing;

        for (int i = 0; i < count; i++) {
            int along = i % framesPerLine; // position within the row or column
            int line = i / framesPerLine;  // which row or column
            Rectangle source = first;
            if (order == FrameOrder::Horizontal) {
                source.x = first.x + (float)along * stepX;
                source.y = first.y + (float)line * stepY;
            }
            else {
                source.x = first.x + (float)line * stepX;
                source.y = first.y + (float)along * stepY;
            }
            frames[i] = makeTextureRegion(texture, source);
        }
    }

    Skin skinNone() {
        Skin skin;
        memset(&skin, 0, sizeof(Skin));
        skin.kind = SkinKind::None;
        return skin;
    }

    Skin skinColor(Color color) {
        Skin skin = skinNone();
        skin.kind = SkinKind::Color;
        skin.color = color;
        return skin;
    }

    Skin skinTexture(TextureRegion region) {
        Skin skin = skinNone();
        skin.kind = SkinKind::Texture;
        skin.color = Color{ 255, 255, 255, 255 };
        skin.region = region;
        return skin;
    }

    Skin skinNineSlice(TextureRegion region, NineSlice nineSlice) {
        Skin skin = skinTexture(region);
        skin.nineSlice = nineSlice;
        return skin;
    }

    Skin skinAnimation(TextureAnimation animation) {
        Skin skin = skinNone();
        skin.kind = SkinKind::Animation;
        skin.color = Color{ 255, 255, 255, 255 };
        skin.animation = animation;
        return skin;
    }

    StateSkins stateSkins(Skin normal, Skin hover, Skin pressed) {
        StateSkins skins;
        skins.normal = normal;
        skins.hover = hover;
        skins.pressed = pressed;
        return skins;
    }

    Vector2 measureText(Context* ctx, const char* text, float fontSize) {
        assert(ctx->text.measure != NULL); // set TextFuncs, e.g. with raylib::useFont
        return ctx->text.measure(ctx->text.user, text, fontSize);
    }

    void setAllTextColors(Style* style, Color color) {
        style->text = color;
        style->button.text = color;
        style->checkbox.text = color;
        style->dropdown.text = color;
        style->textInput.text = color;
    }

    TextShadow makeTextShadow(Color color, Vector2 offset) {
        TextShadow shadow;
        shadow.enabled = true;
        shadow.color = color;
        shadow.colorFromText = false;
        shadow.offset = offset;
        return shadow;
    }

    TextShadow makeTextShadowFromText(Vector2 offset) {
        TextShadow shadow = makeTextShadow(Color{ 0, 0, 0, 0 }, offset);
        shadow.colorFromText = true;
        return shadow;
    }

    TextLine makeTextLine(Color color, float thickness, float position) {
        TextLine line;
        memset(&line, 0, sizeof(TextLine));
        line.enabled = true;
        line.color = color;
        line.colorFromText = false;
        line.thickness = thickness;
        line.position = position;
        return line;
    }

    TextLine makeTextLineGradient(Color left, Color right, float thickness, float position) {
        TextLine line = makeTextLine(left, thickness, position);
        line.gradient = true;
        line.colorEnd = right;
        return line;
    }

    TextLine makeTextLineFromText(float thickness, float position) {
        TextLine line = makeTextLine(Color{ 0, 0, 0, 0 }, thickness, position);
        line.colorFromText = true;
        return line;
    }

    // Pushes past the limit and pops with nothing pushed assert in Debug and
    // are ignored in Release, so a mistake can't write outside the stacks.
    void pushTextColor(Context* ctx, Color color) {
        assert(ctx->textColorCount < O2_GUI_MAX_TEXT_STYLE_STACK);
        if (ctx->textColorCount >= O2_GUI_MAX_TEXT_STYLE_STACK) return;
        ctx->textColorStack[ctx->textColorCount] = color;
        ctx->textColorCount++;
    }

    void popTextColor(Context* ctx) {
        assert(ctx->textColorCount > 0);
        if (ctx->textColorCount > 0) ctx->textColorCount--;
    }

    void pushTextShadow(Context* ctx, TextShadow shadow) {
        assert(ctx->textShadowCount < O2_GUI_MAX_TEXT_STYLE_STACK);
        if (ctx->textShadowCount >= O2_GUI_MAX_TEXT_STYLE_STACK) return;
        ctx->textShadowStack[ctx->textShadowCount] = shadow;
        ctx->textShadowCount++;
    }

    void popTextShadow(Context* ctx) {
        assert(ctx->textShadowCount > 0);
        if (ctx->textShadowCount > 0) ctx->textShadowCount--;
    }

    void pushTextUnderline(Context* ctx, TextLine underline) {
        assert(ctx->underlineCount < O2_GUI_MAX_TEXT_STYLE_STACK);
        if (ctx->underlineCount >= O2_GUI_MAX_TEXT_STYLE_STACK) return;
        ctx->underlineStack[ctx->underlineCount] = underline;
        ctx->underlineCount++;
    }

    void popTextUnderline(Context* ctx) {
        assert(ctx->underlineCount > 0);
        if (ctx->underlineCount > 0) ctx->underlineCount--;
    }

    void pushTextStrikethrough(Context* ctx, TextLine strikethrough) {
        assert(ctx->strikethroughCount < O2_GUI_MAX_TEXT_STYLE_STACK);
        if (ctx->strikethroughCount >= O2_GUI_MAX_TEXT_STYLE_STACK) return;
        ctx->strikethroughStack[ctx->strikethroughCount] = strikethrough;
        ctx->strikethroughCount++;
    }

    void popTextStrikethrough(Context* ctx) {
        assert(ctx->strikethroughCount > 0);
        if (ctx->strikethroughCount > 0) ctx->strikethroughCount--;
    }

    // The arrays are copied into the frame arena: render reads them after
    // the caller's array may be gone. Popping leaves the copy there until the
    // next beginFrame resets the arena, since drawn commands point at it.
    void pushTextCharacterColors(Context* ctx, const Color* colors, int count) {
        assert(ctx->characterColorCount < O2_GUI_MAX_TEXT_STYLE_STACK);
        if (ctx->characterColorCount >= O2_GUI_MAX_TEXT_STYLE_STACK) return;

        TextCharacterColors entry;
        entry.colors = NULL;
        entry.count = 0;
        if (count > 0) {
            Color* copy = (Color*)utils::arena::pushArrayAligned(&ctx->frameArena, sizeof(Color), (size_t)count, alignof(Color));
            assert(copy != NULL); // frame memory ran out
            // still push an empty entry, so the caller's pop matches
            if (copy != NULL) {
                memcpy(copy, colors, sizeof(Color) * (size_t)count);
                entry.colors = copy;
                entry.count = count;
            }
        }
        ctx->characterColorStack[ctx->characterColorCount] = entry;
        ctx->characterColorCount++;
    }

    void popTextCharacterColors(Context* ctx) {
        assert(ctx->characterColorCount > 0);
        if (ctx->characterColorCount > 0) ctx->characterColorCount--;
    }

    void pushTextObfuscation(Context* ctx, const bool* obfuscated, int count, float changesPerSecond) {
        assert(ctx->obfuscationCount < O2_GUI_MAX_TEXT_STYLE_STACK);
        if (ctx->obfuscationCount >= O2_GUI_MAX_TEXT_STYLE_STACK) return;

        TextObfuscation entry;
        entry.obfuscated = NULL;
        entry.count = 0;
        entry.changesPerSecond = changesPerSecond;
        if (count > 0) {
            bool* copy = (bool*)utils::arena::pushArray(&ctx->frameArena, sizeof(bool), (size_t)count);
            assert(copy != NULL); // frame memory ran out
            // still push an empty entry, so the caller's pop matches
            if (copy != NULL) {
                memcpy(copy, obfuscated, sizeof(bool) * (size_t)count);
                entry.obfuscated = copy;
                entry.count = count;
            }
        }
        ctx->obfuscationStack[ctx->obfuscationCount] = entry;
        ctx->obfuscationCount++;
    }

    void popTextObfuscation(Context* ctx) {
        assert(ctx->obfuscationCount > 0);
        if (ctx->obfuscationCount > 0) ctx->obfuscationCount--;
    }

    int countCharacters(const char* text) {
        // every byte that starts a UTF-8 character, except line breaks
        int count = 0;
        for (const char* c = text; *c != '\0'; c++) {
            bool continuation = ((unsigned char)*c & 0xC0) == 0x80;
            if (!continuation && *c != '\n') count++;
        }
        return count;
    }

    void fillGradient(Color* colors, int count, const GradientStop* stops, int stopCount) {
        assert(stopCount <= MAX_GRADIENT_STOPS);
        if (stopCount > MAX_GRADIENT_STOPS) stopCount = MAX_GRADIENT_STOPS;
        if (stopCount <= 0) return;

        int last = stopCount - 1;
        for (int i = 0; i < count; i++) {
            if (i <= stops[0].character) {
                colors[i] = stops[0].color;
                continue;
            }
            if (i >= stops[last].character) {
                colors[i] = stops[last].color;
                continue;
            }

            // the two stops around i, and how far i is between them
            int k = 0;
            while (k + 1 < last && i >= stops[k + 1].character) k++;
            int from = stops[k].character;
            int to = stops[k + 1].character;
            float t = (to > from) ? (float)(i - from) / (float)(to - from) : 1.0f;
            colors[i] = ColorLerp(stops[k].color, stops[k + 1].color, t);
        }
    }

    void fillRainbow(
        Color* colors, 
        int count, 
        float charactersPerCycle, 
        float cyclesPerSecond, 
        float stepsPerSecond, 
        double time, 
        float saturation, 
        float brightness
    ) {
        if (charactersPerCycle <= 0.0f) charactersPerCycle = (float)(count > 0 ? count : 1);

        // how far the colors have travelled, in cycles; held between steps
        double steppedTime = time;
        if (stepsPerSecond > 0.0f) {
            steppedTime = floor(time * stepsPerSecond) / stepsPerSecond;
        }
        double phase = (cyclesPerSecond != 0.0f) ? steppedTime * cyclesPerSecond : 0.0;

        for (int i = 0; i < count; i++) {
            // subtracting the phase moves the colors toward the end of the text
            double cycle = (double)i / charactersPerCycle - phase;
            cycle -= floor(cycle);
            colors[i] = ColorFromHSV((float)(cycle * 360.0), saturation, brightness);
        }
    }

    void fillRange(Color* colors, int count, int first, int length, Color value) {
        int start = (first < 0) ? 0 : first;
        int end = first + length;
        if (end > count) end = count;
        for (int i = start; i < end; i++) colors[i] = value;
    }

    void fillRange(bool* flags, int count, int first, int length, bool value) {
        int start = (first < 0) ? 0 : first;
        int end = first + length;
        if (end > count) end = count;
        for (int i = start; i < end; i++) flags[i] = value;
    }

    // the color at 25% brightness, keeping its transparency
    static Color darken(Color color) {
        Color result = { (unsigned char)(color.r / 4), (unsigned char)(color.g / 4), (unsigned char)(color.b / 4), color.a };
        return result;
    }

    // draws one kind of line (underline or strikethrough) under every line of text
    // draws one kind of line (underline or strikethrough) under every line of
    // text, solid or fading from color to colorEnd
    static void drawTextLines(
        Context* ctx,
        char* text,
        Vector2 position,
        float fontSize,
        TextLine line,
        Color color,
        bool gradient,
        Color colorEnd,
        Vector2 offset
    ) {
        int start = 0;
        for (;;) {
            int end = start;
            while (text[end] != '\0' && text[end] != '\n') end++;

            // the line's width, and its top from the height of the text up to it
            char saved = text[end];
            text[end] = '\0';
            float width = measureText(ctx, text + start, fontSize).x;
            float lineTop = measureText(ctx, text, fontSize).y - fontSize;
            text[end] = saved;

            if (width > 0.0f) {
                Rectangle rect;
                rect.x = roundf(position.x + offset.x);
                rect.y = roundf(position.y + offset.y + lineTop + line.position * fontSize);
                rect.width = width;
                rect.height = line.thickness;
                if (gradient) {
                    drawRectGradientH(ctx, rect, color, colorEnd);
                }
                else {
                    drawRect(ctx, rect, color);
                }
            }

            if (saved == '\0') break;
            start = end + 1;
        }
    }

    void drawStyledText(Context* ctx, const char* text, Vector2 position, float fontSize, Color color) {
        // the newest push of each kind, or the Style's setting when nothing is pushed
        Color textColor = (ctx->textColorCount > 0) ? ctx->textColorStack[ctx->textColorCount - 1] : color;
        TextShadow shadow = (ctx->textShadowCount > 0) ? ctx->textShadowStack[ctx->textShadowCount - 1] : ctx->style.textShadow;
        TextLine underline = (ctx->underlineCount > 0) ? ctx->underlineStack[ctx->underlineCount - 1] : ctx->style.underline;
        TextLine strikethrough = (ctx->strikethroughCount > 0) ? ctx->strikethroughStack[ctx->strikethroughCount - 1] : ctx->style.strikethrough;

        // per-character styling, if any is pushed, for the font code to draw
        TextGlyphStyle* glyphs = NULL;
        TextGlyphStyle* shadowGlyphs = NULL;
        Color firstColor = textColor; // what lines take from the text
        if (ctx->characterColorCount > 0 || ctx->obfuscationCount > 0) {
            glyphs = (TextGlyphStyle*)utils::arena::pushAligned(&ctx->frameArena, sizeof(TextGlyphStyle), alignof(TextGlyphStyle));
            assert(glyphs != NULL); // frame memory ran out
        }
        // without room for the glyph style, the text is drawn plain
        if (glyphs != NULL) {
            memset(glyphs, 0, sizeof(TextGlyphStyle));

            if (ctx->characterColorCount > 0) {
                TextCharacterColors* colors = &ctx->characterColorStack[ctx->characterColorCount - 1];
                glyphs->colors = colors->colors;
                glyphs->colorCount = colors->count;
                if (colors->count > 0) firstColor = colors->colors[0];
            }
            if (ctx->obfuscationCount > 0) {
                TextObfuscation* obfuscation = &ctx->obfuscationStack[ctx->obfuscationCount - 1];
                glyphs->obfuscated = obfuscation->obfuscated;
                glyphs->obfuscatedCount = obfuscation->count;
                // the seed only changes changesPerSecond times a second, so every
                // redraw in between shows the same characters
                if (obfuscation->changesPerSecond > 0.0f) {
                    glyphs->obfuscationSeed = (uint32_t)floor(ctx->time * obfuscation->changesPerSecond);
                }
            }

            // the shadow scrambles the same way; it follows the colors only if
            // it takes its color from the text
            if (shadow.enabled) {
                shadowGlyphs = (TextGlyphStyle*)utils::arena::pushAligned(&ctx->frameArena, sizeof(TextGlyphStyle), alignof(TextGlyphStyle));
                assert(shadowGlyphs != NULL); // frame memory ran out
                // without room, the shadow is drawn plain
                if (shadowGlyphs != NULL) {
                    *shadowGlyphs = *glyphs;
                    if (shadow.colorFromText) {
                        shadowGlyphs->darkenColors = true;
                    }
                    else {
                        shadowGlyphs->colors = NULL;
                        shadowGlyphs->colorCount = 0;
                    }
                }
            }
        }

        Color underlineColor = underline.colorFromText ? firstColor : underline.color;
        bool underlineGradient = !underline.colorFromText && underline.gradient;
        Color strikethroughColor = strikethrough.colorFromText ? firstColor : strikethrough.color;
        bool strikethroughGradient = !strikethrough.colorFromText && strikethrough.gradient;
        Vector2 noOffset = { 0.0f, 0.0f };

        // lines need to measure parts of the text, so work on a copy that can be cut up
        char* copy = NULL;
        if (underline.enabled || strikethrough.enabled) {
            copy = (char*)copyString(ctx, text);
        }

        // back to front: shadow text, underline shadow, text, underline, strikethrough
        if (shadow.enabled) {
            Color shadowColor = shadow.colorFromText ? darken(textColor) : shadow.color;
            Vector2 shadowPosition = { position.x + shadow.offset.x, position.y + shadow.offset.y };
            drawTextWithGlyphs(ctx, text, shadowPosition, fontSize, shadowColor, shadowGlyphs);

            // the underline's shadow is one color, even under a gradient underline
            if (underline.enabled) {
                Color underlineShadow = shadow.colorFromText ? darken(underlineColor) : shadow.color;
                drawTextLines(ctx, copy, position, fontSize, underline, underlineShadow, false, underlineShadow, shadow.offset);
            }
        }

        drawTextWithGlyphs(ctx, text, position, fontSize, textColor, glyphs);

        if (underline.enabled) {
            drawTextLines(ctx, copy, position, fontSize, underline, underlineColor, underlineGradient, underline.colorEnd, noOffset);
        }
        if (strikethrough.enabled) {
            drawTextLines(ctx, copy, position, fontSize, strikethrough, strikethroughColor, strikethroughGradient, strikethrough.colorEnd, noOffset);
        }
    }

    void label(Context* ctx, Rectangle rect, const char* text) {
        float fontSize = ctx->style.fontSize;
        Vector2 size = measureText(ctx, text, fontSize);

        Vector2 position;
        position.x = rect.x;
        position.y = rect.y + (rect.height - size.y) * 0.5f;
        drawStyledText(ctx, text, position, fontSize, ctx->style.text);
    }

    bool button(Context* ctx, Rectangle rect, const char* text) {
        ButtonStyle* style = &ctx->style.button;
        Id id = getId(ctx, text);
        WidgetState* state = getWidgetState(ctx, id);

        // interaction
        bool hovered = isMouseOver(ctx, rect);
        if (hovered && (ctx->activeId == 0 || ctx->activeId == id)) {
            ctx->nextHotId = id;
        }

        bool hot = ctx->hotId == id;
        bool clicked = false;

        if (hot && ctx->mousePressed) {
            ctx->activeId = id;
        }
        if (ctx->activeId == id && ctx->mouseReleased) {
            // only a click if released over the button it was pressed on
            clicked = hovered;
            ctx->activeId = 0;
        }

        bool held = ctx->activeId == id && hovered;

        // animation
        float step = style->animSpeed * ctx->dt;
        state->hoverT = approach(state->hoverT, hot ? 1.0f : 0.0f, step);
        state->pressT = approach(state->pressT, held ? 1.0f : 0.0f, step);

        // drawing
        drawStateSkins(ctx, rect, &style->background, state, hot, held);
        drawRectLines(ctx, rect, style->borderThickness, style->border);

        float fontSize = ctx->style.fontSize;
        Vector2 size = measureText(ctx, text, fontSize);
        Vector2 position;
        position.x = rect.x + (rect.width - size.x) * 0.5f;
        position.y = rect.y + (rect.height - size.y) * 0.5f;
        drawStyledText(ctx, text, position, fontSize, style->text);

        return clicked;
    }

    // Draws the scrollbar in track and lets the thumb be dragged, which
    // writes the panel's scroll position directly.
    static void scrollbar(Context* ctx, WidgetState* panel, Rectangle track, float contentHeight, float maxScroll) {
        PanelStyle* style = &ctx->style.panel;
        // called inside the panel's id scope, so every panel's scrollbar gets its own id
        Id id = getId(ctx, "#scrollbar");
        WidgetState* state = getWidgetState(ctx, id);

        // the thumb is to the track what the visible area is to the content
        float thumbHeight = track.height * (track.height / contentHeight);
        thumbHeight = Clamp(thumbHeight, style->minThumbHeight, track.height);
        float travel = track.height - thumbHeight;

        Rectangle thumb = { track.x, track.y + travel * (panel->scroll / maxScroll), track.width, thumbHeight };

        // interaction
        bool hovered = isMouseOver(ctx, thumb);
        if (hovered && (ctx->activeId == 0 || ctx->activeId == id)) {
            ctx->nextHotId = id;
        }

        bool hot = ctx->hotId == id;
        if (hot && ctx->mousePressed) {
            ctx->activeId = id;
            state->dragOffset = ctx->input.mousePosition.y - thumb.y;
        }

        // keeps following the mouse even when it leaves the thumb, until released
        bool dragging = ctx->activeId == id;
        if (dragging && travel > 0.0f) {
            float thumbY = ctx->input.mousePosition.y - state->dragOffset;
            float fraction = Clamp((thumbY - track.y) / travel, 0.0f, 1.0f);
            panel->scroll = fraction * maxScroll;
            panel->scrollTarget = panel->scroll;
            thumb.y = track.y + travel * fraction;
        }
        if (dragging && ctx->mouseReleased) {
            ctx->activeId = 0;
        }

        // animation
        float step = style->thumbAnimSpeed * ctx->dt;
        state->hoverT = approach(state->hoverT, (hot || dragging) ? 1.0f : 0.0f, step);
        state->pressT = approach(state->pressT, dragging ? 1.0f : 0.0f, step);

        // drawing
        drawSkin(ctx, track, &style->track, state->skinStateTime);
        drawStateSkins(ctx, thumb, &style->thumb, state, hot || dragging, dragging);
    }

    Rectangle beginPanel(Context* ctx, const char* name, Rectangle rect, float contentHeight) {
        PanelStyle* style = &ctx->style.panel;
        Id id = getId(ctx, name);
        WidgetState* state = getWidgetState(ctx, id);

        // scope the panel's widgets to it, so two panels can both have an "OK" button
        pushId(ctx, name);

        // the area inside the border
        float border = style->borderThickness;
        Rectangle inner = { rect.x + border, rect.y + border, rect.width - 2.0f * border, rect.height - 2.0f * border };

        float maxScroll = contentHeight - inner.height;
        if (maxScroll < 0.0f) maxScroll = 0.0f;
        bool scrollable = maxScroll > 0.0f;

        // mouse wheel: panels begin outer to inner, so the innermost hovered
        // panel claims it last and wins. Panels that can't scroll don't claim
        // it, so the wheel passes through to their parent.
        if (scrollable && isMouseOver(ctx, rect)) {
            ctx->nextScrollHotId = id;
        }
        if (ctx->scrollHotId == id) {
            state->scrollTarget -= ctx->input.mouseWheel * style->wheelStep;
        }

        // content can shrink between frames
        state->scrollTarget = Clamp(state->scrollTarget, 0.0f, maxScroll);
        state->scroll = Clamp(state->scroll, 0.0f, maxScroll);

        // ease toward the target, snapping once close so it settles
        float t = Clamp(style->scrollSpeed * ctx->dt, 0.0f, 1.0f);
        state->scroll = Lerp(state->scroll, state->scrollTarget, t);
        if (fabsf(state->scrollTarget - state->scroll) < 0.5f) {
            state->scroll = state->scrollTarget;
        }

        updateSkinState(ctx, state, 0);
        drawSkin(ctx, rect, &style->background, state->skinStateTime);
        drawRectLines(ctx, rect, border, style->border);

        Rectangle view = inner;
        if (scrollable) {
            view.width -= style->scrollbarWidth;
            Rectangle track = { view.x + view.width, inner.y, style->scrollbarWidth, inner.height };
            scrollbar(ctx, state, track, contentHeight, maxScroll);
        }

        pushClip(ctx, view);

        // rounded so text inside doesn't land on half pixels and blur
        Rectangle content = { view.x, view.y - roundf(state->scroll), view.width, contentHeight };
        return content;
    }

    void endPanel(Context* ctx) {
        popClip(ctx);
        popId(ctx);
    }

    // The click behavior of button-like widgets: hover, hot, pressing and
    // clicking, as described in docs/concepts/interaction.md. button spells
    // the same steps out inline, as the example in the docs.
    static bool pressable(Context* ctx, Id id, Rectangle rect, bool* hot, bool* held) {
        bool hovered = isMouseOver(ctx, rect);
        if (hovered && (ctx->activeId == 0 || ctx->activeId == id)) {
            ctx->nextHotId = id;
        }

        *hot = ctx->hotId == id;
        bool clicked = false;

        if (*hot && ctx->mousePressed) {
            ctx->activeId = id;
        }
        if (ctx->activeId == id && ctx->mouseReleased) {
            clicked = hovered;
            ctx->activeId = 0;
        }

        *held = ctx->activeId == id && hovered;
        return clicked;
    }

    static Color fade(Color color, float t) {
        color.a = (unsigned char)((float)color.a * t);
        return color;
    }

    // y of text vertically centered in rect
    static float centeredTextY(Context* ctx, Rectangle rect, const char* text, float fontSize) {
        Vector2 size = measureText(ctx, text, fontSize);
        return rect.y + (rect.height - size.y) * 0.5f;
    }

    bool checkbox(Context* ctx, Rectangle rect, const char* text, bool* checked) {
        CheckboxStyle* style = &ctx->style.checkbox;
        Id id = getId(ctx, text);
        WidgetState* state = getWidgetState(ctx, id);

        // interaction: the whole rect, box and label, is clickable
        bool hot = false;
        bool held = false;
        bool changed = false;
        if (pressable(ctx, id, rect, &hot, &held)) {
            *checked = !*checked;
            changed = true;
        }

        // animation
        float step = style->animSpeed * ctx->dt;
        state->hoverT = approach(state->hoverT, hot ? 1.0f : 0.0f, step);
        state->pressT = approach(state->pressT, held ? 1.0f : 0.0f, step);
        state->checkT = approach(state->checkT, *checked ? 1.0f : 0.0f, step);

        // drawing: a square box at the left, as tall as rect
        Rectangle box = { rect.x, rect.y, rect.height, rect.height };
        drawStateSkins(ctx, box, &style->box, state, hot, held);
        drawRectLines(ctx, box, style->borderThickness, style->border);

        if (state->checkT > 0.0f) {
            float inset = style->checkInset;
            Rectangle check = { box.x + inset, box.y + inset, box.width - 2.0f * inset, box.height - 2.0f * inset };
            Skin checkSkin = fadeSkin(style->check, state->checkT);
            drawSkin(ctx, check, &checkSkin, state->skinStateTime);
        }

        float fontSize = ctx->style.fontSize;
        Vector2 position;
        position.x = box.x + box.width + style->labelGap;
        position.y = centeredTextY(ctx, rect, text, fontSize);
        drawStyledText(ctx, text, position, fontSize, style->text);

        return changed;
    }

    bool slider(Context* ctx, Rectangle rect, const char* name, float* value, float min, float max, float step) {
        SliderStyle* style = &ctx->style.slider;
        Id id = getId(ctx, name);
        WidgetState* state = getWidgetState(ctx, id);

        // the thumb's center travels from left to left + travel, so the thumb stays inside rect
        float halfThumb = style->thumbWidth * 0.5f;
        float left = rect.x + halfThumb;
        float travel = rect.width - style->thumbWidth;
        float range = max - min;

        // interaction: like a button, but once pressed it follows the mouse until release
        bool hovered = isMouseOver(ctx, rect);
        if (hovered && (ctx->activeId == 0 || ctx->activeId == id)) {
            ctx->nextHotId = id;
        }

        bool hot = ctx->hotId == id;
        if (hot && ctx->mousePressed) {
            ctx->activeId = id;
        }

        bool dragging = ctx->activeId == id;
        bool changed = false;
        if (dragging) {
            float t = 0.0f;
            if (travel > 0.0f) {
                t = Clamp((ctx->input.mousePosition.x - left) / travel, 0.0f, 1.0f);
            }

            float newValue = min + t * range;
            if (step > 0.0f) {
                newValue = min + roundf((newValue - min) / step) * step;
                // rounding up can pass max when range isn't a multiple of step
                if (newValue > max) newValue -= step;
            }

            if (newValue != *value) {
                *value = newValue;
                changed = true;
            }

            if (ctx->mouseReleased) {
                ctx->activeId = 0;
            }
        }

        // animation
        float animStep = style->animSpeed * ctx->dt;
        state->hoverT = approach(state->hoverT, (hot || dragging) ? 1.0f : 0.0f, animStep);
        state->pressT = approach(state->pressT, dragging ? 1.0f : 0.0f, animStep);

        // drawing: a value outside the range is drawn at the nearest end
        float t = 0.0f;
        if (range > 0.0f) {
            t = Clamp((*value - min) / range, 0.0f, 1.0f);
        }
        float thumbCenter = left + t * travel;

        float trackY = rect.y + (rect.height - style->trackHeight) * 0.5f;
        Rectangle track = { rect.x, trackY, rect.width, style->trackHeight };
        Rectangle filled = { rect.x, trackY, thumbCenter - rect.x, style->trackHeight };
        Rectangle thumb = { thumbCenter - halfThumb, rect.y, style->thumbWidth, rect.height };

        // record the state first, so the track and fill share the thumb's animation timing
        updateSkinState(ctx, state, dragging ? 2 : (hot ? 1 : 0));
        drawSkin(ctx, track, &style->track, state->skinStateTime);
        drawSkin(ctx, filled, &style->fill, state->skinStateTime);
        drawStateSkins(ctx, thumb, &style->thumb, state, hot || dragging, dragging);

        return changed;
    }

    bool dropdown(Context* ctx, Rectangle rect, const char* name, const char* const* items, int itemCount, int* selected) {
        DropdownStyle* style = &ctx->style.dropdown;
        Id id = getId(ctx, name);
        WidgetState* state = getWidgetState(ctx, id);

        bool open = ctx->openPopupId == id;
        bool justOpened = false;
        bool changed = false;

        // the box: clicking it opens or closes the list
        bool hot = false;
        bool held = false;
        if (pressable(ctx, id, rect, &hot, &held)) {
            if (open) {
                ctx->openPopupId = 0;
            }
            else {
                // replaces any other open popup
                ctx->openPopupId = id;
                justOpened = true;
            }
            open = !open;
        }
        if (open) {
            ctx->popupCalled = true;
        }

        // animation
        float animStep = style->animSpeed * ctx->dt;
        state->hoverT = approach(state->hoverT, hot ? 1.0f : 0.0f, animStep);
        state->pressT = approach(state->pressT, held ? 1.0f : 0.0f, animStep);
        if (open) {
            state->openT = approach(state->openT, 1.0f, animStep);
        }
        else {
            state->openT = 0.0f; // so it fades in again next time it opens
        }

        // drawing the box: selected item on the left, arrow on the right
        float fontSize = ctx->style.fontSize;
        drawStateSkins(ctx, rect, &style->box, state, hot, held);
        drawRectLines(ctx, rect, style->borderThickness, style->border);

        if (*selected >= 0 && *selected < itemCount) {
            const char* text = items[*selected];
            Vector2 position = { rect.x + style->padding, centeredTextY(ctx, rect, text, fontSize) };
            drawStyledText(ctx, text, position, fontSize, style->text);
        }

        // the arrow: whatever skin it's given, in a square at the right; none by default
        Rectangle arrow;
        arrow.width = style->arrowSize;
        arrow.height = style->arrowSize;
        arrow.x = rect.x + rect.width - style->padding - arrow.width;
        arrow.y = rect.y + (rect.height - arrow.height) * 0.5f;
        drawSkin(ctx, arrow, &style->arrow, state->skinStateTime);

        if (!open || itemCount <= 0) {
            return changed;
        }

        // the list: as many items as fit, at most maxVisibleItems
        float border = style->borderThickness;
        float itemHeight = rect.height;
        int visibleItems = itemCount;
        if (visibleItems > style->maxVisibleItems) visibleItems = style->maxVisibleItems;
        if (visibleItems < 1) visibleItems = 1;
        float listHeight = (float)visibleItems * itemHeight + 2.0f * border;

        // direction: down if it fits, else up if it fits, else the side with more room
        bool downward = true;
        bool screenKnown = ctx->screen.width > 0.0f && ctx->screen.height > 0.0f;
        if (screenKnown) {
            float below = ctx->screen.y + ctx->screen.height - (rect.y + rect.height);
            float above = rect.y - ctx->screen.y;
            if (listHeight > below) {
                if (listHeight <= above) {
                    downward = false;
                }
                else {
                    downward = below >= above;
                    float room = downward ? below : above;
                    int fitting = (int)floorf((room - 2.0f * border) / itemHeight);
                    if (fitting < 1) fitting = 1;
                    if (fitting < visibleItems) visibleItems = fitting;
                    listHeight = (float)visibleItems * itemHeight + 2.0f * border;
                }
            }
        }

        Rectangle list = { rect.x, rect.y + rect.height, rect.width, listHeight };
        if (!downward) {
            list.y = rect.y - listHeight;
        }

        // the list draws over everything and isn't cut off by a panel around the dropdown
        Layer previousLayer = ctx->layer;
        setLayer(ctx, Layer::Overlay);
        pushRootClip(ctx, list);
        pushId(ctx, name);

        // open showing the selected item at the top, as far as the list can scroll
        if (justOpened && *selected >= 0 && *selected < itemCount) {
            WidgetState* listState = getWidgetState(ctx, getId(ctx, "#list"));
            listState->scroll = (float)*selected * itemHeight;
            listState->scrollTarget = listState->scroll;
        }

        // the list is a panel in the dropdown's colors, faded in by openT
        float t = state->openT;
        PanelStyle savedPanelStyle = ctx->style.panel;
        ctx->style.panel.background = fadeSkin(style->listBackground, t);
        ctx->style.panel.border = fade(style->border, t);
        ctx->style.panel.borderThickness = border;

        Rectangle content = beginPanel(ctx, "#list", list, (float)itemCount * itemHeight);
        Rectangle view = ctx->clipStack[ctx->clipStackCount - 1];
        for (int i = 0; i < itemCount; i++) {
            Rectangle item = { content.x, content.y + (float)i * itemHeight, content.width, itemHeight };
            // skip items scrolled out of view; long lists only pay for what's shown
            if (item.y + item.height <= view.y || item.y >= view.y + view.height) continue;

            pushId(ctx, i);
            Id itemId = getId(ctx, "#item");
            bool itemHot = false;
            bool itemHeld = false;
            bool picked = pressable(ctx, itemId, item, &itemHot, &itemHeld);
            popId(ctx);

            if (i == *selected) {
                Skin selectedSkin = fadeSkin(style->itemSelected, t);
                drawSkin(ctx, item, &selectedSkin, state->skinStateTime);
            }
            else if (itemHot) {
                Skin hoverSkin = fadeSkin(style->itemHover, t);
                drawSkin(ctx, item, &hoverSkin, state->skinStateTime);
            }
            Vector2 position = { item.x + style->padding, centeredTextY(ctx, item, items[i], fontSize) };
            drawStyledText(ctx, items[i], position, fontSize, fade(style->text, t));

            if (picked) {
                if (*selected != i) {
                    *selected = i;
                    changed = true;
                }
                ctx->openPopupId = 0;
            }
        }
        endPanel(ctx);
        ctx->style.panel = savedPanelStyle;

        popId(ctx);
        popClip(ctx);
        setLayer(ctx, previousLayer);

        // a press anywhere outside the box and the list closes it, and still
        // reaches whatever was pressed
        Vector2 mouse = ctx->input.mousePosition;
        bool onBox = CheckCollisionPointRec(mouse, rect);
        bool onList = CheckCollisionPointRec(mouse, list);
        if (ctx->openPopupId == id && ctx->mousePressed && !onBox && !onList) {
            ctx->openPopupId = 0;
        }

        // while open, the list blocks the mouse for base widgets from next frame
        if (ctx->openPopupId == id) {
            ctx->nextBlockingActive = true;
            ctx->nextBlockingRect = list;
        }

        return changed;
    }

    // UTF-8 helpers for text editing. Positions are byte offsets, always at
    // the start of a character: continuation bytes look like 10xxxxxx.
    static bool isContinuationByte(char c) {
        return ((unsigned char)c & 0xC0) == 0x80;
    }

    static int previousCharStart(const char* text, int position) {
        if (position <= 0) return 0;
        position--;
        while (position > 0 && isContinuationByte(text[position])) position--;
        return position;
    }

    static int nextCharStart(const char* text, int position, int length) {
        if (position >= length) return length;
        position++;
        while (position < length && isContinuationByte(text[position])) position++;
        return position;
    }

    // a word is a run of characters that aren't spaces
    static int previousWordStart(const char* text, int position) {
        while (position > 0 && text[position - 1] == ' ') position--;
        while (position > 0 && text[position - 1] != ' ') position--;
        return position;
    }

    static int nextWordStart(const char* text, int position, int length) {
        while (position < length && text[position] != ' ') position++;
        while (position < length && text[position] == ' ') position++;
        return position;
    }

    // writes codepoint as UTF-8 into out (room for 4 bytes), returns the byte count
    static int encodeUtf8(int codepoint, char* out) {
        if (codepoint < 0x80) {
            out[0] = (char)codepoint;
            return 1;
        }
        if (codepoint < 0x800) {
            out[0] = (char)(0xC0 | (codepoint >> 6));
            out[1] = (char)(0x80 | (codepoint & 0x3F));
            return 2;
        }
        if (codepoint < 0x10000) {
            out[0] = (char)(0xE0 | (codepoint >> 12));
            out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            out[2] = (char)(0x80 | (codepoint & 0x3F));
            return 3;
        }
        out[0] = (char)(0xF0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }

    // removes the bytes from start up to end, keeping the terminating zero
    static void deleteRange(char* text, int* length, int start, int end) {
        memmove(text + start, text + end, (size_t)(*length - end + 1));
        *length -= end - start;
    }

    // width of the first count bytes of text. Terminates the text there for a
    // moment instead of copying it.
    static float prefixWidth(Context* ctx, char* text, int count, float fontSize) {
        char saved = text[count];
        text[count] = '\0';
        float width = measureText(ctx, text, fontSize).x;
        text[count] = saved;
        return width;
    }

    TextInputResult textInput(Context* ctx, Rectangle rect, const char* name, char* buffer, int capacity, const char* placeholder) {
        TextInputStyle* style = &ctx->style.textInput;
        Id id = getId(ctx, name);
        WidgetState* state = getWidgetState(ctx, id);
        float fontSize = ctx->style.fontSize;

        TextInputResult result;
        result.changed = false;
        result.submitted = false;
        result.lostFocus = false;
        result.full = false;

        // the game may have changed the text, so keep the cursor inside it and on a character
        int length = (int)strlen(buffer);
        if (state->cursor < 0 || state->cursor > length) state->cursor = length;
        while (state->cursor > 0 && isContinuationByte(buffer[state->cursor])) state->cursor--;

        // the text sits inside the border, padding in from each side
        float border = style->borderThickness;
        Rectangle inside = { rect.x + border, rect.y + border, rect.width - 2.0f * border, rect.height - 2.0f * border };
        float textLeft = inside.x + style->padding;
        float textWidth = inside.width - 2.0f * style->padding;

        // interaction: pressing on the box focuses it and places the cursor,
        // pressing anywhere else takes focus away
        bool hovered = isMouseOver(ctx, rect);
        if (hovered && (ctx->activeId == 0 || ctx->activeId == id)) {
            ctx->nextHotId = id;
        }
        bool hot = ctx->hotId == id;

        if (hot && ctx->mousePressed) {
            ctx->focusId = id;

            // the character boundary nearest the mouse
            float mouseX = ctx->input.mousePosition.x - textLeft + state->textScroll;
            int best = 0;
            float bestDistance = fabsf(mouseX);
            int position = 0;
            while (position < length) {
                position = nextCharStart(buffer, position, length);
                float distance = fabsf(prefixWidth(ctx, buffer, position, fontSize) - mouseX);
                if (distance < bestDistance) {
                    best = position;
                    bestDistance = distance;
                }
            }
            state->cursor = best;
            state->blinkTime = 0.0f;
        }
        else if (ctx->focusId == id && ctx->mousePressed && !hovered) {
            ctx->focusId = 0;
        }

        // typing
        if (ctx->focusId == id) {
            ctx->focusCalled = true;

            for (int i = 0; i < ctx->input.keyboardEventCount && ctx->focusId == id; i++) {
                KeyboardEvent* event = &ctx->input.keyboardEvents[i];
                int cursor = state->cursor;

                if (event->type == KeyboardEventType::Character) {
                    // printable characters only
                    if (event->codepoint < 32 || event->codepoint == 127) continue;

                    char bytes[4];
                    int byteCount = encodeUtf8(event->codepoint, bytes);
                    if (length + byteCount + 1 > capacity) {
                        result.full = true;
                        continue;
                    }
                    memmove(buffer + cursor + byteCount, buffer + cursor, (size_t)(length - cursor + 1));
                    memcpy(buffer + cursor, bytes, (size_t)byteCount);
                    length += byteCount;
                    state->cursor = cursor + byteCount;
                    result.changed = true;
                }
                else {
                    switch (event->key) {
                    case Key::Backspace:
                        if (cursor > 0) {
                            int start = event->ctrl ? previousWordStart(buffer, cursor) : previousCharStart(buffer, cursor);
                            deleteRange(buffer, &length, start, cursor);
                            state->cursor = start;
                            result.changed = true;
                        }
                        break;
                    case Key::Delete:
                        if (cursor < length) {
                            deleteRange(buffer, &length, cursor, nextCharStart(buffer, cursor, length));
                            result.changed = true;
                        }
                        break;
                    case Key::Left:
                        state->cursor = event->ctrl ? previousWordStart(buffer, cursor) : previousCharStart(buffer, cursor);
                        break;
                    case Key::Right:
                        state->cursor = event->ctrl ? nextWordStart(buffer, cursor, length) : nextCharStart(buffer, cursor, length);
                        break;
                    case Key::Home:
                        state->cursor = 0;
                        break;
                    case Key::End:
                        state->cursor = length;
                        break;
                    case Key::Enter:
                        result.submitted = true;
                        ctx->focusId = 0;
                        break;
                    case Key::Escape:
                        ctx->focusId = 0;
                        break;
                    }
                }
                state->blinkTime = 0.0f; // keep the cursor showing while typing
            }
        }

        bool focused = ctx->focusId == id;
        result.lostFocus = ctx->previousFocusId == id && !focused;

        // animation
        float animStep = style->animSpeed * ctx->dt;
        state->hoverT = approach(state->hoverT, hot ? 1.0f : 0.0f, animStep);
        state->focusT = approach(state->focusT, focused ? 1.0f : 0.0f, animStep);
        state->blinkTime = focused ? state->blinkTime + ctx->dt : 0.0f;

        // sideways scroll: keep the cursor in view while focused, show the start otherwise
        float cursorX = prefixWidth(ctx, buffer, state->cursor, fontSize);
        if (focused) {
            if (cursorX - state->textScroll > textWidth) state->textScroll = cursorX - textWidth;
            if (cursorX - state->textScroll < 0.0f) state->textScroll = cursorX;
            // don't leave empty space after the end of the text once it shrinks
            float maxScroll = measureText(ctx, buffer, fontSize).x - textWidth;
            if (maxScroll < 0.0f) maxScroll = 0.0f;
            state->textScroll = Clamp(state->textScroll, 0.0f, maxScroll);
        }
        else {
            state->textScroll = 0.0f;
        }

        // drawing
        // three color skins fade into each other; anything else switches
        int skinState = focused ? 2 : (hot ? 1 : 0);
        updateSkinState(ctx, state, skinState);
        bool allColors = style->background.kind == SkinKind::Color
            && style->backgroundHover.kind == SkinKind::Color
            && style->backgroundFocused.kind == SkinKind::Color;
        if (allColors) {
            Color fill = ColorLerp(style->background.color, style->backgroundHover.color, state->hoverT);
            fill = ColorLerp(fill, style->backgroundFocused.color, state->focusT);
            drawRect(ctx, rect, fill);
        }
        else {
            const Skin* skin = &style->background;
            if (skinState == 1) skin = &style->backgroundHover;
            if (skinState == 2) skin = &style->backgroundFocused;
            drawSkin(ctx, rect, skin, state->skinStateTime);
        }
        Color borderColor = ColorLerp(style->border, style->borderFocused, state->focusT);
        drawRectLines(ctx, rect, border, borderColor);

        pushClip(ctx, inside);
        float textY = rect.y + (rect.height - fontSize) * 0.5f;
        if (length == 0) {
            if (placeholder != NULL) {
                drawText(ctx, placeholder, Vector2{ textLeft, textY }, fontSize, style->placeholder);
            }
        }
        else {
            drawStyledText(ctx, buffer, Vector2{ textLeft - state->textScroll, textY }, fontSize, style->text);
        }

        // blinking: shown for cursorBlinkTime, hidden for cursorBlinkTime
        bool cursorShown = fmodf(state->blinkTime, 2.0f * style->cursorBlinkTime) < style->cursorBlinkTime;
        if (focused && cursorShown) {
            Rectangle cursorRect = { roundf(textLeft + cursorX - state->textScroll), roundf(textY), style->cursorWidth, fontSize };
            drawRect(ctx, cursorRect, style->cursor);
        }
        popClip(ctx);

        return result;
    }

}
}
