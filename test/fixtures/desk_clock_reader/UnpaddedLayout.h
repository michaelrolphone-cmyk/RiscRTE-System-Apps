/* Test-only layout reference for the preserved unpadded-hour feature.
 * DeskClockFaces.h remains the byte-exact Reader 34d8e694 reference. Reuse its
 * actual segment/glyph drawing; change only the documented one-digit layout.
 * This file is not an original Reader input or a production renderer.
 */
#pragma once
#include "DeskClockFaces.h"

namespace DeskClockUnpaddedLayout {
template<class Canvas>
void draw(Canvas& c, uint8_t selection, int hour24, int minute, bool use12Hour, bool valid) {
    using namespace DeskClockFaces;
    const Face face = sanitize(selection);
    const int hour = use12Hour ? (hour24 % 12 ? hour24 % 12 : 12) : hour24;
    if (!valid || hour >= 10 || face >= Minimal) {
        DeskClockFaces::draw(c, selection, hour24, minute, use12Hour, valid);
        return;
    }
    const int width = c.getScreenWidth(), height = c.getScreenHeight();
    if (face == Segments) {
        // Three six-unit digits, two one-unit gaps, and a two-unit colon gap.
        // The colon is at column 7; minute digits start at columns 9 and 16.
        const int unit = std::max(4, std::min((width - 96) / 22, (height - 200) / 10));
        const int left = (width - 22 * unit) / 2, top = (height - 10 * unit) / 2;
        digit(c, hour, left, top, unit);
        digit(c, minute / 10, left + 9 * unit, top, unit);
        digit(c, minute % 10, left + 16 * unit, top, unit);
        c.fillRect(left + 7 * unit, top + 3 * unit, unit, unit);
        c.fillRect(left + 7 * unit, top + 6 * unit, unit, unit);
        return;
    }
    // Reader already supports a one-digit 12-hour Noto layout. Use the same
    // glyphs/spacing also for 24-hour zero through nine (including midnight).
    const bool serif = face == Serif;
    const int values[] = {hour, 10, minute / 10, minute % 10};
    int naturalWidth = 3 * 12;
    for (int value : values) naturalWidth += numeralWidth(value, 224, serif);
    const int size = std::min(224, std::min(height - 200, (width - 96) * 224 / naturalWidth));
    const int gap = 12 * size / 224;
    int total = 3 * gap;
    for (int value : values) total += numeralWidth(value, size, serif);
    int x = (width - total) / 2;
    for (int value : values) {
        numeral(c, value, x, (height - size) / 2, size, serif);
        x += numeralWidth(value, size, serif) + gap;
    }
}
}  // namespace DeskClockUnpaddedLayout
