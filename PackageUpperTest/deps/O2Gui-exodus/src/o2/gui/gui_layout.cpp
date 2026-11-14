#include <o2/gui/gui_layout.h>
#include <raymath.h>
#include <assert.h>
#include <math.h>

namespace o2 {
namespace gui {
namespace layout {

    Rectangle cutTop(Rectangle* rect, float height) {
        height = Clamp(height, 0.0f, rect->height);

        Rectangle piece = { rect->x, rect->y, rect->width, height };
        rect->y += height;
        rect->height -= height;
        return piece;
    }

    Rectangle cutBottom(Rectangle* rect, float height) {
        height = Clamp(height, 0.0f, rect->height);

        rect->height -= height;
        Rectangle piece = { rect->x, rect->y + rect->height, rect->width, height };
        return piece;
    }

    Rectangle cutLeft(Rectangle* rect, float width) {
        width = Clamp(width, 0.0f, rect->width);

        Rectangle piece = { rect->x, rect->y, width, rect->height };
        rect->x += width;
        rect->width -= width;
        return piece;
    }

    Rectangle cutRight(Rectangle* rect, float width) {
        width = Clamp(width, 0.0f, rect->width);

        rect->width -= width;
        Rectangle piece = { rect->x + rect->width, rect->y, width, rect->height };
        return piece;
    }

    Rectangle inset(Rectangle rect, float amount) {
        return inset(rect, amount, amount);
    }

    Rectangle inset(Rectangle rect, float horizontal, float vertical) {
        // never shrink past zero, so a too-small rect collapses onto its center
        horizontal = Clamp(horizontal, 0.0f, rect.width * 0.5f);
        vertical = Clamp(vertical, 0.0f, rect.height * 0.5f);

        Rectangle result = {
            rect.x + horizontal,
            rect.y + vertical,
            rect.width - 2.0f * horizontal,
            rect.height - 2.0f * vertical
        };
        return result;
    }

    Rectangle center(Rectangle rect, float width, float height) {
        Rectangle result = {
            rect.x + (rect.width - width) * 0.5f,
            rect.y + (rect.height - height) * 0.5f,
            width,
            height
        };
        return result;
    }

    Grid makeGrid(Rectangle area, int columns, float cellHeight, float gap) {
        assert(columns > 0);

        Grid grid;
        grid.area = area;
        grid.columns = columns;
        grid.cellHeight = cellHeight;
        grid.gap = gap;
        return grid;
    }

    Grid makeGridFit(Rectangle area, int columns, int rows, float gap) {
        assert(rows > 0);

        // rows cells and (rows - 1) gaps fill the height, rounded down to whole pixels
        float cellHeight = floorf((area.height - gap * (float)(rows - 1)) / (float)rows);
        if (cellHeight < 0.0f) cellHeight = 0.0f;
        return makeGrid(area, columns, cellHeight, gap);
    }

    Rectangle gridCell(const Grid* grid, int index) {
        assert(index >= 0);
        return gridCell(grid, index % grid->columns, index / grid->columns);
    }

    Rectangle gridCell(const Grid* grid, int column, int row) {
        // columns cells and (columns - 1) gaps fill the width, rounded down to whole pixels
        float cellWidth = floorf((grid->area.width - grid->gap * (float)(grid->columns - 1)) / (float)grid->columns);
        if (cellWidth < 0.0f) cellWidth = 0.0f;

        Rectangle cell = {
            grid->area.x + (float)column * (cellWidth + grid->gap),
            grid->area.y + (float)row * (grid->cellHeight + grid->gap),
            cellWidth,
            grid->cellHeight
        };
        return cell;
    }

    float gridHeight(int count, int columns, float cellHeight, float gap) {
        assert(columns > 0);
        if (count <= 0) return 0.0f;

        // round up: a partly filled last row still takes a full row
        int rows = (count + columns - 1) / columns;
        return (float)rows * cellHeight + (float)(rows - 1) * gap;
    }

}
}
}
