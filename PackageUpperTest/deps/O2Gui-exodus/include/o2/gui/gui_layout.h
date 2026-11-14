#pragma once
#include <raylib.h>

// Layout helpers for o2::gui. Plain rectangle math, no state.
//
// Rect-cut: start with a rectangle and cut pieces off its sides. Each cut
// returns the piece and shrinks the original, so the next cut continues from
// what is left. Cuts larger than what is left are clamped, so sizes never go
// negative.
//
//   Rectangle screen = { 0, 0, 800, 450 };
//   Rectangle area = inset(screen, 20);
//   Rectangle sidebar = cutLeft(&area, 200);
//   cutLeft(&area, 20);                 // a gap is just a cut you don't use
//   Rectangle header = cutTop(&area, 40);
//   // area is now what's left: the main content
//
// Grid: fixed-size cells in rows, filled left to right, top to bottom.
// gridHeight gives the total height for a number of items, which is what a
// panel needs as its contentHeight.
//
//   float height = gridHeight(songCount, 4, 60, 8);
//   Rectangle content = beginPanel(ui, "songs", rect, height);
//   Grid grid = makeGrid(content, 4, 60, 8);
//   for (int i = 0; i < songCount; i++) {
//       button(ui, gridCell(&grid, i), ...);
//   }
//   endPanel(ui);

namespace o2 {
namespace gui {
namespace layout {

    // Rect-cut
    Rectangle cutTop(Rectangle* rect, float height);
    Rectangle cutBottom(Rectangle* rect, float height);
    Rectangle cutLeft(Rectangle* rect, float width);
    Rectangle cutRight(Rectangle* rect, float width);

    // Shrinks every side by amount, or the left/right and top/bottom sides separately
    Rectangle inset(Rectangle rect, float amount);
    Rectangle inset(Rectangle rect, float horizontal, float vertical);

    // A width x height rect centered inside rect
    Rectangle center(Rectangle rect, float width, float height);

    // Grid
    // Cell width isn't set directly: it's chosen so that `columns` cells and
    // the gaps between them fill area.width, rounded down to a whole pixel so
    // cells don't land on half pixels. Because of the rounding, the grid can
    // end up to (columns - 1) pixels narrower than area.
    struct Grid {
        Rectangle area; // the grid starts at area's top-left; see above for cell width
        int columns;
        float cellHeight;
        float gap;      // space between cells, horizontally and vertically
    };

    Grid makeGrid(Rectangle area, int columns, float cellHeight, float gap);

    // A grid whose rows fill area's height. Cell height is rounded down to a
    // whole pixel, like cell width.
    Grid makeGridFit(Rectangle area, int columns, int rows, float gap);

    Rectangle gridCell(const Grid* grid, int index);
    Rectangle gridCell(const Grid* grid, int column, int row);

    // Total height of a grid holding count items
    float gridHeight(int count, int columns, float cellHeight, float gap);

}
}
}
