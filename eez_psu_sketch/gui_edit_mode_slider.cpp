/*
 * EEZ PSU Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "psu.h"

#if OPTION_DISPLAY

#include "gui_edit_mode.h"

namespace eez {
namespace psu {
namespace gui {
namespace edit_mode_slider {

bool scale_is_vertical;
int scale_width = 34;
float scale_height = 204;

static int start_y;
static float start_value;

static int last_scale;

////////////////////////////////////////////////////////////////////////////////

int touchY() {
	return scale_is_vertical ? touch::y : touch::x;
}

int touchX() {
	return scale_is_vertical ? touch::x : touch::y;
}

int displayXSize() {
	return scale_is_vertical ? lcd::lcd.getDisplayWidth() : lcd::lcd.getDisplayHeight();
}

void increment(int counter) {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();

    float value = edit_mode::getEditValue().getFloat() + 0.01f * counter;

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    edit_mode::setValue(value);
}

void onTouchDown() {
    start_value = edit_mode::getEditValue().getFloat();
    start_y = touchY();

    last_scale = 1;
}

void onTouchMove() {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();

    int scale;

    int x = (touchX() / 20) * 20;
    if (x < scale_width) {
        scale = 1;
    }
    else {
        int num_bars = (max - min) >= 10 ? 9 : 5;
        int bar_width = (displayXSize() - scale_width) / num_bars;
        scale = 1 << (1 + (x - scale_width) / bar_width);
    }

    if (scale != last_scale) {
        start_value = edit_mode::getEditValue().getFloat();
        start_y = touchY();
        last_scale = scale;
    }

    float value = start_value + (scale_is_vertical ? 1 : -1) * (start_y - touchY()) * (max - min) / (scale * scale_height);

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    edit_mode::setValue(value);
}

void onTouchUp() {
}

}
}
}
} // namespace eez::psu::gui::edit_mode_slider

#endif