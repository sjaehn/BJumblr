/* B.Jumblr
 * Pattern-controlled audio stream / sample re-sequencer LV2 plugin
 *
 * Copyright (C) 2018 - 2020 by Sven Jähnichen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BWIDGETS_MARKERWIDGET_HPP_
#define BWIDGETS_MARKERWIDGET_HPP_

#include "BWidgets/Widget.hpp"

enum MarkerIndex
{
	MARKER_FWD,
	MARKER_REV
};

class MarkerWidget : public BWidgets::Widget
{
public:
	MarkerWidget () : MarkerWidget (0.0, 0.0, 0.0, 0.0, "marker") {}
	MarkerWidget (const double x, const double y, const double width, const double height, const std::string& name, MarkerIndex index = MARKER_FWD) :
		Widget (x, y, width, height, name),
		index_ (index)
	{}

	/**
	 * Pattern cloning. Creates a new instance of the widget and copies all
	 * its properties.
	 */
	virtual Widget* clone () const override {return new MarkerWidget (*this);}

protected:
	MarkerIndex index_;

	virtual void draw (const BUtilities::RectArea& area) override
	{
		if ((!widgetSurface_) || (cairo_surface_status (widgetSurface_) != CAIRO_STATUS_SUCCESS)) return;

		if ((getWidth () >= 6) && (getHeight () >= 6))
		{

			Widget::draw (area);

			cairo_t* cr = cairo_create (widgetSurface_);
			if (cairo_status (cr) == CAIRO_STATUS_SUCCESS)
			{
				// Limit cairo-drawing area
				cairo_rectangle (cr, area.getX (), area.getY (), area.getWidth (), area.getHeight ());
				cairo_clip (cr);

				double x0 = getXOffset ();
				double y0 = getYOffset ();
				double w = getEffectiveWidth ();
				double h = getEffectiveHeight ();
				double size = (w < h ? 0.8 * w : 0.8 * h);

				// Symbol
				cairo_set_line_width (cr, 0);
				switch (index_)
				{
					case MARKER_FWD:	cairo_move_to (cr, x0 + w/2 - 0.25 * size, y0 + h/2 - 0.375 * size);
								cairo_line_to (cr, x0 + w/2 + 0.25 * size, y0 + h/2);
								cairo_line_to (cr, x0 + w/2 - 0.25 * size, y0 + h/2 + 0.375 * size);
								break;

					case MARKER_REV:	cairo_move_to (cr, x0 + w/2 + 0.25 * size, y0 + h/2 - 0.375 * size);
								cairo_line_to (cr, x0 + w/2 - 0.25 * size, y0 + h/2);
								cairo_line_to (cr, x0 + w/2 + 0.25 * size, y0 + h/2 + 0.375 * size);
								break;
				}
				cairo_close_path (cr);
				cairo_set_source_rgba (cr, CAIRO_RGBA (BColors::yellow));
				cairo_fill (cr);

				cairo_destroy (cr);
			}
		}
	}
};

#endif /* BWIDGETS_MARKERWIDGET_HPP_ */
