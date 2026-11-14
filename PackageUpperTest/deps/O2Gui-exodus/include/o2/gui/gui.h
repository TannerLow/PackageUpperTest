#pragma once
#include <stddef.h>
#include <stdint.h>
#include <raylib.h>
#include <o2/gui/arena.h>

// Immediate-mode UI core.
//
// Every UI update:
//   1. Fill an Input (see o2/gui/gui_raylib.h for a raylib helper)
//   2. beginFrame
//   3. Call widgets, e.g. `if (button(&ui, rect, "Play")) { ... }`
//   4. endFrame
//   5. Render the draw commands (see o2/gui/gui_raylib.h)
//
// Widgets are identified by a hash of their label combined with the current
// id scope (pushId/popId). State that must survive between frames, such as
// animation progress, is kept in a fixed pool inside the Context, keyed by
// that id. Widgets that stop being called have their state recycled.
//
// The core reads input only from the Input struct and never draws directly;
// all drawing goes into a command list that a backend renders.

// These size the arrays inside Context, so to override one, define it for the
// whole project (e.g. in the vcxproj preprocessor definitions). Defining it
// differently in different .cpp files gives them different Context layouts.
#ifndef O2_GUI_MAX_WIDGET_STATES
#define O2_GUI_MAX_WIDGET_STATES 256
#endif

#ifndef O2_GUI_MAX_ID_STACK
#define O2_GUI_MAX_ID_STACK 32
#endif

#ifndef O2_GUI_MAX_CLIP_STACK
#define O2_GUI_MAX_CLIP_STACK 16
#endif

// Sizes the keyboard event array in Input, which the Context also holds a
// copy of, so it's project-wide like the others.
#ifndef O2_GUI_MAX_KEYBOARD_EVENTS
#define O2_GUI_MAX_KEYBOARD_EVENTS 32
#endif

// How deeply each text style stack (color, shadow, underline,
// strikethrough) can nest. Sizes arrays in the Context, so project-wide too.
#ifndef O2_GUI_MAX_TEXT_STYLE_STACK
#define O2_GUI_MAX_TEXT_STYLE_STACK 8
#endif

namespace o2 {
namespace gui {

    typedef uint32_t Id;

    // The editing keys text input understands
    enum class Key {
        Backspace,
        Delete,
        Left,
        Right,
        Home,
        End,
        Enter,
        Escape
    };

    enum class KeyboardEventType {
        Character, // a typed character, already resolved for Shift, Caps Lock and the keyboard layout
        Key        // an editing key press, including the operating system's key repeats
    };

    struct KeyboardEvent {
        KeyboardEventType type;
        int codepoint; // Character: the unicode codepoint typed
        Key key;       // Key: which key
        bool ctrl;     // Key: whether Ctrl was held when it was pressed
    };

    // The mouse is state, not events: beginFrame works out presses and
    // releases by comparing mouseDown with the previous update's. That way the
    // UI can update less often than the game polls input without missing
    // clicks. The wheel and the keyboard are collected since the last update.
    struct Input {
        Vector2 mousePosition;
        bool mouseDown;   // left button is held
        float mouseWheel; // wheel movement since the last UI update, in notches

        // typing since the last UI update, in the order it happened. Events
        // past O2_GUI_MAX_KEYBOARD_EVENTS are dropped.
        KeyboardEvent keyboardEvents[O2_GUI_MAX_KEYBOARD_EVENTS];
        int keyboardEventCount;
    };

    // Commands are rendered layer by layer, so something added to
    // Layer::Overlay (like an open dropdown) draws over everything on
    // Layer::Base regardless of call order.
    enum class Layer {
        Base,
        Overlay,
        Count
    };

    enum class DrawCommandType {
        Rect,
        RectLines,
        Text,
        RectGradientH, // a rect fading from color at the left to colorEnd at the right
        Texture,       // a texture region stretched over rect, tinted by color
        NineSlice      // a texture region 9-sliced over rect, tinted by color
    };

    // Texture, NineSlice: what to draw, kept in the frame arena
    struct TextureDraw {
        Texture2D texture;
        Rectangle source;   // in texture pixels
        int left;           // NineSlice: border sizes in texture pixels
        int top;
        int right;
        int bottom;
    };

    // Per-character styling for one piece of text, kept in the frame arena.
    // Characters are counted from 0 at the start of the text, not counting
    // line breaks; characters past an array's count are drawn normally.
    struct TextGlyphStyle {
        const Color* colors;    // one color per character, replacing the command's color; may be NULL
        int colorCount;
        bool darkenColors;      // draw colors darkened to 25%, for a shadow that follows them
        const bool* obfuscated; // true for characters to draw as random ones of the same width; may be NULL
        int obfuscatedCount;
        uint32_t obfuscationSeed; // which random characters; changes at the obfuscation's rate
    };

    struct DrawCommand {
        DrawCommandType type;
        Layer layer;
        bool clipped;     // if true, only the part inside clip is drawn
        Rectangle clip;
        Rectangle rect;   // Text uses x, y as the text position
        Color color;
        float thickness;  // RectLines
        const char* text; // Text, copied into the frame arena
        float fontSize;   // Text
        Color colorEnd;    // RectGradientH: the color at the right edge
        const TextGlyphStyle* glyphs; // Text: per-character styling, NULL for plain text
        const TextureDraw* texture;   // Texture, NineSlice: the texture and how to slice it
    };

    // Text measuring and drawing go through these so the font can be swapped
    // out. The renderer uses draw; widgets use measure for layout.
    struct TextFuncs {
        void* user;
        Vector2 (*measure)(void* user, const char* text, float fontSize);
        // glyphs is NULL for plain text; otherwise it gives per-character
        // colors and obfuscation (see TextGlyphStyle)
        void (*draw)(
            void* user,
            const char* text,
            Vector2 position,
            float fontSize,
            Color color,
            const TextGlyphStyle* glyphs
        );
    };

    // Textures

    // Which way animation frames run in a texture, for fillFrameStrip
    enum class FrameOrder {
        Horizontal, // left to right; in a grid, row by row from the top
        Vertical    // top to bottom; in a grid, column by column from the left
    };

    // A rectangle of a texture: the whole image, or one image in an atlas.
    struct TextureRegion {
        Texture2D texture;
        Rectangle source; // in texture pixels
    };

    // 9-slicing: corners keep their size, edges and the center stretch.
    // Borders in texture pixels; all 0 means the region simply stretches.
    struct NineSlice {
        int left;
        int top;
        int right;
        int bottom;
    };

    enum class AnimationTiming {
        Looping,  // runs continuously from ctx->time; every widget using it is in step
        Triggered // starts from the first frame when the widget changes state, e.g. becomes hovered
    };

    // Frames of one or more textures, shown one after another.
    struct TextureAnimation {
        const TextureRegion* frames; // read while widgets are called, so it must outlive them
        int frameCount;
        float framesPerSecond;
        AnimationTiming timing;
        bool loop;                   // Triggered: keep looping after the first play; otherwise hold the last frame
    };

    enum class SkinKind {
        None,     // draws nothing
        Color,    // a solid rectangle in color
        Texture,  // region, stretched or 9-sliced, tinted by color
        Animation // animation's current frame, stretched or 9-sliced, tinted by color
    };

    // How one part of a widget is drawn.
    struct Skin {
        SkinKind kind;
        Color color;                // Color: the fill. Texture, Animation: the tint; WHITE leaves it unchanged
        TextureRegion region;       // Texture
        TextureAnimation animation; // Animation
        NineSlice nineSlice;        // Texture, Animation; all 0 to stretch
    };

    // A part's skins for its three states. Between two Color skins the part
    // fades; otherwise it switches instantly.
    struct StateSkins {
        Skin normal;
        Skin hover;
        Skin pressed;
    };

    struct ButtonStyle {
        StateSkins background;
        Color border;
        Color text;
        float borderThickness;
        float animSpeed; // how fast hover/press fades run, in full fades per second
    };

    struct PanelStyle {
        Skin background;
        Color border;
        float borderThickness;

        float scrollbarWidth;
        float minThumbHeight;
        Skin track;
        StateSkins thumb;
        float thumbAnimSpeed; // same meaning as ButtonStyle::animSpeed

        float wheelStep;   // pixels scrolled per mouse wheel notch
        float scrollSpeed; // how quickly the scroll position eases toward its target, higher is snappier
    };

    struct CheckboxStyle {
        StateSkins box;
        Skin check;           // shown inside the box when checked, fading in and out
        Color border;
        Color text;
        float borderThickness;
        float checkInset;     // gap between the box's edge and the check
        float labelGap;       // gap between the box and the label
        float animSpeed;      // hover, press and check fades, in full fades per second
    };

    struct SliderStyle {
        Skin track;
        Skin fill;            // the part of the track left of the thumb
        StateSkins thumb;
        float trackHeight;
        float thumbWidth;
        float animSpeed;
    };

    struct DropdownStyle {
        StateSkins box;       // the closed box, like a button
        Color border;
        Color text;
        Skin arrow;           // drawn at the right of the box; None by default, so there's no arrow
        Skin listBackground;
        Skin itemHover;
        Skin itemSelected;    // the currently selected item in the open list
        float borderThickness;
        float padding;        // space left of the text, and right of the arrow
        float arrowSize;      // the arrow is drawn in a square this big
        int maxVisibleItems;  // longer lists scroll
        float animSpeed;      // hover and press fades, and the list fading in
    };

    struct TextInputStyle {
        Skin background;
        Skin backgroundHover;
        Skin backgroundFocused;
        Color border;
        Color borderFocused;
        Color text;
        Color placeholder;     // the hint shown while the text is empty
        Color cursor;
        float borderThickness;
        float padding;         // space between the border and the text, left and right
        float cursorWidth;
        float cursorBlinkTime; // seconds the cursor is shown, then hidden, while blinking
        float animSpeed;       // hover and focus fades, in full fades per second
    };

    // A shadow drawn behind text: the same text, offset and in another color.
    struct TextShadow {
        bool enabled;
        Color color;         // used when colorFromText is false
        bool colorFromText;  // use the text's own color darkened to 25%, like Minecraft
        Vector2 offset;      // in pixels; a font pixel at your font size is a good start
    };

    // An underline or strikethrough line.
    struct TextLine {
        bool enabled;
        Color color;         // used when colorFromText is false; the left end when gradient is true
        bool colorFromText;  // use the text's own color (its first character's, with per-character colors)
        bool gradient;       // when not colorFromText: fade from color at the left to colorEnd at the right
        Color colorEnd;
        float thickness;     // in pixels
        float position;      // top of the line, as a fraction of the font size from the top of the text
    };

    // A color at a character index, for fillGradient
    struct GradientStop {
        int character;
        Color color;
    };

    const int MAX_GRADIENT_STOPS = 8;

    // A pushed per-character array, copied into the frame arena by its push
    struct TextCharacterColors {
        const Color* colors;
        int count;
    };

    struct TextObfuscation {
        const bool* obfuscated;
        int count;
        float changesPerSecond;
    };

    struct Style {
        Color text;
        float fontSize;

        // text effects used while nothing is pushed (see pushTextShadow etc.);
        // all disabled by default
        TextShadow textShadow;
        TextLine underline;
        TextLine strikethrough;

        ButtonStyle button;
        PanelStyle panel;
        CheckboxStyle checkbox;
        SliderStyle slider;
        DropdownStyle dropdown;
        TextInputStyle textInput;
    };

    struct WidgetState {
        Id id;              // 0 means the slot is free
        uint64_t lastFrame; // last frame the widget was called, used to recycle slots
        float hoverT;       // 0..1, animated toward 1 while hovered
        float pressT;       // 0..1, animated toward 1 while held down

        // panels
        float scroll;       // current scroll offset in pixels, eases toward scrollTarget
        float scrollTarget;
        float dragOffset;   // scrollbar thumb: mouse y minus thumb top when the drag started

        float checkT;       // checkbox: 0..1, animated toward 1 while checked
        float openT;        // dropdown: 0..1, animated toward 1 while open

        // text input
        float focusT;       // 0..1, animated toward 1 while focused
        int cursor;         // cursor position, as a byte offset into the text
        float textScroll;   // how far the text is scrolled left, in pixels
        float blinkTime;    // seconds since the cursor last moved or the text changed

        // skins
        int skinState;        // which of a widget's states its skins last showed, e.g. normal or hover
        double skinStateTime; // ctx->time when skinState last changed, for triggered animations
    };

    struct Context {
        arena::Arena frameArena; // reset every beginFrame
        DrawCommand* commands;
        int commandCount;
        int commandCapacity;

        WidgetState widgets[O2_GUI_MAX_WIDGET_STATES];
        WidgetState overflowWidget; // handed out when the pool is full

        Id idStack[O2_GUI_MAX_ID_STACK];
        int idStackCount;

        Id hotId;     // widget under the mouse, decided last frame
        Id nextHotId; // widget under the mouse this frame, becomes hotId at endFrame
        Id activeId;  // widget being interacted with, e.g. a button held down

        // like hotId, but for which panel gets the mouse wheel
        Id scrollHotId;
        Id nextScrollHotId;

        // The area the UI is drawn in, set with setScreen. Popups use it to
        // stay on screen. Zero size means unknown.
        Rectangle screen;

        // keyboard focus: the widget that typing goes to, 0 if none. Like
        // popups, it's cleared if its widget isn't called for a frame.
        Id focusId;
        bool focusCalled;   // the focused widget was called this frame
        Id previousFocusId; // focusId at the end of the last frame, so widgets can tell they lost focus

        // pushed text styles; each overrides the Style's own while non-empty
        Color textColorStack[O2_GUI_MAX_TEXT_STYLE_STACK];
        int textColorCount;
        TextShadow textShadowStack[O2_GUI_MAX_TEXT_STYLE_STACK];
        int textShadowCount;
        TextLine underlineStack[O2_GUI_MAX_TEXT_STYLE_STACK];
        int underlineCount;
        TextLine strikethroughStack[O2_GUI_MAX_TEXT_STYLE_STACK];
        int strikethroughCount;
        TextCharacterColors characterColorStack[O2_GUI_MAX_TEXT_STYLE_STACK];
        int characterColorCount;
        TextObfuscation obfuscationStack[O2_GUI_MAX_TEXT_STYLE_STACK];
        int obfuscationCount;

        // popups, such as an open dropdown's list. Only one is open at a time.
        // While one is open, its rect blocks the mouse for widgets on
        // Layer::Base, decided a frame ahead like hotId.
        Id openPopupId;          // 0 if none
        bool popupCalled;        // the open popup's widget was called this frame
        bool blockingActive;     // blockingRect blocks the mouse this frame
        Rectangle blockingRect;
        bool nextBlockingActive;
        Rectangle nextBlockingRect;

        Rectangle clipStack[O2_GUI_MAX_CLIP_STACK];
        int clipStackCount;

        Layer layer;
        Input input;
        bool mousePressed;  // mouseDown went from up to down since the last update
        bool mouseReleased; // mouseDown went from down to up since the last update
        float dt;
        uint64_t frame;
        double time;        // total seconds of UI updates: the sum of every dt, for animations

        Style style;
        TextFuncs text;
    };

    Style defaultStyle();

    // Making textures and skins
    TextureRegion makeTextureRegion(Texture2D texture); // the whole texture
    TextureRegion makeTextureRegion(Texture2D texture, Rectangle source);

    // Fills frames[0..count) with regions the size of first, for an
    // animation's frames laid out in a texture: starting at first, one size
    // step at a time to the right (the default) or downward. With
    // framesPerLine, a grid: after that many frames, the next row (Horizontal)
    // or column (Vertical) starts. spacing is the gap between frames in
    // texture pixels, in both directions; atlases drawn with bilinear
    // filtering need gaps so neighbours don't bleed in.
    void fillFrameStrip(TextureRegion* frames, int count, Texture2D texture, Rectangle first);
    void fillFrameStrip(
        TextureRegion* frames,
        int count,
        Texture2D texture,
        Rectangle first,
        FrameOrder order,
        float spacing
    );
    void fillFrameStrip(
        TextureRegion* frames,
        int count,
        Texture2D texture,
        Rectangle first,
        FrameOrder order,
        int framesPerLine,
        float spacing
    );

    Skin skinNone();
    Skin skinColor(Color color);
    Skin skinTexture(TextureRegion region);                       // stretched, untinted
    Skin skinNineSlice(TextureRegion region, NineSlice nineSlice); // untinted
    Skin skinAnimation(TextureAnimation animation);               // stretched, untinted
    StateSkins stateSkins(Skin normal, Skin hover, Skin pressed);

    // Sets the text color of every widget in style at once: labels, buttons,
    // checkboxes, dropdowns and text inputs (not the text input placeholder).
    void setAllTextColors(Style* style, Color color);

    // Filling in text effects
    TextShadow makeTextShadow(Color color, Vector2 offset);
    TextShadow makeTextShadowFromText(Vector2 offset); // darkened text color
    TextLine makeTextLine(Color color, float thickness, float position);
    TextLine makeTextLineFromText(float thickness, float position); // text color
    TextLine makeTextLineGradient(Color left, Color right, float thickness, float position);

    // Per-character arrays. These fill arrays you own, to pass to
    // pushTextCharacterColors and pushTextObfuscation.

    // Number of characters in UTF-8 text, as the per-character arrays count
    // them: every character, spaces included, line breaks not.
    int countCharacters(const char* text);

    // Fills colors[0..count) with a stepped gradient through up to
    // MAX_GRADIENT_STOPS stops, sorted by character. Characters before the
    // first stop take its color, characters after the last take the last.
    void fillGradient(Color* colors, int count, const GradientStop* stops, int stopCount);

    // Fills colors[0..count) with a rainbow: hue goes round once every
    // charactersPerCycle characters. With cyclesPerSecond 0 it's static;
    // otherwise the colors travel along the text that many cycles a second,
    // changing stepsPerSecond times a second (0 for every update). Pass
    // ctx->time as time. saturation and brightness are 0..1.
    void fillRainbow(
        Color* colors, 
        int count, 
        float charactersPerCycle, 
        float cyclesPerSecond, 
        float stepsPerSecond, 
        double time, 
        float saturation, 
        float brightness
    );

    // Sets length entries from first to value, clipped to the array
    void fillRange(Color* colors, int count, int first, int length, Color value);
    void fillRange(bool* flags, int count, int first, int length, bool value);

    // frameMemory backs the per-frame arena that holds draw commands and
    // copied strings. It must stay alive as long as the context.
    void init(Context* ctx, void* frameMemory, size_t frameMemorySize, int maxCommands);

    void beginFrame(Context* ctx, const Input* input, float dt);
    void endFrame(Context* ctx);

    // Ids
    Id getId(Context* ctx, const char* label);
    void pushId(Context* ctx, const char* label);
    void pushId(Context* ctx, int index);
    void popId(Context* ctx);
    WidgetState* getWidgetState(Context* ctx, Id id);

    // Tells the UI the area it's drawn in, e.g. { 0, 0, 1920, 1080 }, so
    // popups such as an open dropdown's list can choose a direction that
    // keeps them on screen. Call once, and again if it changes. Until it's
    // called, popups assume there's always room below.
    void setScreen(Context* ctx, Rectangle screen);

    // Gives id keyboard focus, taking it from whatever had it; 0 removes
    // focus. Call it in the same id scope as the widget, e.g. right before
    // it. Calling it every frame keeps a widget focused permanently.
    void setFocus(Context* ctx, Id id);

    // Layers
    void setLayer(Context* ctx, Layer layer);

    // Clipping. Draw commands only show inside the current clip rect, and a
    // pushed clip is intersected with the one below it so nesting works.
    void pushClip(Context* ctx, Rectangle rect);
    // Like pushClip, but not limited by the current clip: for popups that
    // must draw outside the panel they were called in. Ended by popClip.
    void pushRootClip(Context* ctx, Rectangle rect);
    void popClip(Context* ctx);

    // True if the mouse is over rect and not clipped away, e.g. by being
    // scrolled out of a panel, and not covered by an open popup (unless the
    // caller is drawing on Layer::Overlay). Widgets should use this for
    // hover checks.
    bool isMouseOver(Context* ctx, Rectangle rect);

    // Draw commands, for building custom widgets
    void drawRect(Context* ctx, Rectangle rect, Color color);
    void drawRectLines(Context* ctx, Rectangle rect, float thickness, Color color);
    void drawText(Context* ctx, const char* text, Vector2 position, float fontSize, Color color);
    void drawRectGradientH(Context* ctx, Rectangle rect, Color left, Color right);
    void drawTexture(Context* ctx, Rectangle rect, TextureRegion region, Color tint);
    void drawTextureNineSlice(
        Context* ctx,
        Rectangle rect,
        TextureRegion region,
        NineSlice nineSlice,
        Color tint
    );

    // Draws skin over rect. animationStart is when a triggered animation
    // started (ctx->time units); looping animations ignore it.
    void drawSkin(Context* ctx, Rectangle rect, const Skin* skin, double animationStart);

    // Draws the skin for a widget's state: pressed while held, hover while
    // hot, otherwise normal. Fades by state->hoverT and state->pressT when
    // all three are Color skins; otherwise switches instantly. Restarts
    // triggered animations when the state changes, using state->skinState.
    void drawStateSkins(
        Context* ctx,
        Rectangle rect,
        const StateSkins* skins,
        WidgetState* state,
        bool hot,
        bool held
    );
    Vector2 measureText(Context* ctx, const char* text, float fontSize);

    // Text styling. Each push overrides the Style's setting for every widget's
    // text until the matching pop; pushes nest, and each kind has its own
    // stack, so pops only have to match pushes of the same kind. endFrame
    // asserts that every push was popped.
    void pushTextColor(Context* ctx, Color color);
    void popTextColor(Context* ctx);
    void pushTextShadow(Context* ctx, TextShadow shadow);
    void popTextShadow(Context* ctx);
    void pushTextUnderline(Context* ctx, TextLine underline);
    void popTextUnderline(Context* ctx);
    void pushTextStrikethrough(Context* ctx, TextLine strikethrough);
    void popTextStrikethrough(Context* ctx);

    // Per-character colors: colors[i] colors character i of every widget's
    // text until the pop, beating pushTextColor. Characters past count use
    // the usual color. The array is copied, so it can be a temporary.
    void pushTextCharacterColors(Context* ctx, const Color* colors, int count);
    void popTextCharacterColors(Context* ctx);

    // Per-character obfuscation: characters i with obfuscated[i] true are
    // drawn as random characters of the same width on screen, changing
    // changesPerSecond times a second (0 for never), until the pop. ASCII
    // characters scramble into ASCII, others into characters from their own
    // block of 256 codepoints, picked uniformly. The array is copied, so it
    // can be a temporary.

    // Pops only remove the newest stack entry. The copied array stays in the
    // frame arena until the next beginFrame, since commands drawn during the
    // push still point at it until render.
    void pushTextObfuscation(
        Context* ctx,
        const bool* obfuscated,
        int count,
        float changesPerSecond
    );
    void popTextObfuscation(Context* ctx);

    // Like drawText, but with the current text styling: a pushed color
    // replaces color, and the current shadow, underline and strikethrough are
    // drawn. Widgets draw their text with this; drawText stays exactly as given.
    void drawStyledText(Context* ctx, const char* text, Vector2 position, float fontSize, Color color);

    // Widgets
    void label(Context* ctx, Rectangle rect, const char* text);
    bool button(Context* ctx, Rectangle rect, const char* text); // true on the frame it is clicked

    // Draws skin over rect: a texture, an animation, or a color. Not
    // interactive. Triggered animations play as if triggered on the first
    // frame, since images have no state.
    void image(Context* ctx, Rectangle rect, const Skin* skin);

    // Scrollable panel. contentHeight is the total height of what goes
    // inside. Returns the content rect: place the panel's widgets relative to
    // it. It is already offset by the scroll position, and anything outside
    // the visible area is clipped. A scrollbar appears when contentHeight is
    // taller than the panel. Every beginPanel needs an endPanel.
    //
    //   Rectangle content = beginPanel(ui, "songs", rect, songCount * rowHeight);
    //   for (int i = 0; i < songCount; i++) {
    //       Rectangle row = { content.x, content.y + i * rowHeight, content.width, rowHeight };
    //       ...
    //   }
    //   endPanel(ui);
    Rectangle beginPanel(Context* ctx, const char* name, Rectangle rect, float contentHeight);
    void endPanel(Context* ctx);

    // A box with a label to its right. Clicking anywhere in rect flips
    // *checked. Returns true on the frame it changes. text is also the id.
    bool checkbox(Context* ctx, Rectangle rect, const char* text, bool* checked);

    // A horizontal slider for *value between min and max. Pressing anywhere
    // on it moves the value to the mouse, and dragging keeps moving it, even
    // outside the slider, until release. step snaps the value to multiples of
    // step from min; 0 means no snapping. Returns true on frames the value
    // changes. name is the id and isn't drawn.
    bool slider(Context* ctx, Rectangle rect, const char* name, float* value, float min, float max, float step);

    // A box showing items[*selected]; clicking it opens a list of items
    // below it, or above it if there's no room below (see setScreen), on
    // Layer::Overlay. Picking an item sets *selected and closes
    // the list; clicking anywhere else closes it without changing anything.
    // *selected may be -1 for nothing selected. Returns true on the frame
    // *selected changes. name is the id and isn't drawn.
    bool dropdown(Context* ctx, Rectangle rect, const char* name, const char* const* items, int itemCount, int* selected);

    // What happened to a text input this frame. Any number can be true at once.
    struct TextInputResult {
        bool changed;   // the text changed
        bool submitted; // Enter was pressed; it also loses focus
        bool lostFocus; // focus went away: Enter, Escape, a click elsewhere, or setFocus
        bool full;      // a typed character was refused because the buffer is full
    };

    // A single-line text box editing buffer, a UTF-8 string of at most
    // capacity bytes including the terminating zero. Click it, or setFocus
    // it, to type. Shows placeholder (may be NULL) in its own color while the
    // text is empty. Text wider than the box scrolls sideways to keep the
    // cursor visible. name is the id and isn't drawn.
    TextInputResult textInput(Context* ctx, Rectangle rect, const char* name, char* buffer, int capacity, const char* placeholder);

}
}
