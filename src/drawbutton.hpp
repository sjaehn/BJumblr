#ifndef DRAWBUTTON_HPP_
#define DRAWBUTTON_HPP_

#include "BWidgets/cairoplus.h"
#include "BWidgets/BColors.hpp"
#include <cmath>

enum ButtonSymbol
{
	BUTTON_NO_SYMBOL,
	BUTTON_UP_SYMBOL,
	BUTTON_DOWN_SYMBOL,
	BUTTON_BOTTOM_SYMBOL,
	BUTTON_HOME_SYMBOL,
};

void drawButton (cairo_t* cr, double x, double y, double width, double height, BColors::Color color)
{
	if ((width <= 0) || (height <= 0)) return;

	// Draw button
	BColors::Color illuminated = color; illuminated.applyBrightness (0.05);
	BColors::Color darkened = color; darkened.applyBrightness (-0.33);
	cairo_pattern_t* pat = cairo_pattern_create_radial (x + width / 2, y + height / 2, 0.125 * width, x + width / 2, y + height / 2, 0.5 * width);

	cairo_pattern_add_color_stop_rgba (pat, 0.0, CAIRO_RGBA (illuminated));
	cairo_pattern_add_color_stop_rgba (pat, 1.0, CAIRO_RGBA (darkened));

	double rad = ((width < 20) || (height < 20) ?  (width < height ? width : height) / 4 : 5);
	cairo_rectangle_rounded (cr, x, y, width, height, rad, 0b1111);
	cairo_set_source (cr, pat);
	cairo_fill (cr);
	cairo_pattern_destroy (pat);
}

void drawButton (cairo_surface_t* surface, double x, double y, double width, double height, BColors::Color color)
{
	cairo_t* cr = cairo_create (surface);
	drawButton (cr, x, y, width, height, color);
	cairo_destroy (cr);
}

void drawButton (cairo_surface_t* surface, double x, double y, double width, double height, BColors::Color color, const ButtonSymbol symbol)
{
	cairo_t* cr = cairo_create (surface);
	drawButton (cr, x, y, width, height, color);

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba (cr, CAIRO_RGBA (BColors::black));

	switch (symbol)
	{
		case BUTTON_UP_SYMBOL:		cairo_move_to (cr, x + width * 0.375, y + height * 0.625);
						cairo_line_to (cr, x + width * 0.5, y + height * 0.375);
						cairo_line_to (cr, x + width * 0.625, y + height * 0.625);
						cairo_stroke (cr);
						break;

		case BUTTON_DOWN_SYMBOL:	cairo_move_to (cr, x + width * 0.375, y + height * 0.375);
						cairo_line_to (cr, x + width * 0.5, y + height * 0.625);
						cairo_line_to (cr, x + width * 0.625, y + height * 0.375);
						cairo_stroke (cr);
						break;

		case BUTTON_BOTTOM_SYMBOL:	cairo_move_to (cr, x + width * 0.375, y + height * 0.375);
						cairo_arc (cr, x + width * 0.5, y + height * 0.5, 0.125 * height, - 0.5 * M_PI, 0.5 * M_PI);
						cairo_line_to (cr, x + width * 0.375, y + height * 0.625);
						cairo_move_to (cr, x + width * 0.5, y + height * 0.5);
						cairo_line_to (cr, x + width * 0.375, y + height * 0.625);
						cairo_line_to (cr, x + width * 0.5, y + height * 0.75);
						cairo_stroke (cr);
						break;

		case BUTTON_HOME_SYMBOL:	cairo_move_to (cr, x + width/2, y + height/2 - 0.375 * height);
						cairo_line_to (cr, x + width/2 + 0.375 * height, y + height/2);

						cairo_move_to (cr, x + width/2 + 0.3 * height, y + height/2 - 0.075 * height);
						cairo_line_to (cr, x + width/2 + 0.3 * height, y + height/2 + 0.375 * height);
						cairo_line_to (cr, x + width/2 + 0.3 * height, y + height/2 + 0.375 * height);
						cairo_line_to (cr, x + width/2, y + height/2 + 0.375 * height);
						cairo_line_to (cr, x + width/2, y + height/2 + 0.125 * height);
						cairo_line_to (cr, x + width/2 - 0.15 * height, y + height/2 + 0.125 * height);
						cairo_line_to (cr, x + width/2 - 0.15 * height, y + height/2 + 0.375 * height);
						cairo_line_to (cr, x + width/2 - 0.3 * height, y + height/2 + 0.375 * height);
						cairo_line_to (cr, x + width/2 - 0.3 * height, y + height/2 - 0.075 * height);

						cairo_move_to (cr, x + width/2, y + height/2 - 0.375 * height);
						cairo_line_to (cr, x + width/2 - 0.15 * height, y + height/2 - 0.225 * height);
						cairo_line_to (cr, x + width/2 - 0.15 * height, y + height/2 - 0.375 * height);
						cairo_line_to (cr, x + width/2 - 0.2 * height, y + height/2 - 0.375 * height);
						cairo_line_to (cr, x + width/2 - 0.2 * height, y + height/2 - 0.175 * height);
						cairo_line_to (cr, x + width/2 - 0.375 * height, y + height/2);
						cairo_stroke (cr);
						break;

		default:			break;
	}
	cairo_destroy (cr);
}


#endif /* DRAWBUTTON_HPP_ */
