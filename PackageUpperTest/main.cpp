#include <stdio.h>
#include <raylib.h>
#include <o2/gui/gui.h>
#include <o2/gui/gui_layout.h>
#include <o2/gui/gui_raylib.h>
#include <o2/jam/audio.h>
#include <o2/jam/stream.h>

// A small program that uses O2Jam and O2Gui, both installed into deps/ by
// PackageUpper: one button starts a song, the other stops it.

static const char* SONG_PATH = "resources/example_song.mp3";

static const int WINDOW_WIDTH = 640;
static const int WINDOW_HEIGHT = 360;

// The UI's memory lives in static storage, so the program never calls malloc.
static o2::gui::Context ui;
static unsigned char uiFrameMemory[256 * 1024];

// Writes seconds as m:ss
static void formatTime(
    char* buffer,
    int capacity,
    double seconds
) {
    if (seconds < 0) {
        seconds = 0;
    }
    int total = (int)seconds;
    snprintf(buffer, capacity, "%d:%02d", total / 60, total % 60);
}

int main() {
    if (!o2::jam::init(o2::jam::DEFAULT_DEVICE)) {
        printf("Couldn't start audio: %s\n", o2::jam::getErrorMessage());
        return 1;
    }

    o2::jam::Stream song = {};
    char loadError[256] = "";
    if (!o2::jam::load(&song, SONG_PATH)) {
        // keep running, so the window can show what went wrong
        snprintf(loadError, sizeof(loadError), "Couldn't load %s: %s", SONG_PATH, o2::jam::getErrorMessage());
        printf("%s\n", loadError);
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "PackageUpperTest - O2Jam + O2Gui");
    SetTargetFPS(144);

    o2::gui::init(&ui, uiFrameMemory, sizeof(uiFrameMemory), 1024);
    o2::gui::raylib::TextFont font = o2::gui::raylib::defaultTextFont();
    o2::gui::raylib::useFont(&ui, &font);

    o2::gui::Input input = {};
    char status[128];

    while (!WindowShouldClose()) {
        Rectangle screen = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
        o2::gui::setScreen(&ui, screen);

        o2::gui::raylib::readInput(&input);
        o2::gui::beginFrame(&ui, &input, GetFrameTime());

        Rectangle area = o2::gui::layout::center(screen, 420, 150);
        o2::gui::label(&ui, o2::gui::layout::cutTop(&area, 30), "O2Jam + O2Gui, installed with PackageUpper");
        o2::gui::layout::cutTop(&area, 20);

        Rectangle buttons = o2::gui::layout::cutTop(&area, 50);
        Rectangle playRect = o2::gui::layout::cutLeft(&buttons, 200);
        o2::gui::layout::cutLeft(&buttons, 20);
        Rectangle stopRect = buttons;

        if (o2::gui::button(&ui, playRect, "Play")) {
            if (!o2::jam::play(&song)) {
                printf("Couldn't play: %s\n", o2::jam::getErrorMessage());
            }
        }
        if (o2::gui::button(&ui, stopRect, "Stop")) {
            o2::jam::stop(&song);
        }
        o2::gui::layout::cutTop(&area, 20);

        if (loadError[0] != '\0') {
            o2::gui::label(&ui, o2::gui::layout::cutTop(&area, 30), "Couldn't load the song, see the console");
        } else {
            char position[16];
            char length[16];
            formatTime(position, sizeof(position), o2::jam::getPosition(&song));
            formatTime(length, sizeof(length), song.length);
            const char* state = o2::jam::isPlaying(&song) ? "Playing" : "Stopped";
            snprintf(status, sizeof(status), "%s  %s / %s", state, position, length);
            o2::gui::label(&ui, o2::gui::layout::cutTop(&area, 30), status);
        }

        o2::gui::endFrame(&ui);

        BeginDrawing();
        ClearBackground(Color{ 24, 24, 30, 255 });
        o2::gui::raylib::render(&ui);
        EndDrawing();
    }

    CloseWindow();
    o2::jam::shutdown();
    return 0;
}
